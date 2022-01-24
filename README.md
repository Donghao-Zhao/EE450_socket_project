# EE450_socket_project
# Donghao Zhao

a. What I have done in the assignment.

* Phase 1: 

  Phase 1A: establish the connections between the Clients and Server C
  Phase 1B: establish the connections between Server C and all other backend servers

* Phase 2: 

  After receiving messages from both clients, server C will contact server T to retrieve Topology 
  data and server S to retrieve Score data, then forward them to server P. Server P will calculate 
  and find a social network path that has the smallest matching gap based on the information 
  provided by server C.

* Phase 3: 

Server P then send the social network path that has the smallest matching gap to the server C, 
server C will then relay the information to all clients, and clients will see the matching information 
on their screen. If there's no path, then according information will display.




b. What my code files are and what each one of them does.

* central.c:

1. Bootup and setup TCP and UDP connection on certain ports of the central server(serverC).
2. Waiting to accept client's connection and receive usernames information sent from clients using TCP.
3. Send the usernames to serverT to request and receive the topology graph related with the usernames. 
4. Process the data received from serverT and send a request to serverS to get the score of the usernames within the topology graph.
5. Combine the information received from clients, serverT, serverS, send the processing request to serverP and receive the matching information.
6. Forward matching information to all clients, then loop to step 2.


* clientA.c:

1. Bootup and connect and send the username information to the central server through TCP connection.
2. Receive matching information, score and path, from central server and display them on screen.


* clientB.c:

1. Bootup and connect and send the username information to the central server through TCP connection.
2. Receive matching information, score and path, from central server and display them on screen.


* serverT.c:

1. Bootup and setup UDP connection to the central server using UDP.
2. Wait to receive usernames sent from central server and read topology information from database.
3. Generate topology graph related with the usernames.
4. Send the graph to the central server, then back to step 2.


* serverS.c:

1. Bootup and setup UDP connection to the central server using UDP.
2. Wait to receive query request from the central server and read score information from database.
3. Generate the score data that contains the score of all edgelist within query request.
4. Send the score data to the central server, then back to step 2.


* serverP.c:

1. Bootup and setup UDP connection to the central server using UDP.
2. Wait to receive processing request that contains usernames, topology graph, score informationsent from central server.
3. Generate all path and calculate the matching gap to find the path that has the minimum gap.
4. Send the minimum gap figure and the minimum path to the central server, then back to step 2.



c. The format of all the messages exchanged.

  Here is an example when clientA's input is Victor and clientB's input is Oliver and there is a match.


1. Command to start clientA: ./clientA Victor.

Console output of clientA:

The client is up and running.

The client sent Victor to the Central server.
Found compatibility for Victor and Oliver:
Victor --- Rachael --- Oliver
Compatibility score: 1.06


2. Command to start clientB: ./clientB Oliver.

Console output of clientB:

The client is up and running.

The client sent Oliver to the Central server.
Found compatibility for Oliver and Victor:
Oliver --- Rachael --- Victor
Compatibility score: 1.06


3. Command to start serverC: ./serverC.

Console output of serverC(central server):

The Central server is up and running.
The Central server received input="Victor" from the client using TCP over port 25890.
The Central server received input="Oliver" from the client using TCP over port 26890.
The Central server sent a request to Backend-Server T.
The Central server received information from Backend-Server T using UDP over port 24890.
The Central server sent a request to Backend-Server S.
The Central server received information from Backend-Server S using UDP over port 24890.
The Central server sent a processing request to Backend-Server P.
The Central server received the results from backend server P.
The Central server sent the results to clientA.
The Central server sent the results to clientB.


4. Command to start serverT: ./serverT.

Console output of serverT:

The ServerT is up and running using UDP on port 21890.
The ServerT received a request from Central to get the topology.
The ServerT finished sending the topology to Central.


5. Command to start serverS: ./serverS

Console output of serverS.

The ServerS is up and running using UDP on port 22890.
The ServerS received a request from Central to get the scores.
The ServerS finished sending the scores to Central.


6. Command to start serverP: ./serverP

Console output of serverP.

The ServerP is up and running using UDP on port 23890.
The ServerP received the topology and score information.
The ServerP finished sending the results to the central.


Here is an example when clientA's input is Victor and clientB's input is Hanieh and there is no match.


1. Command to start clientA: ./clientA Victor.

Console output of clientA:

The client is up and running.

The client sent Victor to the Central server.
Found no compatibility between Victor and Hanieh.


2. Command to start clientB: ./clientB Hanieh.

Console output of clientB:

The client is up and running.

The client sent Hanieh to the Central server.
Found no compatibility between Hanieh and Victor.


3. All of the message exchange of the servers are the same with what is mentioned above.



g. Idiosyncrasy of my project.

Due to the serverC combined all information in one buffer and send them to serverP, 
the program might crash when the graph and score information exceed the size of the array or 
the size of the sending buffer that UDP's "sendto()" function can support. The size of arrays 
should change based on demand.


h. Reused Code.

The initiation of TCP and UDP connection refers to the Beej’s Guide to Network Programming.
The functions and variables are similar with those in the book, and I have commented them in my codes.

