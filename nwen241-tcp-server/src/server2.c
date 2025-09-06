// server2.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#define BUFFER_SIZE 1024  // fixed buffer size for network operations

//function Prototypes
int setup_server_socket(int port);
void handle_client(int client_fd);
void str_to_upper(char *str);
void process_get(int client_fd, const char *filename);
void process_put(int client_fd, const char *filename);
void send_response(int client_fd, const char *message);
void reap_zombies(int sig);  //cleans up child processes

// main function
int main(int argc, char *argv[]) {
    // validate the command line arguments and display port 
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return -1;
    }

    int port = atoi(argv[1]);
    // check if the port number in argv is >= 1024 otherwise 
    //show error msg 
    if (port < 1024) {
        fprintf(stderr, "Port number must be >= 1024\n");
        return -1;
    }

    signal(SIGCHLD, reap_zombies);  //set up zombie process reaper

    //set up the server socket
    int server_fd = setup_server_socket(port);
    if (server_fd < 0) return -1;

    printf("[*] Server listening on port %d...\n", port);

    //main server loop for client connections
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        
        // accept any incoming client connections
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd < 0) {
            perror("accept");
            continue;  // continue running even if a client failes connection 
        }

        pid_t pid = fork();

        if (pid < 0) {
            //for when the fork fails 
            send_response(client_fd, "HELLO\n");
            close(client_fd);
        } else if (pid == 0) {
            // child process
            close(server_fd);
            send_response(client_fd, "HELLO\n");  // send welcome message
            handle_client(client_fd);  // handle our client communication
            close(client_fd);  // clean up client connection after disconection
            exit(0);
        } else {
            // parent process
            close(client_fd);
        }
    }

    close(server_fd);
    return 0;
}

// sets up the server socket 
int setup_server_socket(int port) {
    // create a tcp socket
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        return -1;
    }

    // allows the socket port to be reused immediately
    int opt = 1;
    setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // configure the server address
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;  // ipv4
    server_addr.sin_port = htons(port);  // convert port to network byte using htons
    server_addr.sin_addr.s_addr = INADDR_ANY;  // accept server connections on any device 

    // bind socket to the address
    if (bind(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(sock_fd);
        return -1;
    }

    // start listening for connections
    if (listen(sock_fd, 10) < 0) {  // increased backlog to 10 vs 1 on server.c
        perror("listen");
        close(sock_fd);
        return -1;
    }

    return sock_fd;
}

//handles communication with a connected client
void handle_client(int client_fd) {
    char buffer[BUFFER_SIZE];

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);  // clears the buffer
        ssize_t bytes_read = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_read <= 0) break;  // client disconnected or some other error 

        //this parses commands and filenames by handling line endings 
        char *command = strtok(buffer, " \r\n");
        char *filename = strtok(NULL, " \r\n");

        if (!command) continue;  //empty command check

        // converts commands to uppercase for comparison
        char upper_cmd[BUFFER_SIZE];
        strncpy(upper_cmd, command, BUFFER_SIZE);
        str_to_upper(upper_cmd);

        // process different commands
        if (strcmp(upper_cmd, "BYE") == 0) {
            break;  // end session with the client
        } else if (strcmp(upper_cmd, "GET") == 0) {
            if (!filename) {
                send_response(client_fd, "SERVER 500 Get Error\n");
            } else {
                process_get(client_fd, filename);  // handles file downloads 
            }
        } else if (strcmp(upper_cmd, "PUT") == 0) {
            if (!filename) {
                send_response(client_fd, "SERVER 501 Put Error\n");
            } else {
                process_put(client_fd, filename);  //handles file uploads
            }
        } else {
            send_response(client_fd, "SERVER 502 Command Error\n");  // any unknown commands show this msg
        }
    }
}

//helper function to convert a string to uppercase
void str_to_upper(char *str) {
    while (*str) {
        *str = toupper((unsigned char)*str);
        str++;
    }
}

// handles file downloads
void process_get(int client_fd, const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        send_response(client_fd, "SERVER 404 Not Found\n");
        return;
    }

    send_response(client_fd, "SERVER 200 OK\n\n");  // good response msg 

    //read file line by line and sends to our client 
    char line[BUFFER_SIZE];
    while (fgets(line, sizeof(line), fp)) {
        send(client_fd, line, strlen(line), 0);
    }

    send(client_fd, "\n\n", 2, 0);  //end of transmission 
    fclose(fp);
}

//handles file upload (the PUT command)
void process_put(int client_fd, const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        send_response(client_fd, "SERVER 501 Put Error\n");
        return;
    }

    char buffer[BUFFER_SIZE];
    int newline_count = 0;  //counts newlines to detect end of file

    //read until we get 2 consecutive newlines (detects end of writing)
    while (newline_count < 2) {
        ssize_t bytes = recv(client_fd, buffer, sizeof(buffer), 0);
        if (bytes <= 0) break;  //client disconnected or some other random error

        // process each byte in the buffer 
        for (int i = 0; i < bytes && newline_count < 2; i++) {
            if (buffer[i] == '\n') {
                newline_count++;  // increment on newline
            } else {
                newline_count = 0;  // reset on any other character thats not a \n
            }
            fputc(buffer[i], fp);  // write the characters to file
        }
    }

    fclose(fp);
    send_response(client_fd, "SERVER 201 Created\n");  // happy response
}

//sends a response to the client 
void send_response(int client_fd, const char *message) {
    size_t total_sent = 0;
    size_t length = strlen(message);

    //ensure entire message is sent 
    while (total_sent < length) {
        ssize_t sent = send(client_fd, message + total_sent, length - total_sent, 0);
        if (sent <= 0) break;  //random error or client has disconnected
        total_sent += sent;
    }
}

// clean up child processes
void reap_zombies(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}