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
		
		resp->_mask |= _RESPONSE_REASON;

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

void send_response_headers(int thread_id, int sockfd, response_t *resp) {

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

		length = strlen(resp->headers[i]->name)
			+ 2 // strlen(": ")
			+ strlen(resp->headers[i]->value)
			+ 2;		

		buffer = realloc(buffer, strlen(buffer) + length + 1);

		strncat(buffer, resp->headers[i]->name, strlen(resp->headers[i]->name));
		strncat(buffer, ": ", 2);
		strncat(buffer, resp->headers[i]->value, strlen(resp->headers[i]->value));

		strncat(buffer, "\r\n", 2);

	}

	if ((w = send(sockfd, buffer, strlen(buffer), 0)) != strlen(buffer)) {

		if (errno == EBADF || errno == EPIPE) {

			debug(conf.output_level, 
				"[%d] DEBUG: unable to send response headers (%s)\n", 
				thread_id, strerror(errno));

		} else {

			handle_error("send");

		}

	}

	debug(conf.output_level, 
		"[%d] Response:\n%s\n", 
		thread_id, buffer);

	/* Send a new line to end the headers part */
	if ((w = send(sockfd, "\r\n", 2, 0)) != 2) {

		if (errno == EBADF || errno == EPIPE) {

			debug(conf.output_level, 
				"[%d] DEBUG: unable to send response new line (%s)\n", 
				thread_id, strerror(errno));

		} else {

			handle_error("send");

		}

	}

	paranoid_free_string(buffer);

}

void send_response_content(int thread_id, int sockfd, response_t *resp) {

	if (resp->_mask & _RESPONSE_FILE_PATH) {

		send_file(sockfd, resp->file_path);

	}

}

/*
 * Receives the client request and generates the response corresponding
 * to the requested method.
 *
 * @param thread_id: the thread id handling the request
 * @param sockfd: the socket stream
 * @param req: request_t data structure where the request is stored
 * @param resp: response_t data structure
 */
void handle_response(int thread_id, int sockfd, request_t *req, response_t *resp) {

	char *connection;
	char date_buffer[MAX_DATE_SIZE];

	int i;

	connection = NULL;

	get_date(date_buffer, "%a, %d %b %Y %H:%M:%S %Z");

	write_response_header(resp, "Date", date_buffer);
	write_response_header(resp, "Server", conf.server_name);

	get_request_header(req, "Connection", &connection);

	if (connection == NULL) {

		write_response_header(resp, "Connection", "close");

	} else if (strncasecmp(connection, "close", strlen("close")) == 0) {

		write_response_header(resp, "Connection", "close");

	} else {

		write_response_header(resp, "Connection", "keep-alive");

	}

	switch (req->method) {

		case GET:

			if (handle_get(thread_id, req, resp) < 0) {

				set_response_status(resp, 500, "Internal Server Error");
				set_error_document(thread_id, resp, 500);
				send_response_headers(thread_id, sockfd, resp);

			} else {

				send_response_headers(thread_id, sockfd, resp);
				send_response_content(thread_id, sockfd, resp);

			}

			break;

		case HEAD:

			if (handle_head(thread_id, req, resp) < 0) {

				set_response_status(resp, 500, "Internal Server Error");

			} else {

				send_response_headers(thread_id, sockfd, resp);

			}

			break;
		
		case POST:

			if (handle_post(thread_id, req, resp) < 0) {

				set_response_status(resp, 500, "Internal Server Error");
				set_error_document(thread_id, resp, 500);
				send_response_headers(thread_id, sockfd, resp);

			} else {

				send_response_headers(thread_id, sockfd, resp);
				send_response_content(thread_id, sockfd, resp);

			}

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
			send_response_headers(thread_id, sockfd, resp);

			break;

	}

}

/*
 * Generate the corresponding GET response
 *
 * @param thread_id: the thread id handling the request
 * @param req: request_t data structure
 * @param resp: response_t data structure
 * @return: 0 on success, -1 on error
 */
int handle_get(int thread_id, request_t *req, response_t *resp) {

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

			resp->_mask |= _RESPONSE_FILE_PATH;

		}

	} else if (is_file(res_path)) {

		resp->_mask |= _RESPONSE_FILE_PATH;

		string_length = strlen(res_path);
		resp->file_path = malloc(string_length + 1);
		memset(resp->file_path, 0, string_length + 1);
		strncat(resp->file_path, res_path, string_length);

	} else {	
		/* TODO cgi */
	}

	paranoid_free_string(res_path);

	if (resp->_mask & _RESPONSE_FILE_PATH) {

		set_response_status(resp, 200, "OK");

		stat(resp->file_path, &file_info);

		/* Look for mime type */
		file_ext = strrchr(resp->file_path, '.');

		if (get_mime_type(file_ext, &mime_type) == -1) {

			debug(conf.output_level,
				"[%d] DEBUG: default mime type %s\n",
				thread_id, conf.default_type);

			mime_type = conf.default_type;

		} else {

			debug(conf.output_level,
				"[%d] DEBUG: mime type %s\n",
				thread_id, mime_type);

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

		}

		/* Get the file length */
		integer_to_ascii(file_info.st_size, &file_size);

		write_response_header(resp, "Content-Length", file_size);

		paranoid_free_string(file_size);

	} else {

		set_response_status(resp, 404, "Not Found");
		set_error_document(thread_id, resp, 404);

	}

	return 0;

}

/*
 * Generate the corresponding POST response
 *
 * @param thread_id: the thread id handling the request
 * @param req: request_t data structure
 * @param resp: response_t data structure
 * @return: 0 on success, -1 on error
 */
int handle_post(int thread_id, request_t *req, response_t *resp) {

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
			//resp->file_exists = TRUE;
			resp->_mask |= _RESPONSE_FILE_PATH;
		}

	} else if (is_file(res_path)) {

		//resp->file_exists = TRUE;
		resp->_mask |= _RESPONSE_FILE_PATH;

		string_length = strlen(res_path);
		resp->file_path = malloc(string_length + 1);
		memset(resp->file_path, 0, string_length + 1);
		strncat(resp->file_path, res_path, string_length);

	} else {	
		/* TODO cgi */
	}

	paranoid_free_string(res_path);

	//if (resp->file_exists) {
	if (resp->_mask & _RESPONSE_FILE_PATH) {

		set_response_status(resp, 200, "OK");

		stat(resp->file_path, &file_info);

		/* Look for mime type */
		file_ext = strrchr(resp->file_path, '.');

		if (get_mime_type(file_ext, &mime_type) == -1) {

			debug(conf.output_level,
				"[%d] DEBUG: default mime type %s\n",
				thread_id, conf.default_type);

			mime_type = conf.default_type;

		} else {

			debug(conf.output_level,
				"[%d] DEBUG: mime type %s\n",
				thread_id, mime_type);

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

		}

		/* Get the file length */
		integer_to_ascii(file_info.st_size, &file_size);

		write_response_header(resp, "Content-Length", file_size);

		paranoid_free_string(file_size);

	} else {

		set_response_status(resp, 404, "Not Found");
		set_error_document(thread_id, resp, 404);
	}

	return 0;

}

/*
 * Generate only the headers
 *
 * @param thread_id: the thread id handling the request
 * @param req: request_t data structure
 * @param resp: response_t data structure
 * @return: 0 on success, -1 on error
 */
int handle_head(int thread_id, request_t *req, response_t *resp) {

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

			resp->_mask |= _RESPONSE_FILE_PATH;

		}

	} else if (is_file(res_path)) {

		resp->_mask |= _RESPONSE_FILE_PATH;

		string_length = strlen(res_path);
		resp->file_path = malloc(string_length + 1);
		memset(resp->file_path, 0, string_length + 1);
		strncat(resp->file_path, res_path, string_length);

	} else {	
		/* TODO cgi */
	}

	paranoid_free_string(res_path);

	if (resp->_mask & _RESPONSE_FILE_PATH) {

		set_response_status(resp, 200, "OK");

		stat(resp->file_path, &file_info);

		/* Look for mime type */
		file_ext = strrchr(resp->file_path, '.');

		if (get_mime_type(file_ext, &mime_type) == -1) {

			debug(conf.output_level,
				"[%d] DEBUG: default mime type %s\n",
				thread_id, conf.default_type);

			mime_type = conf.default_type;

		} else {

			debug(conf.output_level,
				"[%d] DEBUG: mime type %s\n",
				thread_id, mime_type);

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

		}

		/* Get the file length */
		integer_to_ascii(file_info.st_size, &file_size);

		write_response_header(resp, "Content-Length", file_size);

		paranoid_free_string(file_size);

	} else {

		set_response_status(resp, 404, "Not Found");

	}

	return 0;

}

void set_error_document(int thread_id, response_t *resp, int status_code) {

	char *res_path;
	char *file_size;
	char *file_ext;
	char *mime_type;
	char *charset;

	int i, string_length;

	struct stat file_info;

	res_path = NULL;
	file_size = NULL;
	file_ext = NULL;
	mime_type = NULL;
	charset = NULL;

	i = 0;
	string_length = 0;

	for (i = 0; i < conf.error_documents_count; i++) {

		if (conf.error_documents[i]->status_code == status_code) {

			if (conf.error_documents[i]->file_path[0] == '/') {
				/* Absolute path */
				string_length = strlen(conf.error_documents[i]->file_path);

				res_path = malloc(string_length + 1);
				memset(res_path, 0, string_length + 1);

				snprintf(res_path, string_length + 1, " %s", conf.error_documents[i]->file_path);

			} else {
				/* Path is relative to server root folder */
				string_length = strlen(conf.server_root) 
					+ 1 
					+ strlen(conf.error_documents[i]->file_path);

				res_path = malloc(string_length + 1);
				memset(res_path, 0, string_length);

				snprintf(res_path, string_length + 1, "%s/%s", 
					conf.server_root, 
					conf.error_documents[i]->file_path);	
			}

			if (is_file(res_path)) {

				resp->file_path = malloc(string_length + 1);
				memset(resp->file_path, 0, string_length + 1);
				strncat(resp->file_path, res_path, string_length);

				resp->_mask |= _RESPONSE_FILE_PATH;
				
				stat(resp->file_path, &file_info);

				/* Look for mime type */
				file_ext = strrchr(resp->file_path, '.');

				if (get_mime_type(file_ext, &mime_type) == -1) {

					debug(conf.output_level,
						"[%d] DEBUG: default mime type %s\n",
						thread_id, conf.default_type);

					mime_type = conf.default_type;

				} else {

					debug(conf.output_level,
						"[%d] DEBUG: mime type %s\n",
						thread_id, mime_type);

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

				}

				/* Get the file length */
				integer_to_ascii(file_info.st_size, &file_size);

				write_response_header(resp, "Content-Length", file_size);

				paranoid_free_string(file_size);
				
			}

			paranoid_free_string(res_path);

			break;

		}

	}

}

void free_response(response_t *resp) {

	int i;

	if (resp->num_headers > 0) {

		/* free response headers */
		for (i = 0; i < resp->num_headers; i++) {

			free(resp->headers[i]->name);
			free(resp->headers[i]->value);
			free(resp->headers[i]);

		}

		free(resp->headers);

		resp->num_headers = 0;

	}

	if (resp->_mask & _RESPONSE_FILE_PATH) paranoid_free_string(resp->file_path);
	resp->_mask &= ~_RESPONSE_FILE_PATH;

	if (resp->_mask & _RESPONSE_REASON) paranoid_free_string(resp->reason_phrase);
	resp->_mask &= ~_RESPONSE_REASON;

	resp->status_code = 0;

}