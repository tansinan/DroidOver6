//
// Created by tansinan on 5/4/17.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <android/log.h>

#include <sys/socket.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>

const static int IP_PACKET_MAX_SIZE = 65536;

static void fail(int commandPipeFd, int responsePipeFd, const char *errorMessage) {
    unsigned char command;
    for (;;) {
        int ret = read(commandPipeFd, &command, 1);
        write(responsePipeFd, "err", 3);
    }
}

static int connectTo4Over6Server(const char *hostName, int port, int commandPipeFd, int responsePipeFd) {
    struct sockaddr_in6 serv_addr;
    struct hostent *server;
    int socketFd = socket(AF_INET6, SOCK_STREAM, 0);
    if (socketFd < 0)
        fail(commandPipeFd, responsePipeFd, "Cannot create IPv6 socket.");

    server = gethostbyname2(hostName, AF_INET6);
    if (server == NULL)
        fail(commandPipeFd, responsePipeFd, "Host address resolution failed.");

    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin6_flowinfo = 0;
    serv_addr.sin6_family = AF_INET6;
    memmove((char *) &serv_addr.sin6_addr.s6_addr, (char *) server->h_addr, server->h_length);
    serv_addr.sin6_port = htons((unsigned short)port);

    //Sockets Layer Call: connect()
    if (connect(socketFd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0)
        fail(commandPipeFd, responsePipeFd, "Cannot connect to remote server.");
    return socketFd;
}

static int createEpollFd(int commandPipeFd, int responsePipeFd) {
    int epollFd = epoll_create(1);
    if (epollFd == -1) {
        fail(commandPipeFd, responsePipeFd, "Cannot initialize I/O multiplex.");
    }
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

static int readAllDataToBuffer(int fd, char* buffer, int* bufferUsed, int limit = 2 * IP_PACKET_MAX_SIZE)
{
    const ssize_t READ_SIZE = IP_PACKET_MAX_SIZE;
    while((*bufferUsed) < limit - READ_SIZE)
    {
        int ret = read(fd, buffer + *bufferUsed, READ_SIZE);
        if(ret <= 0)
            return ret;
        (*bufferUsed) += ret;
        return 1;
    }
}

static void sendIpPacketBuffer(int fd, char *buffer, int *bufferUsed) {
    for(;;) {
        if((*bufferUsed) < 4) {
            return;
        }
        int firstPacketSize = buffer[2] * 256 + buffer[3];
        if ((*bufferUsed) < firstPacketSize) {
            return;
        }
        if(firstPacketSize < 20 || ((buffer[0] & 0xF0) != 0x40))
        {
            __android_log_print(ANDROID_LOG_VERBOSE, "backend thread",
                                "Invalid Packet. size = %d "
                                "first byte = %02x", firstPacketSize, bufferUsed[0]);
            exit(0);
        }
        // TODO: if the return value is positive while less than firstPacketSize, this won't work.
        int temp;
        if((temp = write(fd, buffer, firstPacketSize)) < firstPacketSize)
        {
            __android_log_print(ANDROID_LOG_VERBOSE, "backend thread", "fail %d", temp);
            exit(0);
        }
        memmove(buffer, buffer + firstPacketSize, (*bufferUsed) - firstPacketSize);
        (*bufferUsed) -= firstPacketSize;
    }
}

int backend_main(const char* hostName, int port,
                 int tunDeviceFd, int commandPipeFd, int responsePipeFd) {
    __android_log_print(ANDROID_LOG_VERBOSE, "backend thread", "%s:%d", hostName, port);
    int remoteSocketFd = connectTo4Over6Server(hostName, port, commandPipeFd, responsePipeFd);
    int epollFd = createEpollFd(commandPipeFd, responsePipeFd);
    int fds[3] = {tunDeviceFd, commandPipeFd, remoteSocketFd};
    if (!addToEpollFd(epollFd, fds, 3)) {
        fail(commandPipeFd, responsePipeFd, "Cannot initialize I/O multiplex.");
    }

    char *tunDeviceBuffer = new char[IP_PACKET_MAX_SIZE * 100];
    int tunDeviceBufferUsed = 0;
    char *over6PacketBuffer = new char[IP_PACKET_MAX_SIZE * 100];
    int over6PacketBufferUsed = 0;

    epoll_event *events = (epoll_event *) calloc(3, sizeof(epoll_event));

    // Event loop
    for(;;) {
        int eventCount = epoll_wait(epollFd, events, 3, -1);
        for (int i = 0; i < eventCount; i++) {
            if ((events[i].events & EPOLLERR) ||
                (events[i].events & EPOLLHUP) ||
                (!(events[i].events & (EPOLLIN | EPOLLOUT))))
            {
                /* An error has occured on this fd, or the socket is not
                   ready for reading (why were we notified then?) */
                __android_log_print(ANDROID_LOG_VERBOSE, "backend thread", "epoll error\n");
                close (events[i].data.fd);
                exit(0);
                //continue;
            }

            if (events[i].events & EPOLLIN) {
                if (events[i].data.fd == commandPipeFd) {
                    char c;
                    int temp = read(commandPipeFd, &c, 1);
                    if (temp == 1) {
                        write(responsePipeFd, "Alive", 5);
                    }
                } else if (events[i].data.fd == tunDeviceFd) {
                    __android_log_print(ANDROID_LOG_VERBOSE, "backend thread", "TUN read\n");
                    readAllDataToBuffer(tunDeviceFd, tunDeviceBuffer, &tunDeviceBufferUsed);
                } else if (events[i].data.fd == remoteSocketFd) {
                    __android_log_print(ANDROID_LOG_VERBOSE, "backend thread", "REMOTE read\n");
                    // This assumes that ip packet is sent one by one via tcp stream.
                    // TODO: Need to follow 4over6 specification.
                    readAllDataToBuffer(remoteSocketFd, over6PacketBuffer, &over6PacketBufferUsed);
                    sendIpPacketBuffer(tunDeviceFd, over6PacketBuffer, &over6PacketBufferUsed);
                }
            }
            else if(events[i].events & EPOLLOUT) {
                if (events[i].data.fd == tunDeviceFd && over6PacketBufferUsed > 0) {
                    __android_log_print(ANDROID_LOG_VERBOSE, "backend thread", "TUN write\n");
                    sendIpPacketBuffer(tunDeviceFd, over6PacketBuffer, &over6PacketBufferUsed);
                }
                else if (events[i].data.fd == remoteSocketFd && tunDeviceBufferUsed > 0) {
                    __android_log_print(ANDROID_LOG_VERBOSE, "backend thread", "REMOTE write\n");
                    sendIpPacketBuffer(remoteSocketFd, tunDeviceBuffer, &tunDeviceBufferUsed);
                }
            }
        }
    }
}