// SPDX-License-Identifier: GPL-3.0-or-later
// Isolated GameStream interoperability probe. Optional locally leased P2 only.
// Negotiation follows Wolf rtsp/commands.hpp; framing uses Moonlight RtspParser.
// Control V2 nonce layout follows moonlight-common-c ControlStream.c.
#include "common.hpp"
#include "remote_pad.hpp"
#include "shared_source.hpp"
#include <atomic>
#include <array>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <mutex>
#include <thread>
#include <vector>
#include <poll.h>
#include <sys/socket.h>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <gst/video/video-event.h>
#include <gst-plugin/gstrtpmoonlightpay_video.hpp>
#include <gst-plugin/gstrtpmoonlightpay_audio.hpp>
#include <moonlight/fec.hpp>
#include <moonlight/data-structures.hpp>
#include <crypto/crypto.hpp>
extern "C" {
#include <enet/enet.h>
#include "Rtsp.h"
#include "Input.h"
#include "Limelight.h"
}

struct Host {
    const Config& cfg;
    std::atomic<bool> stop{false}, play{false}, authenticated{false};
    std::atomic<unsigned> inputs{0}, rejected{0}, idrs{0}, videoPackets{0}, audioPackets{0};
    std::atomic<unsigned> dropped{0}, encodedIdrs{0}, encodedSps{0};
    std::atomic<int> listeners{0};
    SharedSource* shared=nullptr;std::string connection;int port=2;
    void listening() {if(++listeners==4&&cfg.game){
        if(shared)std::printf("{\"event\":\"ready\",\"connection\":\"%s\"}\n",connection.c_str());else std::puts("{\"event\":\"ready\"}");std::fflush(stdout);}}
    std::mutex mediaLock;
    GstElement* video=nullptr;
    int audioFd=-1;
    int padFd=-1;
    explicit Host(const Config& c,SharedSource* source=nullptr,int fd=-1,int slot=2,std::string id={}):cfg(c),shared(source),connection(std::move(id)),port(slot),padFd(fd) {
        if(cfg.game&&!shared) {
            const char* value=std::getenv("TRAINEROS_GAME_AUDIO_FD");
            if(!value)throw std::runtime_error("Game mode requires the owned-game audio producer");
            const auto end=value+std::strlen(value);
            auto parsed=std::from_chars(value,end,audioFd);
            struct stat st{};
            if(parsed.ec!=std::errc{}||parsed.ptr!=end||audioFd<3||fstat(audioFd,&st)||!S_ISFIFO(st.st_mode))
                throw std::runtime_error("Expected inherited game audio pipe");
            if(const char* lease=std::getenv("TRAINEROS_GAME_PAD_FD")) {
                auto last=lease+std::strlen(lease);
                auto number=std::from_chars(lease,last,padFd);
                if(number.ec!=std::errc{}||number.ptr!=last||padFd<3||padFd==audioFd)
                    throw std::runtime_error("Invalid inherited game input lease");
            }
        }
    }
    int socketAt(int type,int port) {
        int fd=socket(AF_INET,type,0); if(fd<0) throw std::runtime_error("socket failed");
        int yes=1;setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));
        sockaddr_in addr{};addr.sin_family=AF_INET;addr.sin_port=htons(port);
        inet_pton(AF_INET,cfg.address.c_str(),&addr.sin_addr);
        if(bind(fd,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))) {close(fd);throw std::runtime_error("bind failed");}
        return fd;
    }
    bool expected(const sockaddr_in& addr) const {
        in_addr ip{};inet_pton(AF_INET,cfg.peer.c_str(),&ip);
        return addr.sin_addr.s_addr==ip.s_addr;
    }
    void rtsp() {
        int server=socketAt(SOCK_STREAM,RtspPort);listen(server,4);
        listening();
        while(!stop) {
            pollfd p{server,POLLIN,0};if(poll(&p,1,100)<=0)continue;
            sockaddr_in peer{};socklen_t length=sizeof(peer);
            int fd=accept(server,reinterpret_cast<sockaddr*>(&peer),&length);if(fd<0)continue;
            timeval timeout{1,0};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
            setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
            if(!expected(peer)){close(fd);++rejected;continue;}
            std::string data;char buffer[4096];size_t wanted=0;
            while(data.size()<65536&&!stop) {
                int got=recv(fd,buffer,sizeof(buffer),0);if(got<=0)break;data.append(buffer,got);
                auto end=data.find("\r\n\r\n");if(end==std::string::npos)continue;
                wanted=end+4;
                const auto head=data.substr(0,end);
                auto at=head.find("Content-length:");
                if(at==std::string::npos)at=head.find("Content-Length:");
                if(at!=std::string::npos) {
                    try {wanted+=std::stoul(head.substr(at+15));}catch(...){wanted=65537;}
                }
                if(wanted>65536||data.size()>=wanted)break;
            }
            RTSP_MESSAGE req{};
            if(wanted && wanted<=65536 && data.size()==wanted
                && parseRtspMessage(&req,data.data(),data.size())==0) {
                if(req.type==TYPE_REQUEST) respond(fd,req);
                freeMessage(&req);
            } else ++rejected;
            shutdown(fd,SHUT_WR);close(fd);
        }
        close(server);
    }
    void respond(int fd,const RTSP_MESSAGE& req) {
        std::string command=req.message.request.command,target=req.message.request.target;
        int status=200;std::string payload;
        std::map<std::string,std::string> headers{{"CSeq",std::to_string(req.sequenceNumber)}};
        if(command=="DESCRIBE") {
            payload="a=x-ss-general.featureFlags: 0\r\na=x-ss-general.encryptionSupported: 5\r\na=x-ss-general.encryptionRequested: 5\r\n";
        } else if(command=="SETUP") {
            headers["Session"]="traineros-probe;timeout=30";
            if(target.find("audio")!=std::string::npos)headers["Transport"]="server_port="+std::to_string(AudioPort);
            else if(target.find("video")!=std::string::npos)headers["Transport"]="server_port="+std::to_string(VideoPort);
            else if(target.find("control")!=std::string::npos)headers["Transport"]="server_port="+std::to_string(ControlPort);
            else status=404;
            if(target.find("control")!=std::string::npos) headers["X-SS-Connect-Data"]=std::to_string(cfg.connect);
            else headers["X-SS-Ping-Payload"]=cfg.ping;
        } else if(command=="ANNOUNCE") {
            const std::string body(req.payload?req.payload:"",req.payloadLength);
            if(body.find("x-ss-general.encryptionEnabled:5")==std::string::npos
                ||body.find("clientViewportWd:1280")==std::string::npos
                ||body.find("clientViewportHt:720")==std::string::npos)status=400;
        } else if(command=="PLAY") play=true;
        else if(command!="OPTIONS")status=405;
        if(!payload.empty())headers["Content-length"]=std::to_string(payload.size());
        std::vector<OPTION_ITEM> options(headers.size());size_t n=0;
        for(auto& [key,value]:headers) {
            options[n]={0,const_cast<char*>(key.c_str()),value.data(),n+1<options.size()?&options[n+1]:nullptr};++n;
        }
        RTSP_MESSAGE reply{};
        createRtspResponse(&reply,nullptr,0,const_cast<char*>("RTSP/1.0"),status,
            const_cast<char*>(status==200?"OK":"Rejected"),req.sequenceNumber,options.data(),
            payload.empty()?nullptr:payload.data(),payload.size());
        int length=0;char* message=serializeRtspMessage(&reply,&length);
        if(message) {
            size_t sent=0;while(sent<size_t(length)){int v=send(fd,message+sent,length-sent,MSG_NOSIGNAL);if(v<=0)break;sent+=v;}
            free(message);
        }
        std::fprintf(stderr,"rtsp %s %d\n",command.c_str(),status);
    }
    void control() {
        std::unique_ptr<RemotePad> remote;
        if(padFd>=0)remote=std::make_unique<RemotePad>(padFd,port);
        ENetAddress addr{};enet_address_set_host(&addr,cfg.address.c_str());enet_address_set_port(&addr,ControlPort);
        auto host=enet_host_create(AF_INET,&addr,1,0,0,0);
        if(!host)throw std::runtime_error("ENet host failed");
        listening();
        uint32_t highest=0;uint64_t seen=0;bool haveSequence=false;
        while(!stop) {
            if(remote)remote->tick(monotonicNs());
            ENetEvent ev{};if(enet_host_service(host,&ev,100)<=0)continue;
            if(ev.type==ENET_EVENT_TYPE_CONNECT) {
                auto& peer=*reinterpret_cast<sockaddr_in*>(&ev.peer->address.address);
                if(!expected(peer)||ev.data!=cfg.connect){enet_peer_disconnect_now(ev.peer,0);++rejected;}
                else std::fprintf(stderr,"control connected\n");
            } else if(ev.type==ENET_EVENT_TYPE_DISCONNECT) {authenticated=false;stop=true;}
            else if(ev.type==ENET_EVENT_TYPE_RECEIVE) {
                auto* data=ev.packet->data;const size_t len=ev.packet->dataLength;
                try {
                    uint16_t type=0,size=0;uint32_t seq=0;
                    if(len<28||len>4096)throw std::runtime_error("control size");
                    memcpy(&type,data,2);memcpy(&size,data+2,2);memcpy(&seq,data+4,4);
                    type=le16toh(type);size=le16toh(size);seq=le32toh(seq);
                    if(type!=1||size+4!=len)throw std::runtime_error("control envelope");
                    if(haveSequence && seq<=highest && (highest-seq>=64 || (seen&(uint64_t(1)<<(highest-seq)))))
                        throw std::runtime_error("replayed control");
                    std::array<char,12> iv{};uint32_t little=htole32(seq);memcpy(iv.data(),&little,4);iv[10]='C';iv[11]='C';
                    auto plain=crypto::aes_decrypt_gcm({reinterpret_cast<char*>(data+24),len-24},cfg.key,
                        {reinterpret_cast<char*>(data+8),16},{iv.data(),iv.size()},12);
                    if(plain.size()<4)throw std::runtime_error("control payload");
                    memcpy(&size,plain.data()+2,2);
                    if(le16toh(size)+4!=plain.size())throw std::runtime_error("control payload length");
                    if(!haveSequence){highest=seq;seen=1;haveSequence=true;}
                    else if(seq>highest){auto shift=seq-highest;seen=shift>=64?1:(seen<<shift)|1;highest=seq;}
                    else seen|=uint64_t(1)<<(highest-seq);
                    authenticated=true;
                    memcpy(&type,plain.data(),2);type=le16toh(type);
                    if(type==0x0302 || type==0x0301) {
                        if(shared){shared->requestKeyframe();++idrs;}
                        std::lock_guard lock(mediaLock);
                        if(video&&!shared){auto enc=gst_bin_get_by_name(GST_BIN(video),"encoder");
                            gst_element_send_event(enc,gst_video_event_new_upstream_force_key_unit(GST_CLOCK_TIME_NONE,TRUE,0));gst_object_unref(enc);++idrs;}
                    } else if(type==0x0206 && plain.size()>=4+sizeof(NV_MULTI_CONTROLLER_PACKET)) {
                        NV_MULTI_CONTROLLER_PACKET pad{};memcpy(&pad,plain.data()+4,sizeof(pad));
                        if(le32toh(pad.header.magic)!=MULTI_CONTROLLER_MAGIC_GEN5 ||le16toh(pad.controllerNumber)!=0
                            ||le16toh(pad.activeGamepadMask)!=1)continuePacket();
                        else if(remote) {
                            PadState state{le16toh(pad.buttonFlags),pad.leftTrigger,pad.rightTrigger,
                                int16_t(le16toh(pad.leftStickX)),int16_t(le16toh(pad.leftStickY)),
                                int16_t(le16toh(pad.rightStickX)),int16_t(le16toh(pad.rightStickY))};
                            if(remote->update(state,seq,monotonicNs()))++inputs;else ++rejected;
                        } else {++inputs;std::lock_guard lock(mediaLock);
                            if(video){auto pattern=gst_bin_get_by_name(GST_BIN(video),"pattern");
                                if(pattern){g_object_set(pattern,"foreground-color",(le16toh(pad.buttonFlags)&A_FLAG)?0xff00ff00u:0xffff0000u,nullptr);gst_object_unref(pattern);}}
                        }
                    }
                } catch(const std::exception&) {++rejected;}
                enet_packet_destroy(ev.packet);
            }
        }
        enet_host_destroy(host);
    }
    void continuePacket(){++rejected;}
    bool readFrameBytes(void* destination,size_t length,int fd=STDIN_FILENO) {
        auto data=static_cast<char*>(destination);
        while(length&&!stop) {
            pollfd p{fd,POLLIN,0};if(poll(&p,1,100)<=0)continue;
            auto n=read(fd,data,length);if(n<=0){stop=true;return false;}
            data+=n;length-=n;
        }
        return length==0;
    }
    void captureAudio(GstElement* pipeline) {
        auto source=gst_bin_get_by_name(GST_BIN(pipeline),"gameaudio");
        std::array<unsigned char,960> pcm{}; // 5 ms, 48 kHz stereo S16LE
        while(!stop && readFrameBytes(pcm.data(),pcm.size(),audioFd)) {
            auto buffer=gst_buffer_new_allocate(nullptr,pcm.size(),nullptr);
            gst_buffer_fill(buffer,0,pcm.data(),pcm.size());
            GST_BUFFER_DURATION(buffer)=5*GST_MSECOND;
            if(gst_app_src_push_buffer(GST_APP_SRC(source),buffer)!=GST_FLOW_OK){stop=true;break;}
        }
        gst_object_unref(source);
    }
    void capture(GstElement* pipeline) {
        auto source=gst_bin_get_by_name(GST_BIN(pipeline),"capture");
        unsigned previousWidth=0,previousHeight=0;
        while(!stop) {
            uint32_t header[4];if(!readFrameBytes(header,sizeof(header)))break;
            const auto width=header[1],height=header[2];
            if(header[0]!=0x31565254||!width||width>1280||!height||height>720||header[3]!=width*height*4){stop=true;break;}
            if(width!=previousWidth||height!=previousHeight) {
                auto caps=gst_caps_new_simple("video/x-raw","format",G_TYPE_STRING,"BGRx","width",G_TYPE_INT,int(width),
                    "height",G_TYPE_INT,int(height),"framerate",GST_TYPE_FRACTION,60,1,nullptr);
                gst_app_src_set_caps(GST_APP_SRC(source),caps);gst_caps_unref(caps);previousWidth=width;previousHeight=height;
            }
            auto buffer=gst_buffer_new_allocate(nullptr,header[3],nullptr);GstMapInfo map{};
            if(!gst_buffer_map(buffer,&map,GST_MAP_WRITE)){gst_buffer_unref(buffer);stop=true;break;}
            const bool ready=readFrameBytes(map.data,map.size);gst_buffer_unmap(buffer,&map);
            if(!ready){gst_buffer_unref(buffer);break;}
            if(gst_app_src_push_buffer(GST_APP_SRC(source),buffer)!=GST_FLOW_OK){stop=true;break;}
        }
        gst_object_unref(source);
    }
    struct SendContext {int fd; sockaddr_in peer; std::atomic<unsigned>* count;Host* host;bool video;};
    static GstFlowReturn sendSample(GstAppSink* sink,void* pointer) {
        auto& ctx=*static_cast<SendContext*>(pointer);auto sample=gst_app_sink_pull_sample(sink);if(!sample)return GST_FLOW_EOS;
        auto sendBuffer=[&](GstBuffer* buffer) {GstMapInfo map{};if(gst_buffer_map(buffer,&map,GST_MAP_READ)) {
            if(ctx.video&&(ctx.host->cfg.dropEvery||ctx.host->cfg.dropFrame)&&map.size>=32) {
                moonlight::NV_VIDEO_PACKET packet{};memcpy(&packet,map.data+16,sizeof(packet));
                const auto frame=le32toh(packet.frameIndex);
                // Drop the whole frame, including parity: FEC cannot conceal this loss.
                const bool full=ctx.host->cfg.dropFrame && frame==ctx.host->cfg.dropFrame;
                const bool single=ctx.host->cfg.dropEvery && frame%ctx.host->cfg.dropEvery==0
                    && ((le32toh(packet.fecInfo)>>12)&0x3ff)==0;
                if(full||single) {
                    ++ctx.host->dropped;gst_buffer_unmap(buffer,&map);return;
                }
            }
            if(sendto(ctx.fd,map.data,map.size,MSG_NOSIGNAL,reinterpret_cast<sockaddr*>(&ctx.peer),sizeof(ctx.peer))==ssize_t(map.size))++*ctx.count;
            gst_buffer_unmap(buffer,&map);}};
        if(auto list=gst_sample_get_buffer_list(sample)){for(unsigned n=0;n<gst_buffer_list_length(list);++n)sendBuffer(gst_buffer_list_get(list,n));}
        else if(auto buffer=gst_sample_get_buffer(sample))sendBuffer(buffer);
        gst_sample_unref(sample);return GST_FLOW_OK;
    }
    void media(bool audio) {
        int fd=socketAt(SOCK_DGRAM,audio?AudioPort:VideoPort);sockaddr_in destination{};bool ready=false;
        listening();
        while(!stop&&!ready) {pollfd p{fd,POLLIN,0};if(poll(&p,1,100)<=0)continue;
            char ping[128];socklen_t size=sizeof(destination);auto got=recvfrom(fd,ping,sizeof(ping),0,reinterpret_cast<sockaddr*>(&destination),&size);
            ready=got>=16 && expected(destination) && memcmp(ping,cfg.ping.data(),16)==0;
        }
        while(!stop && (!play||!authenticated))std::this_thread::sleep_for(std::chrono::milliseconds(20));
        if(stop){close(fd);return;}
        std::string desc=audio ?
            "audiotestsrc is-live=true wave=sine freq=440 ! audio/x-raw,rate=48000,channels=2 ! opusenc bitrate=96000 bitrate-type=cbr frame-size=5 audio-type=restricted-lowdelay max-payload-size=1400 ! rtpmoonlightpay_audio name=pay ! appsink name=out sync=false buffer-list=true max-buffers=4 drop=true" :
            "videotestsrc name=pattern is-live=true pattern=solid-color foreground-color=4294901760 ! video/x-raw,format=NV12,width=1280,height=720,framerate=60/1 ! v4l2h264enc name=encoder extra-controls=\"controls,video_bitrate=6000000,video_gop_size=60\" ! video/x-h264,profile=constrained-baseline,level=(string)4.2 ! h264parse config-interval=-1 ! video/x-h264,stream-format=byte-stream,alignment=au ! rtpmoonlightpay_video name=pay payload-size=1024 fec-percentage=20 ! appsink name=out sync=false buffer-list=true max-buffers=4 drop=true";
        if(!audio && cfg.game) {
            const auto begin=desc.find(" ! video/x-raw");
            desc="appsrc name=capture is-live=true format=time do-timestamp=true block=false max-buffers=1 leaky-type=downstream ! videoconvert ! videoscale"+desc.substr(begin);
        }
        if(audio && cfg.game) {
            const auto begin=desc.find(" ! opusenc");
            desc="appsrc name=gameaudio is-live=true format=time do-timestamp=true block=false max-buffers=4 leaky-type=downstream caps=\"audio/x-raw,format=S16LE,rate=48000,channels=2,layout=interleaved\""+desc.substr(begin);
        }
        if(shared)desc=audio?
            "appsrc name=encoded is-live=true format=time do-timestamp=true block=false max-buffers=8 leaky-type=downstream caps=\"audio/x-opus,rate=48000,channels=2,channel-mapping-family=0\" ! rtpmoonlightpay_audio name=pay ! appsink name=out sync=false buffer-list=true max-buffers=4 drop=true":
            "appsrc name=encoded is-live=true format=time do-timestamp=true block=false max-buffers=8 leaky-type=downstream caps=\"video/x-h264,stream-format=byte-stream,alignment=au,width=1280,height=720,framerate=60/1\" ! rtpmoonlightpay_video name=pay payload-size=1024 fec-percentage=20 ! appsink name=out sync=false buffer-list=true max-buffers=4 drop=true";
        GError* error=nullptr;auto pipeline=gst_parse_launch(desc.c_str(),&error);
        if(error){std::fprintf(stderr,"pipeline %s\n",error->message);g_error_free(error);stop=true;if(pipeline)gst_object_unref(pipeline);close(fd);return;}
        if(audio){auto pay=gst_bin_get_by_name(GST_BIN(pipeline),"pay");g_object_set(pay,"aes-key",cfg.keyHex.c_str(),"aes-iv","1",nullptr);gst_object_unref(pay);}
        else {
            auto pay=gst_bin_get_by_name(GST_BIN(pipeline),"pay");
            auto pad=gst_element_get_static_pad(pay,"sink");
            gst_pad_add_probe(pad,GST_PAD_PROBE_TYPE_BUFFER,[](GstPad*,GstPadProbeInfo* info,void* data){
                auto& host=*static_cast<Host*>(data);
                auto buffer=GST_PAD_PROBE_INFO_BUFFER(info);GstMapInfo map{};
                if(!GST_BUFFER_FLAG_IS_SET(buffer,GST_BUFFER_FLAG_DELTA_UNIT)&&gst_buffer_map(buffer,&map,GST_MAP_READ)) {
                    bool idr=false,sps=false;
                    for(size_t n=0;n+4<map.size;++n)if(map.data[n]==0&&map.data[n+1]==0&&map.data[n+2]==1) {
                        const auto type=map.data[n+3]&31;idr|=type==5;sps|=type==7;
                    }
                    if(idr)++host.encodedIdrs;if(sps)++host.encodedSps;gst_buffer_unmap(buffer,&map);
                }
                return GST_PAD_PROBE_OK;
            },this,nullptr);
            gst_object_unref(pad);gst_object_unref(pay);
            std::lock_guard lock(mediaLock);video=pipeline;
        }
        auto sink=gst_bin_get_by_name(GST_BIN(pipeline),"out");
        SendContext ctx{fd,destination,audio?&audioPackets:&videoPackets,this,!audio};GstAppSinkCallbacks cb{};cb.new_sample=sendSample;
        gst_app_sink_set_callbacks(GST_APP_SINK(sink),&cb,&ctx,nullptr);
        gst_element_set_state(pipeline,GST_STATE_PLAYING);auto bus=gst_element_get_bus(pipeline);
        std::thread captureWorker;
        GstElement* encoded=shared?gst_bin_get_by_name(GST_BIN(pipeline),"encoded"):nullptr;
        if(encoded)shared->attach(encoded,audio);
        if(!shared&&!audio&&cfg.game)captureWorker=std::thread([&]{capture(pipeline);});
        if(!shared&&audio&&cfg.game)captureWorker=std::thread([&]{captureAudio(pipeline);});
        while(!stop){auto msg=gst_bus_timed_pop_filtered(bus,100*GST_MSECOND,GstMessageType(GST_MESSAGE_ERROR|GST_MESSAGE_EOS));
            if(msg){if(GST_MESSAGE_TYPE(msg)==GST_MESSAGE_ERROR){gchar* debug=nullptr;gst_message_parse_error(msg,&error,&debug);
                std::fprintf(stderr,"media error %s (%s)\n",error->message,debug?debug:"");g_error_free(error);g_free(debug);}gst_message_unref(msg);stop=true;}}
        if(captureWorker.joinable())captureWorker.join();
        if(encoded){shared->detach(encoded);gst_object_unref(encoded);}
        {std::lock_guard lock(mediaLock);if(!audio)video=nullptr;}
        gst_element_set_state(pipeline,GST_STATE_NULL);gst_object_unref(bus);gst_object_unref(sink);gst_object_unref(pipeline);close(fd);
    }
};
#include "host_group.hpp"
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    try {
        gst_init(nullptr,nullptr);moonlight::fec::init();if(enet_initialize())return 3;
        gst_element_register(nullptr,"rtpmoonlightpay_video",GST_RANK_NONE,gst_TYPE_rtp_moonlight_pay_video);
        gst_element_register(nullptr,"rtpmoonlightpay_audio",GST_RANK_NONE,gst_TYPE_rtp_moonlight_pay_audio);
        if(std::string(argv[1])=="--group") {const int result=groupMain();enet_deinitialize();return result;}
        Config cfg(argv[1]);
        Host host(cfg);
        auto guarded=[&](auto fn){try{fn();}catch(const std::exception& e){std::fprintf(stderr,"host error: %s\n",e.what());host.stop=true;}};
        std::thread rtsp([&]{guarded([&]{host.rtsp();});}),control([&]{guarded([&]{host.control();});});
        std::thread video([&]{guarded([&]{host.media(false);});}),audio([&]{guarded([&]{host.media(true);});});
        for(uint64_t n=0;(!cfg.seconds||n<uint64_t(cfg.seconds)*10)&&!host.stop;++n)std::this_thread::sleep_for(std::chrono::milliseconds(100));
        host.stop=true;rtsp.join();control.join();video.join();audio.join();enet_deinitialize();
        std::fprintf(stderr,"deliberately_dropped_video_packets=%u encoded_idrs=%u encoded_sps=%u\n",host.dropped.load(),host.encodedIdrs.load(),host.encodedSps.load());
        std::printf("{\"video_packets\":%u,\"audio_packets\":%u,\"inputs\":%u,\"rejected\":%u,\"idr_requests\":%u}\n",
            host.videoPackets.load(),host.audioPackets.load(),host.inputs.load(),host.rejected.load(),host.idrs.load());
        return host.videoPackets&&host.audioPackets?0:1;
    }catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}
