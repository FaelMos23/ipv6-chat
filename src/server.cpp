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
{
    int clientSocket;
    bool *loopON;
    std::vector<message> *comm;
    int userID;
    char *username;
    char *clientIP;
    int num_users;
} arg;

void *process_inputs(void *);
void *send_results(void *);
uint8_t available(bool*, int);

int main(int argc, char* argv[])
{
    // setting number of users
    int NUM_USERS = 3;
    if(argc == 2)
    {
        if(atoi(argv[1]) > 0 && atoi(argv[1]) < 10)
            NUM_USERS = atoi(argv[1]);
        else
            std::cout << "Bad number of users, range 1-9\nDefault used is 3\n\n" << std::endl;
    }
    else
        std::cout << "Format: \'./server <NUM OF USERS>\'\nDefault used is 3\n\n" << std::endl;

    // variables
    int i;
    std::vector<message> commands[NUM_USERS];
    bool loopON[NUM_USERS];
    for(i=0; i<NUM_USERS; i++)
        loopON[i] = false;
    uint8_t available_entry;
    char conf_msg[33] = "CONNECTION ESTABLISHED, WELCOME ";

    sockaddr_in clientAddress[NUM_USERS];
    socklen_t addressLen = sizeof(clientAddress[0]);
    ssize_t count;
    char username[NUM_USERS][40];
    int clientSocket[NUM_USERS];
    char clientIP[NUM_USERS][INET_ADDRSTRLEN];

    arg *a[NUM_USERS];
    for(i=0; i<NUM_USERS; i++)
        a[i] = (arg *)malloc(sizeof(arg));
    pthread_t socket2proc[NUM_USERS];
    pthread_t proc2client[NUM_USERS];


    // creating socket
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(serverSocket < 0)
    {
        perror("Socket error");
        return 1;
    }

    // specifying the address
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET; // TCP
    serverAddress.sin_port = htons(28657);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    // binding socket.
    int bind_result = bind(serverSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress));
    if(bind_result < 0)
    {
        perror("Bind error");
        return 1;
    }

    // listening to the assigned socket
    int listen_result = listen(serverSocket, 5);
    if(listen_result < 0)
    {
        perror("Listen error");
        return 1;
    }

    while(true)
    {
        sockaddr_in tempAddr;
        int tempAccept = accept(serverSocket, (struct sockaddr *) &(tempAddr), &addressLen);


        // if there is a space available, listen for more connections
        available_entry = available(loopON, NUM_USERS);    // returns an entry if available, returns 255 if none is available

        if(available_entry == 255)
        {
            
            const char full_msg[44] = "\n\nWe have a full server, try again later.\n\n";
            send(tempAccept, full_msg, strlen(full_msg), 0);
            close(tempAccept);
            continue;
        }
        else 
        {
            memcpy(&(clientSocket[available_entry]), &tempAccept, sizeof(tempAccept));
            memcpy(&(clientAddress[available_entry]), &tempAddr, sizeof(tempAccept));

            // accepting connection request and saving client information
            while (clientSocket[available_entry] < 0 && errno == EINTR)
            {
                clientSocket[available_entry] = accept(serverSocket, (struct sockaddr *) &(clientAddress[available_entry]), &addressLen);
            }
            if (clientSocket[available_entry] < 0) {
                perror("Accept error");
                continue;
            }

            // get IP from client
            inet_ntop(AF_INET, &((clientAddress[available_entry]).sin_addr), clientIP[available_entry], INET_ADDRSTRLEN);

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
                if(count == 0)  // client closed connection
                {
                    std::cout << "Client " << available_entry << " Disconnected" << std::endl;
                    continue;
                }
                else 
                {
                    // prints the number of the error
                    std::cout << "Disconnection error number: " << errno << std::endl;
                    continue;
                }
            }

            // start the loop for the thread
            loopON[available_entry] = true;

            // arguments for threads
            if(a[available_entry] == NULL)
                std::cerr << "Error on argument\n";

            a[available_entry]->clientSocket = clientSocket[available_entry];
            a[available_entry]->loopON = &(loopON[0]);  // need access to all of them
            a[available_entry]->comm = &(commands[0]);  // passing the first because you need access to all of them
            a[available_entry]->userID = available_entry;
            a[available_entry]->username = &(username[0][0]); // array of chars /// need access to all of them
            a[available_entry]->clientIP = clientIP[available_entry];
            a[available_entry]->num_users = NUM_USERS;

            // confirmation message
            char send_conf_msg[73];
            strcpy(send_conf_msg, conf_msg);
            strcat(send_conf_msg, username[available_entry]); // adds the username
            int send_result = send(clientSocket[available_entry], send_conf_msg, strlen(send_conf_msg), 0);
            if(send_result < 0)
            {
                std::cout << "Client " << available_entry << " Disconnected. " << "Disconnection error number: " << errno << std::endl;
                loopON[available_entry] = false;   // stop the loop of this client
                continue;   // do not create the threads
            }

            // thread that reads from socket and processes
            pthread_create(&(socket2proc[available_entry]), NULL, process_inputs, (void *)a[available_entry]);

            // thread that sends the result to the client
            pthread_create(&(proc2client[available_entry]), NULL, send_results, (void *)a[available_entry]);

        }
    }

    for(i=0; i<NUM_USERS; i++)
    {
        free(a[i]);
        pthread_join(socket2proc[i], NULL);
        pthread_join(proc2client[i], NULL);

        // closing the socket.
        close(clientSocket[i]);
    }

    // closing the socket.
    close(serverSocket);

    return 0;
}

void *process_inputs(void *args)
{
    arg *a = (arg *)args;
    char buffer[256] = {0};
    message *m = (message *)malloc(sizeof(Message));
    m->userID = a->userID;
    int msg_size, i;
    bool* localLoopON = a->loopON+(a->userID);

    while (*localLoopON)
    {
        msg_size = recv(a->clientSocket, buffer, sizeof(buffer), 0);
        if (msg_size > 0)
        {

            buffer[msg_size] = '\0';

            /// for tests of the information that arrives
            // std::cout << buffer << " by username: " << (a->username[40*m->userID]) << ". ID: " << m->userID << std::endl; // test of username

            memcpy(m->text, buffer, sizeof(m->text));
            for(i=0; i<a->num_users; i++)
                if(*(a->loopON+i))
                    a->comm[i].push_back(*m);   // puts the message on the queue for all the connected devices

        }
        else
        {
            if(msg_size == 0)  // client closed connection
            {
                std::cout << "Client " << a->userID << " Disconnected" << std::endl;
                (*localLoopON) = false;   // stop the loop of this client
            }
            else 
            {
                // prints the number of the error
                std::cout << "Client " << a->userID << " Disconnected. " << "Disconnection error number: " << errno << std::endl;
                (*localLoopON) = false;   // stop the loop of this client
            }
        }

    }

    return (void *)a;
}

void *send_results(void *args)
{
    time_t timestamp = time(NULL);
    struct tm datetime = *localtime(&timestamp);
    int count = 0;
    arg *a = (arg *)args;
    char msg[255] = {0};
    message comm;
    std::vector<message> * pComm = a->comm+(a->userID);
    bool* localLoopON = a->loopON+(a->userID);


    while (*localLoopON)
    {
        sleep(1);

        // should we add the time?
        if (count++ % 60 == 0)  // NUMBER OF SECONDS
        {
            timestamp = time(NULL);
            datetime = *localtime(&timestamp);
            strftime(msg, sizeof(msg), "<%H:%M:%S> ", &datetime);
        }

        // add the message
        if (!((*pComm).empty()))
        {
            comm = (*pComm).at(0);
            (*pComm).erase((*pComm).begin());

            if (comm.userID == a->userID)
            {
                strcat(msg, "[You]: ");
            }
            else
            {
                snprintf(msg, sizeof(msg), "[%s]: ", &(a->username[40*comm.userID])); // getting other usernames
            }

            strcat(msg, comm.text);
        }

        // send what we have (time and/or message)
        // and process commands
        if (msg[0] != '\0')
        {
            if (!strcmp(comm.text, ":quit") && comm.userID == a->userID)
                (*localLoopON) = false;
            else
            {
                if (!strncmp(comm.text, ":name", 5) && comm.userID == a->userID)
                {
                    char *token = strtok(comm.text, " ");
                    token = strtok(NULL, " \n\0"); // get the name

                    if (token != NULL)
                        memcpy(&(a->username[40*comm.userID]), token, 40); // name is not empty
                    else
                        strcpy(&(a->username[40*comm.userID]), a->clientIP); // name is empty, use IP
                }
            }

            int send_result = send(a->clientSocket, msg, strlen(msg), 0);
            if(send_result < 0)
            {
                std::cout << "Client " << a->userID << " Disconnected. " << "Error number: " << errno << std::endl;
                (*localLoopON) = false;   // stop the loop of this client
            }
        }
        msg[0] = '\0';
        comm.text[0] = '\0';
    }

    return (void *)a;
}

uint8_t available(bool* loopON, int num_users)
{
    int i;

    for(i=0; i<num_users; i++)
    {
        if(!loopON[i])
            return i;
    }

    // if all are false, there is no space for a new connection
    return 255;
}