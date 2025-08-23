#include <cstring>
#include <iostream>
#include <netinet/in.h> // sockaddr_in, htons, htonl
#include <sys/socket.h> // socket, connect, bind, listen, accept
#include <arpa/inet.h>  // inet_pton, inet_ntop
#include <unistd.h>     // close
#include <pthread.h>
#include <string.h>


typedef struct Arguments {
    int clientSocket;
    bool* loopON;
}arg;


void* send_message(void*);
void* get_message(void*);


int main()
{
    // variables
    bool loopON = true;

    // creating socket
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    // specifing args for threads
    arg* a = (arg*) malloc(sizeof(arg));
    a->clientSocket = clientSocket;
    a->loopON = &loopON;

    // specifying address
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(28657);
    if (inet_pton(AF_INET, "10.0.2.4", &serverAddress.sin_addr) <= 0) {
        std::cerr << "Invalid address\n";
    }

    // defining username
    std::string username;
    std::cout << "Insert username: " << std::endl;
    getline(std::cin, username);

    // sending connection request
    connect(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress));

    // sending name
    send(clientSocket, username.c_str(), username.size(), 0);

    // connection confirmed
    char buffer[256] = { 0 };
    recv(clientSocket, buffer, sizeof(buffer), 0);
    std::cout << buffer << std::endl;

    // UP TO HERE, THE CODE IS AS INTENDED

    // thread 1
    pthread_t msg_out;
    pthread_create(&msg_out, NULL, send_message, (void*) a);
    
    // thread 2
    pthread_t msg_in;
    pthread_create(&msg_in, NULL, get_message, (void*) a);

    pthread_join(msg_in, NULL);
    pthread_join(msg_out, NULL);

    // closing socket
    close(clientSocket);

    std::cout << "\nConexão finalizada!\n" << std::endl;

    return 0;
}


void* send_message(void* args)
{
    arg* a = (arg*) args;
    std::string message;

    while(*a->loopON)
    {
        getline(std::cin, message);
        std::cout << "\033[A\33[2K";            // erase line that was just typed
        send(a->clientSocket, message.c_str(), message.size(), 0);

        if(!strcmp(message.c_str(), ":quit"))
        {
            *a->loopON = false;
        }
    }

    return args;
}

void* get_message(void* args)
{
    char buffer[256] = { 0 };
    arg* a = (arg*) args;

    while(*a->loopON)
    {
        recv(a->clientSocket, buffer, sizeof(buffer), 0);
        std::cout << buffer << std::endl;
    }

    return args;
}