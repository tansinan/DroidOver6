//
// Created by tansinan on 5/4/17.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <android/log.h>
#include <errno.h>

#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>

#include "communication.h"
#include "frontend_ipc.h"

static int connectTo4Over6Server(const char *hostName, int port) {
    int ret = 0;
    struct sockaddr_in6 serv_addr;
    struct hostent *server;

    int socketFd = socket(AF_INET6, SOCK_STREAM, 0);
    if (socketFd < 0) return socketFd;

    int flags = fcntl(socketFd, F_GETFL, 0);
    if (flags == -1) return 0;
    flags |= O_NONBLOCK;
    if (fcntl(socketFd, F_SETFL, flags)) return 0;

    server = gethostbyname2(hostName, AF_INET6);
    if (server == NULL)
        return -1;

    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin6_flowinfo = 0;
    serv_addr.sin6_family = AF_INET6;
    memmove((char *) &serv_addr.sin6_addr.s6_addr, (char *) server->h_addr, server->h_length);
    serv_addr.sin6_port = htons((unsigned short) port);

    // TODO: Connect is still blocking.
    ret = connect(socketFd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
    if (ret < 0 && errno != EINPROGRESS) return ret;
    return socketFd;
}

static int createEpollFd() {
    int epollFd = epoll_create(1);
    return epollFd;
}

static int addToEpollFd(int epollFd, int *fds, int count) {
    for (int i = 0; i < count; i++) {
        int flags = fcntl(fds[i], F_GETFL, 0);
        if (flags == -1) {
            return 0;
        }
        flags |= O_NONBLOCK;
        if (fcntl(fds[i], F_SETFL, flags)) {
            return 0;
        }
        struct epoll_event event;
        event.data.fd = fds[i];
        event.events = EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP;
        if (epoll_ctl(epollFd, EPOLL_CTL_ADD, fds[i], &event) == -1) {
            return 0;
        }
    }

    return 1;
}

int backend_main(const char *hostName, int port,
                 int commandPipeFd, int responsePipeFd) {
    int tunDeviceFd = -1, remoteSocketFd = -1;
    int epollFd = -1;
    epoll_event *events = NULL;
    int fds[2];

    __android_log_print(ANDROID_LOG_VERBOSE,
                        "4over6 backend", "Entering backend_main @ %s:%d", hostName, port);

    if ((remoteSocketFd = connectTo4Over6Server(hostName, port)) < 0) goto failed;
    if ((epollFd = createEpollFd()) < 0) goto failed;

    fds[0] = commandPipeFd; fds[1] = remoteSocketFd;
    if (!addToEpollFd(epollFd, fds, 2)) goto failed;

    communication_init(remoteSocketFd);
    events = (epoll_event *) calloc(10, sizeof(epoll_event));
    communication_set_status(BACKEND_STATE_CONNECTED);

    // Event loop
    // TODO: heartbeat
    for (; ; ) {
        int eventCount = epoll_wait(epollFd, events, 10, -1);
        for (int i = 0; i < eventCount; i++) {

            if ((events[i].events & EPOLLERR) ||
                (events[i].events & EPOLLHUP)) {
                __android_log_print(ANDROID_LOG_VERBOSE, "backend thread",
                                    "epoll event: err or hup\n");
                goto failed;
            }

            if (events[i].events & EPOLLIN) {
                if (events[i].data.fd == commandPipeFd) {
                    int ret = handle_frontend_command(commandPipeFd, responsePipeFd);
                    if (ret == BACKEND_IPC_COMMAND_SET_TUNNEL_FD) {
                        addToEpollFd(epollFd, &tunFd, 1);
                        tunDeviceFd = tunFd;
                    } else if (ret == BACKEND_IPC_COMMAND_TERMINATE) {
                        goto failed;
                    }
                } else if (events[i].data.fd == tunDeviceFd) {
                    communication_handle_tun_read();
                } else if (events[i].data.fd == remoteSocketFd) {
                    communication_handle_4over6_socket_read();
                }
            }

            if (events[i].events & EPOLLOUT) {
                if (events[i].data.fd == tunDeviceFd) {
                    communication_handle_tun_write();
                } else if (events[i].data.fd == remoteSocketFd) {
                    communication_handle_4over6_socket_write();
                }
            }
        }

        struct epoll_event event;
        // Setup events for tun & remote server.
        if (tunDeviceFd != -1) {
            event.data.fd = tunDeviceFd;
            event.events = EPOLLIN | EPOLLERR | EPOLLHUP;
            if (over6PacketBufferUsed > 0)
                event.events |= EPOLLOUT;
            epoll_ctl(epollFd, EPOLL_CTL_MOD, tunDeviceFd, &event);
        }
        event.data.fd = remoteSocketFd;
        event.events = EPOLLIN | EPOLLERR | EPOLLHUP;
        if (tunDeviceBufferUsed > 0)
            event.events |= EPOLLOUT;
        epoll_ctl(epollFd, EPOLL_CTL_MOD, remoteSocketFd, &event);
    }

failed:
    if (remoteSocketFd >= 0) close(remoteSocketFd);
    if (tunDeviceFd >= 0) close(tunDeviceFd);
    if (commandPipeFd >= 0) close(commandPipeFd);
    communication_set_status(BACKEND_STATE_DISCONNECTED);

    __android_log_print(ANDROID_LOG_VERBOSE,
                        "4over6 backend", "Leaving backend_main", hostName, port);
    return 0;
}