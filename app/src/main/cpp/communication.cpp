//
// Created by tansinan on 5/8/17.
//

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <android/log.h>
#include "communication.h"

static int remoteSocketFd = -1;
static int tunDeviceFd = -1;

static uint8_t *tunDeviceBuffer = NULL;
int tunDeviceBufferUsed = 0;
int tunDeviceBufferRemain = 0;
static uint8_t *over6PacketBuffer = NULL;
int over6PacketBufferUsed = 0;

static int currentStatus = BACKEND_STATE_CONNECTING;

static uint64_t totalInBytes = 0;
static uint64_t totalOutBytes = 0;
uint32_t inPackets = 0;
uint32_t outPackets = 0;

uint8_t comm_ip[4];
uint8_t comm_mask[4];
uint8_t comm_dns1[4];
uint8_t comm_dns2[4];
uint8_t comm_dns3[4];

static int readToBuf(int fd, uint8_t *buffer, int *used) {
    int total = 0, ret = 0;
    const size_t READ_SIZE = IP_PACKET_MAX_SIZE;
    while ((*used) < PIPE_BUF_LEN - READ_SIZE) {
        ret = read(fd, buffer + *used, READ_SIZE);
        if (ret <= 0) break;
        if(fd == tunDeviceFd) {
            outPackets += 1;
        }
        (*used) += ret;
        total += ret;
    }
    if (fd == tunDeviceFd) {
        totalOutBytes += total;
    }
    return ret;
}

void communication_set_tun_fd(int tunFd) {
    tunDeviceFd = tunFd;
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

void over6Handle(int fd, uint8_t *buffer, int *used, bool *heartbeated) {
    for (; ; ) {  // for all packets
        if ((*used) < sizeof(over6Packet)) return;  // need more read

        over6Packet *data = (over6Packet *) buffer;
        uint32_t len = endian_little_to_local_32(data->length);

        if (*used < len) return; // packet not complete

        size_t actual_size = len - sizeof(over6Packet);
        //__android_log_print(ANDROID_LOG_VERBOSE, "backend thread", "over6Handle fd=%d %d\n", fd,
        //                   len);

        if (data->type == TYPE_HEART) {
            __android_log_print(ANDROID_LOG_VERBOSE, "backend thread", "heart beat recv");
            *heartbeated = true;
        } else if (data->type == TYPE_REPLY) {
            // send packet to raw network
            int ret;
            if (fd > 0 && // if tun not ready, just drop all packets
                    (ret = write(fd, data->data, actual_size)) < actual_size) {
                __android_log_print(ANDROID_LOG_WARN, "backend thread",
                                    "RAW size = %d err = %d", len, ret);
            }
            if (ret >= 0 && fd == tunDeviceFd) {
                totalInBytes += ret;
                inPackets += 1;
            }
        } else if (data->type == TYPE_IP_REPLY) {
            __android_log_print(ANDROID_LOG_VERBOSE, "backend thread", "ip conf recv");
            char buf[BUFFER_LENGTH], *ptr = buf;
            int a, b, c, d;
            size_t len = (actual_size >= BUFFER_LENGTH ? BUFFER_LENGTH - 1 : actual_size);
            memcpy(ptr, buffer + sizeof(over6Packet), len); ptr[len] = '\0';
            __android_log_print(ANDROID_LOG_VERBOSE, "backend thread", "%s", ptr);
#define NEXT_IP(A) do { \
                sscanf(ptr, "%d.%d.%d.%d", &a, &b, &c, &d); \
                A[0] = (uint8_t )a; A[1] = (uint8_t )b; \
                A[2] = (uint8_t )c; A[3] = (uint8_t )d; \
                ++ptr; \
                while (*ptr && *ptr != ' ') ptr++; \
            } while (0)
            NEXT_IP(comm_ip); NEXT_IP(comm_mask);
            NEXT_IP(comm_dns1); NEXT_IP(comm_dns2); NEXT_IP(comm_dns3);
        } else {
            __android_log_print(ANDROID_LOG_WARN, "backend thread", "Not implemented yet\n");
        }

        // If tun not writable or busy, data can and must be dropped to avoid delay
        memmove(buffer, buffer + len, *used - len);
        *used -= len;
    }
}

void rawToOver6(int fd, uint8_t *buffer, int *used, int *remain) {
    if (!*used) return;  // no data
    int transf = *used;
    over6Packet header;
    header.type = TYPE_REQUEST;
    header.length = endian_local_to_little_32(transf + sizeof(header));
    int temp;
    if ((*remain) == 0) {
        if ((temp = write(fd, &header, sizeof(header))) < (int)sizeof(header)) {
            __android_log_print(ANDROID_LOG_ERROR, "backend thread",
                                "->O6 size = %ul err = %d\n", sizeof(header), temp);
        }
        if ((temp = write(fd, buffer, transf)) < 0) {
            __android_log_print(ANDROID_LOG_ERROR, "backend thread",
                                "->O6 size = %d err = %d\n", transf, temp);
        }
        (*remain) = transf - temp;
    } else {
        temp = write(fd, buffer, *remain);
        (*remain) -= temp;
    }
    //__android_log_print(ANDROID_LOG_VERBOSE, "backend thread", "rawToOver6 fd=%d %d\n", fd, *used);
    memmove(buffer, buffer + temp, *used - temp);
    *used -= temp;
}

void communication_init(int _remoteSocketFd) {
    remoteSocketFd = _remoteSocketFd;
    tunDeviceFd = -1;
    //TODO: Free memory on thread exit.
    tunDeviceBuffer = new uint8_t[PIPE_BUF_LEN];
    tunDeviceBufferUsed = 0;
    tunDeviceBufferRemain = 0;
    over6PacketBuffer = new uint8_t[PIPE_BUF_LEN];
    over6PacketBufferUsed = 0;
    totalInBytes = 0;
    totalOutBytes = 0;
    inPackets = 0;
    outPackets = 0;
    memset(comm_ip, 255U, 4);
    memset(comm_mask, 255U, 4);
    memset(comm_dns1, 255U, 4);
    memset(comm_dns2, 255U, 4);
    memset(comm_dns3, 255U, 4);
}

int communication_get_status() {
    return currentStatus;
}

void communication_set_status(int _currentStatus) {
    currentStatus = _currentStatus;
}

void communication_handle_tun_read() {
    readToBuf(tunDeviceFd, tunDeviceBuffer, &tunDeviceBufferUsed);
}

void communication_handle_4over6_packets(bool *heartbeated) {
    // Handle different 4over6 packet types (IP/DNS info, IP packet, or heartbeat)
    over6Handle(tunDeviceFd, over6PacketBuffer, &over6PacketBufferUsed, heartbeated);
}

void communication_handle_4over6_socket_read() {
    readToBuf(remoteSocketFd, over6PacketBuffer, &over6PacketBufferUsed);
}

void communication_handle_4over6_socket_write() {
    rawToOver6(remoteSocketFd, tunDeviceBuffer, &tunDeviceBufferUsed, &tunDeviceBufferRemain);
}

void communication_send_heartbeat() {
    over6Packet header;
    header.type = TYPE_HEART;
    header.length = endian_local_to_little_32(sizeof(header));
    if (write(remoteSocketFd, &header, sizeof(header)) < sizeof(header)) {
        __android_log_print(ANDROID_LOG_ERROR, "backend thread",
                            "->O6 failed to send heartbeat");
    }
    __android_log_print(ANDROID_LOG_VERBOSE, "backend thread", "sent heartbeat\n");
}

int communication_is_ip_confiugration_recevied() {
    // Broadcast IP is invalid and initial value.
    return comm_ip[0] != 255 || comm_ip[1] != 255 ||
            comm_ip[2] != 255 || comm_ip[3] != 255 ;
}

void communication_get_ip_confiugration() {
    // Send packet to request IP configuration
    over6Packet header;
    header.type = TYPE_IP_REQUEST;
    header.length = endian_local_to_little_32(sizeof(header));
    if (write(remoteSocketFd, &header, sizeof(header)) < sizeof(header)) {
        __android_log_print(ANDROID_LOG_ERROR, "backend thread",
                            "->O6 failed to send IP request");
    }
    __android_log_print(ANDROID_LOG_VERBOSE, "backend thread", "sent IP request\n");
}

void communication_get_statistics(uint64_t *inBytes, uint64_t *outBytes) {
    (*inBytes) = totalInBytes;
    (*outBytes) = totalOutBytes;
}