#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <bits/ioctls.h>
#include <errno.h>
#include <linux/if.h>
#include <linux/if_tun.h>

typedef struct epoll_event epoll_event;

static const int IP_PACKET_MAX_SIZE = 65536;
static const int PIPE_BUF_LEN = 65536 * 100;
static const int SERVER_PORT = 5678;
static const int BUFFER_LENGTH = 250;

static const uint8_t TYPE_IP_REQUEST = 100;
static const uint8_t TYPE_IP_REPLY   = 101;
static const uint8_t TYPE_REQUEST = 102;
static const uint8_t TYPE_REPLY = 103;
static const uint8_t TYPE_HEART = 104;

int server_fd = -1, tun_dev_fd, over6_fd = -1;

typedef struct {
    uint32_t length;
    uint8_t type;
    uint8_t data[];
} __attribute__((packed)) over6Packet;

static int tunOpen(char *devname) {
    struct ifreq ifr;
    int fd, err;

    if ((fd = open("/dev/net/tun", O_RDWR)) == -1) {
        perror("open /dev/net/tun");
        exit(1);
    }
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
    strncpy(ifr.ifr_name, devname, IFNAMSIZ);

    /* ioctl will use if_name as the name of TUN
     * interface to open: "tun0", etc. */
    if ((err = ioctl(fd, TUNSETIFF, (void *) &ifr)) == -1) {
        perror("ioctl TUNSETIFF");
        close(fd);
        exit(1);
    }

    /* After the ioctl call the fd is "connected" to tun device specified
     * by devname */
    return fd;
}

static uint32_t endian_little_to_local_32(uint32_t val) {
    uint8_t *buffer = (uint8_t *)&val;
    uint32_t ret = 0;
    for(int i = 0; i < 4; i++) {
        ret += (buffer[i] << (8 * i));
    }
    return ret;
};

static uint32_t endian_local_to_little_32(uint32_t val) {
    uint32_t ret;
    uint8_t *data = (uint8_t *)&ret;
    for(int i = 0; i < 4; i++)
    {
        data[i] = (uint8_t)val;
        val >>= 8;
    }
    return ret;
}

static int readToBuf(int fd, uint8_t *buffer, int *used) {
    int total = 0, ret = 0;
    const size_t READ_SIZE = IP_PACKET_MAX_SIZE;
    while ((uint32_t)(*used) < PIPE_BUF_LEN - READ_SIZE) {
        ret = read(fd, buffer + *used, READ_SIZE);
        if (ret <= 0) break;
        (*used) += ret;
        total += ret;
    }
    printf(" >%d< ", total);
    return total ? total : ret;
}

static void over6Handle(int fd, uint8_t *buffer, int *used) {
    for (;;) {  // for all packets
        if ((*used) < 4) return;  // need more read

        over6Packet *data = (over6Packet *) buffer;
        uint32_t len = endian_little_to_local_32(data->length);
        if ((uint32_t)*used < len) return; // packet not complete

        size_t actual_size = len - sizeof(over6Packet);

        if (data->type == TYPE_HEART) {
            printf("\nINFO heart beat recv\n");
        } else if (data->type == TYPE_REQUEST) {
            // send packet to raw network
            int ret = 0;
            if ((ret = write(fd, data->data, actual_size)) < (int)actual_size) {
                printf("\nERROR ->RAW size = %d err = %d\n", len, ret);
            }
        } else if (data->type == TYPE_IP_REQUEST) {
            char reply[] = "10.10.10.2 255.255.255.0 166.111.8.28 166.111.8.29 8.8.8.8";
            over6Packet header;
            header.type = TYPE_IP_REPLY;
            header.length = endian_local_to_little_32(sizeof(reply) + sizeof(header));
            if (write(over6_fd, &header, sizeof(header)) < (int)sizeof(header)) {
                printf("\nERROR ->O6 ip reply\n");
            }
            if (write(over6_fd, reply, sizeof(reply)) < (int)sizeof(reply)) {
                printf("\nERROR ->O6 ip reply\n");
            }
        } else {
            printf("\nWARN Not implemented yet\n");
        }

        printf(" ]%d[ ", len);
        memmove(buffer, buffer + len, *used - len);
        *used -= len;
    }
}

static void rawToOver6(int fd, uint8_t *buffer, int *used, int *remain) {
    if (!*used) return;  // no data
    int transf = *used;
    over6Packet header;
    header.type = TYPE_REPLY;
    header.length = endian_local_to_little_32(transf + sizeof(header));
    int temp;
    if ((*remain) == 0) {
        if ((temp = write(fd, &header, sizeof(header))) < (int)sizeof(header)) {
            printf("ERROR ->O6 size = %lu err = %d\n", sizeof(header), temp);
        }
        if ((temp = write(fd, buffer, transf)) < 0) {
            printf("ERROR ->O6 size = %d err = %d\n", transf, temp);
        }
        (*remain) = transf - temp;
    } else {
        temp = write(fd, buffer, *remain);
        (*remain) -= temp;
    }
    printf(" [%lu] ", temp + sizeof(header));
    memmove(buffer, buffer + temp, *used - temp);
    *used -= temp;
}

static int createEpollFd() {
    int fd = epoll_create1(0);
    if (fd == -1) return -1;
    return fd;
}

static int addToEpollFd(int epollFd, int *fds, int count) {
    int i;
    for (i = 0; i < count; i++) {
        int flags = fcntl(fds[i], F_GETFL, 0);
        if (flags == -1) return 0;
        flags |= O_NONBLOCK;  // set fds[i] to be nonblocking
        if (fcntl(fds[i], F_SETFL, flags)) return 0;
        struct epoll_event event;
        event.data.fd = fds[i];
        event.events = EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP;
        if (epoll_ctl(epollFd, EPOLL_CTL_ADD, fds[i], &event) == -1) return 0;
    }
    return 1;
}

int main(int argc, char *argv[]) {
    int last_heartbeat = time(NULL);

    // init tun dev
    if (argc < 2) {
        tun_dev_fd = tunOpen("over6");
    } else {
        tun_dev_fd = tunOpen(argv[1]);
    }

    // init server socket
    int on = 1;
    struct sockaddr_in6 serveraddr, clientaddr;
    socklen_t addrlen = sizeof(clientaddr);
    char str[INET6_ADDRSTRLEN];
    if ((server_fd = socket(AF_INET6, SOCK_STREAM, 0)) < 0) {
        perror("\nERROR socket() failed");
        goto failed;
    }
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR,
                   (char *) &on, sizeof(on)) < 0) {
        perror("\nERROR setsockopt(SO_REUSEADDR) failed");
        goto failed;
    }
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin6_family = AF_INET6;
    serveraddr.sin6_port = htons(SERVER_PORT);
    serveraddr.sin6_addr = in6addr_any;

    // listen on server
    if (bind(server_fd, (struct sockaddr *) &serveraddr,
             sizeof(serveraddr)) < 0) {
        perror("\nERROR bind() failed");
        goto failed;
    }
    if (listen(server_fd, 10) < 0) {
        perror("\nERROR listen() failed");
        goto failed;
    }

    printf("\nINFO Ready for client connect().\n");

    if ((over6_fd = accept(server_fd, NULL, NULL)) < 0) {
        perror("\nERROR accept() failed");
        goto failed;
    } else {
        getpeername(over6_fd, (struct sockaddr *) &clientaddr, &addrlen);
        if (inet_ntop(AF_INET6, &clientaddr.sin6_addr, str, sizeof(str))) {
            printf("\nINFO Client address: %s\n", str);
            printf("INFO Client port: %d\n", ntohs(clientaddr.sin6_port));
        }
    }

    uint8_t *ipBuffer = malloc(PIPE_BUF_LEN);
    int ipBufferUsed = 0;
    int ipBufferRemain = 0;
    uint8_t *over6PacketBuffer = malloc(PIPE_BUF_LEN);
    int over6PacketBufferUsed = 0;

    int epollFd = createEpollFd();
    if (epollFd < 0) {
        printf("\nCannot initialize I/O multiplex.\n");
        goto failed;
    }
    int fds[2] = {over6_fd, tun_dev_fd};
    if (!addToEpollFd(epollFd, fds, 2)) {
        printf("\nCannot initialize I/O multiplex.\n");
        goto failed;
    }

    epoll_event *events = (epoll_event *) calloc(10, sizeof(epoll_event));
    // Event loop
    for (; ; ) {
        int eventCount = epoll_wait(epollFd, events, 10, 20);
        int i, handled_read = 0;
        for (i = 0; i < eventCount; i++) {
            if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP)) {
                printf("\nERROR epoll event: err or hup\n");
                if (events[i].data.fd == over6_fd) printf("ERROR on over6\n");
                else printf("ERROR on tun\n");
                goto failed;
            } else if (!(events[i].events & (EPOLLIN | EPOLLOUT))) {
                printf("\nWARN Unknown event\n");
                continue;
            }

            if (events[i].events & EPOLLIN) {
                if (events[i].data.fd == tun_dev_fd) {
                    int ret = readToBuf(tun_dev_fd, ipBuffer, &ipBufferUsed);
                    if (ret == EOF) printf("WARN tun is not ready\n");
                    handled_read += 1;
                } else if (events[i].data.fd == over6_fd) {
                    int ret = readToBuf(over6_fd, over6PacketBuffer, &over6PacketBufferUsed);
                    if (ret == 0) {
                        printf("INFO client disconnected\n");
                        goto finish;
                    }
                    handled_read += 1;
                }
            }

            if (events[i].events & EPOLLOUT) {
                if (events[i].data.fd == tun_dev_fd) {
                    over6Handle(tun_dev_fd, over6PacketBuffer, &over6PacketBufferUsed);
                } else if (events[i].data.fd == over6_fd) {
                    rawToOver6(over6_fd, ipBuffer, &ipBufferUsed, &ipBufferRemain);
                }
            }
        }
        
        if (time(NULL) - last_heartbeat > 20 && ipBufferRemain == 0) {
            over6Packet header;
            header.type = TYPE_HEART;
            header.length = endian_local_to_little_32(sizeof(header));
            if ((write(over6_fd, &header, sizeof(header))) < (int)sizeof(header)) {
                printf("\nWARN failed to send heartbeat\n");
            }
            last_heartbeat = time(NULL);
        }
        
        if (handled_read) printf("\n");

        struct epoll_event event;
        // Setup events for tun & remote server.
        if (tun_dev_fd != -1) {
            event.data.fd = tun_dev_fd;
            event.events = EPOLLIN | EPOLLERR | EPOLLHUP;
            if (over6PacketBufferUsed > 0)
                event.events |= EPOLLOUT;
            epoll_ctl(epollFd, EPOLL_CTL_MOD, tun_dev_fd, &event);
        }
        event.data.fd = over6_fd;
        event.events = EPOLLIN | EPOLLERR | EPOLLHUP;
        if (ipBufferUsed > 0)
            event.events |= EPOLLOUT;
        epoll_ctl(epollFd, EPOLL_CTL_MOD, over6_fd, &event);
    }

    finish:
    if (server_fd != -1) close(server_fd);
    if (over6_fd != -1) close(over6_fd);
    if (tun_dev_fd != -1) close(tun_dev_fd);
    return 0;

    failed:
    if (server_fd != -1) close(server_fd);
    if (over6_fd != -1) close(over6_fd);
    if (tun_dev_fd != -1) close(tun_dev_fd);
    return EXIT_FAILURE;
}
