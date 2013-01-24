/*
 * @author dhuertas
 * @email huertas.dani@gmail.com
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

/* local header files */
#include "constants.h"
#include "mime.h"
#include "config.h"
#include "headers.h"
#include "request.h"
#include "response.h"
#include "util.h"

extern config_t conf;

void set_response_status(response_t *resp, int status_code, char *reason_phrase) {

	if (resp->status_code > 0) {
		resp->reason_phrase = realloc(resp->reason_phrase, strlen(reason_phrase) + 1);
	} else {
		resp->reason_phrase = malloc(strlen(reason_phrase) + 1);
		memset(resp->reason_phrase, 0, strlen(reason_phrase) + 1);
	}

	resp->status_code = status_code;

	strncat(resp->reason_phrase, reason_phrase, strlen(reason_phrase));

}

/*
 * Set the response header. If the header name already exists, the function 
 * appends the content of "value" to the existing one using a semicolon as 
 * separator (e.g. "; ")
 *
 * @param resp: a pointer to a response_t struct
 * @param name: a valid HTML header name (e.g. "Content-Type")
 * @param value: the header content
 */
void write_response_header(response_t *resp, char *name, char *value) {

	uint8_t found;
	uint16_t i;

	int new_length;

	found = FALSE;
			
	for (i = 0; i < resp->num_headers; i++) {

		if (strncmp(resp->headers[i]->name, name, strlen(name)) == 0) {

			/* header already exist */
			paranoid_free_string(resp->headers[i]->value);
			
			new_length = strlen(value);
			resp->headers[i]->value = malloc(new_length + 1);
			memset(resp->headers[i]->value, 0, new_length + 1);
			strncat(resp->headers[i]->value, value, strlen(value));

			found = TRUE;
			break;
		}

	}

	if ( ! found) {

		if (resp->num_headers == 0) {

			resp->headers = malloc(sizeof(header_t *));

		} else {

			resp->headers = realloc(resp->headers, (resp->num_headers+1)*sizeof(header_t *));	

		}

		resp->headers[resp->num_headers] = malloc(sizeof(header_t *));

		resp->headers[resp->num_headers]->name = malloc(strlen(name) + 1);
		resp->headers[resp->num_headers]->value = malloc(strlen(value) + 1);
		memset(resp->headers[resp->num_headers]->name, 0, strlen(name) + 1);
		memset(resp->headers[resp->num_headers]->value, 0, strlen(value) + 1);

		strncat(resp->headers[resp->num_headers]->name, name, strlen(name));
		strncat(resp->headers[resp->num_headers]->value, value, strlen(value));

		resp->num_headers++;

	}

}

void append_response_header(response_t *resp, char *name, char *value) {

	uint16_t i;

	int new_length;

	for (i = 0; i < resp->num_headers; i++) {
		
		if (strncmp(resp->headers[i]->name, name, strlen(name)) == 0) {

			/* header already exist, append it */
			new_length = strlen(resp->headers[i]->value) + strlen("; ") + strlen(value);

			resp->headers[i]->value = realloc(resp->headers[i]->value, new_length + 1);

			strncat(resp->headers[i]->value, "; ", 2);
			strncat(resp->headers[i]->value, value, strlen(value));

		}

	}

}

void send_response(int sockfd, response_t *resp) {

	char *buffer;
	char status_code[4];

	int i, w, length;

	sprintf(status_code, "%d", resp->status_code);

	length = strlen(conf.http_version) 
		+ 1 + strlen(status_code) 
		+ 1 + strlen(resp->reason_phrase) 
		+ 3;

	buffer = malloc(length);
	memset(buffer, 0, length);

	strncpy(buffer, conf.http_version, strlen(conf.http_version));
	strncat(buffer, " ", 1);

	strncat(buffer, status_code, strlen(status_code));
	strncat(buffer, " ", 1);
	strncat(buffer, resp->reason_phrase, strlen(resp->reason_phrase));

	strncat(buffer, "\r\n", 2);

	for (i = 0; i < resp->num_headers; i++) {

		length = strlen(resp->headers[i]->name)+strlen(": ")+strlen(resp->headers[i]->value) + 2;		

		buffer = realloc(buffer, strlen(buffer) + length + 1);

		strncat(buffer, resp->headers[i]->name, strlen(resp->headers[i]->name));
		strncat(buffer, ": ", 2);
		strncat(buffer, resp->headers[i]->value, strlen(resp->headers[i]->value));

		strncat(buffer, "\r\n", 2);

	}

	if ((w = send(sockfd, buffer, strlen(buffer), 0)) != strlen(buffer)) {
		if (errno == EBADF) {
			if (conf.output_level >= DEBUG) {
				printf("DEBUG: client closed connection");
			}
		} else {
			handle_error("send");
		}
	}

	if (conf.output_level >= VERBOSE) printf("%s\n", buffer);

	if ((w = send(sockfd, "\r\n", strlen("\r\n"), 0)) != strlen("\r\n")) {
		if (errno == EBADF) {
			if (conf.output_level >= DEBUG) {
				printf("DEBUG: client closed connection");
			}
		} else {
			handle_error("send");
		}
	}

	paranoid_free_string(buffer);

	switch (resp->status_code) {

		case 200: /* OK */
		case 304: /* Not Modified */

			send_file(sockfd, resp->file_path);

			break;

		case 404: /* Not found */
			/* TODO custom 404 files :) */
			break;

		default:
		case 500: /* Server error */
			break;

	}

	close(sockfd);

}

void handle_response(int sockfd, request_t *req, response_t *resp) {

	char *connection;
	char date_buffer[MAX_DATE_SIZE];

	connection = NULL;

	get_date(date_buffer, "%a, %d %b %Y %H:%M:%S %Z");

	write_response_header(resp, "Date", date_buffer);
	write_response_header(resp, "Server", conf.server_name);

	switch (req->method) {

		case GET:
			if (handle_get(req, resp) < 0) {
				set_response_status(resp, 500, "Internal Server Error");
			}
			
			break;

		case HEAD:
			set_response_status(resp, 405, "Method Not Allowed");
			break;
		
		case POST:
			/* Look for "Content-Length" and "Expect" headers before anything else */
			set_response_status(resp, 405, "Method Not Allowed");
			break;
		
		case PUT:
			set_response_status(resp, 405, "Method Not Allowed");
			break;
		
		case DELETE:
			set_response_status(resp, 405, "Method Not Allowed");
			break;
		
		case TRACE:
			set_response_status(resp, 405, "Method Not Allowed");
			break;
		
		case CONNECT:
			set_response_status(resp, 405, "Method Not Allowed");
			break;
		
		default:
			set_response_status(resp, 405, "Method Not Allowed");
			break;
	}

	get_request_header(req, "Connection", &connection);

	if (connection == NULL) {

		write_response_header(resp, "Connection", "close");

	} else if (strncasecmp(connection, "close", strlen("close")) == 0) {

		write_response_header(resp, "Connection", "close");

	} else {

		write_response_header(resp, "Connection", "keep-alive");

	}

	send_response(sockfd, resp);

}

int handle_get(request_t *req, response_t *resp) {

	char *res_path;
	char *file_path;
	char *file_size;
	char *file_ext;
	char *mime_type;
	char *charset;

	int fd, i, s, string_length;

	struct stat file_info;

	res_path = NULL;
	file_path = NULL;
	file_size = NULL;
	file_ext = NULL;
	mime_type = NULL;
	charset = NULL;

	string_length = 0;

	if (resource_path(req->resource, &res_path) < 0) {
		handle_error("resource_path");
	}

	if (is_dir(res_path)) {

		if (directory_index_lookup(res_path, &(resp->file_path)) >= 0) {
			resp->file_exists = TRUE;
		}

	} else if (is_file(res_path)) {

		resp->file_exists = TRUE;

		string_length = strlen(res_path);
		resp->file_path = malloc(string_length + 1);
		memset(resp->file_path, 0, string_length + 1);
		strncat(resp->file_path, res_path, string_length);

	} else {	
		/* TODO cgi */
	}

	paranoid_free_string(res_path);

	if (resp->file_exists) {

		set_response_status(resp, 200, "OK");

		stat(resp->file_path, &file_info);

		/* Look for mime type */
		file_ext = strrchr(resp->file_path, '.');

		if (get_mime_type(file_ext, &mime_type) == -1) {

			if (conf.output_level >= DEBUG){
				printf("DEBUG: default mime type %s\n", default_mime_type);
			}

			mime_type = default_mime_type;

		} else {

			if (conf.output_level >= DEBUG) {
				printf("DEBUG: mime type %s\n", mime_type);
			}

		}

		write_response_header(resp, "Content-Type", mime_type);

		/* Append charset when mime type is text */
		if (strncmp(mime_type, "text", 4) == 0) {

			charset = malloc(strlen("charset=") + strlen(conf.charset) + 1);
			memset(charset, 0, strlen("charset=") + strlen(conf.charset) + 1);
			strncat(charset, "charset=", strlen("charset="));
			strncat(charset, conf.charset, strlen(conf.charset));

			append_response_header(resp, "Content-Type", charset);

			paranoid_free_string(charset);

		} else {

			/* Get the file length */
			integer_to_ascii(file_info.st_size, &file_size);

			write_response_header(resp, "Content-Length", file_size);

			paranoid_free_string(file_size);

		}

	} else {

		set_response_status(resp, 404, "Not Found");

	}

	return 0;

}

int handle_post(request_t *req, response_t *resp) {
	
	return 0;
}

int handle_head(request_t *req, response_t *resp) {
	
	return 0;
}

void free_response(response_t *resp) {

	int i;

	/* free response */
	free(resp->reason_phrase);

	if (resp->file_exists > 0) {
		free(resp->file_path);
	}

	/* free response headers */
	for (i = 0; i < resp->num_headers; i++) {

		free(resp->headers[i]->name);
		free(resp->headers[i]->value);
		free(resp->headers[i]);

	}

	free(resp->headers);

}