//
// Created by tansinan on 5/4/17.
//

#include <unistd.h>

int backend_main(int tunDeviceFd, int commandPipeFd, int responsePipeFd)
{
    char a;
    char *s = "Hello world@";
    int s_len = strlen(s) + 1;
    for(;;) {
        int temp = read(commandPipeFd, &a, 1);
        if(temp == 1)
            write(responsePipeFd, s, s_len);
    }
}