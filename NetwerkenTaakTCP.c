// gcc NetwerkenTaakTCP.c -lws2_32 -o NetwerkenTaakTCP.exe
// _WIN32_WINNT version constants --> https://stackoverflow.com/questions/15370033/how-to-use-inet-pton-with-the-mingw-compiler
// #define _WIN32_WINNT_WINTHRESHOLD           0x0A00 // Windows 10
// #define _WIN32_WINNT_WIN10                  0x0A00 // Windows 10
//
#define _WIN32_WINNT 0x0601

#include <winsock2.h>  // windows netwerking 
#include <ws2tcpip.h> 

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

void print_ip_address( unsigned short family, struct sockaddr * ip )
{
	void * ip_address;
	char * ip_version;
	char ip_string[INET6_ADDRSTRLEN];

	if( family == AF_INET )
	{ // IPv4
		struct sockaddr_in *ipv4 = (struct sockaddr_in *)ip;
		ip_address = &(ipv4->sin_addr);
		ip_version = "IPv4";
	}
	else
	{ // IPv6
		struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)ip;
		ip_address = &(ipv6->sin6_addr);
		ip_version = "IPv6";
	}

	inet_ntop( family, ip_address, ip_string, sizeof ip_string );
	printf( "%s -> %s\n", ip_version, ip_string );
}

void ai_print_ip_address( struct addrinfo * ip )
{
	print_ip_address( ip->ai_family, ip->ai_addr );
}

void ss_print_ip_address( struct sockaddr_storage * ip )
{
	print_ip_address( ip->ss_family, (struct sockaddr*) ip );
}

int main( int argc, char * argv[] )
{
	srand(time(NULL));
	uint32_t RandomNumber = rand() % 2147483647; 

	int closing = 1;
	WSADATA wsaData; //WSAData wsaData; //Could be different case
	if( WSAStartup( MAKEWORD(2,0), &wsaData ) != 0 ) // MAKEWORD(1,1) for Winsock 1.1, MAKEWORD(2,0) for Winsock 2.0:
	{
		fprintf( stderr, "WSAStartup failed.\n" );
		exit( 1 );
	}

	struct addrinfo internet_address_setup, *result_head, *result_item;
	memset( &internet_address_setup, 0, sizeof internet_address_setup );
	internet_address_setup.ai_family = AF_UNSPEC; // AF_INET or AF_INET6 to force version
	internet_address_setup.ai_socktype = SOCK_STREAM;
	internet_address_setup.ai_flags = AI_PASSIVE; // use ANY address for IPv4 and IPv6

	int getaddrinfo_return;
	getaddrinfo_return = getaddrinfo( NULL, "24042", &internet_address_setup, &result_head );
	if( getaddrinfo_return != 0 )
	{
		fprintf( stderr, "getaddrinfo: %s\n", gai_strerror( getaddrinfo_return ) );
		exit( 2 );
	}

	int internet_socket;

	result_item = result_head; //take first of the linked list
	while( result_item != NULL ) //while the pointer is valid
	{
		internet_socket = socket( result_item->ai_family, result_item->ai_socktype, result_item->ai_protocol );
		if( internet_socket == -1 )
		{
			perror( "socket" );
		}
		else
		{
			int return_code;
			return_code = bind( internet_socket, result_item->ai_addr, result_item->ai_addrlen );
			if( return_code == -1 )
			{
				close( internet_socket );
				perror( "bind" );
			}
			else
			{
				int listen_return;
				listen_return = listen( internet_socket, 1 );
				if( listen_return == -1 )
				{
					close( internet_socket );
					perror( "listen" );
				}
				else
				{
					printf( "listen on " );
					ai_print_ip_address( result_item );
					break; //stop running through the linked list
				}
			}
		}
		result_item = result_item->ai_next; //take next in the linked list
	}
	if( result_item == NULL )
	{
		fprintf( stderr, "socket: no valid socket address found\n" );
		exit( 5 );
	}
	freeaddrinfo( result_head ); //free the linked list

	struct sockaddr_storage client_ip_address;
	socklen_t client_ip_address_length = sizeof client_ip_address;
	int client_socket;
	client_socket = accept( internet_socket, (struct sockaddr *) &client_ip_address, &client_ip_address_length );
	if( client_socket == -1 )
	{
		printf( "errno = %d\n", WSAGetLastError() ); //https://docs.microsoft.com/en-us/windows/win32/winsock/windows-sockets-error-codes-2
		close( internet_socket );
		perror( "accept" );
		exit( 6 );
	}
	close( internet_socket );
	printf( "Got connection from " );
	ss_print_ip_address( &client_ip_address );

	uint32_t number_of_bytes_received = 0;
	char buffer[1000];
	uint32_t net_order;
	uint32_t host_order;
	printf("RandomNummer: %i \n" , RandomNumber);
	printf("PreWhile\n");
	while (closing) {
        printf("InWhile\n");

        // Leeg buffer voor veiligheid
        memset(buffer, 0, sizeof(buffer));

        number_of_bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
		printf("number_of_bytes_received: %i \n",number_of_bytes_received );
        if (number_of_bytes_received == -1) {
            printf("errno = %d\n", WSAGetLastError());
            perror("recv");
            closing = 0;
            break;
        }

		// als er geen bytes worden gestuurd sluit de connectie 

        if (number_of_bytes_received == 0) {
            printf("Client closed connection\n");
            closing = 0;
            break;
        }

		// er meer dat 4 bytes worden gestuurd dan sluit de connectie ook ( niet direct nodig maar had ik gebruikt voor error handling )
        if (number_of_bytes_received != 4) {
            printf("Ongeldige invoer: verwacht 4 bytes, kreeg %d.\n", number_of_bytes_received);
            closing = 0;
            break;
        }
		// Zet netwerk byte order om naar host order  
 		memcpy(&net_order, buffer, sizeof(uint32_t)); // buffer word gekopieerd in de net order daarna worden word de netorder omgezet naar een integer om zo te kunnen vergelijken met de randomnummer
 		host_order = ntohl(net_order);
  		printf("Ontvangen gok: %d\n", host_order);

        // Vergelijking
        if (RandomNumber > host_order) {
			printf("groter\n");
            send(client_socket, "Groter", 8, 0);
        } else if (RandomNumber < host_order) {
			printf("kleiner\n");
            send(client_socket, "Kleiner", 7, 0);
        } else {
			printf("correct\n");
            send(client_socket, "Correct", 8, 0);
            closing = 0;
        }
	}

	// shutdown code van de Les  shutdown functie sluit de socket en om de memory te clearen met wsaCleanup 
	int shutdown_return;
	shutdown_return = shutdown( client_socket, SD_RECEIVE ); //Shutdown Send == SD_SEND ; Receive == SD_RECEIVE ; Send/Receive == SD_BOTH ; https://blog.netherlabs.nl/articles/2009/01/18/the-ultimate-so_linger-page-or-why-is-my-tcp-not-reliable --> Linux : Shutdown Send == SHUT_WR ; Receive == SHUT_RD ; Send/Receive == SHUT_RDWR
	if( shutdown_return == -1 )
	{
		perror( "shutdown" );
	}

	close( client_socket );

	WSACleanup();

	return 0;
}