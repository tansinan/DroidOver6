//
// Created by tansinan on 5/8/17.
//

extern int tunFd;
const static unsigned char BACKEND_IPC_COMMAND_STATUS = 0x00;
const static unsigned char BACKEND_IPC_COMMAND_STATISTICS = 0x01;
const static unsigned char BACKEND_IPC_COMMAND_CONFIGURATION = 0x02;
const static unsigned char BACKEND_IPC_COMMAND_SET_TUNNEL_FD = 0x03;
const static unsigned char BACKEND_IPC_COMMAND_TERMINATE = 0xFF;
int handle_frontend_command(int commandPipeFd, int responsePipeFd);