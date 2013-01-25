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
int receive_request(int sockfd, char **data) {

	char chunk[1];
	uint8_t i;
	uint32_t readed = 0, n;

	i = 0;

	/* Read byte by byte and look for the end of line */
	while ((n = read(sockfd, chunk, 1)) > 0) {

		if (readed + n > _REQUEST_MAX_SIZE) {
			/* max size per request has been reached! */
			if (conf.output_level >= DEBUG) {
				printf("DEBUG: max request size reached (%d bytes)\n", readed + n);
			}

			return ERROR;
		}

		if (readed + n > (i + 1) * _REQUEST_ALLOC_SIZE) {

			i++;

			if (conf.output_level >= DEBUG) printf("DEBUG: reallocating...\n");

			if ((*data = realloc(*data, readed + _REQUEST_ALLOC_SIZE)) == NULL) {
				handle_error("realloc");
			}

		}

		memcpy(*data + readed, chunk, n);
		readed += n;

		/* Read the end of line... backwards */
		if (readed >= 4 && strncmp(*data + readed - 4, "\r\n\r\n", 4) == 0) {
			if (conf.output_level >= DEBUG) printf("DEBUG: end of request found\n");
			break;
		}

	}

	return readed;

}

/*
 * Reads the data from the socket and puts it in a buffer
 *
 * @param sockfd: socket file descriptor to read data from
 * @param data: buffer where readed data will be stored
 */
int receive_message_body(int sockfd, char **data) {
	
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
int handle_request(int sockfd, request_t *req) {

	char *buffer = NULL;
	char *method = NULL;
	char *query = NULL;

	int start, end, pos, tmp;
	int string_length;

	uint8_t i;

	start = 0;
	end = 0;

	pos = 0;
	tmp = 0;

	string_length = 0;

	/* allocate first 1024 bytes for the request */
	buffer = malloc(_REQUEST_ALLOC_SIZE);
	memset(buffer, 0, _REQUEST_ALLOC_SIZE);

	if (receive_request(sockfd, &buffer) < 0) {
		/* There has been an error receiving the client request :( */
		free(buffer);
		free_request(req);

		return ERROR;

	}

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

			method = malloc(pos + 1);
			memset(method, 0, pos + 1);
			strncat(method, buffer, pos);

			for (i = 0; i < 7; i++) {
				if(strncmp(methods[i], method, strlen(method)) == 0) {
					req->method = i;
				}
			}

			paranoid_free_string(method);

			pos++;
			
			tmp = pos;

			while (strncmp(&buffer[pos], " ", 1) != 0) pos++;
			
			req->uri = malloc(pos + 1 - tmp);
			memset(req->uri, 0, pos + 1 - tmp);
			strncat(req->uri, &buffer[tmp], pos - tmp);
			
			req->_mask |= _REQUEST_URI;

			pos++;
			
			tmp = pos;

			while (strncmp(&buffer[pos], "\r\n", 2) != 0) pos++;

			req->version = malloc(pos + 1 - tmp);
			memset(req->version, 0, pos + 1 - tmp);
			strncat(req->version, &buffer[tmp], pos - tmp);

			req->_mask |= _REQUEST_VERSION;

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

		end += 2; // strlen("\r\n");
		start = end;

	}

	/* Look for the query part */
	if (strchr(req->uri, '?') != NULL) {

		query = strchr(req->uri, '?');

		string_length = strlen(query);

		req->query = malloc(string_length + 1);
		memset(req->query, 0, string_length + 1);
		strncat(req->query, query, string_length);

		req->_mask |= _REQUEST_QUERY;

		if (conf.output_level >= DEBUG) {
			printf("DEBUG: query %s\n", req->query);	
		}

	}

	/* Get the resource requested */
	if (req->_mask & _REQUEST_QUERY) {
		string_length = strlen(req->uri) - strlen(req->query); // Strip query string from uri
	} else {
		string_length = strlen(req->uri);
	}

	req->resource = malloc(string_length + 1);
	memset(req->resource, 0, string_length + 1);
	strncat(req->resource, req->uri, string_length);

	req->_mask |= _REQUEST_RESOURCE;

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
	
	//if (get_request_header(req, "Content-Length", value) != -1) {
		//receive_message_body(client_sockfd, &buffer);
	//}
	
	return 0;
	
}

/*
 * Free the allocated memory for the request struct
 *
 * @param req: pointer to a request_t struct
 */
void free_request(request_t *req) {

	int i;

	/* free request headers */
	if (req->num_headers > 0) {

		for (i = 0; i < req->num_headers; i++) {

			paranoid_free_string(req->headers[i]->name);
			paranoid_free_string(req->headers[i]->value);
			free(req->headers[i]);

		}
		
		free(req->headers);

	}

	if (req->_mask & _REQUEST_MESSAGE) paranoid_free_string(req->message_body);
	req->_mask &= ~_REQUEST_MESSAGE;

	if (req->_mask & _REQUEST_QUERY) paranoid_free_string(req->query);
	req->_mask &= ~_REQUEST_QUERY;

	if (req->_mask & _REQUEST_RESOURCE) paranoid_free_string(req->resource);
	req->_mask &= ~_REQUEST_RESOURCE;

	if (req->_mask & _REQUEST_VERSION) paranoid_free_string(req->version);
	req->_mask &= ~_REQUEST_VERSION;

	if (req->_mask & _REQUEST_URI) paranoid_free_string(req->uri);
	req->_mask &= ~_REQUEST_URI;

}