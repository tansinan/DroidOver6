//
// Created by tansinan on 5/6/17.
//

#ifndef DROIDOVER6_BACKEND_MAIN_H
#define DROIDOVER6_BACKEND_MAIN_H

// in ms
#define TIMER_EXP   (1000)

// in s
#define HEART_INTV      (20)
#define HEART_TIMEOUT   (60)

int backend_main(const char* hostName, int port,
                 int commandPipeFd, int responsePipeFd);

#endif //DROIDOVER6_BACKEND_MAIN_H
