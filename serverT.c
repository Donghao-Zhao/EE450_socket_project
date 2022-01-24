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
#define PORT_SERVER_UDP_T  "21890"      // UDP port for serverT
#define PORT_CENTRAL_UDP   "24890"      // UDP port for serverC

#define SMALL_CHAR_ARRAY_LENGTH     1024    // length of array for data to transfer usernames
#define MAX_CHAR_ARRAY_LENGTH       204800  // length of array for data flying in the network
#define MAX_STRING_POINTER_LENGTH   200     // length of array for string pointer to be processed

int reach_destination = 0;      //flag for whether the search reached the destination item

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
// split string and put them into the array but no return of index
void split_string_no_return(char *args[], char *message) {
    char *p = strtok (message, " ");
    int i = 0;
    while (p != NULL)
    {
        args[i++] = p;
        p = strtok (NULL, " ");
    }
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

// search and get the items that are connected with the searched item
int search_match(char *array[], int array_length, char *query_item, 
                    char *query_result[])
{
    int i, pair;
    int query_index = 0;
    
    for ( i=0; i < array_length; i++)
    {
        if(!strcmp(array[i], query_item))   // if find a matching item
        {
            // find the pair item
            if(i % 2) pair = i - 1;
            else pair = i + 1;
            query_result[query_index] = array[pair];
            query_index ++;
        }
    }

    return query_index;
}

// generate all paths between beginning item and destination item in the topology
int find_path(char *path[MAX_STRING_POINTER_LENGTH], 
    char *topology_buf[MAX_STRING_POINTER_LENGTH], int topology_index, char *newid_buf[2])
{
    char *stack[MAX_STRING_POINTER_LENGTH]; // stack array
    int stack_index = 0;                    // stack index

    int path_index = 0;                     // path index

    int found_in_path = 0;                  // flag of an item found in the path
    
    int reach_destination_flag = 0;         // flag of whether the path could reach the destination username
    int last_path_index = 1;                // the beginning of the current path

    char *tree[MAX_STRING_POINTER_LENGTH];  // arrays of items that are connected with the searched item
    int tree_index;                         // tree index

    memset(&stack, 0, sizeof stack);

    stack[stack_index] = newid_buf[0];      // push the beginning item to stack
    stack_index++;

    // if stack is not empty
    while( stack_index != 0 )
    {
        // search and get the items that are connected with the searched item at the top of the stack
        tree_index = search_match(topology_buf, topology_index, stack[stack_index -1], tree);

        // if didn't find any item connected with the searched item, end this search
        if (tree_index == 0)
        {
            break;
        }

        // move the item that is searched from stack to path
        path[path_index] = stack[stack_index - 1];
        path_index++;
        stack_index--;

        // check if the result of the search has an item that reaches destination
        for(int i = 0; i < tree_index; i++)
        {
            // if yes, end searching this route
            if(strcmp(tree[i], newid_buf[1]) == 0)
            {
                reach_destination = 1;
                reach_destination_flag = 1;
                path[path_index] = newid_buf[1];
                path_index++;
            }
        }

        // determine whether to push the searched item into stack by checking if the current
        // path has a same item
        for(int i = 0; i < tree_index; i++)
        {
            for(int j = last_path_index; j < path_index; j++)
            {
                if(strcmp(tree[i], path[j]) == 0)
                {
                    found_in_path = 1;             //if found same in path, no push
                }
            }
            if ( (!found_in_path) && (!reach_destination_flag) && (strcmp(tree[i], newid_buf[0]) != 0) )   //if not found in path
            {
                stack[stack_index] = tree[i];   //if not, item go to stack waiting to be searched
                stack_index++;
            }
            found_in_path = 0;
        }

        // if reached destination item, start a new route
        if(reach_destination_flag == 1)
        {
            reach_destination_flag = 0;
            last_path_index = path_index;
        }
    }

    return path_index;
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
    char recv_id[MAX_CHAR_ARRAY_LENGTH];
    socklen_t addr_len;
    char s[INET6_ADDRSTRLEN];
	struct sockaddr_in sin;

	char *newid_buf[2];         // two usernames

    // related with reading the topology database
    FILE *fp;
    char content_1[SMALL_CHAR_ARRAY_LENGTH],content_2[SMALL_CHAR_ARRAY_LENGTH]; // items read from database
	char topology_string[MAX_CHAR_ARRAY_LENGTH];    // topology string read from database
    char *topology_buf[MAX_STRING_POINTER_LENGTH];  // topology buffer
    int topology_index;

    // related with generating path
    char *path_1[MAX_STRING_POINTER_LENGTH], *path_2[MAX_STRING_POINTER_LENGTH];
    int no_result = 0;
    int path_index_1 = 0;
    int path_index_2 = 0;

    char udp_send_graph[MAX_CHAR_ARRAY_LENGTH];     // graph to be sent to the central server

    memset(&topology_buf, 0, sizeof topology_buf);
    
    // setup UDP connection of serverT
    sockfd = setupUDP(PORT_SERVER_UDP_T);
    
    // get the port of the UDP connection and print it
	socklen_t len = sizeof(sin);
	if (getsockname(sockfd, (struct sockaddr *)&sin, &len) == -1)
		perror("getsockname");
    
    printf("The ServerT is up and running using UDP on port %d.\n", ntohs(sin.sin_port));

    // main loop
    while(1)
    {
        /***** Reset *****/

        // reset these flags and arrays at the beginning of the loop
        memset(&path_1, 0, sizeof path_1);
        memset(&path_2, 0, sizeof path_2);
        path_index_1 = 0;
        path_index_2 = 0;
        reach_destination = 0;

        /***** get usernames *****/

        addr_len = sizeof their_addr;

        // receive usernames string that is sent from central server
        if ((numbytes = recvfrom(sockfd, recv_id, MAX_CHAR_ARRAY_LENGTH - 1 , 0, (struct sockaddr *)&their_addr, &addr_len)) == -1)
        {
            perror("recvfrom");
            exit(1);
        }
        recv_id[numbytes] = '\0';

        printf("The ServerT received a request from Central to get the topology.\n");

        // split the usernames string and store them into arrays
        split_string_no_return(newid_buf, recv_id);


        /***** read edgelist *****/

        memset(&topology_string, 0, sizeof topology_string);
        memset(&udp_send_graph, 0, sizeof udp_send_graph);

        // read edgelist from database
        if (!(fp=fopen("edgelist.txt","r")))    // open database
        {
            printf("Error in open file!\n");
            return 1;
        }
        while( fscanf(fp, "%s %s", content_1, content_2) != EOF )   // read data
        {
            sprintf(topology_string, "%s %s %s", topology_string, content_1, content_2);
        }
        // split the edgelist string and store them into arrays
        topology_index = split_string(topology_buf, topology_string);

        
        /***** generate path *****/

        // generate path begin with the first username, reach_destination = 1 if reaches destination
        path_index_1 = find_path(path_1, topology_buf, topology_index, newid_buf);
        // if above search didn't reach the destination, then generate path begin with the second username
        if (reach_destination == 0)
        {
            char *swap;
            swap = newid_buf[0];
            newid_buf[0] = newid_buf[1];
            newid_buf[1] = swap;
            path_index_2 = find_path(path_2, topology_buf, topology_index, newid_buf);
            reach_destination = 0;
        }
        
        /***** generate related graph *****/

        int found_1 = 0;
        int found_2 = 0;

        // the following block copy connection from topology buffer between two items to the graph 
        // to be sent only if the two items can both be found in the two paths
        for ( int p=0; p < topology_index; p+=2)
        {
            found_1 = 0;
            found_2 = 0;
            
            for ( int i = 0; i < path_index_1; i++)
            {
                if (strcmp(topology_buf[p], path_1[i]) == 0)
                found_1 = 1;
            }
            for ( int i = 0; i < path_index_2; i++)
            {
                if (strcmp(topology_buf[p], path_2[i]) == 0)
                found_1 = 1;
            }

            for ( int i = 0; i < path_index_1; i++)
            {
                if (strcmp(topology_buf[p + 1], path_1[i]) == 0)
                found_2 = 1;
            }
            for ( int i = 0; i < path_index_2; i++)
            {
                if (strcmp(topology_buf[p + 1], path_2[i]) == 0)
                found_2 = 1;
            }
            if( found_1 && found_2)
            sprintf(udp_send_graph, "%s %s %s", udp_send_graph, topology_buf[p], topology_buf[p + 1]);
        }

        /***** send the graph *****/

        if ((numbytes = sendto(sockfd, udp_send_graph, strlen(udp_send_graph), 0,
                (struct sockaddr *)&their_addr, addr_len)) == -1)
        {
            //printf("here1\n");
            perror("talker: sendto");
            exit(1);
        }
        
        printf("The ServerT finished sending the topology to Central.\n");

    }

    close(sockfd);

    return 0;
}