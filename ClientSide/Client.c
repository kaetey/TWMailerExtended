#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

///////////////////////////////////////////////////////////////////////////////

#define BUF 1024

///////////////////////////////////////////////////////////////////////////////

int PORT = 0;

///////////////////////////////////////////////////////////////////////////////

void loginUser();
void sendMessage(int create_socket);
void listMessage(int create_socket);
void readMessage(int create_socket);
void delMessage(int create_socket);
void getLines(int maxlen, char* prompt, char* target);
void getInput(int maxlen, char* prompt, char* target);

///////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv)
{

    PORT = atoi(argv[2]);
    int create_socket;
    char buffer[BUF];
    struct sockaddr_in address;
    int size;
    int isQuit;

    ////////////////////////////////////////////////////////////////////////////
    // CREATE A SOCKET
    // IPv4, TCP (connection oriented), IP (same as server)
    if ((create_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("Socket error");
        return EXIT_FAILURE;
    }

    ////////////////////////////////////////////////////////////////////////////
    // INIT ADDRESS
    // Attention: network byte order => big endian
    memset(&address, 0, sizeof(address)); // init storage with 0
    address.sin_family = AF_INET;         // IPv4
    address.sin_port = htons(PORT);
    if (argc < 2)
    {
        inet_aton("127.0.0.1", &address.sin_addr);
    }
    else
    {
        inet_aton(argv[1], &address.sin_addr);
    }

    ////////////////////////////////////////////////////////////////////////////
    // CREATE A CONNECTION
    if (connect(create_socket,
        (struct sockaddr*)&address,
        sizeof(address)) == -1)
    {
        perror("Connect error - no server available");
        return EXIT_FAILURE;
    }

    // ignore return value of printf
    printf("Connection with server (%s) established\n",
        inet_ntoa(address.sin_addr));

    ////////////////////////////////////////////////////////////////////////////
    // RECEIVE DATA
    size = recv(create_socket, buffer, BUF - 1, 0);
    if (size == -1)
    {
        perror("recv error");
    }
    else if (size == 0)
    {
        printf("Server closed remote socket\n"); // ignore error
    }
    else
    {
        buffer[size] = '\0';
        printf("%s", buffer); // ignore error
    }

	printf("LOGIN: authenticate user.\n"
        //"SEND: send a message from client to server.\n"
	    //"LIST: list all messages from a specific user.\n"
		//"READ: read a specific message from a user.\n"
		//"DEL: delete a specific message from a user.\n"
		"QUIT: disconnect.\n");

    do
    c
        printf("Enter command: ");
        if (fgets(buffer, BUF, stdin) != NULL)
        {
            int size = strlen(buffer);
            // remove new-line signs from string at the end
            if (buffer[size - 2] == '\r' && buffer[size - 1] == '\n')
            {
                size -= 2;
                buffer[size] = 0;
            }
            else if (buffer[size - 1] == '\n')
            {
                --size;
                buffer[size] = 0;
            }
            isQuit = strcmp(buffer, "quit") == 0;

            //////////////////////////////////////////////////////////////////////
            // SEND DATA
            // send will fail if connection is closed, but does not set
            // the error of send, but still the count of bytes sent
            
            if(strcmp(buffer, "LOGIN") == 0){
                loginUser(create_socket);
            } else if (strcmp(buffer, "SEND") == 0) {
                sendMessage(create_socket);
            } else if (strcmp(buffer, "LIST") == 0) {
                listMessage(create_socket);
            } else if (strcmp(buffer, "READ") == 0) {
                readMessage(create_socket);
            } else if (strcmp(buffer, "DEL") == 0) {
                delMessage(create_socket);
            } else if (strcmp(buffer, "QUIT") == 0) {
                //isQuit = 1;
                close(create_socket);
                exit(EXIT_SUCCESS);
            } else {
                printf("Please enter a valid command:\nSEND--LIST--READ--DEL--QUIT\n");
                continue;
            }

            //////////////////////////////////////////////////////////////////////
            size = recv(create_socket, buffer, BUF - 1, 0);
            if (size == -1)
            {
                perror("recv error");
                break;
            }
            else if (size == 0)
            {
                printf("Server closed remote socket\n"); // ignore error
                break;
            }
            else
            {
                buffer[size] = '\0';
                printf("<< %s\n", buffer); // ignore error
                buffer[0] = '\0';
            }
        }
    } while (!isQuit);

    ////////////////////////////////////////////////////////////////////////////
    // CLOSES THE DESCRIPTOR
    if (create_socket != -1)
    {
        if (shutdown(create_socket, SHUT_RDWR) == -1)
        {
            // invalid in case the server is gone already
            perror("shutdown create_socket");
        }
        if (close(create_socket) == -1)
        {
            perror("close create_socket");
        }
        create_socket = -1;
    }

    return EXIT_SUCCESS;
}

void loginUser(int create_socket){
   char* message = calloc(sizeof(char)*BUF);
   char* username = calloc(sizeof(char)*9);
   char* password = calloc(sizeof(char)*BUF);

   getInput(9, "Username: ", username);
   getInput(BUF, "Password: ", password);

   strcat(message, "LOGIN\n");
   strcat(message, username);
   strcat(message, "\n");
   strcat(message, password);
   strcat(message, "\n\0");

   if(send(create_socket, message, strlen(message), 0) == -1) {
      perror("LOGIN failed to send");
      exit(EXIT_FAILURE);
   } 
   free(username);
   free(password);
   free(message);
}

void sendMessage(int create_socket) {
    char* message = calloc(BUF, sizeof(char));
    char* sender = calloc(9, sizeof(char));
    char* receiver = calloc(9, sizeof(char));
    char* subject = calloc(81, sizeof(char));
    char* content = calloc(BUF, sizeof(char));

    getInput(9, "Enter the username of the sender: ", sender);
    getInput(9, "Enter the username of the receiver: ", receiver);
    getInput(81, "Enter the subject of the message: ", subject);
    getLines(BUF, "Enter the content of the message: ", content);

    strcat(message, "SEND\n");
    strcat(message, sender);
    strcat(message, "\n");
    strcat(message, receiver);
    strcat(message, "\n");
    strcat(message, subject);
    strcat(message, "\n");
    strcat(message, content);
    strcat(message, "\n\0");

    printf("%s\n", message);
    if (send(create_socket, message, strlen(message), 0) == -1) {
        perror("SEND failed to send");
        exit(EXIT_FAILURE);
    }
    free(sender);
    free(receiver);
    free(subject);
    free(content);
    free(message);
}

void listMessage(int create_socket) {
    char* message = calloc(19, sizeof(char));
    char* username = calloc(9, sizeof(char));

    getInput(9, "From which user do you want to list all messages? Enter the username: ", username);

    strcat(message, "LIST\n");
    strcat(message, username);
    strcat(message, "\n");

    if (send(create_socket, message, strlen(message), 0) == -1) {
        perror("LIST failed to send");
        exit(EXIT_FAILURE);
    }

    free(username);
    free(message);

}

void readMessage(int create_socket) {
    char* message = calloc(19, sizeof(char));
    char* username = calloc(9, sizeof(char));
    char* msgNumber = calloc(4, sizeof(char));

    getInput(9, "From which user do you want to read a message? Enter the username: ", username);
    getInput(4, "Which Message do you want to read? Enter the message number: ", msgNumber);

    strcat(message, "READ\n");
    strcat(message, username);
    strcat(message, "\n");
    strcat(message, msgNumber);
    strcat(message, "\n");

    if (send(create_socket, message, strlen(message), 0) == -1) {
        perror("READ failed to send");
        exit(EXIT_FAILURE);
    }
    free(username);
    free(msgNumber);
    free(message);
}

void delMessage(int create_socket) {
    char* message = calloc(19, sizeof(char));
    char* username = calloc(9, sizeof(char));
    char* msgNumber = calloc(4, sizeof(char));

    getInput(9, "From which user do you want to delete a message? Enter the username: ", username);
    getInput(4, "Which Message do you want to delete? Enter the message number: ", msgNumber);

    strcat(message, "DEL\n");
    strcat(message, username);
    strcat(message, "\n");
    strcat(message, msgNumber);
    strcat(message, "\n");

    if (send(create_socket, message, strlen(message), 0) == -1) {
        perror("DEL failed to send");
        exit(EXIT_FAILURE);
    }
    free(username);
    free(msgNumber);
    free(message);

}

void getLines(int maxlen, char* prompt, char* target) {
    char* c = malloc(sizeof(char));
    int i = 0;
    printf("%s", prompt);
    do {
        *c = getchar();
        strcat(target, c);
        i++;
    } while (*c != '.' && i < maxlen);
    free(c);
}

void getInput(int maxlen, char* prompt, char* target) {

    printf("%s", prompt);

    if (fgets(target, maxlen, stdin) != NULL) {
        int size = strlen(target);
        if (target[size - 2] == '\r' && target[size - 1] == '\n') {
            size -= 2;
            target[size] = 0;
        }
        else if (target[size - 1] == '\n') {
            --size;
            target[size] = 0;
        }
    }
    else {
        exit(EXIT_FAILURE);
    }
}

