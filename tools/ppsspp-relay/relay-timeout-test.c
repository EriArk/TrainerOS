/* SPDX-License-Identifier: GPL-3.0-or-later
 * A real TCP relay that accepts a connection but withholds its handshake.
 * Compile the library with RELAY_IO_TIMEOUT_MS=300 for the bounded fixture;
 * LEGACY_STALL proves the old call stays blocked until the server closes.
 */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <netinet/tcp.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include "client/sock_impl.h"
#include "client/postoffice_mem.h"
#include "aemu_postoffice_packets.h"

static long now_ms(void) {
    struct timespec t; assert(clock_gettime(CLOCK_MONOTONIC, &t) == 0);
    return t.tv_sec * 1000L + t.tv_nsec / 1000000;
}
struct server { int listener; bool reply; };
static void *serve(void *arg) {
    struct server *s = arg;
    int fd = accept(s->listener, NULL, NULL); assert(fd >= 0);
    struct aemu_postoffice_init init;
    assert(recv(fd, &init, sizeof(init), MSG_WAITALL) == sizeof(init));
    if (s->reply) {
        struct aemu_postoffice_ptp_connect ack = {0};
        assert(send(fd, &ack, sizeof(ack), MSG_NOSIGNAL) == sizeof(ack));
    } else usleep(900000);
    close(fd); return NULL;
}
static void handshake(void) {
    int listener = socket(AF_INET, SOCK_STREAM, 0); assert(listener >= 0);
    struct sockaddr_in address = {.sin_family=AF_INET, .sin_addr.s_addr=htonl(INADDR_LOOPBACK)};
    assert(bind(listener, (struct sockaddr *)&address, sizeof(address)) == 0);
    assert(listen(listener, 1) == 0);
    socklen_t size=sizeof(address);
    assert(getsockname(listener, (struct sockaddr *)&address, &size) == 0);
    struct aemu_post_office_sock_addr target={.addr=address.sin_addr.s_addr,.port=address.sin_port};
    const char mac[6]={2,0,0,0,0,1};
    aemu_post_office_init();
    for (int n=0;n<3;++n) {
        struct server s={listener,n==2}; pthread_t thread;
        assert(pthread_create(&thread,NULL,serve,&s)==0);
        int state=0; long begin=now_ms();
        void *session=ptp_connect_v4(&target,mac,1234,mac,1235,&state);
        long elapsed=now_ms()-begin;
        if (s.reply) {
            assert(session && state==0);
#ifndef LEGACY_STALL
            unsigned timeout=0; socklen_t size=sizeof(timeout);
            assert(getsockopt(ptp_get_native_sock(session),IPPROTO_TCP,TCP_USER_TIMEOUT,&timeout,&size)==0 && timeout==15000);
#endif
            ptp_close(session);
        }
        else {
            assert(!session && state==AEMU_POSTOFFICE_CLIENT_SESSION_NETWORK);
#ifdef LEGACY_STALL
            assert(elapsed>=800);
#else
            assert(elapsed>=250 && elapsed<750);
#endif
        }
        assert(pthread_join(thread,NULL)==0);
        printf("Handshake %d: %ld ms, state %d\n",n,elapsed,state);
    }
    close(listener);
}
int main(int argc, char **argv) {
    if (argc>1 && strcmp(argv[1],"--production")==0) {
        int fd[2];assert(socketpair(AF_UNIX,SOCK_STREAM,0,fd)==0);
        assert(fcntl(fd[0],F_SETFL,O_NONBLOCK)==0);
        char byte;bool abort=false;long begin=now_ms();
        assert(native_recv_till_done(fd[0],&byte,1,false,&abort)==-1 && errno==ETIMEDOUT);
        long elapsed=now_ms()-begin;assert(elapsed>=14500 && elapsed<16500);
        close(fd[0]);close(fd[1]);printf("Production timeout: %ld ms\n",elapsed);return 0;
    }
    handshake();
#ifndef LEGACY_STALL
    int fd[2]; assert(socketpair(AF_UNIX,SOCK_STREAM,0,fd)==0);
    assert(fcntl(fd[0],F_SETFL,O_NONBLOCK)==0);
    char data[4096]={0}; bool abort=false;
    while(send(fd[0],data,sizeof(data),MSG_NOSIGNAL)>0) {}
    assert(errno==EAGAIN || errno==EWOULDBLOCK);
    long begin=now_ms();
    assert(native_send_till_done(fd[0],data,sizeof(data),true,&abort)==AEMU_POSTOFFICE_CLIENT_SESSION_WOULD_BLOCK);
    assert(now_ms()-begin<100);
    begin=now_ms(); errno=0;
    assert(native_send_till_done(fd[0],data,sizeof(data),false,&abort)==-1 && errno==ETIMEDOUT);
    assert(now_ms()-begin>=250 && now_ms()-begin<750);
    abort=true;
    assert(native_send_till_done(fd[0],data,sizeof(data),false,&abort)==NATIVE_SOCK_ABORTED);
    close(fd[0]);close(fd[1]);
    assert(socketpair(AF_UNIX,SOCK_STREAM,0,fd)==0);
    assert(fcntl(fd[0],F_SETFL,O_NONBLOCK)==0);
    assert(write(fd[1],"a",1)==1);abort=false;begin=now_ms();
    assert(native_recv_till_done(fd[0],data,2,true,&abort)==-1 && errno==ETIMEDOUT);
    assert(now_ms()-begin>=250 && now_ms()-begin<750);
    close(fd[0]);close(fd[1]);
#endif
    puts("Relay stall/reconnect checks passed.");
}
