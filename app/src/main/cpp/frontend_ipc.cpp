//
// Created by tansinan on 5/8/17.
//

#include "frontend_ipc.h"
#include "communication.h"
#include <unistd.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <android/log.h>

int tunFd = -1;

int handle_frontend_command(int commandPipeFd, int responsePipeFd) {
    unsigned char command;
    int ret = read(commandPipeFd, &command, 1);
    if (ret != 1) return ret;
    if (command == BACKEND_IPC_COMMAND_TERMINATE) {
        return command;
    }
    if (command == BACKEND_IPC_COMMAND_STATUS) {
        uint8_t status = communication_get_status();
        write(responsePipeFd, &status, 1);
    } else if (command == BACKEND_IPC_COMMAND_STATISTICS) {
        uint64_t inBytes, outBytes;
        uint32_t inPacketsSend, outPacketsSend;
        communication_get_statistics(&inBytes, &outBytes);
        inBytes = htonq(inBytes);
        outBytes = htonq(outBytes);
        inPacketsSend = htonl(inPackets);
        outPacketsSend = htonl(outPackets);
        write(responsePipeFd, &inBytes, sizeof(inBytes));
        write(responsePipeFd, &outBytes, sizeof(outBytes));
        write(responsePipeFd, &inPacketsSend, sizeof(inPacketsSend));
        write(responsePipeFd, &outPacketsSend, sizeof(outPacketsSend));
    } else if (command == BACKEND_IPC_COMMAND_CONFIGURATION) {
        if (!communication_is_ip_confiugration_recevied()) {
            communication_get_ip_confiugration();
            // comm_ip ... will be invalid, which means none, just write
        }
        write(responsePipeFd, comm_ip, 4);
        write(responsePipeFd, comm_mask, 4);
        write(responsePipeFd, comm_dns1, 4);
        write(responsePipeFd, comm_dns2, 4);
        write(responsePipeFd, comm_dns3, 4);
    } else if (command == BACKEND_IPC_COMMAND_SET_TUNNEL_FD) {
        for (; ; ) {
            int ret = read(commandPipeFd, &tunFd, 4);
            if (ret > 0) {
                tunFd = ntohl(tunFd);
                __android_log_print(ANDROID_LOG_VERBOSE, "backend thread",
                                    "communication set tun_fd");
                communication_set_tun_fd(tunFd);
                break;
            }
        }
    }
    return command;
}
