/* Written by Donghao Zhao */
/* Nov 28th, 2021 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>             //memset
#include <netdb.h>
#include <sys/types.h>          //socket
#include <sys/socket.h>	        //socket
#include <netinet/in.h>         //sockaddr_in
#include <arpa/inet.h>          //inet_addr

#define CENTRAL_IP                  "127.0.0.1"
#define PORT_CENTRAL_TCP_A          "25890"     // the port clientA will be connecting to
#define MAX_CHAR_ARRAY_LENGTH       204800      // length of array for data flying in the network
#define MAX_STRING_POINTER_LENGTH   200         // length of array for string pointer to be processed
#define BACKLOG                     10          // number of pending connections queue server will hold

/* the following function is from Beej's code */
// to get address of socket
void *get_in_addr(struct sockaddr *sa)
{
    if (sa -> sa_family == AF_INET)
    {
        return & ( ( (struct sockaddr_in*) sa) -> sin_addr);  //IPv4
    }
    return & ( ( (struct sockaddr_in6*) sa) -> sin6_addr);    //IPv6
}

/* the following function is revised from "man" */
// split string and put them into the array
int split_string(char *destination_array[], char *long_string)
{
    char *p = strtok (long_string, " ");    // split input string by space
    int i = 0;
    while (p != NULL)
    {
        destination_array[i++] = p;     // copy splitted items into the array
        p = strtok (NULL, " ");
    }
    return i;
}

/* trivial and very detailed comments are in central.c, I didn't repeat the 
    " you know what" comments in this file to simply save time */
int main(int argc, char *argv[])
{
    int sockfd, recv_bytes, send_bytes;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];
    char recv_buf[MAX_CHAR_ARRAY_LENGTH];
    int result_split_total;
    char *result_split[MAX_STRING_POINTER_LENGTH];

    /* the following function is revised based on Beej's code */
    // to connect to central server by TCP
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(CENTRAL_IP, PORT_CENTRAL_TCP_A, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    for(p = servinfo; p != NULL; p = p->ai_next) 
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            perror("clientA: socket");
            continue;
        }

        if ( connect(sockfd, p->ai_addr, p->ai_addrlen)  == -1 )
        {
            perror("clientA: connect");
            continue;
        }

        break;

    }

    if (p == NULL)
    {
        fprintf(stderr, "clientA: failed to connect\n");
        return 2;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);

    printf("The client is up and running.\n\n");

    freeaddrinfo(servinfo);


    // send the username to the central server
    if (send_bytes = send(sockfd, argv[1], sizeof(argv[1]), 0) == -1)
    {
        perror("send");
        exit(0);
    }
    printf("The client sent %s to the Central server.\n", argv[1]);


    // receive the matching information
    if ((recv_bytes = recv(sockfd, recv_buf, MAX_CHAR_ARRAY_LENGTH, 0)) == -1)
    {
        perror("recv");
        exit(1);
    }
    recv_buf[recv_bytes] = '\0';

    // split string and put them into the array
    result_split_total = split_string(result_split, recv_buf);

    /* the following block processes the matching information and prints them on screen */
    // if the first item is a digit, means that compatibility found
    if ( strspn(result_split[0], "0123456789.") ==  strlen(result_split[0]) )
    {
        // if the second item didn't match the username, switch the second item and the third item
        if (strcmp(result_split[1], argv[1]) != 0)
        {
            char *swap;
            swap = result_split[1];
            result_split[1] = result_split[result_split_total - 1];
            result_split[result_split_total - 1] = swap;
        }
        // print the result
        printf("Found compatibility for %s and %s:\n", 
                result_split[1], result_split[result_split_total - 1]);

        printf("%s", result_split[1]);
        
        for ( int p=2; p < result_split_total; p++)
        {
            printf(" --- %s", result_split[p]);
        }
        printf("\n");
        printf("Compatibility score: %s\n", result_split[0]);
    }
    // if the first item is not a digit, means that compatibility not found, client will receive usernames
    else
    {
        // if the first item didn't match the username, switch the first item and the second item
        if (strcmp(result_split[0], argv[1]) != 0)
        {
            char *swap;
            swap = result_split[0];
            result_split[0] = result_split[result_split_total - 1];
            result_split[result_split_total - 1] = swap;
        }
        printf("Found no compatibility between %s and %s.\n", result_split[0], result_split[1]);
    }

    close(sockfd);

    return 0;
}
