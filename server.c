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

bool is_command(char command[]){
    if(strncmp(command, "login", 5) == 0)
        return true;
    else if(strcmp(command,"get-logged-users") == 0)
        return true;
    else if(strncmp(command,"get-proc-info", 13) == 0)
        return true;
    else if(strcmp(command,"logout") == 0)
        return true;
    else if(strcmp(command,"quit") == 0)
        return true;
    return false;
}

int get_output_size(char o[]){
    int i = 0;
    while(o[i] != '\0')
        i++;
    return i;
}

char* logged_in(char command[]){
    FILE * users;
    char * line = NULL;
    size_t len = 0;

    users = fopen("users.txt", "r");
    if(users == NULL){
        return "Couldn't open users file\n";
        exit(EXIT_FAILURE);
    }
    
    while(getline(&line, &len, users) != -1){
        int i = 0;
        char name[MAX] = "";
        while(line[i] != '\n')
            name[i++] = line[i];
        name[i] = '\0';
        if(strcmp(name, command+8) == 0){
            return "Logged in:\n";
        }
    }
    return "Failed to log in! Unknown user!\n";
}

char * log_out(){
    return "Logged out!\n";
}

char* get_logged_users(){
    struct utmp *n;
    setutent();
    n = getutent();
    char output[MAX] = "";
    while(n){
        if(n->ut_type == USER_PROCESS){
            time_t t = n->ut_tv.tv_sec;
            strcat(output, "user: ");
            strcat(output, n->ut_user);
            strcat(output, "   host: ");
            strcat(output, n->ut_host);
            strcat(output, "   entry_time: ");
            strcat(output, ctime(&t));
        }
        n = getutent();
    }
    int len = strlen(output);
    output[len] = '\0';

    char *output_p = (char*)malloc(len*sizeof(char));
    memcpy(output_p, output, len);

    return output_p;
}

char* get_proc_info(char pid[]){
    FILE *fp;
    char * line = NULL;
    size_t line_len = 0;
    
    char path[20] = "/proc/";
    strcat(path, pid);
    strcat(path, "/status");

    char proc_info[MAX] = "Process details:\n";
    fp = fopen(path, "r");
    while(getline(&line, &line_len, fp) != -1){
        if(strncmp("Name", line, 4) == 0)
            strcat(proc_info,line);
        else if(strncmp("State", line, 5) == 0)
            strcat(proc_info,line);
        else if(strncmp("PPid", line, 4) == 0)
            strcat(proc_info,line);
        else if(strncmp("Uid", line, 3) == 0)
            strcat(proc_info,line);
        else if(strncmp("VmSize", line, 6) == 0)
            strcat(proc_info,line);
    }

    int output_len = strlen(proc_info);
    char *output_p = (char*)malloc(output_len*sizeof(char));
    memcpy(output_p, proc_info, output_len);

    return output_p;
}

int main() {
    if(access("server-client.fifo", F_OK) == -1){
        mknod("server-client.fifo", S_IFIFO | 0666, 0);
    }
    if(access("client-server.fifo", F_OK) == -1){
        mknod("client-server.fifo", S_IFIFO | 0666, 0);
    }

    int client_server = open("client-server.fifo", O_RDONLY);
    int server_client = open("server-client.fifo", O_WRONLY);

    while(1){
        char buff[MAX];
        int len;
        
        if((len = read(client_server, buff, 100)) < 0){
            write(server_client, "Error while reading command!\n", 29);
            exit(EXIT_FAILURE);
        }
        buff[len] = '\0';

        if(strcmp(buff, "quit") == 0){
            write(server_client, "Server stopped!\n", 16);
            return 0;
        }

        int login_status = 0;
        if(!is_command(buff)){
            write(server_client, "Invalid command.\n", 17);
        }
        else{
            
            int sockets[2];
            if(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == -1){
                write(server_client, "Socketpair error!\n", 18);
                exit(EXIT_FAILURE);
            }

            int login_pipe[2];
            pipe(login_pipe);

            pid_t child = fork();
            char ans[MAX];
            if(child > 0){ // parent
                close(sockets[0]);

                write(sockets[1], buff, len);
                read(sockets[1], ans, strlen(ans));

                close(login_pipe[1]);
                char b[10];
                int l = read(login_pipe[0], b, 100);
                b[l] = '\0';
                if(strcmp(b, "yes") == 0)
                    login_status = 1;
                else
                    login_status = 0;

                close(sockets[1]);
            }
            else { // child
                close(sockets[1]);

                read(sockets[0], buff, len);

                int local_status = login_status;

                if(strncmp(buff, "login :", 7) == 0){
                    if(local_status == 1)
                        write(server_client, "Already logged in!\n", 19);
                    else{
                        write(server_client, logged_in(buff), strlen(logged_in(buff)));
                        local_status = 1;
                    }
                }
                else if(strcmp(buff,"get-logged-users") == 0){
                    write(server_client, get_logged_users(), strlen(get_logged_users()));
                }
                else if(strncmp(buff,"get-proc-info :", 15) == 0){{
                    write(server_client, get_proc_info(buff+16), strlen(get_proc_info(buff+16)));
                }
                }else if(strcmp(buff,"logout") == 0){
                    write(server_client, "Logged out!\n", 12);
                }
                write(sockets[0], ans, strlen(ans));

                close(login_pipe[0]);
                if(local_status == 0)
                    write(login_pipe[1], "no", 2);
                else
                    write(login_pipe[1], "yes", 3);

                close(sockets[0]);
            }
        }
    }

    return 0;
}