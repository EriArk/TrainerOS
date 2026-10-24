// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "common.hpp"
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>
#include <gst/video/video-event.h>
#include <atomic>
#include <array>
#include <thread>
#include <mutex>
#include <vector>
#include <poll.h>

// An I-frame flag is insufficient for joining an existing H.264 stream. The
// receiver needs IDR plus its SPS/PPS, not references to earlier pictures.
inline bool restartableH264(GstBuffer* buffer) {
    if(!buffer||GST_BUFFER_FLAG_IS_SET(buffer,GST_BUFFER_FLAG_DELTA_UNIT))return false;
    GstMapInfo map{};if(!gst_buffer_map(buffer,&map,GST_MAP_READ))return false;
    bool idr=false,sps=false,pps=false;
    for(size_t n=0;n+4<map.size;++n)if(map.data[n]==0&&map.data[n+1]==0&&map.data[n+2]==1) {
        const auto type=map.data[n+3]&31;idr|=type==5;sps|=type==7;pps|=type==8;
    }
    gst_buffer_unmap(buffer,&map);return idr&&sps&&pps;
}

// One capture/encoder per owned game. Only encoded buffers are shared; every
// subscriber retains its own packetizer, encryption, input lease and lifetime.
class SharedSource {
    struct Sink {GstElement* source;bool audio,needsKeyframe;};
    std::mutex lock_;
    std::vector<Sink> sinks_;
    GstElement *video_=nullptr,*audio_=nullptr;
    int audioFd_;
    std::thread videoReader_,audioReader_;
    std::atomic<bool> videoSubscribed_{false};
    bool readBytes(int fd,void* data,size_t size) {
        auto bytes=static_cast<char*>(data);
        while(size&&!stopped) {
            pollfd p{fd,POLLIN,0};if(poll(&p,1,50)<=0)continue;
            auto n=read(fd,bytes,size);if(n<=0){stopped=true;return false;}
            bytes+=n;size-=n;
        }
        return size==0;
    }
    static GstFlowReturn sample(GstAppSink* sink,void* pointer) {
        auto& self=*static_cast<SharedSource*>(pointer);
        auto sample=gst_app_sink_pull_sample(sink);if(!sample)return GST_FLOW_EOS;
        const bool audio=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(sink),"audio"));
        auto buffer=gst_sample_get_buffer(sample);
        const bool restartable=!audio&&restartableH264(buffer);
        if(buffer) {
            std::lock_guard guard(self.lock_);
            for(auto& target:self.sinks_) {
                if(target.audio!=audio)continue;
                if(!audio&&target.needsKeyframe) {
                    if(!restartable)continue;
                    target.needsKeyframe=false;
                }
                // Each receiver's pipeline has its own base time. Share memory,
                // but give appsrc fresh timestamps, never the encoder's clock.
                auto copy=gst_buffer_copy(buffer);GST_BUFFER_PTS(copy)=GST_CLOCK_TIME_NONE;GST_BUFFER_DTS(copy)=GST_CLOCK_TIME_NONE;
                gst_app_src_push_buffer(GST_APP_SRC(target.source),copy);
            }
        }
        gst_sample_unref(sample);return GST_FLOW_OK;
    }
    GstElement* pipeline(const char* description,bool audio) {
        GError* error=nullptr;auto p=gst_parse_launch(description,&error);
        if(error){g_error_free(error);if(p)gst_object_unref(p);throw std::runtime_error("Shared encoder unavailable");}
        auto sink=gst_bin_get_by_name(GST_BIN(p),"out");g_object_set_data(G_OBJECT(sink),"audio",GINT_TO_POINTER(audio));
        GstAppSinkCallbacks cb{};cb.new_sample=sample;gst_app_sink_set_callbacks(GST_APP_SINK(sink),&cb,this,nullptr);gst_object_unref(sink);
        if(gst_element_set_state(p,GST_STATE_PLAYING)==GST_STATE_CHANGE_FAILURE){gst_element_set_state(p,GST_STATE_NULL);gst_object_unref(p);throw std::runtime_error("Shared encoder failed to start");}
        return p;
    }
public:
    std::atomic<bool> stopped{false};
    explicit SharedSource(int audioFd):audioFd_(audioFd) {
        struct stat st{};if(audioFd<3||fstat(audioFd,&st)||!S_ISFIFO(st.st_mode))throw std::runtime_error("Expected owned audio pipe");
    }
    void start() {
        video_=pipeline("appsrc name=capture is-live=true format=time do-timestamp=true block=false max-buffers=1 leaky-type=downstream ! videoconvert ! videoscale ! video/x-raw,format=NV12,width=1280,height=720,framerate=60/1 ! v4l2h264enc name=encoder extra-controls=\"controls,video_bitrate=6000000,video_gop_size=60\" ! video/x-h264,profile=constrained-baseline,level=(string)4.2 ! h264parse config-interval=-1 ! video/x-h264,stream-format=byte-stream,alignment=au ! appsink name=out sync=false max-buffers=2 drop=true",false);
        audio_=pipeline("appsrc name=capture is-live=true format=time do-timestamp=true block=false max-buffers=4 leaky-type=downstream caps=\"audio/x-raw,format=S16LE,rate=48000,channels=2,layout=interleaved\" ! opusenc bitrate=96000 bitrate-type=cbr frame-size=5 audio-type=restricted-lowdelay max-payload-size=1400 ! appsink name=out sync=false max-buffers=4 drop=true",true);
        videoReader_=std::thread([this]{
            auto source=gst_bin_get_by_name(GST_BIN(video_),"capture");unsigned w=0,h=0;
            // Do not consume the encoder's first IDR before a receiver exists.
            while(!stopped&&!videoSubscribed_)std::this_thread::sleep_for(std::chrono::milliseconds(5));
            while(!stopped) {
                uint32_t header[4];if(!readBytes(0,header,sizeof(header)))break;
                if(header[0]!=0x31565254||!header[1]||header[1]>1280||!header[2]||header[2]>720||header[3]!=header[1]*header[2]*4){stopped=true;break;}
                if(w!=header[1]||h!=header[2]) {
                    w=header[1];h=header[2];auto caps=gst_caps_new_simple("video/x-raw","format",G_TYPE_STRING,"BGRx","width",G_TYPE_INT,int(w),"height",G_TYPE_INT,int(h),"framerate",GST_TYPE_FRACTION,60,1,nullptr);
                    gst_app_src_set_caps(GST_APP_SRC(source),caps);gst_caps_unref(caps);
                }
                auto buffer=gst_buffer_new_allocate(nullptr,header[3],nullptr);GstMapInfo map{};
                if(!gst_buffer_map(buffer,&map,GST_MAP_WRITE)){gst_buffer_unref(buffer);stopped=true;break;}
                const bool ok=readBytes(0,map.data,map.size);gst_buffer_unmap(buffer,&map);
                if(!ok){gst_buffer_unref(buffer);break;}
                if(gst_app_src_push_buffer(GST_APP_SRC(source),buffer)!=GST_FLOW_OK){stopped=true;break;}
            }
            gst_object_unref(source);
        });
        audioReader_=std::thread([this]{
            auto source=gst_bin_get_by_name(GST_BIN(audio_),"capture");std::array<char,960> pcm{};
            while(!stopped&&readBytes(audioFd_,pcm.data(),pcm.size())) {
                auto buffer=gst_buffer_new_allocate(nullptr,pcm.size(),nullptr);gst_buffer_fill(buffer,0,pcm.data(),pcm.size());GST_BUFFER_DURATION(buffer)=5*GST_MSECOND;
                if(gst_app_src_push_buffer(GST_APP_SRC(source),buffer)!=GST_FLOW_OK){stopped=true;break;}
            }
            gst_object_unref(source);
        });
    }
    void attach(GstElement* source,bool audio) {
        {std::lock_guard guard(lock_);gst_object_ref(source);sinks_.push_back({source,audio,!audio});}
        if(!audio){videoSubscribed_=true;requestKeyframe();}
    }
    void detach(GstElement* source) {
        std::lock_guard guard(lock_);
        for(auto it=sinks_.begin();it!=sinks_.end();++it)if(it->source==source){gst_object_unref(source);sinks_.erase(it);return;}
    }
    void requestKeyframe() {
        if(!video_)return;auto enc=gst_bin_get_by_name(GST_BIN(video_),"encoder");
        gst_element_send_event(enc,gst_video_event_new_upstream_force_key_unit(GST_CLOCK_TIME_NONE,TRUE,0));gst_object_unref(enc);
    }
    bool healthy() {
        for(auto p:{video_,audio_})if(p) {
            auto bus=gst_element_get_bus(p);auto msg=gst_bus_pop_filtered(bus,GstMessageType(GST_MESSAGE_ERROR|GST_MESSAGE_EOS));gst_object_unref(bus);
            if(msg){gst_message_unref(msg);stopped=true;}
        }
        return !stopped;
    }
    ~SharedSource() {
        stopped=true;if(videoReader_.joinable())videoReader_.join();if(audioReader_.joinable())audioReader_.join();
        for(auto p:{video_,audio_})if(p){gst_element_set_state(p,GST_STATE_NULL);gst_object_unref(p);}
        for(const auto& s:sinks_)gst_object_unref(s.source);
    }
};
