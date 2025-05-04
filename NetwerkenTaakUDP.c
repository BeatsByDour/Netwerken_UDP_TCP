#define _WIN32_WINNT 0x0601
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024
#define INITIAL_TIMEOUT 16.0
#define CONFIRMATION_TIMEOUT 8.0

typedef struct {
    struct sockaddr_storage addr;
    socklen_t addr_len;
    int guess;
    int valid;
} ClientGuess;

// het printen van addres van de client
void print_client_address(struct sockaddr_storage *addr) {
    char ipstr[INET6_ADDRSTRLEN];
    int port = 0;

    if (addr->ss_family == AF_INET) {
        struct sockaddr_in *s = (struct sockaddr_in *)addr;
        port = ntohs(s->sin_port);
        inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);
    } else {
        struct sockaddr_in6 *s = (struct sockaddr_in6 *)addr;
        port = ntohs(s->sin6_port);
        inet_ntop(AF_INET6, &s->sin6_addr, ipstr, sizeof ipstr);
    }

    printf("Client: %s:%d\n", ipstr, port);
}

int main() {

	// creating tha WSA data and starting the WSAdata, with an errorhandling for if it fails
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }

	// making use of the adressinformation hints and pointer res.
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;


	// check for correct addres information with error handling 
    if (getaddrinfo(NULL, "24042", &hints, &res) != 0) {
        fprintf(stderr, "getaddrinfo failed\n");
        WSACleanup();
        return 1;
    }

	// checks for the socket and checks if its created correctly 
    SOCKET sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock == INVALID_SOCKET) {
        fprintf(stderr, "socket creation failed\n");
        freeaddrinfo(res);
        WSACleanup();
        return 1;
    }


	// creates the bind and checks if there is no error  ( will fail if you have already a server open on said port )
    if (bind(sock, res->ai_addr, res->ai_addrlen) == SOCKET_ERROR) {
        fprintf(stderr, "bind failed\n");
        closesocket(sock);
        freeaddrinfo(res);
        WSACleanup();
        return 1;
    }

	// gives the address infromation and a print for the port that is used
    freeaddrinfo(res);
    printf("Server active on port 24042\n");
    srand((unsigned int)time(NULL));


	// while loop to check for the messages and the time out, the loop will be broken if there is an error, or if the player manually aborts the game
    while (1) {

		// make an array for the amount of clients that can guess with a maximum of 100( max_Clients )
		// these variables the check how many messages needs to be sent to the clients and if the server needs to generata a number. 
        ClientGuess clients[MAX_CLIENTS] = {0};
        int client_count = 0;
        int number = -1;  // No number generated yet
        double current_timeout = INITIAL_TIMEOUT;
        char buffer[BUFFER_SIZE];

        printf("Waiting for first guess to start the round...\n");

        // Phase 1: Wait for first guess before generating number
		// this while loop works till the first gues is made and will start a timer if the first guess is made or closes the server if abort is typed
        while (number == -1) {
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(sock, &readfds);

            struct timeval tv;
            tv.tv_sec = 1;
            tv.tv_usec = 0;

			//
            if (select(0, &readfds, NULL, NULL, &tv) > 0) {
                struct sockaddr_storage client_addr;
                socklen_t client_len = sizeof(client_addr);
                
                int bytes_received = recvfrom(sock, buffer, BUFFER_SIZE - 1, 0,
                                            (struct sockaddr*)&client_addr, &client_len);
                if (bytes_received == SOCKET_ERROR) {
                    continue;
                }

                buffer[bytes_received] = '\0';
					// closses server
                if (strcmp(buffer, "abort") == 0) {
                    printf("Abort received. Shutting down server.\n");
                    closesocket(sock);
                    WSACleanup();
                    return 0;
                }
					// checks that the guess is valid ( the guess needs to be within 1 and 100)
                int guess = atoi(buffer);
                if (guess < 1 || guess > 100) {
                    printf("Invalid guess received: %d\n", guess);
                    continue;
                }
					// stores the guess in the clients array struct with the addres, length of the guess and if the guess is valid
                // Store first client and generate number
                clients[client_count].addr = client_addr;
                clients[client_count].addr_len = client_len;
                clients[client_count].guess = guess;
                clients[client_count].valid = 1;
                client_count++;

				//generate the random number
                number = rand() % 100 + 1;
                printf("New round started! Target number: %d\n", number);
                print_client_address(&client_addr);
                printf("First guess received: %d\n", guess);
            }
        }

        time_t round_start = time(NULL);

        // Phase 2: Collect additional guesses with timeout
		// the while loop checks for additional guesses and will increase the timer for each guess the is made 
        while (difftime(time(NULL), round_start) < current_timeout) {
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(sock, &readfds);

            struct timeval tv;
            tv.tv_sec = 1;
            tv.tv_usec = 0;

            int select_result = select(0, &readfds, NULL, NULL, &tv);
            if (select_result == SOCKET_ERROR) {
                fprintf(stderr, "select error\n");
                break;
            }

            if (select_result > 0 && FD_ISSET(sock, &readfds)) {
                struct sockaddr_storage client_addr;
                socklen_t client_len = sizeof(client_addr);
                
                int bytes_received = recvfrom(sock, buffer, BUFFER_SIZE - 1, 0,
                                            (struct sockaddr*)&client_addr, &client_len);
                if (bytes_received == SOCKET_ERROR) {
                    continue;
                }

                buffer[bytes_received] = '\0';

                if (strcmp(buffer, "abort") == 0) {
                    printf("Abort received. Shutting down server.\n");
                    closesocket(sock);
                    WSACleanup();
                    return 0;
                }

                int guess = atoi(buffer);
                if (guess < 1 || guess > 100) {
                    continue;
                }

                // Check if client already exists
                int existing_client = -1;
                for (int i = 0; i < client_count; i++) {
                    if (memcmp(&client_addr, &clients[i].addr, sizeof(client_addr)) == 0) {
                        existing_client = i;
                        break;
                    }
                }

                if (existing_client >= 0) {
                    clients[existing_client].guess = guess;
                    clients[existing_client].valid = 1;
                    printf("Updated guess from existing client: %d\n", guess);
                } else if (client_count < MAX_CLIENTS) {
                    clients[client_count].addr = client_addr;
                    clients[client_count].addr_len = client_len;
                    clients[client_count].guess = guess;
                    clients[client_count].valid = 1;
                    client_count++;
                    printf("New guess from client: %d\n", guess);
                    print_client_address(&client_addr);
                }

                // Increase timeout by half of remaining time
                double remaining_time = current_timeout - difftime(time(NULL), round_start);
                current_timeout += remaining_time / 2.0;
                printf("Timeout extended to %.2f seconds\n", current_timeout);
            }
        }

        // Determine winner
        int winner_index = -1;
        int smallest_diff = 100;

		// this checks the client array for which guess was the closest to the random number
        for (int i = 0; i < client_count; i++) {
            if (clients[i].valid) {
                int diff = abs(clients[i].guess - number);
                if (diff < smallest_diff) {
                    smallest_diff = diff;
                    winner_index = i;
                }
            }
        }

		// checks to make sure that one of the guesses is a winning guess
        if (winner_index == -1) {
            printf("No valid guesses received\n");
            continue;
        }

        // Notify all clients
		// send a message to all client if the have won or lost 
		// the winning client will recieve a you won? and will recieve a you won! if the server doesnt recieve a new message from the client
        const char *win_msg = "You won?";
        const char *lose_msg = "You lost!";

        for (int i = 0; i < client_count; i++) {
            if (clients[i].valid) {
                const char *msg = (i == winner_index) ? win_msg : lose_msg;
                printf("Sending to client: %s\n", msg);
                sendto(sock, msg, strlen(msg), 0,
                      (struct sockaddr*)&clients[i].addr, clients[i].addr_len);
            }
        }

        // Winner confirmation phase
		// the server will wait for 8 seconds to confirm the client doesnt send another message
        int winner_responded = 0;
        time_t confirm_start = time(NULL);
        
        while (difftime(time(NULL), confirm_start) < CONFIRMATION_TIMEOUT) {
            fd_set confirmfds;
            FD_ZERO(&confirmfds);
            FD_SET(sock, &confirmfds);

            struct timeval tv;
            tv.tv_sec = 1;
            tv.tv_usec = 0;

            if (select(0, &confirmfds, NULL, NULL, &tv) > 0) {
                struct sockaddr_storage responder_addr;
                socklen_t responder_len = sizeof(responder_addr);
                
                int bytes = recvfrom(sock, buffer, BUFFER_SIZE - 1, 0,
                                   (struct sockaddr*)&responder_addr, &responder_len);
                if (bytes == SOCKET_ERROR) continue;

                buffer[bytes] = '\0';

                // Check if response came from the winner
                if (memcmp(&responder_addr, &clients[winner_index].addr, sizeof(responder_addr)) == 0) {
                    winner_responded = 1;
                    break;
                }
            }
        }

		// the server will send the last message to the players that won. 
        // Send final result to winner
        const char *final_msg = winner_responded ? "Lost!" : "Won!";
        printf("Sending to winner: %s\n", final_msg);
        sendto(sock, final_msg, strlen(final_msg), 0,
              (struct sockaddr*)&clients[winner_index].addr, clients[winner_index].addr_len);

        printf("Round completed. Winner's guess: %d (number was %d)\n\n", 
               clients[winner_index].guess, number);

			   // the server will reset and wait for the next guess of the players, the server will then generate a new number and the players can play again. 
			   // the server will only stop if the players send Abort in the request list
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}