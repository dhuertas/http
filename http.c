/*
 * A small, light HTTP server
 *
 * @author dhuertas
 * @email huertas.dani@gmail.com
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* integers */
#include <stdint.h>

/* connection */
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* time & date */
#include <sys/time.h>
#include <time.h>

/* threading */
#include <pthread.h>

/* error */
#include <errno.h>

/* file */
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

/* local header files */
#include "constants.h"
#include "config.h"
#include "mime.h"
#include "headers.h"
#include "request.h"
#include "response.h"

/*
 * GLOBALS
 */
pthread_cond_t cond_sockfd_full, cond_sockfd_empty;
pthread_mutex_t mutex_sockfd;

volatile int client_sockfd[MAX_THREADS];
volatile int client_sockfd_rd;
volatile int client_sockfd_wr;
volatile int client_sockfd_count;

/* Server config */
config_t conf;

void request_handler(int id, int client_sockfd) {

	char *connection;

	int i, n;

	fd_set select_set;
 
 	struct timeval timeout;

	request_t request;
	response_t response;

	connection = NULL;

 	i = 0;
	n = 0;

	/* Init request */
	request.num_headers = 0;
	memset(&(request._mask), 0, sizeof(uint32_t));

	/* Init response */
	response.num_headers = 0;
	memset(&(response._mask), 0, sizeof(uint32_t));
	response.status_code = 0;
	response.file_exists = FALSE;
	
	timeout.tv_sec = TIME_OUT;
	timeout.tv_usec = 0;

	if (conf.output_level >= DEBUG) {
		printf("DEBUG: thread %d handling request at socket %d\n", id, client_sockfd);
	}

	if (handle_request(client_sockfd, &request) < 0) {
		/* 
		 * There has been an error with the client request. Notify the client and
		 * close connection.
		 */
		set_response_status(&response, 500, "Internal Server Error");
		write_response_header(&response, "Connection", "Close");

		send_response(client_sockfd, &response);

		close(client_sockfd);

		free_request(&request);
		free_response(&response);

		return;
	}

	get_request_header(&request, "Connection", connection);

	if (connection == NULL) {
		/* Some http clients (e.g. curl) may not send the Connection header ... */
		handle_response(client_sockfd, &request, &response);

	} else if (strncasecmp(connection, "close", strlen("close")) == 0) {

		handle_response(client_sockfd, &request, &response);

	} else {

		if (conf.output_level >= DEBUG) {
			printf("DEBUG: Persistent connection. Connection will remain open for %d seconds\n", TIME_OUT);
		}

		/* Persistent connections are the default behavior in HTTP/1.1 */
		handle_response(client_sockfd, &request, &response);

		FD_ZERO(&select_set);

		FD_SET(client_sockfd, &select_set);

		while ((n = select(client_sockfd + 1, &select_set, NULL, NULL, &timeout)) > 0) {

			if (FD_ISSET(client_sockfd, &select_set)) {

				free_request(&request);
				free_response(&response);

				if (conf.output_level >= DEBUG) {
					printf("DEBUG: connection is still open\n");
				}

				handle_request(client_sockfd, &request);

				handle_response(client_sockfd, &request, &response);

			}

		}

		if (n < 0) {
			
			if (errno != EBADF){
				handle_error("select");
			} 
			
			if (conf.output_level >= DEBUG) {
				printf("DEBUG: client closed connection\n");
			}

		} else {
			
			if (conf.output_level >= DEBUG) {
				printf("DEBUG: connection timed out\n");
			}

		}

	}

	free_request(&request);
	free_response(&response);

	close(client_sockfd);

}

void *run(void *arg) {
	
	int *id, sockfd;

	id = (int *) arg;

	if (conf.output_level >= DEBUG) {
		printf("DEBUG: thread with local id %d running\n", *id);
	}

	while (1) {

		/* start of mutex area */

		pthread_mutex_lock(&mutex_sockfd);

		while (client_sockfd_count == 0) {

			pthread_cond_wait(&cond_sockfd_empty, &mutex_sockfd);

		}

		sockfd = client_sockfd[client_sockfd_rd];

		client_sockfd_rd++;
		client_sockfd_rd = client_sockfd_rd % MAX_THREADS;
		client_sockfd_count--;

		pthread_cond_broadcast(&cond_sockfd_full);
		pthread_mutex_unlock(&mutex_sockfd);
		
		/* end of mutex area */

		request_handler(*id, sockfd);

	}
}

int main(int argc, char *argv[]) {

	if (argc != 2) {
		printf("Usage: %s config_file\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	printf("Custom HTTP Server 0.1.2\n");

	read_config(argv[1]);

	char date_buffer[MAX_DATE_SIZE];

	int server_sockfd;
	int sockfd, client_size;
	
	int i, tid[MAX_THREADS]; // Local thread id (e.g. 0, 1, 2, 3 ... N)

	struct sockaddr_in server_addr, client_addr;
	pthread_t thread_id[MAX_THREADS];

	server_sockfd = socket(PF_INET, SOCK_STREAM, 0);

	/* Initialize addr structs */
	memset(&server_addr, 0, sizeof(server_addr));
	memset(&client_addr, 0, sizeof(client_addr));

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(conf.listen_port);

	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	bind(server_sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr));

	listen(server_sockfd, MAX_LISTEN);

	client_size = sizeof(client_addr);

	/* Initialize threads mutex and conditions */
	pthread_mutex_init(&mutex_sockfd, NULL);
	pthread_cond_init(&cond_sockfd_empty, NULL);
	pthread_cond_init(&cond_sockfd_full, NULL);

	client_sockfd_count = 0;
	client_sockfd_rd = 0;
	client_sockfd_wr = 0;

	/* Wake up threads */
	for (i = 0; i < MAX_THREADS; i++) {

		tid[i] = i;

		if (pthread_create(&thread_id[i], NULL, run, &tid[i]) != 0) {
			handle_error("pthread_create");
		}
 
	}

	while (1) {

		sockfd = accept(server_sockfd, (struct sockaddr *) &client_addr, &client_size);

		/* enter of mutex area */
		pthread_mutex_lock(&mutex_sockfd);

		while (client_sockfd_count > MAX_THREADS) {
			pthread_cond_wait(&cond_sockfd_full, &mutex_sockfd);
		}

		client_sockfd[client_sockfd_wr] = sockfd;

		client_sockfd_count++;
		client_sockfd_wr++;
		client_sockfd_wr = client_sockfd_wr % MAX_THREADS;

		
		pthread_cond_broadcast(&cond_sockfd_empty);
		pthread_mutex_unlock(&mutex_sockfd);

		/* exit mutex area */

		get_date(date_buffer, "%H:%M:%S, %a %b %d %Y");

		/* Request received. Lets do this! */
		printf("[%s] %s \n", date_buffer, inet_ntoa(client_addr.sin_addr));

	}

	for (i = 0; i < MAX_THREADS; i++) {
	    pthread_join(thread_id[i], NULL);
	}
	  
	pthread_cond_destroy(&cond_sockfd_empty);
	pthread_cond_destroy(&cond_sockfd_full);
	pthread_mutex_destroy(&mutex_sockfd);

	/* Closing time */
	close(server_sockfd);

	exit(EXIT_SUCCESS);

}