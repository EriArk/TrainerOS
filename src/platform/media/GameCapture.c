/* SPDX-License-Identifier: GPL-3.0-or-later
 * PipeWire consumer for Gamescope's existing application-specific capture.
 * Protocol fields: ValveSoftware/gamescope src/pipewire_gamescope.hpp.
 * The trusted media worker supplies a verified game appid; zero/root capture
 * is intentionally unavailable. No networking, encoder or compositor here.
 */
#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>
#include <spa/pod/builder.h>
#include <sys/prctl.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>

enum { RequestedSize=0x70000, FocusAppId=0x70001 };
struct Capture {
    struct pw_main_loop *loop;
    struct pw_stream *stream;
    struct spa_hook listener,registry_listener,core_listener;
    uint32_t node;
    uint64_t appid,last;
    int sync,failed;
    int stopping;
    struct spa_source *output;
    uint8_t *frame;
    size_t pending,offset;
    struct spa_video_info_raw format;
};
static uint64_t now(void) {struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1000000000ull+t.tv_nsec;}
static void stop(struct Capture *c) {c->failed=1;pw_main_loop_quit(c->loop);}
static void terminate(void *data,int signal_number) {
    (void)signal_number;struct Capture*c=data;c->stopping=1;pw_main_loop_quit(c->loop);
}
static void flush(void *data,int fd,uint32_t mask) {
    (void)mask;struct Capture*c=data;
    /* Never block PipeWire's loop on a slow encoder or a closed downstream
     * pipe. Keep at most one owned frame and return producer buffers promptly. */
    if(c->failed||c->stopping)return;
    size_t budget=256*1024;
    while(c->offset<c->pending&&budget) {
        size_t bytes=c->pending-c->offset;if(bytes>budget)bytes=budget;
        ssize_t n=write(fd,c->frame+c->offset,bytes);
        if(n<0&&(errno==EAGAIN||errno==EWOULDBLOCK||errno==EINTR))return;
        if(n<0&&errno==EPIPE) {terminate(c,0);return;}
        if(n<=0) {stop(c);return;}
        c->offset+=n;budget-=n;
    }
    if(c->offset==c->pending) {
        c->pending=c->offset=0;
        pw_loop_update_io(pw_main_loop_get_loop(c->loop),c->output,0);
    }
}
static void process(void *data) {
    struct Capture *c=data;struct pw_buffer *buffer=pw_stream_dequeue_buffer(c->stream);if(!buffer)return;
    struct spa_buffer *b=buffer->buffer;uint64_t stamp=now();
    unsigned width=c->format.size.width,height=c->format.size.height;
    if(!c->failed&&!c->stopping&&!c->pending&&b->n_datas&&b->datas[0].data&&b->datas[0].chunk
       &&width&&height&&width<=1280&&height<=720&&stamp-c->last>=33000000) {
        struct spa_data *d=&b->datas[0];int stride=d->chunk->stride;
        uint64_t bytes=stride>0?(uint64_t)(height-1)*stride+width*4:0;
        if(stride>=(int)width*4&&d->chunk->size>=bytes&&d->chunk->offset+bytes<=d->maxsize) {
            /* Fixed little-endian hosts are the supported Linux runtime targets. */
            uint32_t header[4]={0x31565254,width,height,width*height*4};
            memcpy(c->frame,header,sizeof(header));
            for(unsigned y=0;y<height;y++)
                memcpy(c->frame+sizeof(header)+(size_t)y*width*4,(char*)d->data+d->chunk->offset+(size_t)y*stride,width*4);
            c->pending=sizeof(header)+(size_t)width*height*4;c->offset=0;
            c->last=stamp;
        }
    }
    pw_stream_queue_buffer(c->stream,buffer);
    if(c->pending)pw_loop_update_io(pw_main_loop_get_loop(c->loop),c->output,SPA_IO_OUT);
}
static void parameter(void *data,uint32_t id,const struct spa_pod *param) {
    struct Capture *c=data;if(id!=SPA_PARAM_Format||!param)return;
    uint64_t appid=0;
    if(spa_format_video_raw_parse(param,&c->format)<0||c->format.format!=SPA_VIDEO_FORMAT_BGRx
        ||spa_pod_parse_object(param,SPA_TYPE_OBJECT_Format,NULL,FocusAppId,SPA_POD_Long(&appid))<0||appid!=c->appid)stop(c);
}
static void state(void *data,enum pw_stream_state old,enum pw_stream_state current,const char *error) {
    (void)old;(void)error;if(current==PW_STREAM_STATE_ERROR)stop(data);
}
static const struct pw_stream_events StreamEvents={PW_VERSION_STREAM_EVENTS,.state_changed=state,.param_changed=parameter,.process=process};
static void global(void *data,uint32_t id,uint32_t permissions,const char *type,uint32_t version,const struct spa_dict *props) {
    (void)permissions;(void)version;struct Capture *c=data;
    if(!props||strcmp(type,PW_TYPE_INTERFACE_Node))return;
    const char *name=spa_dict_lookup(props,PW_KEY_NODE_NAME);
    if(name&&!strcmp(name,"gamescope"))c->node=id;
}
static const struct pw_registry_events RegistryEvents={PW_VERSION_REGISTRY_EVENTS,.global=global};
static void done(void *data,uint32_t id,int sequence) {struct Capture*c=data;if(id==PW_ID_CORE&&sequence==c->sync)pw_main_loop_quit(c->loop);}
static const struct pw_core_events CoreEvents={PW_VERSION_CORE_EVENTS,.done=done};
int main(int argc,char **argv) {
    if(argc!=2)return 2;
    char *end=NULL;uint64_t appid=strtoull(argv[1],&end,10);if(!appid||appid>UINT32_MAX||!end||*end)return 2;
    /* PipeWire starts worker threads. Block before creating any of them so its
     * loop signal source, not an arbitrary worker's default action, handles stop. */
    sigset_t signals;sigemptyset(&signals);sigaddset(&signals,SIGTERM);sigaddset(&signals,SIGINT);
    if(sigprocmask(SIG_BLOCK,&signals,NULL)<0)return 3;
    prctl(PR_SET_PDEATHSIG,SIGTERM);if(getppid()==1)return 2;signal(SIGPIPE,SIG_IGN);
    struct Capture c={0};c.node=PW_ID_ANY;c.appid=appid;pw_init(&argc,&argv);
    c.loop=pw_main_loop_new(NULL);struct pw_context*context=pw_context_new(pw_main_loop_get_loop(c.loop),NULL,0);
    struct pw_loop *loop=pw_main_loop_get_loop(c.loop);
    pw_loop_add_signal(loop,SIGTERM,terminate,&c);pw_loop_add_signal(loop,SIGINT,terminate,&c);
    struct pw_core*core=pw_context_connect(context,NULL,0);if(!core)return 3;
    struct pw_registry*registry=pw_core_get_registry(core,PW_VERSION_REGISTRY,0);
    pw_registry_add_listener(registry,&c.registry_listener,&RegistryEvents,&c);
    pw_core_add_listener(core,&c.core_listener,&CoreEvents,&c);c.sync=pw_core_sync(core,PW_ID_CORE,0);pw_main_loop_run(c.loop);
    if(c.stopping||c.node==PW_ID_ANY)return c.stopping?0:4;
    c.frame=malloc(16+1280*720*4);if(!c.frame)return 3;
    int output_flags=fcntl(STDOUT_FILENO,F_GETFL);
    if(output_flags<0||fcntl(STDOUT_FILENO,F_SETFL,output_flags|O_NONBLOCK)<0)return 3;
    c.output=pw_loop_add_io(loop,STDOUT_FILENO,0,false,flush,&c);if(!c.output)return 3;
    c.stream=pw_stream_new(core,"TrainerOS game capture",pw_properties_new(PW_KEY_MEDIA_TYPE,"Video",PW_KEY_MEDIA_CATEGORY,"Capture",PW_KEY_MEDIA_ROLE,"Screen",NULL));
    pw_stream_add_listener(c.stream,&c.listener,&StreamEvents,&c);
    uint8_t bytes[1024];struct spa_pod_builder builder=SPA_POD_BUILDER_INIT(bytes,sizeof(bytes));
    const struct spa_pod*params[1];struct spa_rectangle size={1280,720};struct spa_fraction rate={0,1};
    /* Gamescope advertises variable rate as 0/1. Forcing 30/1 causes negotiation
     * failure on its producer. Rate-limit consumption, not the format request. */
    params[0]=spa_pod_builder_add_object(&builder,SPA_TYPE_OBJECT_Format,SPA_PARAM_EnumFormat,
        SPA_FORMAT_mediaType,SPA_POD_Id(SPA_MEDIA_TYPE_video),SPA_FORMAT_mediaSubtype,SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
        SPA_FORMAT_VIDEO_format,SPA_POD_Id(SPA_VIDEO_FORMAT_BGRx),SPA_FORMAT_VIDEO_framerate,SPA_POD_Fraction(&rate),
        RequestedSize,SPA_POD_Rectangle(&size),FocusAppId,SPA_POD_Long(appid));
    int result=pw_stream_connect(c.stream,PW_DIRECTION_INPUT,c.node,PW_STREAM_FLAG_AUTOCONNECT|PW_STREAM_FLAG_MAP_BUFFERS,params,1);
    if(result<0)c.failed=1;else pw_main_loop_run(c.loop);
    pw_loop_destroy_source(loop,c.output);
    pw_stream_set_active(c.stream,false);pw_stream_disconnect(c.stream);
    pw_stream_destroy(c.stream);pw_core_disconnect(core);pw_context_destroy(context);pw_main_loop_destroy(c.loop);
    fcntl(STDOUT_FILENO,F_SETFL,output_flags);free(c.frame);pw_deinit();return c.failed?5:0;
}
