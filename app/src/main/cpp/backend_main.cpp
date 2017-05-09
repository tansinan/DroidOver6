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
    if (socketFd < 0)
        return socketFd;

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
    if (ret < 0)
        return ret;
    return socketFd;
}

static int createEpollFd(int commandPipeFd, int responsePipeFd) {
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
        event.events = EPOLLIN | EPOLLOUT;
        if (epoll_ctl(epollFd, EPOLL_CTL_ADD, fds[i], &event) == -1) {
            return 0;
        }
    }

    return 1;
}

int backend_main(const char *hostName, int port,
                 int tunDeviceFd, int commandPipeFd, int responsePipeFd) {
    __android_log_print(ANDROID_LOG_VERBOSE,
                        "4over6 backend", "Entering backend_main @ %s:%d", hostName, port);
    do {
        int remoteSocketFd = connectTo4Over6Server(hostName, port);
        if (remoteSocketFd < 0) {
            communication_set_status(BACKEND_STATE_DISCONNECTED);
            break;
        }
        int epollFd = createEpollFd(commandPipeFd, responsePipeFd);
        if(epollFd < 0) {
            communication_set_status(BACKEND_STATE_DISCONNECTED);
            break;
        }
        int fds[2] = {commandPipeFd, remoteSocketFd};
        if (!addToEpollFd(epollFd, fds, 2)) {
            communication_set_status(BACKEND_STATE_DISCONNECTED);
            break;
        }

        communication_init(remoteSocketFd);
        epoll_event *events = (epoll_event *) calloc(3, sizeof(epoll_event));
        communication_set_status(BACKEND_STATE_CONNECTED);

        // Event loop
        // TODO: To sent heartbeat packet, an timer fd needs to be created.
        for (;;) {
            bool encounterError = false;
            int eventCount = epoll_wait(epollFd, events, 3, -1);
            for (int i = 0; i < eventCount; i++) {
                if ((events[i].events & EPOLLERR) ||
                    (events[i].events & EPOLLHUP) ||
                    (!(events[i].events & (EPOLLIN | EPOLLOUT)))) {
                    __android_log_print(ANDROID_LOG_VERBOSE, "backend thread", "epoll error\n");
                    encounterError = true;
                    break;
                }

                if (events[i].events & EPOLLIN) {
                    if (events[i].data.fd == commandPipeFd) {
                        int ret = handle_frontend_command(commandPipeFd, responsePipeFd);
                        if (ret == BACKEND_IPC_COMMAND_SET_TUNNEL_FD) {
                            addToEpollFd(epollFd, &tunFd, 1);
                            tunDeviceFd = tunFd;
                        }
                        else if(ret == BACKEND_IPC_COMMAND_TERMINATE)
                        {
                            encounterError = true;
                            break;
                        }
                    } else if (events[i].data.fd == tunDeviceFd) {
                        communication_handle_tun_read();
                    } else if (events[i].data.fd == remoteSocketFd) {
                        communication_handle_4over6_socket_read();
                    }
                } else if (events[i].events & EPOLLOUT) {
                    if (events[i].data.fd == tunDeviceFd) {
                        communication_handle_tun_write();
                    } else if (events[i].data.fd == remoteSocketFd) {
                        communication_handle_4over6_socket_write();
                    }
                }
            }
            if (encounterError) {
                break;
            }
        }
    } while (0);
    while (handle_frontend_command(commandPipeFd, responsePipeFd) != BACKEND_IPC_COMMAND_TERMINATE);
    __android_log_print(ANDROID_LOG_VERBOSE,
                        "4over6 backend", "Leaving backend_main", hostName, port);
    return 0;
}