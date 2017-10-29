/* 
 * File:   main.c
 * Author: Arthur Ferdinand Lindner <arthur.lindner@outlook.de>
 *
 * Created on 27. Oktober 2017, 20:43
 */
/*
 * Default imports
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <pthread.h>

#ifdef _WIN32
/*
 * Headerfiles für Windows
 */
#    include <winsock2.h>
#    include <io.h>
#else
/* 
 * Headerfiles für UNIX/Linux
 */
#    include <sys/types.h>
#    include <sys/socket.h>
#    include <netinet/in.h>
#    include <netdb.h>
#    include <arpa/inet.h>
#    include <unistd.h>
#    include <fileapi.h>
#endif

#define PORT 55065/* port */
#define RCVBUFSIZE 8192/* incoming message buffer */
#define MAX_CLIENTS 100/* max amount of clients */
#define MSG_LEN 200/* max message length */
#define USR_LEN 25/* max user length */

typedef struct{
    struct sockaddr_in addr; /* Client remote address */
    int connfd; /* Connection file descriptor */
    int uid; /* Client unique identifier */
    char name[25]; /* Client name */
} client_t;

client_t *clients[MAX_CLIENTS];
static unsigned int cli_count = 0;
static int uid = 10;

#ifdef _WIN32
static void echo(SOCKET);
#else
static void echo(int);
#endif

#ifdef _WIN32
SOCKET server_sock, client_sock;
#else
int server_sock, client_sock;
#endif

static void error_exit(char *errorMessage);

/**
 * prints data of client vie stdout to cli
 * 
 * @param client_socket the clients socket id
 */
#ifdef _WIN32
static void echo(SOCKET client_socket)
#else

static void echo(int client_socket)
#endif
{
    char echo_buffer[RCVBUFSIZE];
    int recv_size;
    time_t _time;
    if((recv_size = recv(client_socket, echo_buffer, RCVBUFSIZE, 0))<0)
        error_exit("Error at recv()");
    echo_buffer[recv_size] = '\0';
    time(&_time);
    printf("Messages from client: %s \t%s",
            echo_buffer, ctime(&_time));
}

/**
 * Prints the error message and  exits the program.
 * 
 * @param errorMessage pointer to the error message
 */
static void error_exit(char *error_message){
#ifdef _WIN32
    fprintf(stderr, "%s: %d\n", error_message, WSAGetLastError());
#else
    fprintf(stderr, "%s: %s\n", error_message, strerror(errno));
#endif
    exit(EXIT_FAILURE);
}

/**
 * This function adds the Client to the queue
 * 
 * @param cl (client_t) the client that has to be added to the queue.
 */
void queue_add(client_t *cl){
    int i;
    for(i = 0; i<MAX_CLIENTS; i++){
        if(!clients[i]){
            clients[i] = cl;
            return;
        }
    }
}

/**
 * This function delets a client from the queue.
 * 
 * @param uid the id of the client.
 */
void queue_delete(int uid){
    int i;
    for(i = 0; i<MAX_CLIENTS; i++){
        if(clients[i]){
            if(clients[i]->uid==uid){
                clients[i] = NULL;
                return;
            }
        }
    }
}

/**
 * This function sends a message to all clients except the sender.
 * @param s the message
 * @param uid the clients id
 */
void send_message(char *s, int uid){
    int i;
    for(i = 0; i<MAX_CLIENTS; i++){
        if(clients[i]){
            if(clients[i]->uid!=uid){
                write(clients[i]->connfd, s, strlen(s));
            }
        }
    }
}

/**
 * This funtion sends a message to all clients.
 * 
 * @param s the message
 */
void send_message_all(char *s){
    int i;
    for(i = 0; i<MAX_CLIENTS; i++){
        if(clients[i]){
            write(clients[i]->connfd, s, strlen(s));
        }
    }
}

/**
 * This function sends a message to a specific client
 * 
 * @param s
 * @param uid
 */
void send_message_client(char *s, int uid){
    int i;
    for(i = 0; i<MAX_CLIENTS; i++){
        if(clients[i]){
            if(clients[i]->uid==uid){
                write(clients[i]->connfd, s, strlen(s));
            }
        }
    }
}

/**
 * This function sends a message to the sender.
 * 
 * @param s
 * @param connfd
 */
void send_message_self(const char *s, int connfd){
    write(connfd, s, strlen(s));
}

/**
 * This function sends a list of active clients.
 * 
 * @param connfd
 */
void send_active_clients(int connfd){
    int i;
    char s[64];
    for(i = 0; i<MAX_CLIENTS; i++){
        if(clients[i]){
            sprintf(s, "<<CLIENT %d | %s\r\n", clients[i]->uid, clients[i]->name);
            send_message_self(s, connfd);
        }
    }
}

/**
 * 
 * @param s
 */
void strip_newline(char *s){
    while(*s!='\0'){
        if(*s=='\r'|| *s=='\n'){
            *s = '\0';
        }
        s++;
    }
}

/**
 * This function prints the IP Address of the Socket
 * 
 * @param addr the Socket that contains the IP Address
 */
void print_client_addr(struct sockaddr_in addr){
    printf("%d.%d.%d.%d",
            addr.sin_addr.s_addr&0xFF,
            (addr.sin_addr.s_addr&0xFF00)>>8,
            (addr.sin_addr.s_addr&0xFF0000)>>16,
            (addr.sin_addr.s_addr&0xFF000000)>>24);
}

void exitClient(int fd, fd_set *readfds, char fd_array[], int *num_clients){
    int i;

    close(fd);
    FD_CLR(fd, readfds); //clear the leaving client from the set
    for(i = 0; i<(*num_clients)-1; i++)
        if(fd_array[i]==fd)
            break;
    for(; i<(*num_clients)-1; i++)
        (fd_array[i]) = (fd_array[i+1]);
    (*num_clients)--;
}

/**
 * This function handles all communication of the client
 * 
 * @param arg
 * @return 
 */
void *handle_client(void *arg){
    char buff_out[1024];
    char buff_in[1024];
    int rlen;

    cli_count++;
    client_t *cli = (client_t *) arg;

    printf("<<ACCEPT ");
    print_client_addr(cli->addr);
    printf(" REFERENCED BY %d\n", cli->uid);

    sprintf(buff_out, "%s joined the Chatroom\r\n", cli->name);
    send_message_all(buff_out);

    /* Receive input from client */
    while((rlen = read(cli->connfd, buff_in, sizeof(buff_in)-1))>0){
        buff_in[rlen] = '\0';
        buff_out[0] = '\0';
        strip_newline(buff_in);

        /* Ignore empty buffer */
        if(!strlen(buff_in)){
            continue;
        }

        /* Special options */
        if(buff_in[0]=='\\'){
            char *command, *param;
            command = strtok(buff_in, " ");
            if(!strcmp(command, "\\QUIT")){
                break;
            }else if(!strcmp(command, "\\PING")){
                send_message_self("<<PONG\r\n", cli->connfd);
            }else if(!strcmp(command, "\\NAME")){
                param = strtok(NULL, " ");
                if(param){
                    char *old_name = strdup(cli->name);
                    strcpy(cli->name, param);
                    sprintf(buff_out, "<<RENAME, %s TO %s\r\n", old_name, cli->name);
                    free(old_name);
                    printf("<<RENAME, %s TO %s\r\n", old_name, cli->name);
//                    send_message_all(buff_out);
                }else{
                    send_message_self("<<NAME CANNOT BE NULL\r\n", cli->connfd);
                }
            }else if(!strcmp(command, "\\PRIVATE")){
                param = strtok(NULL, " ");
                if(param){
                    int uid = atoi(param);
                    param = strtok(NULL, " ");
                    if(param){
                        sprintf(buff_out, "[PM][%s]", cli->name);
                        while(param!=NULL){
                            strcat(buff_out, " ");
                            strcat(buff_out, param);
                            param = strtok(NULL, " ");
                        }
                        strcat(buff_out, "\r\n");
                        send_message_client(buff_out, uid);
                    }else{
                        send_message_self("<<MESSAGE CANNOT BE NULL\r\n", cli->connfd);
                    }
                }else{
                    send_message_self("<<REFERENCE CANNOT BE NULL\r\n", cli->connfd);
                }
            }else if(!strcmp(command, "\\ACTIVE")){
                sprintf(buff_out, "<<CLIENTS %d\r\n", cli_count);
                send_message_self(buff_out, cli->connfd);
                send_active_clients(cli->connfd);
            }else if(!strcmp(command, "\\HELP")){
                strcat(buff_out, "\\QUIT     Quit chatroom\r\n");
                strcat(buff_out, "\\PING     Server test\r\n");
                strcat(buff_out, "\\NAME     <name> Change nickname\r\n");
                strcat(buff_out, "\\PRIVATE  <reference> <message> Send private message\r\n");
                strcat(buff_out, "\\ACTIVE   Show active clients\r\n");
                strcat(buff_out, "\\HELP     Show help\r\n");
                send_message_self(buff_out, cli->connfd);
            }else{
                send_message_self("<<UNKOWN COMMAND\r\n", cli->connfd);
            }
        }else{
            /* Send message */
            sprintf(buff_out, "%s: %s\r\n", cli->name, buff_in);
            send_message(buff_out, cli->uid);
        }
    }

    /* Close connection */
    close(cli->connfd);
    sprintf(buff_out, "<<LEAVE, BYE %s\r\n", cli->name);
    send_message_all(buff_out);

    /* Delete client from queue and yeild thread */
    queue_delete(cli->uid);
    printf("<<LEAVE ");
    print_client_addr(cli->addr);
    printf(" REFERENCED BY %d\n", cli->uid);
    free(cli);
    cli_count--;
    pthread_detach(pthread_self());

    return NULL;
}

/**
 * 
 * @param argc
 * @param argv
 * @return 
 */
int main(int argc, char** argv){
    int listenfd = 0, connfd = 0;
    struct sockaddr_in serv_addr;
    struct sockaddr_in cli_addr;
    pthread_t tid;

    /* Socket settings */
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(PORT);

    /* Bind */
    if(bind(listenfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr))<0){
        perror("Socket binding failed");
        return 1;
    }

    /* Listen */
    if(listen(listenfd, 10)<0){
        perror("Socket listening failed");
        return 1;
    }

    printf("Server started!\n");

    /* Accept clients */
    while(1){
        socklen_t clilen = sizeof(cli_addr);
        connfd = accept(listenfd, (struct sockaddr*) &cli_addr, &clilen);

        /* Check if max clients is reached */
        if((cli_count+1)==MAX_CLIENTS){
            printf("Too many clients!\n");
            printf("Rejecting connection of ");
            print_client_addr(cli_addr);
            printf("!\n");
            close(connfd);
            continue;
        }

        /* Client settings */
        client_t *cli = (client_t *) malloc(sizeof(client_t));
        cli->addr = cli_addr;
        cli->connfd = connfd;
        cli->uid = uid++;
        sprintf(cli->name, "%d", cli->uid);

        /* Add client to the queue and fork thread */
        queue_add(cli);
        pthread_create(&tid, NULL, &handle_client, (void*) cli);

        /* Reduce CPU usage */
        sleep(1);
    }
}