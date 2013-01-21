/*
 * @author dhuertas
 * @email huertas.dani@gmail.com
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

/* local header files */
#include "constants.h"
#include "config.h"
#include "headers.h"
#include "request.h"

extern config_t conf;

/*
 * Reads the data from the socket and puts it in a buffer
 *
 * @param sockfd: socket file descriptor to read data from
 * @param data: buffer where readed data will be stored
 */
void receive_request(int sockfd, char **data) {

	char chunk[1];
	uint8_t i;
	uint32_t readed = 0, n;

	i = 0;

	/* Read byte by byte and look for the end of line */
	while ((n = read(sockfd, chunk, 1)) > 0) {

		if (readed + n > MAX_REQ) {
			/* max size per request has been reached! */
			handle_error("receive_request: max request size reached");
		}

		if (readed + n > (i + 1) * ALLOC_REQ) {

			i++;

			if (conf.output_level >= DEBUG) printf("DEBUG: reallocating...\n");

			if ((*data = realloc(*data, readed + ALLOC_REQ)) == NULL) {
				handle_error("receive_request: realloc");
			}

		}

		memcpy(*data + readed, chunk, n);
		readed += n;

		/* Read the end of line... backwards */
		if (readed >= 4 && strncmp(*data + readed-4, "\r\n\r\n", 4) == 0) {
			if (conf.output_level >= DEBUG) printf("DEBUG: end of request found\n");
			break;
		}

	}

}

/*
 * Iterate through the header_t content looking for the specified header name
 *
 * @param req: pointer to the request struct
 * @param name: the name of the header we are looking for
 * @param value: a pointer to access the header value when found
 * @return: the position of the header in the request struct, -1 otherwise
 */
uint16_t get_request_header(request_t *req, char *name, char *value) {

	int i;

	for (i = 0; i < req->num_headers; i++) {

		if (strncmp(req->headers[i]->name, name, strlen(name)) == 0) {
			value = req->headers[i]->value;
			return i;
		}

	}

	return -1;

}

/*
 * Handles the request process. It uses the receive_request function
 * and parses the received data filling a request_t data structure.
 *
 * @param sockfd: socket file descriptor to read data from the client
 * @param req: request_t data structure to store the parsed data
 */
void handle_request(int sockfd, request_t *req) {

	char *buffer = NULL;
	char *method = NULL;
	char *query = NULL;
	char *header_name = NULL;
	char *header_value = NULL;

	int start, end, pos, tmp;
	int string_length;

	uint8_t i;

	start = 0;
	end = 0;

	pos = 0;
	tmp = 0;

	string_length = 0;

	/* Initialize data structures */
	req->num_headers = 0;
	req->query_length = 0;

	/* allocate first 1024 bytes for the request */
	buffer = malloc(ALLOC_REQ);
	memset(buffer, 0, ALLOC_REQ);

	receive_request(sockfd, &buffer);

	if (conf.output_level >= VERBOSE) {
		printf("%s\n", buffer);
	}

	while (strncmp(&buffer[start], "\r\n", 2) != 0) {

		while (strncmp(&buffer[end], "\r\n", 2) != 0) end++;

		if (start == 0) {

			pos = 0;
			tmp = 0;

			/* allocate request. REMEMBER TO FREE request AFTER sending response */
			while (strncmp(&buffer[pos], " ", 1) != 0) pos++;
			pos++;
			method = malloc(pos);
			memcpy(method, buffer, pos);
			method[pos - 1] = '\0';
			tmp = pos;

			while (strncmp(&buffer[pos], " ", 1) != 0) pos++;
			pos++;
			req->uri = malloc(pos - tmp);

			memcpy(req->uri, &buffer[tmp], pos - tmp);
			req->uri[pos - tmp - 1] = '\0';
			tmp = pos;

			while (strncmp(&buffer[pos], "\r\n", 2) != 0) pos++;
			req->version = malloc(pos - tmp);
			memcpy(req->version, &buffer[tmp], pos - tmp);
			req->version[pos - tmp - 1] = '\0';

			for (i = 0; i < 7; i++) {
				if(strncmp(methods[i], method, strlen(method)) == 0) {
					req->method = i;
				}
			}

			free(method);

		} else {

			pos = 0;
			tmp = 0;

			/* reallocate header_t struct of request_t */
			if (req->num_headers == 0) {
				req->headers = malloc(sizeof(header_t *));
			} else {
				req->headers = realloc(req->headers, (req->num_headers + 1)*sizeof(header_t *));
			}

			req->headers[req->num_headers] = malloc(sizeof(header_t *));

			/* parse headers */
			while (strncmp(&buffer[start + pos], ":", 1) != 0) pos++;
			req->headers[req->num_headers]->name = malloc(pos++);
			tmp = pos;

			while (strncmp(&buffer[start + pos], "\r\n", 2) != 0) pos++;
			req->headers[req->num_headers]->value = malloc((pos++) - tmp);
			tmp = pos;

			sscanf(&buffer[start], "%[^:]: %[^\r\n]", 
				req->headers[req->num_headers]->name, 
				req->headers[req->num_headers]->value);

			req->num_headers++;

		}

		end += strlen("\r\n");
		start = end;

	}

	/* Look for the query part */
	if (strchr(req->uri, '?') != NULL) {

		/* We may receive an empty query (e.g "/somefile.ext?") */
		query = strchr(req->uri, '?');

		string_length = strlen(query);

		req->query = malloc(string_length + 1);
		memset(req->query, 0, string_length + 1);
		memcpy(req->query, query, string_length);
		req->query[string_length] = '\0';

		req->query_length = string_length;

		if (conf.output_level >= DEBUG) {
			printf("DEBUG: query %s\n", req->query);	
		}

	}

	/* Get the resource requested */
	if (req->query_length > 0) {
		string_length = strlen(req->uri) - req->query_length; // Strip query string from uri
	} else {
		string_length = strlen(req->uri);
	}

	req->resource = malloc(string_length + 1);
	memset(req->resource, 0, string_length + 1);
	strncpy(req->resource, req->uri, string_length);
	req->resource[string_length] = '\0';

	string_length = 0;

	/* free buffer */
	free(buffer);

	/* 
	 * TODO; Does the request have a message-body? Look for Transfer-Encoding 
	 * header. If there is a Transfer-Encoding header we are probably talking to 
	 * a HTML/1.0 client, otherwise the expected behavior for the client is to 
	 * send a "Content-Length: <length>" and "Expect: 100-continue" headers before 
	 * sending the message body.
	 */
	// if (get_request_header(req, "Content-Length", value) != -1) {
	//     receive_content(client_sockfd, &buffer);
	// }
}