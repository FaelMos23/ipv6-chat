// C++ program to illustrate the client application in the
// socket programming
#include <cstring>
#include <iostream>
#include <netinet/in.h> // sockaddr_in, htons, htonl
#include <sys/socket.h> // socket, connect, bind, listen, accept
#include <arpa/inet.h>  // inet_pton, inet_ntop
#include <unistd.h>     // close

int main()
{
    // creating socket
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);

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
    char buffer[1024] = { 0 };
    recv(clientSocket, buffer, sizeof(buffer), 0);
    std::cout << buffer << std::endl;

    // UP TO HERE, THE CODE IS AS INTENDED

    // sending data
    std::string message;
    getline(std::cin, message);
    send(clientSocket, message.c_str(), message.size(), 0);

    // closing socket
    close(clientSocket);

    return 0;
}