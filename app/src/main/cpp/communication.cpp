//
// Created by tansinan on 5/8/17.
//

#include <unistd.h>
#include <stdlib.h>
#include <android/log.h>
#include "communication.h"

static int remoteSocketFd = -1;
static int tunDeviceFd = -1;

static uint8_t *tunDeviceBuffer = NULL;
int tunDeviceBufferUsed = 0;
static uint8_t *over6PacketBuffer = NULL;
int over6PacketBufferUsed = 0;

static int currentStatus = BACKEND_STATE_CONNECTING;

// TODO: Define stored IP/DNS info.

static int readToBuf(int fd, uint8_t *buffer, int *used) {
    int total = 0, ret = 0;
    const size_t READ_SIZE = IP_PACKET_MAX_SIZE;
    while ((*used) < PIPE_BUF_LEN - READ_SIZE) {
        ret = read(fd, buffer + *used, READ_SIZE);
        if (ret <= 0) break;
        (*used) += ret;
        total += ret;
    }
    return ret;
}

void communication_set_tun_fd(int tunFd) {
    tunDeviceFd = tunFd;
}

void over6Handle(int fd, uint8_t *buffer, int *used) {
    for (;;) {  // for all packets
        if ((*used) < 4) return;  // need more read

        over6Packet *data = (over6Packet *) buffer;
        uint32_t len = ntohl(data->length);
        if (*used < len) return; // packet not complete

        size_t actual_size = len - sizeof(over6Packet);
        __android_log_print(ANDROID_LOG_VERBOSE, "backend thread", "over6Handle fd=%d %d\n", fd,
                            len);

        if (data->type == TYPE_HEART) {
            // ignore
        } else if (data->type == TYPE_REPLY) {
            // send packet to raw network
            int ret = 0;
            if ((ret = write(fd, data->data, actual_size)) < actual_size) {
                __android_log_print(ANDROID_LOG_ERROR, "backend thread",
                                    "RAW size = %d err = %d", len, ret);
            }
        } else {
            __android_log_print(ANDROID_LOG_WARN, "backend thread", "Not implemented yet\n");
        }

        memmove(buffer, buffer + len, *used - len);
        *used -= len;
    }
}

void rawToOver6(int fd, uint8_t *buffer, int *used) {
    if (!*used) return;  // no data
    over6Packet header;
    header.type = TYPE_REQUEST;
    header.length = htonl(*used + sizeof(header));
    int temp;
    if ((temp = write(fd, &header, sizeof(header))) < sizeof(header)) {
        __android_log_print(ANDROID_LOG_ERROR, "backend thread",
                            "->O6 size = %d err = %d\n", sizeof(header), temp);
    }
    if ((temp = write(fd, buffer, *used)) < *used) {
        __android_log_print(ANDROID_LOG_ERROR, "backend thread",
                            "->O6 size = %d err = %d\n", *used, temp);
    }
    __android_log_print(ANDROID_LOG_VERBOSE, "backend thread", "rawToOver6 fd=%d %d\n", fd, *used);
    *used = 0;
}

void communication_init(int _remoteSocketFd) {
    remoteSocketFd = _remoteSocketFd;
    //TODO: Free memory on thread exit.
    tunDeviceBuffer = new uint8_t[PIPE_BUF_LEN];
    tunDeviceBufferUsed = 0;
    over6PacketBuffer = new uint8_t[PIPE_BUF_LEN];
    over6PacketBufferUsed = 0;
}

int communication_get_status() {
    return currentStatus;
}

void communication_set_status(int _currentStatus) {
    currentStatus = _currentStatus;
}

void communication_handle_tun_read() {
    // TODO: Add output bytes counter for statistics.
    readToBuf(tunDeviceFd, tunDeviceBuffer, &tunDeviceBufferUsed);
}

void communication_handle_tun_write() {
    // TODO: Add input bytes counter for statistics.
    over6Handle(tunDeviceFd, over6PacketBuffer, &over6PacketBufferUsed);
}

void communication_handle_4over6_socket_read() {
    // TODO: Need to follow 4over6 specification.
    // TODO: Actual 4over6 packet have different types (IP/DNS info, IP packet, or heartbeat)
    // This assumes that ip packet is sent one by one via tcp stream.
    readToBuf(remoteSocketFd, over6PacketBuffer, &over6PacketBufferUsed);
}

void communication_handle_4over6_socket_write() {
    // TODO: Need to follow 4over6 specification.
    rawToOver6(remoteSocketFd, tunDeviceBuffer, &tunDeviceBufferUsed);
}

void communication_handle_timer() {
    // TODO: Send heartbeat packet
}

int communication_is_ip_confiugration_recevied() {
    // TODO: Returns whether IPv4 Configuration is already received through network.
}

void communication_get_ip_confiugration() {
    // TODO: Returns IPv4 Configuration
}

void communication_get_statistics() {
    // TODO: Returns network statistics
}