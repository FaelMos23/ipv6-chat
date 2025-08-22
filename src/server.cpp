#include <cstring>
#include <iostream>
#include <netinet/in.h> // sockaddr_in, htons, htonl
#include <sys/socket.h> // socket, connect, bind, listen, accept
#include <arpa/inet.h>  // inet_pton, inet_ntop
#include <unistd.h>     // close
#include <pthread.h>
#include <string.h>
#include <ctime>
#include <vector>
#include <string>


typedef struct Arguments
{
    int clientSocket;
    bool* loopON;
    bool* needProcessing;
    std::vector<std::string>* comm;
}arg;


void currTime(char*);
void* process_inputs(void*);
void* send_results(void*);


int main()
{
    // variables
    char TIME[50];
    std::vector<std::string> commands; 
    bool loopON = true;
    bool needProcessing = false; // when true, it means the received command has been processed
                                // used as a green/red light
    
    currTime(TIME);
    std::cout << TIME << std::endl;    

    // creating socket
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    // specifying the address
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET; // TCP
    serverAddress.sin_port = htons(28657);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    // binding socket.
    bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress));

    // listening to the assigned socket
    listen(serverSocket, 5);

    // accepting connection request
    int clientSocket = accept(serverSocket, nullptr, nullptr);

    // arguments for threads
    arg* a = (arg*) malloc(sizeof(arg));
    a->clientSocket = clientSocket;
    a->loopON = &loopON;
    a->needProcessing = &needProcessing;
    a->comm = &commands;

    // saving username
    char username[40]; // should be expanded to an array later
    ssize_t count = recv(clientSocket, username, sizeof(username)-1, 0);
    if (count > 0) {
        username[count] = '\0';
    } else {
        std::cerr << "Empty username\n";
    }

    // confirmation message
    std::string conf_msg = "CONNECTION ESTABLISHED, WELCOME ";
    conf_msg += username; // adds the username
    send(clientSocket, conf_msg.c_str(), conf_msg.size(), 0);

    // UP TO HERE, THE CODE IS AS INTENDED

    // thread that reads from socket and processes
    pthread_t socket2proc;
    //pthread_create(&socket2proc, NULL, process_inputs, (void*) a);

    // thread that sends the result to the client
    pthread_t proc2client;
    //pthread_create(&proc2client, NULL, send_results, (void*) a);

    // receiving data example
    //char buffer[256] = { 0 };
    //recv(clientSocket, buffer, sizeof(buffer), 0);
    //std::cout << "Message from " << username << ": " << buffer << std::endl;

    //pthread_join(socket2proc, NULL);
    //pthread_join(proc2client, NULL);

    // closing the socket.
    close(serverSocket);
    close(clientSocket);

    return 0;
}


void currTime(char* str)
{
    time_t timestamp = time(NULL);
    struct tm datetime = *localtime(&timestamp);

    char output[50];

    strftime(output, 50, "%H:%M:%S", &datetime);

    strcpy(str, output);
    
    return;
}


void* process_inputs(void* args)
{
    arg* a = (arg*) args;
    char buffer[256] = { 0 };

    while(*a->loopON)
    {

        while(*a->needProcessing) // red light
            sched_yield();

        recv(a->clientSocket, buffer, sizeof(buffer), 0);
        (*a->comm).push_back(buffer);

        *a->needProcessing = true;
    
    }

    return (void*) a;
}

void* send_results(void* args)
{
    arg* a = (arg*) args;
    char msg[255];
    std::string comm;

    while(*a->loopON)
    {

        while(!(*a->needProcessing)) // red light
            sched_yield();

        if(!(*a->comm.empty()))
        {
            comm = (*a->comm)[0];

            send(a->clientSocket, msg, sizeof(msg), 0);
        }

    }

    return (void*) a;
}