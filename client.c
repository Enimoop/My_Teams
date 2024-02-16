#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include<arpa/inet.h>
#include <string.h>
#include <stdlib.h>


int main(int argc, char* argv[]){
    if(argc==3)
    {
        char* ip = argv[1];
        int port = atoi(argv[2]);
        char e = '\n';
        int socket_desc;
        char message[100];
        char buffer[1024];
        char connection[100];

        
       
            

        struct sockaddr_in server;

        socket_desc = socket(AF_INET, SOCK_STREAM, 0);

        if (socket_desc == -1) 
        {
            printf("Could not create socket");
        }

        server.sin_addr.s_addr = inet_addr(ip);
        server.sin_family = AF_INET;
        server.sin_port = htons(port);

        if (connect(socket_desc, (struct sockaddr * )&server, sizeof(server)) < 0)
        {
            puts("connect error");
            return 1;
        } else {
            ssize_t bytes_received = recv(socket_desc, connection, sizeof(connection), 0);
            printf("%s\n", connection);
        }

        ùputs("Connected");
        while(1) {
            
            printf("Enter the message: \n");
            scanf("%s", message);
            
            
            send_message(socket_desc, message);

            receive_message(socket_desc, buffer);
            

            
        }
        return 0;
    } else {
        printf("Erreur: nombre d'arguments incorrect\n");
    }
}

void send_message(int socket_desc, char* message) {
    if (send(socket_desc, message, strlen(message), 0) < 0)
    {
        puts("Send failed\n");
        return;
    }
}

void receive_message(int socket_desc, char* buffer) {
    memset(buffer, '\0', sizeof(buffer));
    ssize_t message_recu = recv(socket_desc, buffer, sizeof(buffer), 0);
    if (message_recu > 0)
    {
        printf("Message recu: ");
        printf("%s\n", buffer);
    }
}