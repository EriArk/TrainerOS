// SPDX-License-Identifier: GPL-3.0-or-later
// Moonlight client core with GStreamer codec callbacks; isolated diagnostic UI.
#include "common.hpp"
#include <atomic>
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>
extern "C" {
#include "Limelight.h"
}
struct Guest {
    const Config& cfg;
    GstElement *video=nullptr,*audio=nullptr,*videoSource=nullptr,*audioSource=nullptr;
    std::atomic<bool> ended{false};
    std::atomic<unsigned> frames{0},decoded{0},audioPackets{0},audioDecoded{0},audioPeak{0};
    std::atomic<int> currentColor{-1},pendingColor{-1};
    std::atomic<uint64_t> sentAt{0};
    std::mutex measurementsLock;
    std::vector<double> roundtrips,decodeTimes;
    std::map<uint64_t,uint64_t> decodeStarts;
    explicit Guest(const Config& c):cfg(c){}
    bool pipeline(const std::string& desc,GstElement*& target,GstElement*& source) {
        GError* error=nullptr;target=gst_parse_launch(desc.c_str(),&error);
        if(error){std::fprintf(stderr,"pipeline %s\n",error->message);g_error_free(error);return false;}
        source=gst_bin_get_by_name(GST_BIN(target),"in");
        return gst_element_set_state(target,GST_STATE_PLAYING)!=GST_STATE_CHANGE_FAILURE;
    }
    void errors(GstElement* pipeline) {
        if(!pipeline)return;auto bus=gst_element_get_bus(pipeline);
        while(auto msg=gst_bus_pop_filtered(bus,GstMessageType(GST_MESSAGE_ERROR|GST_MESSAGE_WARNING))) {
            GError* error=nullptr;gchar* debug=nullptr;
            if(GST_MESSAGE_TYPE(msg)==GST_MESSAGE_ERROR){gst_message_parse_error(msg,&error,&debug);ended=true;}
            else gst_message_parse_warning(msg,&error,&debug);
            std::fprintf(stderr,"guest media: %s (%s)\n",error->message,debug?debug:"");
            g_error_free(error);g_free(debug);gst_message_unref(msg);
        }
        gst_object_unref(bus);
    }
    static Guest* active;
    static int videoSetup(int format,int width,int height,int fps,void*,int) {
        if(format!=VIDEO_FORMAT_H264||width!=1280||height!=720||fps!=60)return -1;
        auto& g=*active;
        std::string desc="appsrc name=in is-live=true block=false format=time do-timestamp=false max-buffers=2 leaky-type=downstream caps=\"video/x-h264,stream-format=byte-stream,alignment=au,framerate=60/1\" ! h264parse ! v4l2h264dec ! videoconvert ! video/x-raw,format=RGBA ! tee name=decoded ";
        desc+="decoded. ! queue max-size-buffers=1 max-size-time=0 max-size-bytes=0 leaky=downstream ! videoconvert ! video/x-raw,format=RGB ! appsink name=measure sync=false max-buffers=1 drop=true ";
        if(g.cfg.display)desc+="decoded. ! queue max-size-buffers=1 max-size-time=0 max-size-bytes=0 leaky=downstream ! glimagesink name=renderer sync=false ";
        if(!g.pipeline(desc,g.video,g.videoSource))return -1;
        auto sink=gst_bin_get_by_name(GST_BIN(g.video),"measure");
        GstAppSinkCallbacks cb{};cb.new_sample=onFrame;gst_app_sink_set_callbacks(GST_APP_SINK(sink),&cb,&g,nullptr);gst_object_unref(sink);
        return 0;
    }
    static GstFlowReturn onFrame(GstAppSink* sink,void* data) {
        auto& g=*static_cast<Guest*>(data);auto sample=gst_app_sink_pull_sample(sink);if(!sample)return GST_FLOW_EOS;
        ++g.decoded;auto buffer=gst_sample_get_buffer(sample);GstVideoInfo info{};GstVideoFrame frame{};
        if(g.decoded==1){auto caps=gst_caps_to_string(gst_sample_get_caps(sample));std::fprintf(stderr,"decoded_caps %s\n",caps);g_free(caps);}
        if(gst_video_info_from_caps(&info,gst_sample_get_caps(sample))&&gst_video_frame_map(&frame,&info,buffer,GST_MAP_READ)) {
            auto* p=static_cast<unsigned char*>(GST_VIDEO_FRAME_PLANE_DATA(&frame,0));
            const int stride=GST_VIDEO_FRAME_PLANE_STRIDE(&frame,0);
            uint64_t red=0,green=0;
            for(int y=100;y<200;y+=10)for(int x=100;x<200;x+=10){red+=p[y*stride+x*3];green+=p[y*stride+x*3+1];}
            const int color=green>red*2?1:red>green*2?0:-1;
            g.currentColor=color;
            const auto now=monotonicNs();
            if(color>=0&&g.pendingColor.load()==color) {
                const auto start=g.sentAt.load();
                if(start && now>start) {
                    std::lock_guard lock(g.measurementsLock);g.roundtrips.push_back((now-start)/1e6);
                    g.pendingColor=-1;
                }
            }
            if(GST_BUFFER_PTS_IS_VALID(buffer)) {
                std::lock_guard lock(g.measurementsLock);
                auto found=g.decodeStarts.find(GST_BUFFER_PTS(buffer));
                if(found!=g.decodeStarts.end()) {
                    g.decodeTimes.push_back((now-found->second)/1e6);
                    g.decodeStarts.erase(g.decodeStarts.begin(),std::next(found));
                }
            }
            gst_video_frame_unmap(&frame);
        }
        gst_sample_unref(sample);return GST_FLOW_OK;
    }
    static int submitVideo(PDECODE_UNIT du) {
        auto& g=*active;if(g.ended||!g.videoSource)return DR_OK;
        if(du->fullLength<=0||du->fullLength>16*1024*1024)return DR_NEED_IDR;
        auto buffer=gst_buffer_new_allocate(nullptr,du->fullLength,nullptr);size_t pos=0;
        for(auto entry=du->bufferList;entry;entry=entry->next) {
            if(entry->length<=0 ||pos+entry->length>size_t(du->fullLength)){gst_buffer_unref(buffer);return DR_NEED_IDR;}
            gst_buffer_fill(buffer,pos,entry->data,entry->length);pos+=entry->length;
        }
        if(pos!=size_t(du->fullLength)){gst_buffer_unref(buffer);return DR_NEED_IDR;}
        auto clock=gst_element_get_clock(g.video);
        if(!clock){gst_buffer_unref(buffer);return DR_NEED_IDR;}
        GST_BUFFER_PTS(buffer)=gst_clock_get_time(clock)-gst_element_get_base_time(g.video);gst_object_unref(clock);
        {std::lock_guard lock(g.measurementsLock);
            if(g.decodeStarts.size()>512)g.decodeStarts.erase(g.decodeStarts.begin());
            g.decodeStarts[GST_BUFFER_PTS(buffer)]=monotonicNs();}
        ++g.frames;
        return gst_app_src_push_buffer(GST_APP_SRC(g.videoSource),buffer)==GST_FLOW_OK?DR_OK:DR_NEED_IDR;
    }
    static int audioInit(int,const POPUS_MULTISTREAM_CONFIGURATION opus,void*,int) {
        auto& g=*active;if(opus->channelCount!=2)return -1;
        if(!g.pipeline("appsrc name=in is-live=true block=false format=time do-timestamp=true max-buffers=8 leaky-type=downstream caps=\"audio/x-opus,rate=48000,channels=2,channel-mapping-family=0\" ! opusdec ! audioconvert ! audio/x-raw,format=S16LE ! appsink name=measure sync=false max-buffers=8 drop=true",g.audio,g.audioSource))return -1;
        auto sink=gst_bin_get_by_name(GST_BIN(g.audio),"measure");GstAppSinkCallbacks cb{};
        cb.new_sample=[](GstAppSink* sink,void* ptr){auto& g=*static_cast<Guest*>(ptr);auto s=gst_app_sink_pull_sample(sink);if(!s)return GST_FLOW_EOS;
            ++g.audioDecoded;GstMapInfo map{};auto b=gst_sample_get_buffer(s);
            if(gst_buffer_map(b,&map,GST_MAP_READ)) {auto p=reinterpret_cast<const int16_t*>(map.data);unsigned peak=0;
                for(size_t n=0;n<map.size/2;++n)peak=std::max(peak,unsigned(std::abs(int(p[n]))));
                g.audioPeak=std::max(g.audioPeak.load(),peak);gst_buffer_unmap(b,&map);}
            gst_sample_unref(s);return GST_FLOW_OK;};
        gst_app_sink_set_callbacks(GST_APP_SINK(sink),&cb,&g,nullptr);gst_object_unref(sink);return 0;
    }
    static void audioSample(char* data,int size) {
        auto& g=*active;if(size<=0||size>4096||g.ended||!g.audioSource)return;
        auto buffer=gst_buffer_new_allocate(nullptr,size,nullptr);gst_buffer_fill(buffer,0,data,size);++g.audioPackets;
        gst_app_src_push_buffer(GST_APP_SRC(g.audioSource),buffer);
    }
    void cleanup() {
        for(auto pipeline:{video,audio})if(pipeline){gst_element_set_state(pipeline,GST_STATE_NULL);gst_object_unref(pipeline);}
        for(auto source:{videoSource,audioSource})if(source)gst_object_unref(source);
        video=audio=videoSource=audioSource=nullptr;
    }
};
Guest* Guest::active=nullptr;
void logMessage(const char* format,...) {va_list ap;va_start(ap,format);vfprintf(stderr,format,ap);va_end(ap);}
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    try {
        Config cfg(argv[1]);gst_init(nullptr,nullptr);Guest g(cfg);Guest::active=&g;
        SERVER_INFORMATION info{};LiInitializeServerInformation(&info);
        std::string url="rtsp://"+cfg.address+":"+std::to_string(RtspPort);
        info.address=cfg.address.c_str();info.serverInfoAppVersion="7.1.431.-1";info.serverInfoGfeVersion="3.23.0.74";
        info.rtspSessionUrl=url.c_str();info.serverCodecModeSupport=SCM_H264;
        STREAM_CONFIGURATION stream{};LiInitializeStreamConfiguration(&stream);
        stream.width=1280;stream.height=720;stream.fps=60;stream.bitrate=8000;stream.packetSize=1024;
        stream.streamingRemotely=STREAM_CFG_LOCAL;stream.audioConfiguration=AUDIO_CONFIGURATION_STEREO;
        stream.supportedVideoFormats=VIDEO_FORMAT_H264;stream.encryptionFlags=ENCFLG_ALL;
        memcpy(stream.remoteInputAesKey,cfg.key.data(),16);stream.remoteInputAesIv[3]=1;
        DECODER_RENDERER_CALLBACKS video{};LiInitializeVideoCallbacks(&video);
        video.setup=Guest::videoSetup;video.submitDecodeUnit=Guest::submitVideo;
        AUDIO_RENDERER_CALLBACKS audio{};LiInitializeAudioCallbacks(&audio);
        audio.init=Guest::audioInit;audio.decodeAndPlaySample=Guest::audioSample;
        CONNECTION_LISTENER_CALLBACKS listener{};LiInitializeConnectionCallbacks(&listener);
        listener.logMessage=logMessage;
        listener.connectionTerminated=[](int why){std::fprintf(stderr,"terminated %d\n",why);Guest::active->ended=true;};
        const int result=LiStartConnection(&info,&stream,&listener,&video,&audio,nullptr,0,nullptr,0);
        if(result){std::fprintf(stderr,"connection failed %d\n",result);g.cleanup();return 1;}
        auto end=monotonicNs()+uint64_t(cfg.seconds)*1000000000;uint64_t next=monotonicNs()+1000000000;
        while(!g.ended && monotonicNs()<end) {
            g.errors(g.video);g.errors(g.audio);
            const auto now=monotonicNs();
            if(!cfg.game&&now>next&&g.currentColor>=0&&g.pendingColor<0) {
                const int target=1-g.currentColor.load();g.sentAt=now;g.pendingColor=target;
                LiSendMultiControllerEvent(0,1,target?A_FLAG:0,0,0,0,0,0,0);next=now+400000000;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        LiSendMultiControllerEvent(0,1,0,0,0,0,0,0,0);LiStopConnection();g.cleanup();
        std::sort(g.roundtrips.begin(),g.roundtrips.end());
        const auto n=g.roundtrips.size();
        const double p50=n?g.roundtrips[(n-1)/2]:0,p95=n?g.roundtrips[std::min(n-1,size_t(n*.95))]:0;
        std::sort(g.decodeTimes.begin(),g.decodeTimes.end());
        const auto d=g.decodeTimes.size();
        std::fprintf(stderr,"decode_samples=%zu decode_p50_ms=%.3f decode_p95_ms=%.3f\n",d,d?g.decodeTimes[(d-1)/2]:0,d?g.decodeTimes[std::min(d-1,size_t(d*.95))]:0);
        std::printf("{\"frames\":%u,\"decoded\":%u,\"audio_packets\":%u,\"audio_decoded\":%u,\"audio_peak\":%u,\"input_to_decoded_samples\":%zu,\"input_to_decoded_p50_ms\":%.3f,\"input_to_decoded_p95_ms\":%.3f,\"display_requested\":%s}\n",
            g.frames.load(),g.decoded.load(),g.audioPackets.load(),g.audioDecoded.load(),g.audioPeak.load(),n,p50,p95,cfg.display?"true":"false");
        return g.decoded>60&&g.audioDecoded>100&&g.audioPeak>100&&(cfg.game||n>=5)?0:1;
    }catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}
