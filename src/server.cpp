#include <cstring>
#include <cstdint>
#include <iostream>
#include <netinet/in.h> // sockaddr_in, htons, htonl
#include <sys/socket.h> // socket, connect, bind, listen, accept
#include <arpa/inet.h>  // inet_pton, inet_ntop
#include <unistd.h>     // close
#include <pthread.h>
#include <string.h>
#include <ctime>
#include <vector>

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
uint8_t available(bool*);

int main()
{
    // variables
    std::vector<message> commands;
    bool loopON[3] = {false, false, false};
    uint8_t available_entry;
    char conf_msg[33] = "CONNECTION ESTABLISHED, WELCOME ";

    sockaddr_in clientAddress[3];
    socklen_t clientAddressLen;
    ssize_t count;
    char username[3][40];
    int clientSocket[3];
    char clientIP[3][INET_ADDRSTRLEN];

    arg *a[3];
    pthread_t socket2proc[3];
    pthread_t proc2client[3];

    // creating socket
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    // specifying the address
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET; // TCP
    serverAddress.sin_port = htons(28657);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    // binding socket.
    bind(serverSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress));





    while(true)
    {
        // if there is a space available, listen for more connections
        available_entry = available(loopON);    // returns an entry if available, returns 255 if none is available
        
        if(available_entry == 255)
        {
            sched_yield();
        }
        else 
        {

            // listening to the assigned socket
            listen(serverSocket, 5);

            // accepting connection request and saving client information
            clientAddressLen = sizeof(clientAddress[available_entry]);
            clientSocket[available_entry] = accept(serverSocket, (struct sockaddr *) &(clientAddress[available_entry]), &clientAddressLen);

            inet_ntop(AF_INET, &((clientAddress[available_entry]).sin_addr), clientIP[available_entry], INET_ADDRSTRLEN);
            // int clientPort = ntohs((clientAddress[available_entry]).sin_port);  // port used by client
            // TODO: prepare for empty username -> save IP

            // saving username
            count = recv(clientSocket[available_entry], username[available_entry], sizeof(username[0]) - 1, 0);
            if (count > 0)
            {
                if (username[available_entry][0] == '\n') // we are using this as a control character as the client can't type this as a username
                    strcpy(username[available_entry], clientIP[available_entry]);
                else
                    username[available_entry][count] = '\0';
            }
            else
            {
                std::cerr << "Empty username\n";
            }

            // start the loop for the thread
            loopON[available_entry] = true;

            // arguments for threads
            a[available_entry] = (arg *)malloc(sizeof(arg));
            if(a[available_entry] == NULL)
                std::cerr << "Error on argument\n";

            a[available_entry]->clientSocket = clientSocket[available_entry];
            a[available_entry]->loopON = &(loopON[available_entry]);
            a[available_entry]->comm = &commands;
            a[available_entry]->userID = 0;
            a[available_entry]->username = username[available_entry]; // array of chars
            a[available_entry]->clientIP = clientIP[available_entry];

            // confirmation message
            char send_conf_msg[73];
            strcpy(send_conf_msg, conf_msg);
            strcat(send_conf_msg, username[available_entry]); // adds the username
            send(clientSocket[available_entry], send_conf_msg, sizeof(send_conf_msg), 0);

            // thread that reads from socket and processes
            pthread_create(&(socket2proc[available_entry]), NULL, process_inputs, (void *)a[available_entry]);

            // thread that sends the result to the client
            pthread_create(&(proc2client[available_entry]), NULL, send_results, (void *)a[available_entry]);

        }
    }

    free(a[0]);
    free(a[1]);
    free(a[2]);

    pthread_join(socket2proc[0], NULL);
    pthread_join(proc2client[0], NULL);
    pthread_join(socket2proc[1], NULL);
    pthread_join(proc2client[1], NULL);
    pthread_join(socket2proc[2], NULL);
    pthread_join(proc2client[2], NULL);

    // closing the socket.
    close(serverSocket);
    close(clientSocket[0]);
    close(clientSocket[1]);
    close(clientSocket[2]);

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

uint8_t available(bool* loopON)
{
    if(!loopON[0])
        return 0;

    if(!loopON[1])
        return 1;
    
    if(!loopON[2])
        return 2;

    return 255;
}