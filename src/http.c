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

volatile int *client_sockfd;
volatile int client_sockfd_rd;
volatile int client_sockfd_wr;
volatile int client_sockfd_count;

/* Server config */
config_t conf;

static const char *name = "ARiSO HTTP web server";
static const char *version = "0.1.3";
static const char *author = "Dani Huertas";

void close_conn(int thread_id, int sockfd) {

	if (close(sockfd) < 0) {

		debug(conf.output_level, 
			"[%d] DEBUG: error closing socket (%s)\n", 
			thread_id, strerror(errno));

	}
 
}

void request_handler(int thread_id, int client_sockfd) {

	char *connection;

	int i, n, req_count;

	fd_set select_set;
 
 	struct timeval timeout;

	request_t request;
	response_t response;

	connection = NULL;

 	i = 0;
	n = 0;
	req_count = 0;

	/* Init request */
	request.num_headers = 0;
	memset(&(request._mask), 0, sizeof(uint32_t));

	/* Init response */
	response.num_headers = 0;
	memset(&(response._mask), 0, sizeof(uint32_t));
	response.status_code = 0;
	response.file_exists = FALSE;
	
	timeout.tv_sec = conf.request_timeout;
	timeout.tv_usec = 0;

	debug(conf.output_level, 
		"[%d] DEBUG: handling request at socket %d\n", 
		thread_id, client_sockfd);

	/* Wait for data to be received or connection timeout */
	FD_ZERO(&select_set);
	FD_SET(client_sockfd, &select_set);

	n = select(client_sockfd + 1, &select_set, NULL, NULL, &timeout);

	if (n < 0) {

		if (errno != EBADF) {
			handle_error("select");
		} 

		close_conn(thread_id, client_sockfd);

		debug(conf.output_level, 
			"[%d] DEBUG: client closed connection\n",
			thread_id);

		free_request(&request);
		free_response(&response);

		return;

	} else if (n == 0) {
		
		debug(conf.output_level, 
			"[%d] DEBUG: connection timed out\n",
			thread_id);

		free_request(&request);
		free_response(&response);

		return;

	} else {

		if (FD_ISSET(client_sockfd, &select_set)) {

			if (handle_request(thread_id, client_sockfd, &request) < 0) {
				/* 
				 * There has been an error with the client request. Close connection.
				 */
				close_conn(thread_id, client_sockfd);

				free_request(&request);
				free_response(&response);

				return;

			}

		}

	}

	get_request_header(&request, "Connection", &connection);

	if (connection == NULL) {
		/* Some http clients (e.g. curl) may not send the Connection header ... */
		handle_response(thread_id, client_sockfd, &request, &response);

	} else if (strncasecmp(connection, "close", strlen("close")) == 0) {

		handle_response(thread_id, client_sockfd, &request, &response);

	} else {

		debug(conf.output_level, 
			"[%d] DEBUG: Connection keep alive (%d seconds)\n", 
			thread_id, TIME_OUT);

		timeout.tv_sec = conf.keep_alive_timeout;
		timeout.tv_usec = 0;

		/* Persistent connections are the default behavior in HTTP/1.1 */
		handle_response(thread_id, client_sockfd, &request, &response);

		while ((n = select(client_sockfd + 1, &select_set, NULL, NULL, &timeout)) > 0) {

			if (FD_ISSET(client_sockfd, &select_set)) {

				free_request(&request);
				free_response(&response);

				debug(conf.output_level, 
					"[%d] DEBUG: connection is still open\n", 
					thread_id);

				handle_request(thread_id, client_sockfd, &request);
				handle_response(thread_id, client_sockfd, &request, &response);

				req_count++;

			}

			if (req_count > conf.max_keep_alive_requests) {

				debug(conf.output_level, 
					"[%d] DEBUG: Max keep alive requests reached\n", 
					thread_id);

				break;

			}
		}

	}

	if (n < 0) {

		if (errno != EBADF) {
			handle_error("select");
		}

		debug(conf.output_level, 
			"[%d] DEBUG: client closed connection\n",
			thread_id);

	} else if (n == 0) {

		debug(conf.output_level, 
			"[%d] DEBUG: connection timed out\n",
			thread_id);

	}

	close_conn(thread_id, client_sockfd);

	FD_CLR(client_sockfd, &select_set);
	FD_ZERO(&select_set);

	free_request(&request);
	free_response(&response);

}

void *run(void *arg) {
	
	int *id, sockfd;

	id = (int *) arg;

	debug(conf.output_level, "[%d] DEBUG: thread with local id %d running\n", *id, *id);

	while (1) {

		// start of mutex area

		pthread_mutex_lock(&mutex_sockfd);

		while (client_sockfd_count == 0) {

			pthread_cond_wait(&cond_sockfd_empty, &mutex_sockfd);

		}

		sockfd = client_sockfd[client_sockfd_rd];

		client_sockfd_rd++;
		client_sockfd_rd = client_sockfd_rd % conf.thread_pool_size;
		client_sockfd_count--;

		pthread_cond_broadcast(&cond_sockfd_full);
		pthread_mutex_unlock(&mutex_sockfd);
		
		// end of mutex area

		request_handler(*id, sockfd);

	}

}

int main(int argc, char *argv[]) {

	char *cvalue = NULL;
	char *ovalue = NULL;

	int aflag = 0;
	int bflag = 0;
	int index;
	int c;
     
	opterr = 0;

	while ((c = getopt (argc, argv, "c:o:")) != -1) {

		switch (c) {
			// Comment this and leave it for future use
			/*
			case 'a':

				aflag = 1;
				break;

			case 'b':

				bflag = 1;
				break;
			*/
			case 'c':
				// Config option
				cvalue = optarg;
				break;

			case 'o':
				// Output file option
				ovalue = optarg;
				break;

			case '?':

				if (optopt == 'c' || optopt == 'o') {
					fprintf (stderr, "Option -%c requires an argument\n", optopt);
				} else if (isprint(optopt)) {
					fprintf (stderr, "Unknown option `-%c'\n", optopt);
				} else {
					fprintf (stderr, "Unknown option character `\\x%x'\n", optopt);
				}

				
				exit(0);

			default: break;

		}
	}

	for (index = optind; index < argc; index++) {
		printf ("Non-option argument %s\n", argv[index]);
	}

	if (cvalue == NULL) {
		printf("Usage: %s -c config_file [-o output_file]\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	if (ovalue != NULL) {
		close(1);
		if (open(ovalue, O_WRONLY|O_CREAT|O_APPEND, 0644) < 0) {
			handle_error("open");
		}
	}

	printf("%s (version %s)\n", name, version);

	read_config(cvalue);

	char date_buffer[MAX_DATE_SIZE];

	int server_sockfd;
	int sockfd, client_size;

	// Local thread id (e.g. 0, 1, 2, 3 ... N)
	int i, tid[conf.thread_pool_size];

	struct sockaddr_in server_addr, client_addr;
	pthread_t thread_id[conf.thread_pool_size];

	// Initialize the client_sockfd array
	client_sockfd = malloc(conf.thread_pool_size*sizeof(int));
	
	server_sockfd = socket(PF_INET, SOCK_STREAM, 0);

	// Initialize addr structs
	memset(&server_addr, 0, sizeof(server_addr));
	memset(&client_addr, 0, sizeof(client_addr));

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(conf.listen_port);

	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	bind(server_sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr));

	listen(server_sockfd, MAX_LISTEN);

	client_size = sizeof(client_addr);

	// Initialize threads mutex and conditions
	pthread_mutex_init(&mutex_sockfd, NULL);
	pthread_cond_init(&cond_sockfd_empty, NULL);
	pthread_cond_init(&cond_sockfd_full, NULL);

	client_sockfd_count = 0;
	client_sockfd_rd = 0;
	client_sockfd_wr = 0;

	// Wake up threads
	for (i = 0; i < conf.thread_pool_size; i++) {

		tid[i] = i;

		if (pthread_create(&thread_id[i], NULL, run, &tid[i]) != 0) {
			handle_error("pthread_create");
		}
 
	}

	while (1) {

		sockfd = accept(server_sockfd, (struct sockaddr *) &client_addr, &client_size);

		// start of mutex area
		pthread_mutex_lock(&mutex_sockfd);

		while (client_sockfd_count > conf.thread_pool_size) {
			pthread_cond_wait(&cond_sockfd_full, &mutex_sockfd);
		}

		client_sockfd[client_sockfd_wr] = sockfd;

		client_sockfd_count++;
		client_sockfd_wr++;
		client_sockfd_wr = client_sockfd_wr % conf.thread_pool_size;

		
		pthread_cond_broadcast(&cond_sockfd_empty);
		pthread_mutex_unlock(&mutex_sockfd);

		// end of mutex area

		get_date(date_buffer, "%H:%M:%S, %a %b %d %Y");

		// Request received :)
		printf("[%s] %s \n", date_buffer, inet_ntoa(client_addr.sin_addr));

	}

	for (i = 0; i < conf.thread_pool_size; i++) {
	    pthread_join(thread_id[i], NULL);
	}

	pthread_cond_destroy(&cond_sockfd_empty);
	pthread_cond_destroy(&cond_sockfd_full);
	pthread_mutex_destroy(&mutex_sockfd);

	// Closing time
	close(server_sockfd);

	// free(client_sockfd);

	exit(EXIT_SUCCESS);

}
