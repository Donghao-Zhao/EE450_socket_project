/* Written by Donghao Zhao */
/* Nov 28th, 2021 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define CENTRAL_IP         "127.0.0.1"  // Host IP
#define PORT_SERVER_UDP_S  "22890"      // UDP port for serverS
#define PORT_CENTRAL_UDP   "24890"      // UDP port for serverC

#define SMALL_CHAR_ARRAY_LENGTH     1024    // length of array for data to transfer usernames
#define MAX_CHAR_ARRAY_LENGTH       204800  // length of array for data flying in the network
#define MAX_STRING_POINTER_LENGTH   200     // length of array for string pointer to be processed

/* the following function is revised based on Beej's code */
// to set up UDP connection of the given port number
int setupUDP(char* port)
{
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    socklen_t addr_len;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(CENTRAL_IP, port, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    for(p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
        p->ai_protocol)) == -1)
        {
            perror("listener: socket");
            continue;
        }
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(sockfd);
            perror("listener: bind");
            continue;
        }

        break;
    }

    if (p == NULL)
    {
        fprintf(stderr, "listener: failed to bind socket\n");
        return 2;
    }

    freeaddrinfo(servinfo);
    return sockfd;
}

/* the following function is revised from "man" */
// split string and put them into the array
int split_string(char *destination_array[], char *long_string)
{
    char *p = strtok (long_string, " ");    // split input string by space
    int i = 0;
    while (p != NULL)
    {
        destination_array[i++] = p;         // copy splitted items into the array
        p = strtok (NULL, " ");
    }
    return i;
}

/* trivial and very detailed comments are in central.c, I didn't repeat the 
    " you know what" comments in this file to simply save time */
int main(void)
{
    // related with socket
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    int numbytes;
    struct sockaddr_storage their_addr;
    socklen_t addr_len;
    char s[INET6_ADDRSTRLEN]; 
	struct sockaddr_in sin;

    char recv_query[MAX_CHAR_ARRAY_LENGTH];         // query string from central server

    // items read from database
    FILE *fp_score;
    char content_s1[SMALL_CHAR_ARRAY_LENGTH];
    int content_s2;

	char udp_send_score[MAX_CHAR_ARRAY_LENGTH];     // score items sent to central server
    
    // setup UDP connection of serverP
    sockfd = setupUDP(PORT_SERVER_UDP_S);
    
    // obtain port number of the UDP connection
	socklen_t len = sizeof(sin);
	if (getsockname(sockfd, (struct sockaddr *)&sin, &len) == -1)
		perror("getsockname");

    printf("The ServerS is up and running using UDP on port %d.\n", ntohs(sin.sin_port));

    // main loop
    while(1)
    {
        /***** receive from central server *****/

        addr_len = sizeof their_addr;

        // receive query string from central server
        if ((numbytes = recvfrom(sockfd, recv_query, MAX_CHAR_ARRAY_LENGTH - 1 , 0, (struct sockaddr *)&their_addr, &addr_len)) == -1)
        {
            perror("recvfrom");
            exit(1);
        }

        recv_query[numbytes] = '\0';

        printf("The ServerS received a request from Central to get the scores.\n");

        /***** read database to get scores *****/

        char *score_query[10];
        int score_query_index = 0;
        memset(&score_query, 0, sizeof score_query);
        
        // split query string into query buffer
        score_query_index = split_string(score_query, recv_query);

        memset(&udp_send_score, 0, sizeof udp_send_score);

        if (!(fp_score=fopen("scores.txt","r")))
        {
            printf("Error in open file!\n");
            return 1;
        }

        /***** assamble score items *****/

        // copy the information according to the query buffer 
        while( fscanf(fp_score, "%s %d", content_s1, &content_s2) != EOF )
        {
            for ( int i=0; i < score_query_index; i++)
            {
                if (strcmp(content_s1, score_query[i]) == 0)
                {
                    sprintf(udp_send_score, "%s %s %d", udp_send_score, content_s1, content_s2);
                    break;
                }
            }
        }
        
        /***** send score items *****/

        if ((numbytes = sendto(sockfd, udp_send_score, strlen(udp_send_score), 0,
                (struct sockaddr *)&their_addr, addr_len)) == -1)
        {
            printf("here1\n");
            perror("talker: sendto");
            exit(1);
        }

        printf("The ServerS finished sending the scores to Central.\n");
    }

    close(sockfd);

    return 0;
}