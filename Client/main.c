/* 
 * File:   main.c
 * Author: Arthur Ferdinand Lindner <arthur.lindner@outlook.de>
 *
 * Created on 27. Oktober 2017, 22:03
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

/* port */
#define PORT 55065
/* incoming message buffer */
#define RCVBUFSIZE 8192
/* max length of message */
#define MSG_LEN 200
/* max username length */
#define USR_LEN 25

/* seprator (seperates user from msg) */
//static const char command_indicator[2] = "\\";

#ifdef _WIN32
    SOCKET sock;
#else
    int sock;
#endif

/**
 * Prints the error message and  exits the program.
 * 
 * @param errorMessage pointer to the error message
 */
static void error_exit(char *errorMessage){
#ifdef _WIN32
    fprintf(stderr, "%s: %d\n", errorMessage, WSAGetLastError());
#else
    fprintf(stderr, "%s: %s\n", errorMessage, strerror(errno));
#endif
    exit(EXIT_FAILURE);
}

void terminate_breakline(char str[]){
    if(str[strlen(str)-1]=='\n')
        str[strlen(str)-1] = '\0';
}

void check_command(char str[], char msg[]){
    if(strcmp(str, "/quit\n")==0){
        sprintf(msg, "XClient is shutting down.\n");
        write(sock, msg, strlen(msg));
        close(sock);
        exit(0);
    }
    terminate_breakline(msg);
}

void remote_rename(char user[]){
    terminate_breakline(user);
    char rename_command[] = "\\NAME ";
    char *tmp = malloc((USR_LEN+(strlen(rename_command)))*sizeof(char));
    strcpy(tmp, rename_command);
    strcat(tmp, user);
    printf("%s\n", tmp);
    write(sock, tmp, strlen(tmp));
    free(tmp);
}

/**
 * 
 * @param argc
 * @param argv
 * @return 
 */
int main(int argc, char *argv[]){
    struct sockaddr_in server;
    struct hostent *host_info;
    unsigned long addr;

    char *echo_string;
    int echo_len;

#ifdef _WIN32
    /* initializing tcp for windows */
    WORD wVersionRequested;
    WSADATA wsaData;
    wVersionRequested = MAKEWORD(1, 1);
    if(WSAStartup(wVersionRequested, &wsaData)!=0)
        error_exit("Fehler beim Initialisieren von Winsock");
    else
        printf("Winsock initialisiert\n");
#endif

    /* Primitive check whether all CLI arguments have been passed. */
    if(argc<3)
        error_exit("usage: client server-ip echo_word\n");

    /* initializes socket */
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock<0)
        error_exit("Error while initializing socket!");

    /* Creating socketaddress of server. 
     * TYP, IP, PORT */
    memset(&server, 0, sizeof(server));
    if((addr = inet_addr(argv[1]))!=INADDR_NONE){
        /* argv[1] is a numeric IP */
        memcpy((char *) &server.sin_addr, &addr, sizeof(addr));
    }else{
        host_info = gethostbyname(argv[1]); /* converting char hosts to ip*/
        if(NULL==host_info)
            error_exit("invalid server");
        memcpy((char *) &server.sin_addr,
                host_info->h_addr, host_info->h_length); /* Server-IP */
    }
    server.sin_family = AF_INET; /* IPv4 connection */
    server.sin_port = htons(PORT); /* port */

    /* connect to server */
    if(connect(sock, (struct sockaddr*) &server, sizeof(server))<0)
        error_exit("Cannot connect to server!");

    int fd;
    fd_set testfds, clientfds;
    FD_ZERO(&clientfds);
    FD_SET(sock, &clientfds);
    FD_SET(0, &clientfds);
    int result;
    char msg[MSG_LEN+1];
    char msg_kb[MSG_LEN+10];
    

    char user[USR_LEN];
    printf("enter your username: ");
    fgets(user, USR_LEN, stdin);
    remote_rename(user);    
    printf("Welcome %s!\nThe max message length is %d characters.\n\n",
            user, MSG_LEN);    
    while(1){
        testfds = clientfds;
        select(FD_SETSIZE, &testfds, NULL, NULL, NULL);
        for(fd=0;fd<FD_SETSIZE;fd++){
            if(FD_ISSET(fd,&testfds)){
                if(fd==sock){
                    result = read(sock, msg, MSG_LEN);
                    msg[result] = '\0';
                    printf("%s\n", msg +1);
                    if(msg[0]=='X'){
                        close(sock);
                        exit(0);
                    }
                }
            }else if(fd == 0){
//                if()
                fgets(msg_kb, MSG_LEN+1, stdin);                
                check_command(msg_kb, msg);                
                write(sock, msg_kb, strlen(msg_kb));
                /* send terminated string to server */
//                if(send(sock, transfer_msg, (MSG_LEN+USR_LEN), 0)!=(MSG_LEN+USR_LEN))
//                    error_exit("send() send a unexpected amount of bytes!");               

            }
        }
//        char msg[MSG_LEN+1];
        
        sleep(1);
    }


    /* closing connection and socket. */
#ifdef _WIN32
    closesocket(sock);
    /* Cleanup Winsock */
    WSACleanup();
#else
    close(sock);
#endif

    return EXIT_SUCCESS;
}