//
// Created by tansinan on 5/8/17.
//

#ifndef DROIDOVER6_COMMUNICATION_H_H
#define DROIDOVER6_COMMUNICATION_H_H

#include <stdint.h>
#include <arpa/inet.h>

const static uint8_t BACKEND_STATE_CONNECTING = 0;
const static uint8_t BACKEND_STATE_WAITING_FOR_IP_CONFIGURATION = 1;
const static uint8_t BACKEND_STATE_CONNECTED = 2;
const static uint8_t BACKEND_STATE_DISCONNECTED = 3;

const static int IP_PACKET_MAX_SIZE = 65536;
const static int PIPE_BUF_LEN = 65536 * 100;
const static int SERVER_PORT = 13872;
const static int BUFFER_LENGTH = 250;

const static uint8_t TYPE_IP_REQUEST = 100;
const static uint8_t TYPE_IP_REPLY   = 101;
const static uint8_t TYPE_REQUEST = 102;
const static uint8_t TYPE_REPLY   = 103;
const static uint8_t TYPE_HEART   = 104;

extern uint32_t inPackets;
extern uint32_t outPackets;
extern int tunDeviceBufferRemain;

typedef struct {
    uint32_t length;
    uint8_t type;
    uint8_t data[];
} __attribute__((packed)) over6Packet;

extern int tunDeviceBufferUsed;
extern int over6PacketBufferUsed;

extern uint8_t comm_ip[4];
extern uint8_t comm_mask[4];
extern uint8_t comm_dns1[4];
extern uint8_t comm_dns2[4];
extern uint8_t comm_dns3[4];

void communication_init(int remoteSocketFd);

int communication_get_status();

void communication_set_status(int _currentStatus);

void communication_set_tun_fd(int tunFd);

void communication_handle_tun_read();

void communication_handle_4over6_packets(bool *heartbeated);

void communication_handle_4over6_socket_read();

void communication_handle_4over6_socket_write();

void communication_send_heartbeat();

int communication_is_ip_confiugration_recevied();

void communication_get_ip_confiugration();

void communication_get_statistics(uint64_t *inBytes, uint64_t *outBytes);

#endif //DROIDOVER6_COMMUNICATION_H_H
