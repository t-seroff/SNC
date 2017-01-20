// CS 176B Programming Assignment 1
// Simplified NetCat utility - snc
// Tristan Seroff, tristan_seroff, 3563301
// Usage: snc [-l] [-u] [-s source_ip_address] [hostname] port

// Standard libraries
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>     // POSIX threads

// Transport-layer libraries
#include <sys/socket.h>  // Sockets!
#include <netinet/tcp.h> // UDP
#include <netinet/udp.h> // TCP
#include <arpa/inet.h>   // inet library
#include <netinet/in.h>  // in_addr struct, among other constants

#include <errno.h>
#include <signal.h>
#include <netdb.h>

// Structure to pass necessary connection information into the two threads
struct connectionInfo{
  int sock;
  int udp;
  int source;
  struct sockaddr_in *sourceAddress;
  struct sockaddr_in *remoteAddress;
};

// Thread worker functions, forward declared
void sendInput(struct connectionInfo *infoPtr);
void printReceived(struct connectionInfo *infoPtr);

int main (int argc, char **argv)
{
  struct connectionInfo info;
  struct connectionInfo *infoPtr = &info;


  // ARGUMENT PARSING

  int parsingError = 0;
  int listenOpt = 0;
  int udp = 0;
  int source = 0;
  char *sourceIP = NULL;
  char *hostname = NULL;
  int portNum = -1;
//  struct in_addr sourceAddressInput;

  struct sockaddr_in src; // Initialize
  struct sockaddr_in rmt; // Initialize
  struct sockaddr_in *sourceAddress = &src;
  struct sockaddr_in *remoteAddress = &rmt;

  // Parse options
  char option;
  while((option = getopt(argc, argv, "lus:")) != -1){
    switch(option){
      case 'l':{
        // Enable listen option
        listenOpt = 1;
        break;
      }
      case 'u':{
        // Enable UDP option
        udp = 1;
        break;
      }
      case 's':{
        // Enable specify-source-IP option
        source = 1;
        sourceIP = optarg;

        // Check that source_ip_address is actually parseable.
        if (inet_aton(sourceIP, &sourceAddress->sin_addr) == 0){
          printf("source_ip_address was invalid or not present.\n");
          printf("A valid source_ip_address must be specified when using -s!\n");
          parsingError = 1;
        }

        break;
      }
      // Invalid option, or '-s' used without providing source_ip_address
      case '?':{
        if (optopt == 's'){
          printf("source_ip_address must be specified when using -s!\n");
        }
        else{
          printf("Invalid option provided: '-%c'\n", optopt);
          parsingError = 1;
        }
        break;
      }
      default:{
        printf("Option parsing error.\n");
        parsingError = 1;
        break;
      }
    }
  }
  
  // Check that '-l' and '-s' are not used together!
  if (listenOpt && source){
    printf("The '-l' and '-s' flags cannot be used concurrently!\n");
    parsingError = 1;
  }  


  if (!parsingError){
    // Parse '[hostname] port' arguments

    if (optind + 1 == argc){
      // Only one argument provided - Check if this is allowed based on the option flags provided.
      if (!listenOpt){
        // If connecting, check that there's a hostname
        printf("The hostname must be specified if the -l option is not provided!\n");
        parsingError = 1;
      }
      
      // Otherwise providing just the port number is fine.
      portNum = atoi(argv[optind]);
    }
    else if (optind + 2 == argc){
      // Two arguments provided - they should be hostname and port number, in that order.
      hostname = argv[optind];
      portNum = atoi(argv[optind+1]);
    }
    else{
      // No arguments provided!
      printf("You must specify a port number!\n");
    }
    
    // If unable to parse the port number, return an error and quit
    if (portNum == 0 || portNum == -1){
      printf("Un-parseable port number specified. Did you supply a hostname but no port number?\n");
      parsingError = 1;
    }
    // Otherwise portNum is now correct
  }

  // Print the correct usage and quit if there were any parsing errors.
  if (parsingError){
    printf("invalid or missing options\n");
    printf("usage: snc [-l] [-u] [-s source_ip_address] [hostname] port\n");
    exit(1);
  }

  // Possible TODO: check that there weren't extra arguments (such as providing source_ip_address without '-s')?
  // This will likely cause the port number to be unparseable anyways
  // Concern - source IP provided and being interpreted as hostname - validate this somehow?
  
  // If parsing finished successfully, print the parsed arguments
  printf("listenOpt = %d, udp = %d, source = %d, sourceIP = %s\n", listenOpt, udp, source, sourceIP);
  printf("hostname = %s, portNum = %d\n", hostname, portNum);






  // ACCEPT/CREATE CONNECTION
  int sock = 0; // Connected socket, to be populated below.

  // Choose the main operation mode based on whether the -l option was chosen
  if (listenOpt){
    // Listen for a connect as a server
    //   Use the provided port number
    //   Use TCP unless the -u option was chosen
    //   Listen on the provided host name if provided
  
    printf("Listening for incoming socket connection.\n");

    // Create the listen-socket, using the selected transport protocol
    int server_sock;
    if (udp){
      server_sock = socket(AF_INET, SOCK_DGRAM, 0);
    }
    else{
      server_sock = socket(AF_INET, SOCK_STREAM, 0);
    }

    // Construct the listen-socket client address struct and populate it
    remoteAddress->sin_family = AF_INET;
    remoteAddress->sin_port = htons(portNum);
    if (hostname == NULL){    
      remoteAddress->sin_addr.s_addr = INADDR_ANY;
      printf("using 'any address'\n");
    }
    else{
      if(inet_aton(hostname, &remoteAddress->sin_addr) == 0){
        printf("problem parsing hostname / host address.\n");
      }
      printf("hostname is %s\n", inet_ntoa(remoteAddress->sin_addr));
    }

    // Bind the socket to the port/address combination specified
    if (bind(server_sock, (struct sockaddr *) remoteAddress, sizeof(*remoteAddress)) != 0){
      printf("Error binding socket!\n");
      exit(1);
    }
    printf("Socket is bound.\n");

    // Accept a new socket connection via listening to the existing socket, for TCP. Otherwise just use the socket.
    if(!udp){
      // Listen on that newly-bound socket
      if (listen(server_sock, 5) != 0){
        printf("Error listening!\n");
        exit(1);
      }
      printf("Listening for an incoming connection.\n");
    
      // Accept incoming connections, which creates a new socket and connection-socket client address struct
      socklen_t cliAddrSize;
      printf("Waiting to accept a connection.\n");
      sock = accept(server_sock, (struct sockaddr *) remoteAddress, &cliAddrSize);
      printf("Socket connected!\n");    }
    else{
      sock = server_sock;
    }
    sourceAddress = NULL; // The -s flag cannot be used concurrently with the -l flag.    
  }
  else{
    // Open the connection as a client
    //   Use the provided port number
    //   Use TCP unless the -u option was chosen
    //   Use the provided hostname as the destination
    //   Use the provided source IP, if the -s option was specified
    
    printf("Creating outgoing socket connection.\n");

    // Create the transmission-socket, using the selected transport protocol
    int client_sock;
    if (udp){
      client_sock = socket(AF_INET, SOCK_DGRAM, 0);
    }
    else{
      client_sock = socket(AF_INET, SOCK_STREAM, 0);
    }

    // Create the address struct for the server we're going to connect to
    remoteAddress->sin_family = AF_INET;
    remoteAddress->sin_port = htons(portNum);
    if(inet_aton(hostname, &remoteAddress->sin_addr) == 0){
      printf("problem parsing hostname / host address.\n");
      exit(1);
    }
  

    /* TEST THIS
    // Bind the source address to the socket, if provided
    if (source){
      // Construct the outgoing 'source' address struct and populate it
      sourceAddress->sin_family = AF_INET;
      sourceAddress->sin_port = htons(9999); // Change this later?
      if (sourceIP == NULL){    
        sourceAddress->sin_addr.s_addr = INADDR_ANY;
      }
      // Otherwise the argument parsing already did the copy+conversion here.
      if (bind(client_sock, (struct sockaddr *) sourceAddress, sizeof(*sourceAddress)) != 0){
        printf("Error binding socket!\n");
        exit(1);
      }
    }
    else{
      sourceAddress = NULL;
    }
    */

  
    if (!udp){
      // For TCP, actively connect the socket to that remote server
      int code = connect(client_sock, (struct sockaddr *) remoteAddress, sizeof(*remoteAddress));
      if (code == 0){
        printf("Socket connected!\n");
        sock = client_sock;
      }
      else{
        // Quit if there was an error connecting
        printf("Could not connect socket\n");
        exit(1);
      }
    }
    else{
      // For UDP, just use the socket as it's already been configured
      sock = client_sock;
    }

  }

   
  
  // SEND/RECEIVE MESSAGES

  // Build the information struct to pass to the two threads.
  info.sock = sock;
  info.udp = udp;
  info.source = source;
  info.sourceAddress = sourceAddress;
  info.remoteAddress = remoteAddress;
  printf("hostname is %s\n", inet_ntoa(info.remoteAddress->sin_addr));


  // Create reading and printing threads, then wait for the input-reading one to join after a CTRL-D
  pthread_t sendThread;
  pthread_create(&sendThread, NULL, (void *)sendInput, (void *) infoPtr);

  pthread_t printThread;
  printf("Creating second thread...\n");
  pthread_create(&printThread, NULL, (void *)printReceived, (void *) infoPtr);

  // Wait for the input+send thread to join back
  pthread_join(sendThread, NULL);

  printf("Exiting.\n");
  exit(0);
}

void sendInput(struct connectionInfo *infoPtr){
  // Read input from stdin and send every line over the socket
  printf("Read+send thread started!\n");

  //printf("SEND THREAD \nremoteAddress.sin_addr is %s, .sin_port is %d \n", inet_ntoa(infoPtr->remoteAddress->sin_addr), ntohs(infoPtr->remoteAddress->sin_port));


  // Loop reading from stdin, send each line after it is entered.
  char *input;
  size_t inputSize;
  int done = 0;
  while(!done){
    if (getline(&input, &inputSize, stdin) != -1){
      // Quit if CTRL-D is entered
      if (input[0] == 'q'){ // THIS NEEDS TO BE EOF from CTRL-D INSTEAD!
        done = 1;
      }
      // Otherwise send the line
      else{
        if (infoPtr->udp){
          // If UDP, specify the destination
          sendto(infoPtr->sock, input, inputSize-1, 0, (struct sockaddr *) infoPtr->remoteAddress, sizeof(*infoPtr->remoteAddress));
        }
        else{
          // If TCP, just send using the existing socket settings.
          send(infoPtr->sock, input, inputSize-1, 0);
        }
      }
    }
  }

  printf("exiting sendInput\n");
  return;

  
}

void printReceived(struct connectionInfo *infoPtr){
  printf("Receive+print thread started!\n");

  int bufferSize = 100;
  char buffer[bufferSize];

  int msgLen = 0;
  //struct sockaddr_in address;
  socklen_t addressLen = sizeof(infoPtr->remoteAddress);

  while(1){
    msgLen = recvfrom(infoPtr->sock, buffer, 100, 0, (struct sockaddr *) infoPtr->remoteAddress, &addressLen);
    if (msgLen > 0){
      if (buffer[0] == -1){ // EOF
        exit(0);
      }
      printf("Message received is: %s", buffer);
      //printf("RECV THREAD \nremoteAddress.sin_addr is %s, .sin_port is %d addressLen is %d\n", inet_ntoa(infoPtr->remoteAddress->sin_addr), ntohs(infoPtr->remoteAddress->sin_port), addressLen);
    }
  }
}
