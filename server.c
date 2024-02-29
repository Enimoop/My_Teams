#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#define PORT 4242 // le port de notre serveur

// Structure pour représenter chaque client
typedef struct {
    int socket_fd;
    char pseudo[256];
    char status[256];
} Client;

int server_socket;
int first_client_socket = -1; // Socket du premier client connecté

int create_server_socket(void);
void accept_new_connection(int listener_socket,fd_set *all_sockets,Client clients[], int *num_clients, int *fd_max);
void read_data_from_socket(int socket,fd_set *all_sockets, Client clients[], int *num_clients, int server_socket, int *fd_max);
void handle_server_interrupt(int signal);
void conversation_log(const char* message);
void handle_kick_command(char *username, int server_socket, fd_set *all_sockets, Client clients[], int *num_clients, int *fd_max);
void handle_status_command(char *status, int socket, Client clients[], int num_clients); // Nouvelle fonction pour gérer la commande /status
char* heure();
void notify_clients_on_connection(Client clients[], int num_clients, char* new_client_pseudo);
void notify_clients_on_disconnection(Client clients[], int num_clients, char* disconnected_client_pseudo);

int main(void)
{
    //int server_socket;
    int status;
    int i; // Pour notre bloucle de vérification des sockets
    fd_set all_sockets; // Ensemble de toutes les sockets du serveur
    fd_set read_fds; // Ensemble temporaire pour select()
    int fd_max; // Descripteur de la plus grande socket
    struct timeval timer;
    Client clients[FD_SETSIZE]; // Tableau pour stocker les informations de chaque client
    int num_clients = 0;

    // Initialisation du tableau des clients
    for (i = 0; i < FD_SETSIZE; i++) {
        clients[i].socket_fd = -1; // Initialise tous les descripteurs de fichiers à -1 (non connecté)
        memset(clients[i].pseudo, '\0', sizeof(clients[i].pseudo)); 
        memset(clients[i].status, '\0', sizeof(clients[i].status)); 
    }


    // Création de la socket du serveur
    server_socket = create_server_socket();
    if (server_socket == -1)
        exit(-1);
    // Écoute du port via la socket
    printf("[Server] Listening on port %d\n", PORT);
    status = listen(server_socket, 10);
    if (status != 0)
    {
        printf("[Server] Listen error: %s\n", strerror(errno));
        exit(-1);
    }

    // Préparation des ensembles de sockets pour select()
    FD_ZERO(&all_sockets);
    FD_ZERO(&read_fds);
    FD_SET(server_socket, &all_sockets); // Ajout de la socket principale à l'ensemble
    fd_max = server_socket; // Le descripteur le plus grand est forcément celui de notre seule socket
    printf("[Server] Set up select fd sets\n");

    while (1) // Boucle principale
    {
        // Copie l'ensemble des sockets puisque select() modifie l'ensemble surveillé
        read_fds = all_sockets;
        // Timeout de 2 secondes pour select()
        timer.tv_sec = 2;
        timer.tv_usec = 0;
        // Surveille les sockets prêtes à être lues
        status = select(fd_max + 1, &read_fds, NULL, NULL, &timer);
        if (status == -1)
        {
            printf("[Server] Select error: %s\n", strerror(errno));
            exit(-1);
        }
        else if (status == 0)
        {
            // Aucun descipteur de fichier de socket n'est prêt pour la lecture
            printf("[Server] Waiting...\n");
            continue;
        }
        // Boucle sur nos sockets
        i = 0;
        while (i <= fd_max)
        {
            if (FD_ISSET(i, &read_fds) != 1)
            {
                // Le fd i n'est pas une socket à surveiller
                // on s'arrête là et on continue la boucle
                i++;
                continue ;
            }
            printf("[%d] Ready for I/O operation\n", i);
            // La socket est prête à être lue !
            if (i == server_socket)
            // La socket est notre socket serveur qui écoute le port
                accept_new_connection(server_socket,&all_sockets, clients, &num_clients, &fd_max);
            else
            // La socket est une socket client, on va la lire
                read_data_from_socket(i,&all_sockets, clients, &num_clients, server_socket, &fd_max);
            i++;
        }
        signal(SIGINT, handle_server_interrupt);
    }
        return (0);
}
// Renvoie la socket du serveur liée à l'adresse et au port qu'on veut écouter


int create_server_socket(void)
{
    struct sockaddr_in sa;
    int socket_fd;
    int status;
    int enable_reuse = 1; // Définir l'option SO_REUSEADDR
    // Préparaton de l'adresse et du port pour la socket de notre serveur
    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET; // IPv4
    // sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1, localhost
    sa.sin_addr.s_addr = htonl(INADDR_ANY); // Toutes les interfaces
    sa.sin_port = htons(PORT);
    // Création de la socket
    socket_fd = socket(sa.sin_family, SOCK_STREAM, 0);
    if (socket_fd == -1)
    {
        printf("[Server] Socket error: %s\n", strerror(errno));
        return (-1);
    }
    printf("[Server] Created server socket fd: %d\n", socket_fd);
    printf("BUFFER SIZE: %d\n", BUFSIZ);

    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &enable_reuse, sizeof(enable_reuse)) < 0) {
        printf("[Server] setsockopt(SO_REUSEADDR) failed: %s\n", strerror(errno));
        close(socket_fd);
        return (-1);
    }

    // Liaison de la socket à l'adresse et au port
    status = bind(socket_fd, (struct sockaddr *)&sa, sizeof sa);
    if (status != 0)
    {
        printf("[Server] Bind error: %s\n", strerror(errno));
        return (-1);
    }
    printf("[Server] Bound socket to localhost port %d\n", PORT);
    return (socket_fd);
}


// Accepte une nouvelle connexion et ajoute la nouvelle socket à l'ensemble des sockets
void accept_new_connection(int server_socket,fd_set *all_sockets, Client clients[], int *num_clients, int *fd_max)
{
    int client_fd;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    char msg_to_send[BUFSIZ+256];
    int status;

    client_fd = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd == -1)
    {
        printf("[Server] Accept error: %s\n", strerror(errno));
        return;
    }

    FD_SET(client_fd, all_sockets); // Ajoute la socket client à l'ensemble

    if (client_fd > *fd_max)
        *fd_max = client_fd; // Met à jour la plus grande socket

    char pseudo[256];
    ssize_t bytes_received = recv(client_fd, pseudo, sizeof(pseudo), 0);
    if (bytes_received <= 0) {
        printf("[Server] Receive error for client %d: %s\n", client_fd, strerror(errno));
        close(client_fd);
        FD_CLR(client_fd, all_sockets);
        return;
    }
    



    // Ajoute le nouveau client à la liste des clients
    strncpy(clients[*num_clients].pseudo, pseudo, sizeof(clients[*num_clients].pseudo));
    clients[*num_clients].socket_fd = client_fd;
    (*num_clients)++;

    printf("[Server] Accepted new connection on client socket %d.\n", client_fd);

    if (first_client_socket == -1) {
        first_client_socket = client_fd; // Enregistrer le socket du premier client connecté
    }


    memset(&msg_to_send, '\0', sizeof msg_to_send);
    sprintf(msg_to_send, "Welcome %s. You are client fd [%d]\n",pseudo, client_fd);
    printf("msg_to_send: %s\n", msg_to_send);
    status = send(client_fd, msg_to_send, strlen(msg_to_send), 0);
    memset(&msg_to_send, '\0', sizeof msg_to_send);
    
    if (status == -1) {
        printf("[Server] Send error to client %d: %s\n", client_fd, strerror(errno));
    }

    memset(&msg_to_send, '\0', sizeof msg_to_send);

    if (*num_clients > 1) {
        sprintf(msg_to_send, "Connected clients:\n");
        for (int i = 0; i < *num_clients; i++) {
            if (strcmp(clients[i].pseudo, pseudo) != 0) { 
                strcat(msg_to_send, clients[i].pseudo);
                if (strlen(clients[i].status) > 0) {
                    strcat(msg_to_send, " (");
                    strcat(msg_to_send, clients[i].status);
                    strcat(msg_to_send, ")");
                }
                strcat(msg_to_send, "\n");
            }
        }
        strcat(msg_to_send, "\n");
    } else {
    sprintf(msg_to_send, "Connected clients:\nNone\n");
    }
    status = send(client_fd, msg_to_send, strlen(msg_to_send), 0);
    notify_clients_on_connection(clients, *num_clients, pseudo);

    if (status == -1)
        printf("[Server] Send error to client %d: %s\n", client_fd, strerror(errno));
    memset(&msg_to_send, '\0', sizeof msg_to_send);
    memset(&pseudo, '\0', sizeof pseudo);
}

// Lit le message d'une socket et relaie le message à toutes les autres
void read_data_from_socket(int socket,fd_set *all_sockets, Client clients[], int *num_clients, int server_socket, int *fd_max)
{
    char buffer[BUFSIZ];
    char msg_to_send[BUFSIZ+800];
    int bytes_read;
    int status;
    int i;

    memset(&buffer, '\0', sizeof buffer);
    bytes_read = recv(socket, buffer, BUFSIZ, 0);
    if (bytes_read <= 0)
    {
        if (bytes_read == 0){
            printf("[%d] Client socket closed connection.\n", socket);
            char pseudo[256];
            for (i = 0; i < *num_clients; i++)
            {
                if (clients[i].socket_fd == socket)
                {
                    strncpy(pseudo, clients[i].pseudo, sizeof(pseudo));
                    break;
                }
            }
            notify_clients_on_disconnection(clients, *num_clients, pseudo);
           
            
        } else {
            printf("[Server] Recv error: %s\n", strerror(errno));
        }
        close(socket); // Ferme la socket
        FD_CLR(socket, all_sockets); // Enlève la socket de l'ensemble




        // Enlève le client de la liste des clients
        for (i = 0; i < *num_clients; i++) {
            if (clients[i].socket_fd == socket) {
                for (int j = i; j < *num_clients - 1; j++) {
                    clients[j] = clients[j + 1];
                }
                break;
            }
        }
        (*num_clients)--;
        


    } else {
        printf("[%d] Got message: %s\n", socket, buffer);
        char pseudo[256];
        char pseudo_with_status[600];
       /*  if (strstr(buffer, "/info") != NULL) {
             

                
        } */
        // Relaie le message à toutes les autres sockets
        
        for (i = 0; i < *num_clients; i++) {
            if (clients[i].socket_fd == socket) {
                strncpy(pseudo, clients[i].pseudo, sizeof(pseudo));
                if (strlen(clients[i].status) > 0)
                {
                    snprintf(pseudo_with_status, sizeof(pseudo_with_status), "%s (%s)", pseudo, clients[i].status);
                }
                else
                {
                    strncpy(pseudo_with_status, pseudo, sizeof(pseudo_with_status));
                }
                break;
            }
        }
        
        if (strncmp(buffer, "/kick", 5) == 0 && socket == first_client_socket)
        {
            char *token = strtok(buffer, " ");
            if (token != NULL)
            {
                token = strtok(NULL, " ");
                if (token != NULL)
                {
                    handle_kick_command(token, server_socket, all_sockets, clients, num_clients, fd_max);
                    return;
                }
            }
            printf("[Server] Invalid /kick command format.\n");
        }
        else if (strncmp(buffer, "/status", 7) == 0)
        {
            char *token = strtok(buffer, " ");
            if (token != NULL)
            {
                token = strtok(NULL, " ");
                if (token != NULL)
                {
                    handle_status_command(token, socket, clients, *num_clients);
                    return;
                }
            }
            printf("[Server] Invalid /status command format.\n");
        }
        else
        {
            for (i = 0; i <= *num_clients - 1; i++)
            {
                printf("Client %d\n", i);
                if (clients[i].socket_fd != server_socket && clients[i].socket_fd != socket)
                {
                    printf("Sending message to client %s\n", clients[i].pseudo);
                    memset(&msg_to_send, '\0', sizeof msg_to_send);
                    sprintf(msg_to_send, "[%s]: %s", pseudo_with_status, buffer);
                    printf("msg_to_send: %s\n", msg_to_send);
                
                    status = send(clients[i].socket_fd, msg_to_send, strlen(msg_to_send), 0);

                    printf("status: %d\n", status);
                    if (status == -1)
                        printf("[Server] Send error to client %d: %s\n", i, strerror(errno));
                        
                }
                else if (clients[i].socket_fd == socket)
                {
                    memset(&msg_to_send, '\0', sizeof(msg_to_send));
                    sprintf(msg_to_send, "[%s] (Me): %s",pseudo_with_status, buffer);
                    printf("msg_to_send: %s\n", msg_to_send);
                    status = send(clients[i].socket_fd, msg_to_send, strlen(msg_to_send), 0);
                    if (status == -1)
                        printf("[Server] Send error to client %d: %s\n", i, strerror(errno));
                }
            }
            conversation_log(msg_to_send);
        }
        memset(&pseudo, '\0', sizeof pseudo);
        memset(&pseudo_with_status, '\0', sizeof pseudo_with_status);
        memset(&buffer, '\0', sizeof buffer);
        memset(&msg_to_send, '\0', sizeof msg_to_send);
    }
        
}

void handle_kick_command(char *username, int server_socket, fd_set *all_sockets, Client clients[], int *num_clients, int *fd_max)
{
    // Parcourir la liste des clients pour trouver celui à kicker
    int i;
    for (i = 0; i < *num_clients; i++) {
        if (strcmp(clients[i].pseudo, username) == 0) {
            // Fermer la socket du client à kicker
            close(clients[i].socket_fd);
            FD_CLR(clients[i].socket_fd, all_sockets); // Enlever la socket de l'ensemble

            // Supprimer le client de la liste des clients
            for (int j = i; j < *num_clients - 1; j++) {
                clients[j] = clients[j + 1];
            }
            (*num_clients)--;

            printf("[Server] User '%s' has been kicked.\n", username);

            
            char kick_msg[256];
            sprintf(kick_msg, "[Server] User '%s' has been kicked.\n", username);
            for (int k = 0; k < *num_clients; k++) {
                if (clients[k].socket_fd != server_socket) {
                    send(clients[k].socket_fd, kick_msg, strlen(kick_msg), 0);
                }
            }

            return;
        }
    }

    
    printf("[Server] User '%s' not found.\n", username);
}

void handle_status_command(char *status, int socket, Client clients[], int num_clients)
{
    char status_msg[BUFSIZ];
    int status_sent = 0;

    for (int i = 0; i < num_clients; i++) {
        if (clients[i].socket_fd == socket) {
            strncpy(clients[i].status, status, sizeof(clients[i].status)); // Définir le statut de l'utilisateur
            sprintf(status_msg,"[Server] User '%s' status set to '%s'.\n", clients[i].pseudo, status);
            for (int i = 0; i < num_clients; i++) {
                
                    int status = send(clients[i].socket_fd, status_msg, strlen(status_msg), 0);
                    if (status == -1) {
                        printf("[Server] Send error to client %d: %s\n", clients[i].socket_fd, strerror(errno));
                    } else {
                        status_sent = 1;
                    }
                
            }

            
            if (status_sent) {
                printf("[Server] Status change notification sent to all clients.\n");
            } else {
                printf("[Server] Failed to send status change notification to any clients.\n");
            }
            
            return;


        }
    }
    printf("[Server] User not found.\n");
}

void handle_server_interrupt(int signal)
{
    close(server_socket);
    exit(0);
} 

void conversation_log(const char* message)
{
    FILE *fichier;

    fichier = fopen("conversation.log", "at, ccs=UTF-8");

    if (fichier == NULL) {
        fprintf(stderr, "Peut pas ouvrir le fichier avec UTF-8\n");
        return;
    }

    fprintf(fichier,"[%s] %s\n", heure(), message);

    fclose(fichier);
}

char* heure()
{
    static char buffer[80];
    time_t timestamp = time(NULL);

    strftime(buffer, sizeof(buffer),"%X", localtime(&timestamp));
    return buffer;
}


void notify_clients_on_connection(Client clients[], int num_clients, char* new_client_pseudo)
{
    char msg_to_send[BUFSIZ];
    int status;

    memset(&msg_to_send, '\0', sizeof(msg_to_send));
    sprintf(msg_to_send, "[%s] s'est connecté.\n", new_client_pseudo);

    for (int i = 0; i < num_clients; i++) {
        if (clients[i].socket_fd != server_socket) {
            status = send(clients[i].socket_fd, msg_to_send, strlen(msg_to_send), 0);
            if (status == -1) {
                printf("[Server] Send error to client %d: %s\n", clients[i].socket_fd, strerror(errno));
            }
        }
    }
}

void notify_clients_on_disconnection(Client clients[], int num_clients, char* disconnected_client_pseudo)
{
    char msg_to_send[BUFSIZ];
    int status;

    memset(&msg_to_send, '\0', sizeof(msg_to_send));
    sprintf(msg_to_send, "[%s] s'est déconnecté.\n", disconnected_client_pseudo);

    for (int i = 0; i < num_clients; i++) {
        if (strcmp(clients[i].pseudo, disconnected_client_pseudo) != 0) { 
            status = send(clients[i].socket_fd, msg_to_send, strlen(msg_to_send), 0);
            if (status == -1) {
                printf("[Server] Send error to client %d: %s\n", clients[i].socket_fd, strerror(errno));
            }
        }
    }
}