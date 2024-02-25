#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>
#include <curses.h>

#define MESSAGE_WINDOW_HEIGHT 20
#define MESSAGE_WINDOW_WIDTH 80
#define INPUT_WINDOW_HEIGHT 3
#define INPUT_WINDOW_WIDTH 80
#define HEADER_WINDOW_HEIGHT 1
#define HEADER_WINDOW_WIDTH 80

void init_curses() {
    initscr(); // Initialise l'environnement curses
    cbreak(); // Permet la saisie en mode ligne
    //noecho(); // Désactive l'écho des caractères saisis
    keypad(stdscr, TRUE); // Permet l'utilisation des touches spéciales
}

void end_curses() {
    endwin(); // Termine l'environnement curses
}

void send_message(int socket_desc, char* message) {
    if (send(socket_desc, message, strlen(message), 0) < 0) {
        printf("Send failed\n");
    }
}

void receive_message(int socket_desc, WINDOW *message_win) {
    char buffer[1024];
    memset(buffer, '\0', sizeof(buffer));
    ssize_t message_recu = recv(socket_desc, buffer, sizeof(buffer), 0);
    if (message_recu <= 0) {
        printf("Receive failed\n");

        if (message_recu == 0) {
            close(socket_desc);
            end_curses();
            printf("Server disconnected\n");
            
            exit(1);
        }
    } else {
        
        wprintw(message_win, "%s\n", buffer);
        wrefresh(message_win);
    }
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        printf("Erreur: nombre d'arguments incorrect\n");
        return 1;
    }

    char* ip = argv[1];
    int port = atoi(argv[2]);
    int socket_desc;
    char message[1024];
    char connection[1024];
    char* pseudo = argv[3];
    WINDOW *message_win, *input_win, *header_win;
    
    fd_set read_fds;
    struct timeval timeout;

    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_desc == -1) {
        printf("Could not create socket");
        return 1;
    }

    struct sockaddr_in server;
    server.sin_addr.s_addr = inet_addr(ip);
    server.sin_family = AF_INET;
    server.sin_port = htons(port);

    init_curses();

    // Création des fenêtres
    message_win = newwin(MESSAGE_WINDOW_HEIGHT, MESSAGE_WINDOW_WIDTH, 0, 0);
    header_win = newwin(HEADER_WINDOW_HEIGHT, HEADER_WINDOW_WIDTH, MESSAGE_WINDOW_HEIGHT, 0);
    input_win = newwin(INPUT_WINDOW_HEIGHT, INPUT_WINDOW_WIDTH, MESSAGE_WINDOW_HEIGHT + HEADER_WINDOW_HEIGHT, 0);

    if (connect(socket_desc, (struct sockaddr*)&server, sizeof(server)) < 0) {
        printf("connect error\n");
        return 1;
    } else {
        int status = send(socket_desc, pseudo, strlen(pseudo), 0);
        if (status < 0) {
            printf("Send failed\n");
        }
        ssize_t bytes_received = recv(socket_desc, connection, sizeof(connection), 0);
        wprintw(header_win, "%s\n", "Entrez votre message:");
        wrefresh(header_win);
        wprintw(message_win, "%s\n", connection);
        wrefresh(message_win);
        memset(connection, '\0', sizeof(connection));
        
    }

    

    

    

    scrollok(message_win, TRUE); // Active le défilement automatique dans la fenêtre des messages

    
    while(1) {
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds); 
        FD_SET(socket_desc, &read_fds); 

        timeout.tv_sec = 2;
        timeout.tv_usec = 0;

        int activity = select(socket_desc + 1, &read_fds, NULL, NULL, &timeout);
        if (activity < 0) {
            printf("Select error\n");
            return 1;
        }

        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            
            werase(input_win);
            mvwgetstr(input_win, 1, 1, message);
            send_message(socket_desc, message);
            werase(input_win);
            wrefresh(input_win);
        }

        if (FD_ISSET(socket_desc, &read_fds)) {
            
            receive_message(socket_desc, message_win);
        }
    }

    end_curses();
    return 0;
}
