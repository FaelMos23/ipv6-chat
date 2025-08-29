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

typedef struct Message
{
    char text[256];
    int userID;
} message;

typedef struct Arguments
{ // add username list
    int clientSocket;
    bool *loopON;
    std::vector<message> *comm;
    int userID;
    char *username;
    char *clientIP;
} arg;

void *process_inputs(void *);
void *send_results(void *);

int main()
{
    // variables
    std::vector<message> commands;
    bool loopON = true;

    // creating socket
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    // specifying the address
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET; // TCP
    serverAddress.sin_port = htons(28657);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    // binding socket.
    bind(serverSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress));

    // listening to the assigned socket
    listen(serverSocket, 5);

    // accepting connection request and saving client information
    sockaddr_in clientAddress;
    socklen_t clientAddressLen = sizeof(clientAddress);
    int clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddress, &clientAddressLen);

    char clientIP[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clientAddress.sin_addr, clientIP, INET_ADDRSTRLEN);
    // int clientPort = ntohs(clientAddress.sin_port);  // port used by client
    // TODO: prepare for empty username -> save IP

    // saving username
    char username[40]; // should be expanded to an array later
    ssize_t count = recv(clientSocket, username, sizeof(username) - 1, 0);
    if (count > 0)
    {
        if (username[0] == '\n') // we are using this as a control character as the client can't type this as a username
            strcpy(username, clientIP);
        else
            username[count] = '\0';
    }
    else
    {
        std::cerr << "Empty username\n";
    }

    // arguments for threads
    arg *a = (arg *)malloc(sizeof(arg));
    a->clientSocket = clientSocket;
    a->loopON = &loopON;
    a->comm = &commands;
    a->userID = 0;
    a->username = username; // array of chars
    a->clientIP = clientIP;

    // confirmation message
    std::string conf_msg = "CONNECTION ESTABLISHED, WELCOME ";
    conf_msg += username; // adds the username
    send(clientSocket, conf_msg.c_str(), conf_msg.size(), 0);

    // thread that reads from socket and processes
    pthread_t socket2proc;
    pthread_create(&socket2proc, NULL, process_inputs, (void *)a);

    // thread that sends the result to the client
    pthread_t proc2client;
    pthread_create(&proc2client, NULL, send_results, (void *)a);

    pthread_join(socket2proc, NULL);
    pthread_join(proc2client, NULL);

    // closing the socket.
    close(serverSocket);
    close(clientSocket);

    return 0;
}

void *process_inputs(void *args)
{
    arg *a = (arg *)args;
    char buffer[256] = {0};
    message *m = (message *)malloc(sizeof(Message));
    m->userID = a->userID;
    int msg_size;

    while (*a->loopON)
    {
        msg_size = recv(a->clientSocket, buffer, sizeof(buffer), 0);
        buffer[msg_size] = '\0';

        /// for tests of the information that arrives
        // std::cout << buffer << std::endl;
        // std::cout << "username: " << a->username << std::endl; // test of username

        memcpy(m->text, buffer, sizeof(m->text));
        (*a->comm).push_back(*m);
    }

    return (void *)a;
}

void *send_results(void *args)
{
    time_t timestamp = time(NULL);
    struct tm datetime = *localtime(&timestamp);
    int count;
    arg *a = (arg *)args;
    char msg[255] = {0};
    message comm;

    while (*a->loopON)
    {
        sleep(1);

        if (count++ % 10 == 0)
        {
            timestamp = time(NULL);
            datetime = *localtime(&timestamp);
            strftime(msg, sizeof(msg), "<%H:%M:%S> ", &datetime);
        }

        if (!((*a->comm).empty()))
        {
            comm = (*a->comm).at(0);
            (*a->comm).erase((*a->comm).begin());

            if (comm.userID == a->userID)
            {
                strcat(msg, "[Você]: ");
            }
            else
            {
                snprintf(msg, sizeof(msg), "[%s]: ", a->username); // getting other usernames
            }

            strcat(msg, comm.text);
        }

        if (msg[0] != '\0') // missing processing of commands
        {
            if (!strcmp(comm.text, ":quit"))
                (*a->loopON) = false;
            else
            {
                if (!strncmp(comm.text, ":name", 5))
                {
                    char *token = strtok(comm.text, " ");
                    token = strtok(NULL, " \n\0"); // get the name

                    if (token != NULL)
                        memcpy((a->username), token, 40); // name is not empty
                    else
                        strcpy(a->username, a->clientIP); // name is empty, use IP
                }
            }

            send(a->clientSocket, msg, sizeof(msg), 0);
        }
        msg[0] = '\0';
        comm.text[0] = '\0';
    }

    return (void *)a;
}