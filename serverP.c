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
#define PORT_SERVER_UDP_P  "23890"      // UDP port for serverP
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

// generate the score array of a given path array
int obtain_score(char *array[], int array_length, char *query_item[], int query_item_length,
                    char *query_result[])
{
    int i, j, pair;
    int query_index = 0;

    for (j = 0; j < query_item_length; j++)
    {
        for ( i=0; i < array_length; i++)
        {
            if(!strcmp(array[i], query_item[j]))    // if find a matching item
            {
                // find the pair score
                pair = i + 1;
                query_result[query_index] = array[pair];
                query_index ++;
            }
        }
    }

    return query_index;
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
    struct sockaddr_in sin;
    char s[INET6_ADDRSTRLEN];

    char recv_buf_id[SMALL_CHAR_ARRAY_LENGTH];          // username string
	char sendbuffer[SMALL_CHAR_ARRAY_LENGTH] = "PPP";   // simply for return status
	char *newid_buf[2];                                 // username array

    char recv_origin[MAX_CHAR_ARRAY_LENGTH];            // topology and score string

    int no_result = 0;                              // flag for didn't find any path of the two usernames
    char newid_buf_back[SMALL_CHAR_ARRAY_LENGTH];   // send the usernames back
    
    sockfd = setupUDP(PORT_SERVER_UDP_P);
    
	socklen_t len = sizeof(sin);
	if (getsockname(sockfd, (struct sockaddr *)&sin, &len) == -1)
		perror("getsockname");
    
    printf("The ServerP is up and running using UDP on port %d.\n", ntohs(sin.sin_port));

    while(1)
    {
        /***** receive usernames from central server *****/

        addr_len = sizeof their_addr;
        if ((numbytes = recvfrom(sockfd, recv_buf_id, MAX_CHAR_ARRAY_LENGTH - 1 , 0, (struct sockaddr *)&their_addr, &addr_len)) == -1)
        {
            perror("recvfrom");
            exit(1);
        }
        recv_buf_id[numbytes] = '\0';

        // split username string and store them in arrays
        split_string(newid_buf, recv_buf_id);

        // reply to central server to let it proceed
        if ((numbytes = sendto(sockfd, sendbuffer, strlen(sendbuffer), 0,
                (struct sockaddr *)&their_addr, addr_len)) == -1)
        {
            perror("talker: sendto");
            exit(1);
        }

        /***** receive topology and score from central server *****/

        if ((numbytes = recvfrom(sockfd, recv_origin, MAX_CHAR_ARRAY_LENGTH - 1 , 0, (struct sockaddr *)&their_addr, &addr_len)) == -1)
        {
            perror("recvfrom");
            exit(1);
        }
        recv_origin[numbytes] = '\0';

        printf("The ServerP received the topology and score information.\n");


        // topology and score arrays
        char *buf_split[2 * MAX_CHAR_ARRAY_LENGTH];
        int buf_split_total = 0;
        
        // topology arrays
        char *topology_buf[MAX_CHAR_ARRAY_LENGTH];
        int topology_index = 0;
        
        // score arrays
        char *score_buf[MAX_CHAR_ARRAY_LENGTH];
        int score_index = 0;

        int type_flag = 0;      // flag to seperate topology and score information from original data

        // split topology and score string and store them in arrays
        buf_split_total = split_string(buf_split, recv_origin);

        /***** seperate topology and score arrays from original arrays  *****/

        for ( int p=0; p < buf_split_total; p++)
        {
            // when the item is a digit, means that we reached score arrays field
            if( (type_flag == 0) && (strspn(buf_split[p + 1], "0123456789") == strlen(buf_split[p + 1])) )
            {
                type_flag = 1;  // set the type flag
            }        
            
            // if the type flag is not set, copy the item into topology arrays
            if (type_flag == 0)
            {
                topology_buf[p] = buf_split[p];
                topology_index++;
            }
            // if the type flag is not set, copy the item into score arrays
            else if (type_flag == 1)
            {
                score_buf[p - topology_index] = buf_split[p];
                score_index++;
            }
        }


        /***** generate all paths  *****/
        // the following block generate all paths between beginning item and destination item in the topology
        
        char *path[MAX_STRING_POINTER_LENGTH];  // path array
        char *stack[MAX_STRING_POINTER_LENGTH]; // stack array
        int stack_index = 0;                    // stack index

        int path_index = 0;                     // path index

        int found_in_path = 0;                  // flag of an item found in the path
        
        int reach_destination = 0;              // flag of whether reached the destination username in this query
        int reach_destination_flag = 0;         // flag of whether the path could reach the destination username
        int last_path_index = 1;                // the beginning of the current path

        char *tree[MAX_STRING_POINTER_LENGTH];  // arrays of items that are connected with the searched item
        int tree_index;                         // tree index

        memset(&path, 0, sizeof path);
        memset(&stack, 0, sizeof stack);

        //printf("here\n");
        stack[stack_index] = newid_buf[0];    //original item to stack
        stack_index++;

        // if stack is not empty
        while( stack_index != 0 )
        {
            // search and get the items that are connected with the searched item at the top of the stack
            tree_index = search_match(topology_buf, topology_index, stack[stack_index -1], tree);

            // if didn't find any item connected with the searched item, end this search
            if (tree_index == 0)
            {
                // sent the usernames back for no result
                memset(&newid_buf_back, 0, sizeof newid_buf_back);
                sprintf(newid_buf_back, "%s %s", newid_buf[0], newid_buf[1]);
                if ((numbytes = sendto(sockfd, newid_buf_back, strlen(newid_buf_back), 0,
                (struct sockaddr *)&their_addr, addr_len)) == -1)
                {
                    perror("talker: sendto");
                    exit(1);
                }
                no_result = 1;  // set the flag for no result
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
                    reach_destination = 1;              // reached the destination username in this query
                    reach_destination_flag = 1;         // reached the destination username in this loop
                    path[path_index] = newid_buf[1];    // copy the destination username to the path
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
                        found_in_path = 1;          //if found same in path, no push
                    }
                }
                if ( (!found_in_path) && (!reach_destination_flag) && (strcmp(tree[i], newid_buf[0]) != 0) )   //if not found in path
                {
                    stack[stack_index] = tree[i];   //if not, item go to stack waiting to be searched
                    stack_index++;
                }
                found_in_path = 0;                  // reset the flag of item found in path
            }
            // if reached destination item, start a new route
            if(reach_destination_flag == 1)
            {
                reach_destination_flag = 0;         // reset the flag of reached destination in this loop
                last_path_index = path_index;       // begin with the new path
            }
        }

        // if we found a result but
        if (no_result == 0)
        {
            // we didn't reached the destination username
            if (reach_destination == 0)
            {
                // sent the usernames back for no result also
                memset(&newid_buf_back, 0, sizeof newid_buf_back);
                sprintf(newid_buf_back, "%s %s", newid_buf[0], newid_buf[1]);
                if ((numbytes = sendto(sockfd, newid_buf_back, strlen(newid_buf_back), 0,
                (struct sockaddr *)&their_addr, addr_len)) == -1)
                {
                    perror("talker: sendto");
                    exit(1);
                }
            }
        }

        /***** calculate the path that has minimum score  *****/

        // the path that hold the minimun score
        char *min_path[MAX_STRING_POINTER_LENGTH];
        int min_index = 1;
        float score_min = 0;

        // the current path and its score
        char *tmp_path[MAX_STRING_POINTER_LENGTH];
        int tmp_index = 1;
        float score_tmp = 0;

        int path_finder = 1;        // index of path

        char *score_result_tmp[MAX_STRING_POINTER_LENGTH]; // score result array
        int score_count_tmp = 0;    // index of score result array

        // if we found a result and we reached the destination username
        if (no_result == 0 && reach_destination == 1)
        {
            tmp_path[0] = path[0];      // the beginning username come into current path directly
            
            for ( path_finder = 1; path_finder < path_index; path_finder++)
            {
                // generate the current path
                tmp_index = 1;
                while(strcmp(path[path_finder], newid_buf[1]) != 0) // if item didn't reach the destination username
                {
                    // handle an exception that seems never happen
                    //if( (path_finder == (path_index - 1)) && (strcmp(path[path_finder - 1], newid_buf[1])) )
                    //{
                    //    return 3;
                    //}
                    tmp_path[tmp_index] = path[path_finder];    // item go into current path
                    tmp_index++;
                    path_finder++;
                }
                tmp_path[tmp_index] = path[path_finder];        // destination username go into current path
                tmp_index++;

                // obtain score of the current path
                score_count_tmp = obtain_score(score_buf, score_index, tmp_path, tmp_index, score_result_tmp);
                
                // calculate the score of the current path
                score_tmp = 0;
                for (int i = 0; i < (score_count_tmp-1); i++)
                {
                    score_tmp += (float) abs(( atoi(score_result_tmp[i]) - atoi(score_result_tmp[i+1]) )) / 
                    ( atoi(score_result_tmp[i]) + atoi(score_result_tmp[i+1]) ) ;
                }

                // if the score of the current path is smaller than the minimum path, make it the minimum
                if( (score_tmp < score_min) || !score_min )
                {
                    for(int k = 0; k < tmp_index; k++)
                    {
                        min_path[k] = tmp_path[k];
                    }
                    score_min = score_tmp;
                    min_index = tmp_index;
                }
            }


            /***** calculate the path that has minimum score  *****/

            char udp_send[MAX_CHAR_ARRAY_LENGTH];       // buffer that is to be sent to central server
            char trans_tmp[20];                         // used to round
            
            memset(&udp_send, 0, sizeof udp_send);

            sprintf(trans_tmp,"%.2f",score_min);    // round to the nearest hundredth
            sprintf(udp_send, "%s", trans_tmp);     // add score to the buffer that is to be sent

            // add the minimum path to the buffer that is to be sent 
            for ( int p=0; p < min_index; p++)
            {
                sprintf(udp_send, "%s %s", udp_send, min_path[p]);
            }

            // send the score and matching result to the central server
            if ((numbytes = sendto(sockfd, udp_send, strlen(udp_send), 0,
                    (struct sockaddr *)&their_addr, addr_len)) == -1)
            {
                perror("talker: sendto");
                exit(1);
            }
        }
        // if no result, reset the flags
        else
        {
            no_result = 0;
            reach_destination = 0;
        }
        printf("The ServerP finished sending the results to the central.\n");
    }
    close(sockfd);

    return 0;
}