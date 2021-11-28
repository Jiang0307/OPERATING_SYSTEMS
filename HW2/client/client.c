#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <netdb.h>
#include <ctype.h>
#include <stdatomic.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <limits.h>
#include "types.h"
#include "sock.h"

#define SIZE 1024

int main(int argc, char **argv)
{
    int opt;
    char *server_host_name = NULL, *server_port = NULL;
    /* Parsing args */
    while ((opt = getopt(argc, argv, "h:p:")) != -1)
    {
        switch (opt)
        {
        case 'h':
            server_host_name = malloc(strlen(optarg) + 1);
            strncpy(server_host_name, optarg, strlen(optarg));
            break;
        case 'p':
            server_port = malloc(strlen(optarg) + 1);
            strncpy(server_port, optarg, strlen(optarg));
            break;
        case '?':
            fprintf(stderr, "Unknown option \"-%c\"\n", isprint(optopt) ? optopt : '#');
            return 0;
        }
    }
    if (!server_host_name)
    {
        fprintf(stderr, "Error!, No host name provided!\n");
        exit(1);
    }
    if (!server_port)
    {
        fprintf(stderr, "Error!, No port number provided!\n");
        exit(1);
    }
    /* Open a client socket fd */
    int clientfd __attribute__((unused)) = open_clientfd(server_host_name, server_port);
    // Start your coding client code here!
    char *input_line = malloc(sizeof(char) * SIZE);
    char command[SIZE],key[SIZE],value[SIZE],return_from_server[SIZE],check_input_format[SIZE],send_to_server[SIZE];
    int status=1;

    while(status == 1)
    {
        memset(input_line,0,SIZE);
        memset(command,0,SIZE);
        memset(key,0,SIZE);
        memset(value,0,SIZE);
        memset(return_from_server,0,SIZE);
        memset(check_input_format,0,SIZE);
        memset(send_to_server,0,SIZE);

        printf(">");
        if( fgets(input_line,SIZE,stdin) == NULL )
            status = 0;

        send(clientfd,input_line, SIZE,0);
        recv(clientfd,return_from_server,SIZE,0);
        printf("%s",return_from_server);
        free(server_port);
        free(server_host_name);
    }
    return 0;
}