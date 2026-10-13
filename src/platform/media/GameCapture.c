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

enum { RequestedSize=0x70000, FocusAppId=0x70001 };
struct Capture {
    struct pw_main_loop *loop;
    struct pw_stream *stream;
    struct spa_hook listener,registry_listener,core_listener;
    uint32_t node;
    uint64_t appid,last;
    int sync,failed;
    struct spa_video_info_raw format;
};
static uint64_t now(void) {struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1000000000ull+t.tv_nsec;}
static int write_all(const void *data,size_t size) {
    const char *p=data;
    while(size) {ssize_t n=write(STDOUT_FILENO,p,size);if(n<0&&errno==EINTR)continue;if(n<=0)return -1;p+=n;size-=n;}
    return 0;
}
static void stop(struct Capture *c) {c->failed=1;pw_main_loop_quit(c->loop);}
static void process(void *data) {
    struct Capture *c=data;struct pw_buffer *buffer=pw_stream_dequeue_buffer(c->stream);if(!buffer)return;
    struct spa_buffer *b=buffer->buffer;uint64_t stamp=now();
    unsigned width=c->format.size.width,height=c->format.size.height;
    if(b->n_datas&&b->datas[0].data&&width&&height&&width<=1280&&height<=720&&stamp-c->last>=33000000) {
        struct spa_data *d=&b->datas[0];int stride=d->chunk->stride;
        uint64_t bytes=stride>0?(uint64_t)(height-1)*stride+width*4:0;
        if(stride>=(int)width*4&&d->chunk->size>=bytes&&d->chunk->offset+bytes<=d->maxsize) {
            /* Fixed little-endian hosts are the supported Linux runtime targets. */
            uint32_t header[4]={0x31565254,width,height,width*height*4};
            if(write_all(header,sizeof(header)))stop(c);
            for(unsigned y=0;y<height&&!c->failed;y++)
                if(write_all((char*)d->data+d->chunk->offset+y*stride,width*4))stop(c);
            c->last=stamp;
        }
    }
    pw_stream_queue_buffer(c->stream,buffer);
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
    prctl(PR_SET_PDEATHSIG,SIGKILL);if(getppid()==1)return 2;signal(SIGPIPE,SIG_DFL);
    struct Capture c={0};c.node=PW_ID_ANY;c.appid=appid;pw_init(&argc,&argv);
    c.loop=pw_main_loop_new(NULL);struct pw_context*context=pw_context_new(pw_main_loop_get_loop(c.loop),NULL,0);
    struct pw_core*core=pw_context_connect(context,NULL,0);if(!core)return 3;
    struct pw_registry*registry=pw_core_get_registry(core,PW_VERSION_REGISTRY,0);
    pw_registry_add_listener(registry,&c.registry_listener,&RegistryEvents,&c);
    pw_core_add_listener(core,&c.core_listener,&CoreEvents,&c);c.sync=pw_core_sync(core,PW_ID_CORE,0);pw_main_loop_run(c.loop);
    if(c.node==PW_ID_ANY)return 4;
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
    pw_stream_destroy(c.stream);pw_core_disconnect(core);pw_context_destroy(context);pw_main_loop_destroy(c.loop);return c.failed?5:0;
}
