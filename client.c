#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <utmp.h>
#include <time.h>

#define MAX 1000

int get_command_size(char command[]){
    int i = 0;
    while(command[i] != '\n')
        i++;
    return i;
}

int main() {
    if(access("server-client.fifo", F_OK) == -1){
        mknod("server-client.fifo", S_IFIFO | 0666, 0);
    }
    if(access("client-server.fifo", F_OK) == -1){
        mknod("client-server.fifo", S_IFIFO | 0666, 0);
    }
    
    int client_server = open("client-server.fifo", O_WRONLY);

    while(1){
        char command[MAX] = "";
        char answer[MAX] = "";
        int server_client = open("server-client.fifo", O_RDONLY);
        printf("Type your command: ");
        fgets(command, sizeof(command), stdin);
        write(client_server, command, get_command_size(command));
        
        read(server_client, answer, MAX);
        int i = MAX;
        while(answer[i] != '\n')
            i--;
        answer[i+1] = '\0'; 

        printf("---> %s", answer);
        answer[0] = '\0';

        if(strncmp(command, "quit", get_command_size(command)) == 0){
            return 0;
        }
    }

    return 0;
}