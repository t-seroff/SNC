// CS 176B Programming Assignment 1
// Simplified NetCat utility - snc
// Tristan Seroff, tristan_seroff, 3563301
// Usage: snc [-l] [-u] [-s source_ip_address] hostname [port]

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

#include <signal.h>
#include <netdb.h>

void sendInput(int sock);
void printReceived(int sock);

int main (int argc, char **argv)
{
  // ARGUMENT PARSING

  int parsingError = 0;
  int listenOpt = 0;
  int udp = 0;
  int source = 0;
  char *sourceIP = NULL;
  char *hostname = NULL;
  int portNum = -1;
  struct in_addr sourceAddress;

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
        if (inet_aton(sourceIP, &sourceAddress) == 0){
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
    printf("Usage: snc [-l] [-u] [-s source_ip_address] hostname [port]\n");
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
  struct sockaddr_in source_addr; // Source address socket - only to be used if -s was chosen and -l was not.

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
    struct sockaddr_in client_addr;
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(portNum);
    struct in_addr listenAddress;
    if (hostname == NULL){    
      listenAddress.s_addr = INADDR_ANY;
    }
    else{
      if(inet_aton(hostname, &listenAddress) == 0){
        printf("problem parsing hostname / host address.\n");
      }
    }
    client_addr.sin_addr = listenAddress;


    // Bind the socket to the port/address combination specified
    if (bind(server_sock, (struct sockaddr *) &client_addr, sizeof(client_addr)) != 0){
      printf("Error binding socket!\n");
    }
    printf("Socket is bound.\n");

    // Accept a new socket connection via listening to the existing socket, for TCP. Otherwise just use the socket.
    if(!udp){
      // Listen on that newly-bound socket
      if (listen(server_sock, 5) != 0){
        printf("Error listening!\n");
      }
      printf("Listening for an incoming connection.\n");
    
      // Accept incoming connections, which creates a new socket and connection-socket client address struct
      struct sockaddr clientAddress;
      socklen_t cliAddrSize;
      printf("Waiting to accept a connection.\n");
      sock = accept(server_sock, &clientAddress, &cliAddrSize);
      printf("Socket connected!\n");
    }
    else{
      sock = server_sock;
    }
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

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(portNum);
    struct in_addr hostAddress;
    if(inet_aton(hostname, &hostAddress) == 0){
      printf("problem parsing hostname / host address.\n");
    }
    server_addr.sin_addr = hostAddress;
    
    sock = connect(client_sock, (struct sockaddr *) &server_addr, sizeof(server_addr));
    printf("Socket connected!\n");
    
    if (source){
      // Construct the outgoing 'source' address struct and populate it

      source_addr.sin_family = AF_INET;
      source_addr.sin_port = htons(9999); // Change this later?
      if (sourceIP == NULL){    
        sourceAddress.s_addr = INADDR_ANY;
      }
      // Otherwise the argument parsing already did the copy+conversion here.
      source_addr.sin_addr = sourceAddress;
    }
  }

   
  
  // SEND/RECEIVE MESSAGES

  // Create reading and printing threads, then wait for the input-reading one to join after a CTRL-D
  pthread_t sendThread;
  pthread_t printThread;
  pthread_create(&sendThread, NULL, (void *)sendInput, (void *) sock);
  pthread_create(&printThread, NULL, (void *)printReceived, (void *) sock);
  //sleep(3);
  pthread_join(sendThread, NULL);
  printf("Exiting.\n");
  exit(0);
}

void sendInput(int sock){
  // Read input from stdin and send every line over the socket
  printf("Read+send thread started!\n");

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
        send(sock, input, inputSize-1, 0);
      }
    }
  }

  printf("exiting sendInput\n");
  return;

  
}

void printReceived(int sock){
  printf("Receive+print thread started!\n");

  int bufferSize = 100;
  char buffer[bufferSize];

  while(1){
    recv(sock, buffer, 100, 0);
    if (buffer[0] == -1){ // EOF
      exit(0);
    }
    printf("Message received is: %s", buffer);
  }
}
