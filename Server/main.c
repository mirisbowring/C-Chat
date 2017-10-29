/* 
 * File:   main.c
 * Author: Arthur Ferdinand Lindner <arthur.lindner@outlook.de>
 *
 * Created on 27. Oktober 2017, 20:43
 */
/* Default imports */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <pthread.h>

#ifdef _WIN32
/* Headerfiles für Windows */
    #include <winsock2.h>
    #include <io.h>
#else
/* Headerfiles für UNIX/Linux */
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netdb.h>
    #include <arpa/inet.h>
    #include <unistd.h>
#endif

#define PORT 55065/* port */
#define RCVBUFSIZE 8192/* incoming message buffer */
#define MAX_CLIENTS 100/* max amount of clients */
#define MSG_LEN 1024/* max message length */
#define USR_LEN 25 /* max user length */

typedef struct{
    struct sockaddr_in addr; /* Client remote address */
    int connfd; /* Connection file descriptor (Sock) */
    int uid; /* Client unique identifier */
    char name[USR_LEN]; /* Client name */
} client_t;

client_t *clients[MAX_CLIENTS];
static unsigned int cli_count = 0;
static int uid = 10;

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
 * This function delets the client from the queue.
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
 * This function sends the message to all clients but the sender.
 * 
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
 * This funtion sends the message to all clients.
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

/**
 * This function handles all communication of the client
 * 
 * @param arg
 * @return 
 */
void *handle_client(void *arg){
    char buff_out[MSG_LEN];
    char buff_in[MSG_LEN];
    int rlen;

    cli_count++;
    client_t *cli = (client_t *) arg;

    printf("ACCEPTED CONNECTION FROM ");
    print_client_addr(cli->addr);
    printf(" REFERENCED BY %d\n", cli->uid);

    sprintf(buff_out, "%s joined the Chatroom\r\n", cli->name);
    send_message(buff_out, cli->uid);

    /* receive input from client */
    while((rlen = read(cli->connfd, buff_in, sizeof(buff_in)-1))>0){
        buff_in[rlen] = '\0';
        buff_out[0] = '\0';
        strip_newline(buff_in);

        /* ignore empty buffer */
        if(!strlen(buff_in)){
            continue;
        }

        /* check if rename command was called */
        if(buff_in[0]=='\\'){
            char *command, *param;
            command = strtok(buff_in, " ");
            if(!strcmp(command, "\\NAME")){
                param = strtok(NULL, " ");
                if(param){
                    char *old_name = strdup(cli->name);
                    strcpy(cli->name, param);
                    printf("<[Renamed %s -> %s]>\r\n", old_name, cli->name);
                    free(old_name);
                }
            }
        }else{
            /* send message */
            sprintf(buff_out, "%s: %s\r\n", cli->name, buff_in);
            send_message(buff_out, cli->uid);
        }
    }

    /* close connection */
    close(cli->connfd);
    sprintf(buff_out, "%s left the chatroom\r\n", cli->name);
    send_message_all(buff_out);

    /* delete client from queue and yeild thread */
    queue_delete(cli->uid);
    print_client_addr(cli->addr);
    printf(" REFERENCED BY %d LEFT THE CHATROOM\n", cli->uid);
    free(cli);
    cli_count--;
    pthread_detach(pthread_self());

    return NULL;
}

/**
 * The main function
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

    /* socket settings */
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(PORT);

    /* bind */
    if(bind(listenfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr))<0){
        perror("Socket binding failed");
        return 1;
    }

    /* listen */
    if(listen(listenfd, 10)<0){
        perror("Socket listening failed");
        return 1;
    }

    printf("Server started!\n");

    /* accept clients */
    while(1){
        socklen_t clilen = sizeof(cli_addr);
        connfd = accept(listenfd, (struct sockaddr*) &cli_addr, &clilen);

        /* check if max clients are reached */
        if((cli_count+1)==MAX_CLIENTS){
            printf("Too many clients!\n");
            printf("Rejecting connection of ");
            print_client_addr(cli_addr);
            printf("!\n");
            close(connfd);
            continue;
        }

        /* client settings */
        client_t *cli = (client_t *) malloc(sizeof(client_t));
        cli->addr = cli_addr;
        cli->connfd = connfd;
        cli->uid = uid++;
        sprintf(cli->name, "%d", cli->uid);

        /* add client to the queue and fork thread */
        queue_add(cli);
        pthread_create(&tid, NULL, &handle_client, (void*) cli);

        /* reduce CPU usage */
        sleep(1);
    }
}