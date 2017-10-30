/* 
 * File:   main.c
 * Author: Arthur Ferdinand Lindner <arthur.lindner@outlook.de>
 *
 * Created on 27. Oktober 2017, 22:03
 */
/* Default imports */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>

#ifdef _WIN32
/* Headerfiles für Windows */
    #include <winsock2.h>
    #include <io.h>
#else
/* Headerfiles für UNIX/Linux */
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netdb.h>
    #include <arpa/inet.h>
    #include <unistd.h>
#endif

/* port */
#define PORT 55065
/* incoming message buffer */
#define RCVBUFSIZE 8192
/* max length of message */
#define MSG_LEN 1024
/* max username length */
#define USR_LEN 25

struct read_write_arg{
#ifdef _WIN32
    SOCKET connfd;
#else
    int connfd;
#endif
    char cli[MSG_LEN];
};

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
    /* closing connection and socket. */
#ifdef _WIN32
    closesocket(sock);
    WSACleanup(); /* Cleanup Winsock */
#else
    close(sock);
#endif
    exit(EXIT_FAILURE);
}

/**
 * This function replaces the brackline with a null termination.
 * 
 * @param str char array that has to be tranformed
 */
void terminate_breakline(char str[]){
    if(str[strlen(str)-1]=='\n')
        str[strlen(str)-1] = '\0';
}

/**
 * This function checks whether a String starts with a specific subString.
 * 
 * @param pre The String that should be included
 * @param str The whole String
 * @return 
 */
bool startsWith(const char *pre, const char *str){
    size_t lenpre = strlen(pre),
           lenstr = strlen(str);
    return lenstr < lenpre ? false : strncmp(pre, str, lenpre) == 0;
}

/**
 * This function checks whether a commandline command has been passed.
 * 
 * @param str
 */
void check_command(char str[]){
    if(startsWith("/quit", str)||startsWith("/QUIT", str)){
        close(sock);
        exit(0);
    }
    terminate_breakline(str);
}

/**
 * This function sends a command to the Server to rename the current user.
 * 
 * @param user
 */
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
 * This is a thread function that handles all read tasks.
 * 
 * @param arg the clients read_write_arg struct
 * @return 
 */
void *handle_read(void *arg){
    struct read_write_arg *rwa = arg;
    int rlen;
    char buff_in[1024];
    while((rlen = read(rwa->connfd, buff_in, sizeof(buff_in)-1))>0){
        buff_in[rlen] = '\0';
        printf("%c[2K\r", 27); /*cleares the current line and resets the cursor*/
        printf("%s", buff_in);
    }
    if(rlen>0)
        error_exit("Could not connect to server for receiving messages!");
}

/**
 * This is a thread function that handles all write tasks.
 * 
 * @param arg the clients read_write_arg struct
 * @return 
 */
void *handle_write(void *arg){
    struct read_write_arg *rwa = arg;
    char c;
    int i;
    while(1){
        i = 0;
        while((c = getchar())!='\n'){
            if(i<MSG_LEN)
                rwa->cli[i] = c;
            i++;
        }
        if(i>MSG_LEN)
            printf("%c[2k\rToo many characters!\nSending: %s", 27, rwa->cli);
        check_command(rwa->cli);
        write(rwa->connfd, rwa->cli, strlen(rwa->cli));
        /* NULLs the string -> User is not typing */
        memset(rwa->cli,0,sizeof(rwa->cli));
    }
}

/**
 * The main function
 * 
 * @param argc
 * @param argv
 * @return 
 */
int main(int argc, char *argv[]){
    struct sockaddr_in server;
    struct hostent *host_info;
    unsigned long addr;

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
    /* reading the IP Address*/
    char ip_addr[15];
    printf("Please enter the IP Address you want to connect to: ");
    fgets(ip_addr, sizeof(ip_addr), stdin);

    /* initializes socket */
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock<0)
        error_exit("Error while initializing socket!");

    /* Creating socketaddress of server. */
    memset(&server, 0, sizeof(server));
    if((addr = inet_addr(ip_addr))!=INADDR_NONE){
        /* ip_addr is a numeric IP */
        memcpy((char *) &server.sin_addr, &addr, sizeof(addr));
    }else{
        host_info = gethostbyname(ip_addr); /* converting char hosts to ip*/
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

    struct read_write_arg rwa;
    rwa.connfd = sock;
    /* NULLs the User Input Chache */
    memset(rwa.cli,0,sizeof(rwa.cli));

    pthread_t read;
    pthread_t write;

    /* welcome user */
    char user[USR_LEN];
    printf("enter your username: ");
    fgets(user, USR_LEN, stdin);
    remote_rename(user);
    printf("Welcome %s!\nThe max message length is %d characters.\nYou can "
            "exit the chat with </quit>.\n\n", user, MSG_LEN);

    /* starting threads */
    pthread_create(&read, NULL, &handle_read, (void*) &rwa);
    pthread_create(&write, NULL, &handle_write, (void*) &rwa);

    /* keeping the main alive */
    while(1){
        sleep(1);
    }

    return EXIT_SUCCESS;
}
