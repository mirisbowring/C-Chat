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

#ifdef _WIN32
/*
 * Headerfiles für Windows
 */
#include <winsock.h>
#include <io.h>
#else
/* 
 * Headerfiles für UNIX/Linux
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fileapi.h>
#endif

/* port */
#define PORT 55065
/* incoming message buffer */
#define RCVBUFSIZE 8192

#ifdef _WIN32
static void echo(SOCKET);
#else
static void echo(int);
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
    if ((recv_size = recv(client_socket, echo_buffer, RCVBUFSIZE, 0)) < 0)
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
static void error_exit(char *error_message) {
#ifdef _WIN32
    fprintf(stderr, "%s: %d\n", error_message, WSAGetLastError());
#else
    fprintf(stderr, "%s: %s\n", error_message, strerror(errno));
#endif
    exit(EXIT_FAILURE);
}

/**
 * 
 * @param argc
 * @param argv
 * @return 
 */
int main(int argc, char** argv) {
    struct sockaddr_in server, client;

#ifdef _WIN32
    SOCKET sock, fd;
#else
    int sock, fd;
#endif

    unsigned int len;

#ifdef _WIN32
    WORD wVersionRequested;
    WSADATA wsaData;
    wVersionRequested = MAKEWORD(1, 1);
    if (WSAStartup(wVersionRequested, &wsaData) != 0)
        error_exit("Error while initializing Winsock!");
    else
        printf("Winsock initialized successfull");
#endif

    //    /* Primitive check whether all CLI arguments have been passed. */
    //    if (argc < 3)
    //        error_exit("usage: client server-ip echo_word\n");

    /* initializing socket */
    sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0)
        error_exit("Error while initializing socket!");
    /* declare socket-address of server */
    memset(&server, 0, sizeof (server));
    /* IPv4 connectivity */
    server.sin_family = AF_INET;
    /*accept all IP adrresses*/
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    /* port */
    server.sin_port = htons(PORT);

    /* bind socket to ip:port */
    if (bind(sock, (struct sockaddr*) &server, sizeof (server)) < 0)
        error_exit("Cannot bind socket!");
    /* allow incoming connections on socket port */
    if (listen(sock, 5) == -1)
        error_exit("Error while listen!");

    printf("Server ready - waiting for connections...\n");
    /* Processing connection requests of clients in an endless loop.
     * accept() blocks until a client connects. */
    for (;;) {
        len = sizeof (client);
        fd = accept(sock, (struct sockaddr*) &client, &len);
        if (fd < 0)
            error_exit("Error while accepting");
        printf("handling client %s\n", inet_ntoa(client.sin_addr));
        /* print client data */
        echo(fd);
        /* closing connection */
#ifdef _WIN32
        closesocket(fd);
#else
        close(fd);
#endif
    }
    return EXIT_SUCCESS;
}

