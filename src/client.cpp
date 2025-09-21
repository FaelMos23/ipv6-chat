#include <cstring>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h> // sockaddr_in, htons, htonl
#include <sys/socket.h> // socket, connect, bind, listen, accept
#include <arpa/inet.h>  // inet_pton, inet_ntop
#include <unistd.h>     // close
#include <pthread.h>
#include <string.h>

typedef struct Arguments
{
    int clientSocket;
    bool *loopON;
} arg;

void *send_message(void *); // thread1
void *get_message(void *);  // thread2

int main(int argc, char* argv[])
{

    if(argc < 2)
    {
        std::cout << "Use of client: \'./client <IP_ADDRESS>\'\n" << std::endl;
        return 1;
    }

    // variables
    bool loopON = true;

    // creating socket
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(clientSocket < 0)
    {
        perror("Socket error");
        return 1;
    }

    // specifing args for threads
    arg *a = (arg *)malloc(sizeof(arg));
    a->clientSocket = clientSocket;
    a->loopON = &loopON;

    // specifying address
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(28657);
    if (inet_pton(AF_INET, argv[1], &serverAddress.sin_addr) <= 0)
    {
        std::cerr << "Invalid address\n";
    }

    // defining username
    std::string username;
    std::cout << "Insert username: " << std::endl;
    getline(std::cin, username);

    // sending connection request
    int connect_result = connect(clientSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress));
    if(connect_result <0)
    {
        perror("Connect error");
        return 1;
    }

    // sending name
    if (username.empty())
        username.assign("\n\0");
    send(clientSocket, username.c_str(), username.size(), 0);

    // connection confirmed
    char buffer[256] = {0};
    recv(clientSocket, buffer, sizeof(buffer), 0);
    std::cout << buffer << std::endl;

    // thread 1
    pthread_t msg_out;
    pthread_create(&msg_out, NULL, send_message, (void *)a);

    // thread 2
    pthread_t msg_in;
    pthread_create(&msg_in, NULL, get_message, (void *)a);

    pthread_join(msg_in, NULL);
    pthread_join(msg_out, NULL);

    free(a);

    // closing socket
    close(clientSocket);

    std::cout << "\nConnection ended!\n"
              << std::endl;

    return 0;
}

void *send_message(void *args)
{
    arg *a = (arg *)args;
    std::string message;

    while (*a->loopON)
    {
        getline(std::cin, message);
        std::cout << "\033[A\33[2K"; // erase line that was just typed
        int send_result = send(a->clientSocket, message.c_str(), message.size(), 0);
        if(send_result <0)
        {
            perror("Send error");
            return a;
        }

        if (!strcmp(message.c_str(), ":quit"))
        {
            *a->loopON = false;
        }
    }

    return args;
}

void *get_message(void *args)
{
    char buffer[256] = {0};
    arg *a = (arg *)args;

    while (*a->loopON)
    {
        int recv_result = recv(a->clientSocket, buffer, sizeof(buffer), 0);
        if (recv_result < 0) {
            perror("Recv error");
            *a->loopON = false;
            break;
        }
        if (recv_result == 0) {
            std::cout << "Server closed the connection." << std::endl;
            *a->loopON = false;
            break;
        }
        buffer[recv_result] = '\0';
        
        std::cout << buffer << std::endl;
    }

    return args;
}