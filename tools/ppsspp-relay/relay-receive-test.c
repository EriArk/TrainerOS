/* TrainerOS relay regression fixture. SPDX-License-Identifier: GPL-3.0-or-later
 * Real local stream sockets; no public server or game content is used.
 * Compile with -DLEGACY_RELAY for the pinned 1.20.4 library to demonstrate
 * the old results. Without it, all repaired behaviours must pass.
 */
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "client/postoffice_mem.h"
#include "aemu_postoffice_packets.h"

static void open_pair(int fd[2], struct pdp_session *s) {
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fd) == 0);
    assert(fcntl(fd[0], F_SETFL, O_NONBLOCK) == 0);
    memset(s, 0, sizeof(*s));
    s->sock = fd[0];
}
static void send_packet(int fd, const char *bytes, unsigned size) {
    struct aemu_postoffice_pdp h = {.port = 1234, .size = size};
    assert(write(fd, &h, sizeof(h)) == sizeof(h));
    assert(write(fd, bytes, size) == size);
}
int main(void) {
    int fd[2], len, port, rc, status;
    char out[4096], payload[4096], mac[6];
    struct pdp_session session;
    memset(payload, 0x5a, sizeof(payload));

    open_pair(fd, &session);
    close(fd[1]);
    len = sizeof(out);
    rc = pdp_recv(&session, mac, &port, out, &len, true);
#ifdef LEGACY_RELAY
    assert(rc == AEMU_POSTOFFICE_CLIENT_SESSION_WOULD_BLOCK && !session.dead);
    close(fd[0]);
#else
    assert(rc == AEMU_POSTOFFICE_CLIENT_SESSION_DEAD && session.dead);
#endif
    printf("EOF result: %d\n", rc);

    open_pair(fd, &session);
    struct aemu_postoffice_pdp header = {.port = 1234, .size = sizeof(payload)};
    assert(write(fd[1], &header, sizeof(header)) == sizeof(header));
    assert(write(fd[1], payload, 2048) == 2048);
    pid_t child = fork();
    assert(child >= 0);
    if (!child) {
        close(fd[0]);
        usleep(300000);
        assert(write(fd[1], payload + 2048, 2048) == 2048);
        close(fd[1]);
        _exit(0);
    }
    len = sizeof(out);
    rc = pdp_recv(&session, mac, &port, out, &len, true);
#ifdef LEGACY_RELAY
    assert(rc == AEMU_POSTOFFICE_CLIENT_OK);
#else
    assert(rc == AEMU_POSTOFFICE_CLIENT_SESSION_WOULD_BLOCK);
#endif
    printf("Incomplete packet, nonblocking result: %d\n", rc);
    assert(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    if (rc == AEMU_POSTOFFICE_CLIENT_SESSION_WOULD_BLOCK) {
        len = sizeof(out);
        assert(pdp_recv(&session, mac, &port, out, &len, true) == 0);
    }
    assert(port == 1234 && len == sizeof(out) && memcmp(payload, out, sizeof(out)) == 0);
    close(fd[0]); close(fd[1]);

    open_pair(fd, &session);
    send_packet(fd[1], payload, 72);
    send_packet(fd[1], payload, 120);
#ifdef LEGACY_RELAY
    assert(pdp_peek_next_size(&session) == 72);
#else
    assert(pdp_buffered_data_size(&session) == 192);
    // The OS descriptor is empty, but the emulator must still report readable.
    fd_set readable;
    FD_ZERO(&readable); FD_SET(fd[0], &readable);
    struct timeval now = {0};
    assert(select(fd[0] + 1, &readable, NULL, NULL, &now) == 0);
    assert(pdp_peek_next_size(&session) == 72);
    // Consume enough packets to wrap the ring, checking boundaries and content.
    for (int n = 0; n < 200; ++n) {
        len = sizeof(out);
        assert(pdp_recv(&session, mac, &port, out, &len, true) == 0);
        assert(len == (n % 2 ? 120 : 72));
        assert(memcmp(payload, out, len) == 0);
        if (n < 198) send_packet(fd[1], payload, n % 2 ? 120 : 72);
    }
    assert(pdp_buffered_data_size(&session) == 0);
#endif
    close(fd[0]); close(fd[1]);
#ifndef LEGACY_RELAY
    // PTP is a byte stream: data already buffered must survive short reads.
    struct ptp_session stream;
    memset(&stream, 0, sizeof(stream));
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fd) == 0);
    assert(fcntl(fd[0], F_SETFL, O_NONBLOCK) == 0);
    stream.sock = fd[0];
    struct aemu_postoffice_ptp_data stream_header = {.size = 120};
    assert(write(fd[1], &stream_header, sizeof(stream_header)) == sizeof(stream_header));
    assert(write(fd[1], payload, 72) == 72);
    len = 36;
    assert(ptp_recv(&stream, out, &len, true) == 0 && len == 36);
    assert(memcmp(payload, out, len) == 0);
    assert(ptp_peek_next_size(&stream) == 36);
    len = sizeof(out);
    assert(ptp_recv(&stream, out, &len, true) == 0 && len == 36);
    len = sizeof(out);
    assert(ptp_recv(&stream, out, &len, true) == AEMU_POSTOFFICE_CLIENT_SESSION_WOULD_BLOCK);
    assert(write(fd[1], payload + 72, 48) == 48);
    len = sizeof(out);
    assert(ptp_recv(&stream, out, &len, true) == 0 && len == 48);
    assert(memcmp(payload, out, len) == 0);
    close(fd[1]);
    assert(ptp_peek_next_size(&stream) == AEMU_POSTOFFICE_CLIENT_SESSION_DEAD && stream.dead);
#endif
    puts("Relay receive regression checks passed.");
    return 0;
}
