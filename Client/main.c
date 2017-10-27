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
#endif

/* port */
#define PORT 1234
/* incoming message buffer */
#define RCVBUFSIZE 8192

/**
 * Prints the error message and  exits the program.
 * 
 * @param errorMessage pointer to the error message
 */
static void error_exit(char *errorMessage) {
#ifdef _WIN32
    fprintf(stderr, "%s: %d\n", errorMessage, WSAGetLastError());
#else
    fprintf(stderr, "%s: %s\n", errorMessage, strerror(errno));
#endif
    exit(EXIT_FAILURE);
}

/**
 * 
 * @param argc
 * @param argv
 * @return 
 */
int main(int argc, char *argv[]) {
    struct sockaddr_in server;
    struct hostent *host_info;
    unsigned long addr;

#ifdef _WIN32
    SOCKET sock;
#else
    int sock;
#endif

    char *echo_string;
    int echo_len;

#ifdef _WIN32
    /* initializing tcp for windows */
    WORD wVersionRequested;
    WSADATA wsaData;
    wVersionRequested = MAKEWORD(1, 1);
    if (WSAStartup(wVersionRequested, &wsaData) != 0)
        error_exit("Fehler beim Initialisieren von Winsock");
    else
        printf("Winsock initialisiert\n");
#endif

    /* Primitive check whether all CLI arguments have been passed. */
    if (argc < 3)
        error_exit("usage: client server-ip echo_word\n");

    /* initializes socket */
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
        error_exit("Error while initializing socket!");

    /* Creating socketaddress of server. 
     * TYP, IP, PORT */
    memset(&server, 0, sizeof (server));
    if ((addr = inet_addr(argv[1])) != INADDR_NONE) {
        /* argv[1] is a numeric IP */
        memcpy((char *) &server.sin_addr, &addr, sizeof (addr));
    } else {
        /* converting char hosts to ip*/
        host_info = gethostbyname(argv[1]);
        if (NULL == host_info)
            error_exit("invalid server");
        /* Server-IP */
        memcpy((char *) &server.sin_addr,
                host_info->h_addr, host_info->h_length);
    }
    /* IPv4 connection */
    server.sin_family = AF_INET;
    /* port */
    server.sin_port = htons(PORT);

    /* connect to server */
    if (connect(sock, (struct sockaddr*) &server, sizeof (server)) < 0)
        error_exit("Cannot connect to server!");

    /* Using second argument for echo on the server. */
    echo_string = argv[2];
    /* length of input */
    echo_len = strlen(echo_string);

    /* send terminated string to server */
    if (send(sock, echo_string, echo_len, 0) != echo_len)
        error_exit("send() send a unexpected amount of bytes!");


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