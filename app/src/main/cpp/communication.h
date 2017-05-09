//
// Created by tansinan on 5/8/17.
//

#ifndef DROIDOVER6_COMMUNICATION_H_H
#define DROIDOVER6_COMMUNICATION_H_H

#include <stdint.h>

const static uint8_t BACKEND_STATE_CONNECTING = 0;
const static uint8_t BACKEND_STATE_WAITING_FOR_IP_CONFIGURATION = 1;
const static uint8_t BACKEND_STATE_CONNECTED = 2;
const static uint8_t BACKEND_STATE_DISCONNECTED = 3;

extern int tunDeviceBufferUsed;
extern int over6PacketBufferUsed;

void communication_init(int remoteSocketFd);

int communication_get_status();

void communication_set_status(int _currentStatus);

void communication_set_tun_fd(int tunFd);

void communication_handle_tun_read();

void communication_handle_tun_write();

void communication_handle_4over6_socket_read();

void communication_handle_4over6_socket_write();

void communication_handle_timer();

int communication_is_ip_confiugration_recevied();

void communication_get_ip_confiugration();

void communication_get_statistics();

#endif //DROIDOVER6_COMMUNICATION_H_H
