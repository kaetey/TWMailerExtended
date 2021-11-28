#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <dirent.h>
#include <fcntl.h>

///////////////////////////////////////////////////////////////////////////////

#define BUF 1024

///////////////////////////////////////////////////////////////////////////////

int PORT = 0;
int abortRequested = 0;
int create_socket = -1;
int new_socket = -1;
char* DIRECTORY;

///////////////////////////////////////////////////////////////////////////////

void *clientCommunication(void *data);
void signalHandler(int sig);

void mailHandler(int* current_socket, char buffer[]);
void sendMessage(int* current_socket, char buffer[]);
void listMessage(int* current_socket, char buffer[]);
void readMessage(int* current_socket, char buffer[]);
void delMessage(int* current_socket, char buffer[]);

///////////////////////////////////////////////////////////////////////////////

int main(int argc, char **argv)
{
   PORT = atoi(argv[1]);
   DIRECTORY = argv[2];
   socklen_t addrlen;
   struct sockaddr_in address, cliaddress;
   int reuseValue = 1;
   
   struct stat st = {0};
   if (stat(DIRECTORY, &st) == -1) {
      mkdir(DIRECTORY, 0777);
   }

   ////////////////////////////////////////////////////////////////////////////
   // SIGNAL HANDLER
   // SIGINT (Interrup: ctrl+c)
   if (signal(SIGINT, signalHandler) == SIG_ERR)
   {
      perror("signal can not be registered");
      return EXIT_FAILURE;
   }

   ////////////////////////////////////////////////////////////////////////////
   // CREATE A SOCKET
   // IPv4, TCP (connection oriented), IP (same as client)
   if ((create_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
   {
      perror("Socket error"); // errno set by socket()
      return EXIT_FAILURE;
   }

   ////////////////////////////////////////////////////////////////////////////
   // SET SOCKET OPTIONS
   // socket, level, optname, optvalue, optlen
   if (setsockopt(create_socket,
                  SOL_SOCKET,
                  SO_REUSEADDR,
                  &reuseValue,
                  sizeof(reuseValue)) == -1)
   {
      perror("set socket options - reuseAddr");
      return EXIT_FAILURE;
   }

   if (setsockopt(create_socket,
                  SOL_SOCKET,
                  SO_REUSEPORT,
                  &reuseValue,
                  sizeof(reuseValue)) == -1)
   {
      perror("set socket options - reusePort");
      return EXIT_FAILURE;
   }

   ////////////////////////////////////////////////////////////////////////////
   // INIT ADDRESS
   // Attention: network byte order => big endian
   memset(&address, 0, sizeof(address));
   address.sin_family = AF_INET;
   address.sin_addr.s_addr = INADDR_ANY;
   address.sin_port = htons(PORT);

   ////////////////////////////////////////////////////////////////////////////
   // ASSIGN AN ADDRESS WITH PORT TO SOCKET
   if (bind(create_socket, (struct sockaddr *)&address, sizeof(address)) == -1)
   {
      perror("bind error");
      return EXIT_FAILURE;
   }

   ////////////////////////////////////////////////////////////////////////////
   // ALLOW CONNECTION ESTABLISHING
   // Socket, Backlog (= count of waiting connections allowed)
   if (listen(create_socket, 5) == -1)
   {
      perror("listen error");
      return EXIT_FAILURE;
   }

   while (!abortRequested)
   {
      /////////////////////////////////////////////////////////////////////////
      // ignore errors here... because only information message
      printf("Waiting for connections...\n");

      /////////////////////////////////////////////////////////////////////////
      // ACCEPTS CONNECTION SETUP
      // blocking, might have an accept-error on ctrl+c
      addrlen = sizeof(struct sockaddr_in);
      if ((new_socket = accept(create_socket,
                               (struct sockaddr *)&cliaddress,
                               &addrlen)) == -1)
      {
         if (abortRequested)
         {
            perror("accept error after aborted");
         }
         else
         {
            perror("accept error");
         }
         break;
      }

      /////////////////////////////////////////////////////////////////////////
      // START CLIENT
      // ignore printf error handling
      printf("Client connected from %s:%d...\n",
             inet_ntoa(cliaddress.sin_addr),
             ntohs(cliaddress.sin_port));
      clientCommunication(&new_socket); // returnValue can be ignored
      new_socket = -1;
   }

   // frees the descriptor
   if (create_socket != -1)
   {
      if (shutdown(create_socket, SHUT_RDWR) == -1)
      {
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

void *clientCommunication(void *data)
{
   char buffer[BUF];
   int size;
   int *current_socket = (int *)data;

   ////////////////////////////////////////////////////////////////////////////
   // SEND welcome message
   strcpy(buffer, "Welcome to the server!\r\nPlease enter your commands...\r\n");
   if (send(*current_socket, buffer, strlen(buffer), 0) == -1)
   {
      perror("send failed");
      return NULL;
   }

   do
   {
      /////////////////////////////////////////////////////////////////////////
      // RECEIVE
      size = recv(*current_socket, buffer, BUF - 1, 0);
      if (size == -1)
      {
         if (abortRequested)
         {
            perror("recv error after aborted");
         }
         else
         {
            perror("recv error");
         }
         break;
      }

      if (size == 0)
      {
         printf("Client closed remote socket\n"); // ignore error
         break;
      }

      // remove ugly debug message, because of the sent newline of client
      if (buffer[size - 2] == '\r' && buffer[size - 1] == '\n')
      {
         size -= 2;
      }
      else if (buffer[size - 1] == '\n')
      {
         --size;
      }

      buffer[size] = '\0';
      printf("Message received: %s\n", buffer); // ignore error
      
      mailHandler(current_socket, buffer);
      
   } while (strcmp(buffer, "quit") != 0 && !abortRequested);

   // closes/frees the descriptor if not already
   if (*current_socket != -1)
   {
      if (shutdown(*current_socket, SHUT_RDWR) == -1)
      {
         perror("shutdown new_socket");
      }
      if (close(*current_socket) == -1)
      {
         perror("close new_socket");
      }
      *current_socket = -1;
   }

   return NULL;
}

void signalHandler(int sig)
{
   if (sig == SIGINT)
   {
      printf("abort Requested... "); // ignore error
      abortRequested = 1;
      /////////////////////////////////////////////////////////////////////////
      // With shutdown() one can initiate normal TCP close sequence ignoring
      // the reference count.
      if (new_socket != -1)
      {
         if (shutdown(new_socket, SHUT_RDWR) == -1)
         {
            perror("shutdown new_socket");
         }
         if (close(new_socket) == -1)
         {
            perror("close new_socket");
         }
         new_socket = -1;
      }

      if (create_socket != -1)
      {
         if (shutdown(create_socket, SHUT_RDWR) == -1)
         {
            perror("shutdown create_socket");
         }
         if (close(create_socket) == -1)
         {
            perror("close create_socket");
         }
         create_socket = -1;
      }
   }
   else
   {
      exit(sig);
   }
}

////////////////////////////////////////////////////////////////////////////
// HANDLES CLIENT MESSAGES
void mailHandler(int* current_socket, char buffer[]) {
   char* bufcpy = malloc(sizeof(char)*BUF);
   strcpy(bufcpy, buffer);
   char* pch = strtok(bufcpy, "\n");
   
   //printf("%s\n", pch);
   
   if(strcmp(pch, "SEND") == 0){
      sendMessage(current_socket, buffer);
   } else if (strcmp(pch, "LIST") == 0) {
      listMessage(current_socket, buffer);
   } else if (strcmp(pch, "READ") == 0) {
      readMessage(current_socket, buffer);
   } else if (strcmp(pch, "DEL") == 0) {
      delMessage(current_socket, buffer);
   } else {
      perror("command unknown");
   }
   
   free(bufcpy);

}


void sendMessage(int* current_socket, char buffer[]) {
   
   //char* sender = strtok(NULL, "\n");
   //char* receiver = strtok(NULL, "\n");
   
  
   char senderDir[BUF] = "./";
   strcat(senderDir, DIRECTORY);
   strcat(senderDir, "/");
   strcat(senderDir, strtok(NULL, "\n"));
   
   char receiverDir[BUF] = "./";
   strcat(receiverDir, DIRECTORY);
   strcat(receiverDir, "/");
   strcat(receiverDir, strtok(NULL, "\n"));
   
   char* subject = strtok(NULL, "\n");
   
   char filepath[BUF];
   
   struct stat st = {0};
   
   int file;
   
   
   if (stat(senderDir, &st) == -1) {
      mkdir(senderDir, 0777);
   }
   
   strcat(filepath, senderDir);
   strcat(filepath, "/");
   strcat(filepath, subject);
   strcat(filepath, ".txt");
   
   file = open(filepath, O_RDWR | O_CREAT, S_IRUSR | S_IRGRP | S_IROTH);
   write(file, buffer, strlen(buffer));
   
   if (stat(receiverDir, &st) == -1) {
      mkdir(receiverDir, 0777);
   }
   
   strcpy(filepath, receiverDir);
   strcat(filepath, "/");
   strcat(filepath, subject);
   strcat(filepath, ".txt");
   
   file = open(filepath, O_RDWR | O_CREAT, S_IRUSR | S_IRGRP | S_IROTH);
   write(file, buffer, strlen(buffer));
   
   if (send(*current_socket, "OK", 3, 0) == -1)
      {
         perror("send answer failed");
      }
   
}
void listMessage(int* current_socket, char buffer[]) {
   char* message = malloc(sizeof(char)*BUF);
   char temp[BUF];
   char * path = malloc(sizeof(char)*BUF);
   DIR* dirp;
   struct dirent *entry;
   int fileCount = 0;
   char fileCountString[3];
   
   strcat(path, DIRECTORY);
   strcat(path, "/");
   strcat(path, strtok(NULL, "\n"));
   
   dirp = opendir(path);
   while ((entry = readdir(dirp)) != NULL) {
      if (entry->d_type == DT_REG) {
         strcat(temp, entry->d_name);
         strcat(temp, "\n");
         fileCount++;
      }
   }
   closedir(dirp);
   
   sprintf(fileCountString, "%d", fileCount);
   
   strcat(message, fileCountString);
   strcat(message, "\n");
   strcat(message, temp);
   
   if (send(*current_socket, message, strlen(message), 0) == -1) {
      perror("send answer failed");
   }
   
   free(message);
   free(path);
}
void readMessage(int* current_socket, char buffer[]) {
   char* message = malloc(sizeof(char)*BUF);
   char * path = malloc(sizeof(char)*BUF);
   DIR* dirp;
   struct dirent *entry;
   int file;
   int filesize;
   int fileCount = 1;
   int targetFile = 0;
   
   struct stat st;
   
   strcat(path, DIRECTORY);
   strcat(path, "/");
   strcat(path, strtok(NULL, "\n"));
   
   targetFile = atoi(strtok(NULL, "\n"));
   
   dirp = opendir(path);
   while ((entry = readdir(dirp)) != NULL) {
      if (entry->d_type == DT_REG) {
         if(fileCount == targetFile) {
            strcat(path, "/");
            strcat(path, entry->d_name);
            file = open(path, O_RDONLY);
            stat(path, &st);
            filesize = st.st_size;
            read(file, message, filesize);
         }
         fileCount++;
      }
   }
   closedir(dirp);
   
   if (send(*current_socket, message, strlen(message), 0) == -1) {
      perror("send answer failed");
   }
   
   free(message);
   free(path);
}
void delMessage(int* current_socket, char buffer[]) {
   char * path = malloc(sizeof(char)*BUF);
   DIR* dirp;
   struct dirent *entry;
   int fileCount = 1;
   int targetFile = 0;
   
   strcat(path, DIRECTORY);
   strcat(path, "/");
   strcat(path, strtok(NULL, "\n"));
   
   targetFile = atoi(strtok(NULL, "\n"));
   
   dirp = opendir(path);
   while ((entry = readdir(dirp)) != NULL) {
      if (entry->d_type == DT_REG) {
         if(fileCount == targetFile) {
            strcat(path, "/");
            strcat(path, entry->d_name);
            remove(path);
         }
         fileCount++;
      }
   }
   closedir(dirp);
   
   if (send(*current_socket, "OK", 3, 0) == -1) {
      perror("send answer failed");
   }
   free(path);
}