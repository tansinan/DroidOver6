//
// Created by tansinan on 5/8/17.
//

#include <unistd.h>
#include <stdlib.h>
#include <android/log.h>
#include "communication.h"

const static int IP_PACKET_MAX_SIZE = 65536;

static int remoteSocketFd = -1;
static int tunDeviceFd = -1;

static char *tunDeviceBuffer = NULL;
int tunDeviceBufferUsed = 0;
static char *over6PacketBuffer = NULL;
int over6PacketBufferUsed = 0;

static int currentStatus = BACKEND_STATE_CONNECTING;

// TODO: Define stored IP/DNS info.

static int readAllDataToBuffer(int fd, unsigned char* buffer, int* bufferUsed, int limit = 2 * IP_PACKET_MAX_SIZE)
{
    // TODO: Use a circular buffer is much better
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

void communication_set_tun_fd(int tunFd)
{
    tunDeviceFd = tunFd;
}

int sendIpPacketBuffer(int fd, unsigned char *buffer, int *bufferUsed) {
    // TODO: Use a circular buffer is much better
    for(;;) {
        if((*bufferUsed) < 4) {
            return 0;
        }
        int firstPacketSize = buffer[2] * 256 + buffer[3];
        if ((*bufferUsed) < firstPacketSize) {
            return 0;
        }
        if(firstPacketSize < 20 || ((buffer[0] & 0xF0) != 0x40))
        {
            /*__android_log_print(ANDROID_LOG_VERBOSE, "backend thread",
                                "Invalid Packet. size = %d "
                                        "first byte = %02x", firstPacketSize, bufferUsed[0]);*/
            return -1;
        }
        // TODO: if the return value is positive while less than firstPacketSize, this won't work.
        int temp;
        if((temp = write(fd, buffer, firstPacketSize)) < firstPacketSize)
        {
            //__android_log_print(ANDROID_LOG_VERBOSE, "backend thread", "fail %d", temp);
            return -1;
        }
        memmove(buffer, buffer + firstPacketSize, (*bufferUsed) - firstPacketSize);
        (*bufferUsed) -= firstPacketSize;
    }
}

void communication_init(int _remoteSocketFd)
{
    remoteSocketFd = _remoteSocketFd;
    //TODO: Free memory on thread exit.
    tunDeviceBuffer = new char[IP_PACKET_MAX_SIZE * 100];
    tunDeviceBufferUsed = 0;
    over6PacketBuffer = new char[IP_PACKET_MAX_SIZE * 100];
    over6PacketBufferUsed = 0;
}

int communication_get_status()
{
    return currentStatus;
}

void communication_set_status(int _currentStatus)
{
    currentStatus = _currentStatus;
}

void communication_handle_tun_read()
{
    // TODO: Add output bytes counter for statistics.
    readAllDataToBuffer(tunDeviceFd, (unsigned char*)tunDeviceBuffer, &tunDeviceBufferUsed);
}

void communication_handle_tun_write()
{
    // TODO: Add input bytes counter for statistics.
    if( over6PacketBufferUsed > 0) {
        sendIpPacketBuffer(tunDeviceFd, (unsigned char*)over6PacketBuffer, &over6PacketBufferUsed);
    }
}

void communication_handle_4over6_socket_read()
{
    // TODO: Need to follow 4over6 specification.
    // TODO: Actual 4over6 packet have different types (IP/DNS info, IP packet, or heartbeat)
    // This assumes that ip packet is sent one by one via tcp stream.
    readAllDataToBuffer(remoteSocketFd, (unsigned char*)over6PacketBuffer,
                        &over6PacketBufferUsed);
    sendIpPacketBuffer(tunDeviceFd, (unsigned char*)over6PacketBuffer, &over6PacketBufferUsed);
}

void communication_handle_4over6_socket_write()
{
    // TODO: Need to follow 4over6 specification.
    if(tunDeviceBufferUsed > 0)
        sendIpPacketBuffer(remoteSocketFd, (unsigned char*)tunDeviceBuffer, &tunDeviceBufferUsed);
}

void communication_handle_timer()
{
    // TODO: Send heartbeat packet
}

int communication_is_ip_confiugration_recevied()
{
    // TODO: Returns whether IPv4 Configuration is already received through network.
}

void communication_get_ip_confiugration()
{
    // TODO: Returns IPv4 Configuration
}

void communication_get_statistics()
{
    // TODO: Returns network statistics
}