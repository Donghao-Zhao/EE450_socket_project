/* Written by Donghao Zhao */
/* Nov 28th, 2021 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>              // errno
#include <string.h>             // memset
#include <sys/types.h>          // socket
#include <sys/socket.h>	        // socket
#include <netinet/in.h>         //sockaddr_in
#include <netdb.h>              // getaddrinfo
#include <arpa/inet.h>          // inet_addr
#include <sys/wait.h>           // waitpid
#include <signal.h>             // signal
#include <sys/time.h>           // select

#define CENTRAL_IP         "127.0.0.1"  // Host IP
#define PORT_SERVER_UDP_T  "21890"      // UDP port for serverT
#define PORT_SERVER_UDP_S  "22890"      // UDP port for serverS
#define PORT_SERVER_UDP_P  "23890"      // UDP port for serverP
#define PORT_CENTRAL_UDP   "24890"      // UDP port for serverC
#define PORT_CENTRAL_TCP_A "25890"      // TCP port for clientA
#define PORT_CENTRAL_TCP_B "26890"      // TCP port for clientB

#define SMALL_CHAR_ARRAY_LENGTH     1024    // length of array for data to transfer usernames
#define MAX_CHAR_ARRAY_LENGTH       204800  // length of array for data flying in the network
#define MAX_STRING_POINTER_LENGTH   200     // length of array for string pointer to be processed
#define BACKLOG                     10      // number of pending connections queue server will hold
#define INVALID_SOCKET              -1      // invalid socket

int first_time = 1;         // flag for if this is the first time sending data to serverP

/* the following function is from Beej's code */
// handle signal
void sigchld_handler(int s)
{
    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;
    while ( waitpid(-1, NULL, WNOHANG) > 0 );
    errno = saved_errno;
}

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

/* the following function is revised based on Beej's code */
// to set up TCP connection of the given port number
int setup_TCP(char *port)
{
    int sockfd;                             // socket file descriptor
    struct addrinfo hints, *servinfo, *p;   // struct to hold socket information
    struct sigaction sa;                    //
    int yes = 1;                            // for reuseable purpose
    int rv;                                 // status for loading address structs

    memset(&hints, 0, sizeof hints);    // reset given buf
    hints.ai_family = AF_UNSPEC;        // IPv4 and IPv6
    hints.ai_socktype = SOCK_STREAM;    // TCP segment
    hints.ai_flags = AI_PASSIVE;        // fill IP for me: use my IP

    // load up address structs
    if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) 
    {
        // make a socket
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            perror("server: socket");
            continue;
        }
        // lose the pesky "Address already in use" error message
        if( setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1)
        {
            perror("setsockopt");
            exit(1);
        }

        // bind the socket
        if( bind(sockfd, p->ai_addr, p->ai_addrlen)  == -1)
        {
            close(sockfd);
            perror("server: bind");
            continue;
        }
        
        break;
    }

    freeaddrinfo(servinfo); // servinfo can retire for its mission is accomplished

    // raise error if we can't find a valid socket to bind
    if (p == NULL)
    {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    // listen on the socket
    if( listen(sockfd, BACKLOG) == -1)      
    {
        perror("Listen failed:");
        return 1;
    }

    // reap all dead processes and empty them
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGCHLD, &sa, NULL) == -1)
    {
        perror("sigaction");
        exit(1);
    }

    return sockfd;
}

/* the following function is revised based on Beej's code */
// to set up UDP connection of the given port number
int setup_UDP(char* port)
{
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM; // UDP datagram

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

/* the following function is revised based on Beej's code */
// to send information using UDP connection by the given port number
int send_UDP(int sockfd, char* port, char *query, char *data)
{
    int numbytes, recv_bytes;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    struct sockaddr_in sin;
    memset(&hints, 0, sizeof hints);

    char recv_data[MAX_CHAR_ARRAY_LENGTH];  // data received from the backend servers
    char destination_server;                // destination server for printf

    if ((rv = getaddrinfo(CENTRAL_IP, port, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    
    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((socket(p->ai_family, p->ai_socktype,
        p->ai_protocol)) == -1)
        {
            perror("listener: socket");
            continue;
        }
        break;
    }

    if (p == NULL)
    {
        fprintf(stderr, "listener: failed to bind socket\n");
        return 2;
    }


    if ((numbytes = sendto(sockfd, query, strlen(query), 0,
			 p->ai_addr, p->ai_addrlen)) == -1)
    {
        printf("here1\n");
		perror("talker: sendto");
		exit(1);
	}

    // printf information based on destination port
    switch ( atoi(port) )
    {
        case 21890:     //serverT
        {
            destination_server = 'T';
            printf("The Central server sent a request to Backend-Server T.\n");
        }
        break;
        
        case 22890:     //serverS
        {
            destination_server = 'S';
            printf("The Central server sent a request to Backend-Server S.\n");
        }
        break;

        case 23890:     //serverP
        {
            destination_server = 'P';
            if (first_time == 0)        // do not printf if this is the first time
            printf("The Central server sent a processing request to Backend-Server P.\n");
        }
        break;
    }

    // waiting to receive data from the destination server
    recv_bytes = recvfrom(sockfd, recv_data, sizeof recv_data, 0, NULL, NULL);
    if(recv_bytes == -1)
    {
        perror("recvfrom");
        exit(1);
    }
    recv_data[recv_bytes] = '\0';

    // printf information based on destination port
    socklen_t len = sizeof(sin);
    if (getsockname(sockfd, (struct sockaddr *)&sin, &len) == -1)
        perror("getsockname");

    if ( destination_server== 'P')
    {
        if (first_time == 1) first_time = 0;        // do not printf if this is the first time
        else 
        {
            printf("The Central server received the results from backend server P.\n");
            first_time = 1;
        }
    }
    else printf("The Central server received information from Backend-Server %c using UDP over port %d.\n",
                 destination_server, ntohs(sin.sin_port));

    strcpy(data, recv_data);

    return 0;
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

int main(int argc, char *argv[])
{
    int sockfd[2], new_fd, udp_sockfd;                  // define all sockets to be used
    int recv_bytes;                                     // number of bytes received
    struct addrinfo hints;
    struct sockaddr_storage their_addr;                 // connector's address information
    socklen_t sin_size;
    char s[INET6_ADDRSTRLEN];
    
    char recv_buf[MAX_CHAR_ARRAY_LENGTH];
    char new_id_username[SMALL_CHAR_ARRAY_LENGTH];
    struct sockaddr_in sin;

    char newid_buf[2][MAX_CHAR_ARRAY_LENGTH/2];         // array to store usernames
    int newid_index = 0;                                // index of usernames
    char udp_send[MAX_STRING_POINTER_LENGTH];           // information sent using UDP

    // information received from backend servers
    char recvT[MAX_CHAR_ARRAY_LENGTH], recvS[MAX_CHAR_ARRAY_LENGTH], 
            recvP[MAX_CHAR_ARRAY_LENGTH], recvT_tmp[MAX_CHAR_ARRAY_LENGTH];
    
    char *topology_buf[MAX_STRING_POINTER_LENGTH];      // buffer to store topology information
    int topology_index;                                 // index of topology buffer
    memset(&topology_buf, 0, sizeof topology_buf);
    
    /* create pipe for parent process to communicate with child processes */
    int n, fd_pipe_1[2], fd_pipe_2[2];                  // pipe descriptor
    pid_t pid;
    char pipe_content[MAX_CHAR_ARRAY_LENGTH];           // information to be piped
    memset(&pipe_content, 0, sizeof pipe_content);

    sockfd[0] = setup_TCP(PORT_CENTRAL_TCP_A);          // setup TCP connection over 25890
    sockfd[1] = setup_TCP(PORT_CENTRAL_TCP_B);          // setup TCP connection over 26890
    udp_sockfd = setup_UDP(PORT_CENTRAL_UDP);           // setup UDP connection over 24890

    printf("The Central server is up and running.\n");

    /* the following block is revised based on Beej's code */
    // use "select" to handle multiple TCP clients
    fd_set read_fds, master;
    int maxfd, fd;
    unsigned int i;
    int status;
    FD_ZERO(&read_fds);
    FD_ZERO(&master);
    maxfd = -1;
    for (i = 0; i < 2; i++) {
        FD_SET(sockfd[i], &master);     // add sockets to select set
        if (sockfd[i] > maxfd)
            maxfd = sockfd[i];          // "maxfd" number of sockfd to be select
    }

    // main loop
    while(1)
	{
        memset(&topology_buf, 0, sizeof topology_buf);
        sin_size = sizeof(their_addr);
        read_fds = master;
        // select from sockfds
        if(status = select(maxfd + 1, &read_fds, NULL, NULL, NULL) == -1)
        {
            if (errno == EINTR) // skip the EINTR error for slow system call
                continue;
            else
                perror("select");
                exit(4);
        }
        if (status < 0)
        {
            return INVALID_SOCKET;
        }
        // select to see if item in the set is set
        fd = INVALID_SOCKET;
        for (i = 0; i <= maxfd; i++)
            if (FD_ISSET(sockfd[i], &read_fds)) {
                fd = sockfd[i];
                break;
            }
        // accept new connection
        if (fd == INVALID_SOCKET)
            return INVALID_SOCKET;
        else
            new_fd = accept(fd, (struct sockaddr *)&their_addr, &sin_size);
        
        // transform data from numeric to presentation
        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
        
        // receive username information sent from client
        if ( (recv_bytes = recv(new_fd, new_id_username, MAX_CHAR_ARRAY_LENGTH - 1, 0)) == -1 )
        {
            perror("recv");
            exit(1);
        }
        
        // obtain port information of the new socket
        socklen_t len = sizeof(sin);
        if (getsockname(new_fd, (struct sockaddr *)&sin, &len) == -1)
            perror("getsockname");

        printf("The Central server received input=\"%s\" from the client using TCP over port %d.\n", 
        new_id_username, ntohs(sin.sin_port));

        // save username information
        strcpy(newid_buf[newid_index], new_id_username);
        newid_index++;

        // create pipe for the first client
        if (newid_index == 1)
        {
            if (pipe (fd_pipe_1) < 0)
            {
                printf ("pipe error!\n");
                return (1);
            }
        }

        // create pipe for the second client
        if (newid_index == 2)
        {
            if (pipe (fd_pipe_2) < 0)
            {
                printf ("pipe error!\n");
                return (1);
            }
        }

        // create child process
        if (pid = fork() > 0)       // if this is parent process
        {
            // close "read" side of the pipe of the first client
            if (newid_index == 1)
            {
                close (fd_pipe_1[0]);
            }
            // close "read" side of the pipe of the second client
            if (newid_index == 2)
            {
                close (fd_pipe_2[0]);
            }

            // do the following block only when both two clients are connected
            if(newid_index == 2)
            {
                /***** serverT *****/

                newid_index = 0;
                // combine usernames to one string
                sprintf(udp_send, "%s %s", newid_buf[0], newid_buf[1]);
                // send the combined username string to serverT and receive Topology graph in recvT
                send_UDP(udp_sockfd, PORT_SERVER_UDP_T, udp_send, recvT);


                /***** serverS *****/

                // copy the received Topology information for future process
                sprintf(recvT_tmp, "%s", recvT);
                // split Topology information string and put them into a buffer
                topology_index = split_string(topology_buf, recvT_tmp);

                /* the following block used to generate information for query scores */
                /* elements in topology graph will be selected to query buffer unrepeatedly*/
                int found = 0;
                char *score_query[MAX_STRING_POINTER_LENGTH];
                int score_query_index = 1;
                memset(&score_query, 0, sizeof score_query);

                score_query[0] = topology_buf[0];
                for ( int p=1; p < topology_index; p++)
                {
                    found = 0;
                    for ( int i=0; i < score_query_index; i++)
                    {
                        if (strcmp(topology_buf[p], score_query[i]) == 0)
                        {
                            found = 1;
                            break;
                        }
                        else found = 0;
                    }
                    if (found == 0)
                    {
                        score_query[score_query_index] = topology_buf[p];
                        score_query_index++;
                    }
                }
                // transform array to a string to be sent to serverS
                char score_query_send[MAX_CHAR_ARRAY_LENGTH];
                memset(score_query_send, 0, sizeof score_query_send);
                for ( int p = 0; p < score_query_index; p++)
                {
                    sprintf(score_query_send, "%s %s", score_query_send, score_query[p]);
                }
                // send information to serverS to query Score and receiver Score information in recvS
                send_UDP(udp_sockfd, PORT_SERVER_UDP_S, score_query_send, recvS);
                

                /***** serverP *****/
                // send usernames to server P
                send_UDP(udp_sockfd, PORT_SERVER_UDP_P, udp_send, recvP);
                // combine Topology and Score information into a string
                sprintf(udp_send, "%s %s", recvT, recvS);
                // send the string to serverP and receive matching information in recvP
                send_UDP(udp_sockfd, PORT_SERVER_UDP_P, udp_send, recvP);
                // relay matching information to child processes
                write (fd_pipe_1[1], recvP, (int)strlen(recvP));
                write (fd_pipe_2[1], recvP, (int)strlen(recvP));

                memset(&recvP, 0, sizeof recvP);

            }
        }
        
        if (pid == 0) // if this is child process
        {
            /* the following block determine which client connected based on port information*/
            char new_id_type;
            struct sockaddr_in sin;
            socklen_t len = sizeof(sin);
            if (getsockname(new_fd, (struct sockaddr *)&sin, &len) == -1)
                perror("getsockname");
            
            switch ( ntohs(sin.sin_port) )
            {
                case 25890:
                {
                    new_id_type = 'A';
                }
                break;
                
                case 26890:
                {  
                    new_id_type = 'B';
                }
                break;
            }
            /* close "write" side of the pipe of the clients and receive 
                information piped from parent process*/
            if (newid_index == 1)
            {
                close (fd_pipe_1[1]);

                n = read (fd_pipe_1[0], pipe_content, 1000);
            }
            if (newid_index == 2)
            {
                close (fd_pipe_2[1]);
                n = read (fd_pipe_2[0], pipe_content, 1000);
            }
            //send the piped information to according clients
            if (send(new_fd, pipe_content, strlen(pipe_content), 0) == -1)
            perror("send");
            printf("The Central server sent the results to client%c.\n", new_id_type);
            
            newid_index = 0;

            close(new_fd);
            exit(0);
        }
        
        close(new_fd); // parent doesn't need this
        
        //printf("newid_index 3 %d\n", newid_index);
        
        
    }
    return 0;
}
