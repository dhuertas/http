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

extern config_t conf;

void set_response_status(response_t *resp, int status_code, char *reason_phrase) {

	if (resp->status_code > 0) {
		resp->reason_phrase = realloc(resp->reason_phrase, strlen(reason_phrase) + 1);
	} else {
		resp->reason_phrase = malloc(strlen(reason_phrase) + 1);
	}

	resp->status_code = status_code;

	memcpy(resp->reason_phrase, reason_phrase, strlen(reason_phrase));
	resp->reason_phrase[strlen(reason_phrase)] = '\0';

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
void set_response_header(response_t *resp, char *name, char *value) {

	uint16_t i = 0;
	char found = 0;

	int new_length;

	for (i = 0; i < resp->num_headers; i++) {
		
		if ( ! found && strncmp(resp->headers[i]->name, name, strlen(name)) == 0) {

			/* header already exist, append it */
			new_length = strlen(resp->headers[i]->value) + strlen("; ") + strlen(value);

			resp->headers[i]->value = realloc(resp->headers[i]->value, new_length + 1);

			strncat(resp->headers[i]->value, "; ", 2);
			strncat(resp->headers[i]->value, value, strlen(value));

			found = 1;

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

		memcpy(resp->headers[resp->num_headers]->name, name, strlen(name));
		memcpy(resp->headers[resp->num_headers]->value, value, strlen(value));
		resp->headers[resp->num_headers]->name[strlen(name)] = '\0';
		resp->headers[resp->num_headers]->value[strlen(value)] = '\0';

		resp->num_headers++;
	}

}

void send_response(int sockfd, request_t *req, response_t *resp) {

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

		buffer = realloc(buffer, strlen(buffer) + length);

		strncat(buffer, resp->headers[i]->name, strlen(resp->headers[i]->name));
		strncat(buffer, ": ", 2);
		strncat(buffer, resp->headers[i]->value, strlen(resp->headers[i]->value));

		strncat(buffer, "\r\n", 2);

	}

	if ((w = send(sockfd, buffer, strlen(buffer), 0)) != strlen(buffer)) {
		handle_error("send_response: send");
	}

	if (conf.output_level >= VERBOSE) printf("%s\n", buffer);

	if ((w = send(sockfd, "\r\n", strlen("\r\n"), 0)) != strlen("\r\n")) {
		handle_error("send_response: send");
	}

	free(buffer);

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

	char *file_path;
	char *resource;

	char *file_size;
	char *mime_type;
	char *charset;

	char date_buffer[MAX_DATE_SIZE];
	char file_exists;
	
	int fd, i, s, length;

	struct stat file_info;

	file_path = NULL;
	resource = NULL;
	file_size = NULL;
	mime_type = NULL;
	charset = NULL;

	resp->status_code = 0;
	resp->file_exists = FALSE;
	resp->num_headers = 0;

	length = 0;

	get_date(date_buffer, "%a, %d %b %Y %H:%M:%S %Z");

	set_response_header(resp, "Date", date_buffer);
	set_response_header(resp, "Server", conf.server_name);

	switch (req->method) {

		case GET:

			if (req->query_length > 0) {
				/* Strip query string from uri */
				length = strlen(req->uri) - req->query_length;
			} else {
				length = strlen(req->uri);
			}
			
			resource = malloc(length + 1);
			memset(resource, 0, length + 1);
			strncpy(resource, req->uri, length);
			resource[length] = '\0';

			length = 0;

			if (strncmp(&(resource[strlen(resource) - 1]), "/", 1) == 0) {

				/* requested resource is a directory */
				for (i = 0; i < sizeof(default_files) / sizeof(char *); i++) {

					length = strlen(conf.document_root)
						+ strlen(resource) 
						+ strlen(default_files[i]);
					
					file_path = malloc(length + 1);

					strncpy(file_path, conf.document_root, strlen(conf.document_root));
					strncat(file_path, resource, strlen(resource));
					strncat(file_path, default_files[i], strlen(default_files[i]));

					if (conf.output_level >= DEBUG) printf("DEBUG: looking for file %s\n", file_path);

					s = stat(file_path, &file_info);

					if (s == 0) {
						/* file exists */
						resp->file_exists = TRUE;
						break;
					
					} else if (s == -1) {
						
						if (conf.output_level >= DEBUG) printf("DEBUG: file not found %s\n", file_path);
					
					} else {

						memset(file_path, 0, length + 1);
						free(file_path);

					}

				}

				if (resp->file_exists) {

					set_response_status(resp, 200, "OK");
					
					/* Look for mime type */
					if (get_mime_type(strrchr(resource, '.'), &mime_type) == -1) {

						if (conf.output_level >= DEBUG) printf("DEBUG: default mime type %s\n", default_mime_type);
						mime_type = default_mime_type;

					} else {

						if (conf.output_level >= DEBUG) printf("DEBUG: mime type %s\n", mime_type);

					}

					set_response_header(resp, "Content-Type", mime_type);

					/* Append charset when mime type is text */
					if (strncmp(mime_type, "text", 4) == 0) {

						if (conf.output_level >= DEBUG) {
							printf("DEBUG: conf.charset %s (length %lu)\n", conf.charset, strlen(conf.charset));
						}

						charset = malloc(strlen("charset=") + strlen(conf.charset) + 1);
						strncpy(charset, "charset=", strlen("charset="));
						strncat(charset, conf.charset, strlen(conf.charset));

						set_response_header(resp, "Content-Type", charset);	/* Append */

						free(charset);

					} else {

						/* Get the file length */
						integer_to_ascii(file_info.st_size, &file_size);

						set_response_header(resp, "Content-Length", file_size);

						free(file_size);

					}

					/* TODO Keep-Alive friendly */
					set_response_header(resp, "Connection", "Close");

					resp->file_path = malloc(strlen(file_path) + 1);
					memset(resp->file_path, 0, strlen(resp->file_path) + 1);
					strncat(resp->file_path, file_path, strlen(file_path));

					free(file_path);

				} else {

					/* default file not found */
					set_response_status(resp, 404, "Not Found");

				}

			} else {

				/* TODO uri may still point to a directory ... */
				/* TODO strip query string from uri */
				length = strlen(conf.document_root) + strlen(resource);

				file_path = malloc(length + 1);

				memset(file_path, 0, length);

				strncpy(file_path, conf.document_root, sizeof(conf.document_root));
				strncat(file_path, resource, strlen(resource));

				if (conf.output_level >= DEBUG) printf("DEBUG: file path %s\n", file_path);

				s = stat(file_path, &file_info);

				if (s == 0 && ! (file_info.st_mode & S_IFDIR)) {

					/* File exists and is not a directory */
					set_response_status(resp, 200, "OK");

					/* Look for mime type */
					if (get_mime_type(strrchr(resource, '.'), &mime_type) == -1) {

						if (conf.output_level >= DEBUG) printf("DEBUG: default mime type %s\n", default_mime_type);
						mime_type = default_mime_type;

					} else {

						if (conf.output_level >= DEBUG) printf("DEBUG: mime type %s\n", mime_type);

					}

					set_response_header(resp, "Content-Type", mime_type);

					/* Append charset */
					if (strncmp(mime_type, "text", 4) == 0) {

						printf("DEBUG: conf.charset %s (length %lu)\n", conf.charset, strlen(conf.charset));

						charset = malloc(strlen("charset=") + strlen(conf.charset) + 1);
						strncpy(charset, "charset=", strlen("charset="));
						strncat(charset, conf.charset, strlen(conf.charset));

						set_response_header(resp, "Content-Type", charset);	/* Append */

						free(charset);

					} else {

						/* Get the file length */
						integer_to_ascii(file_info.st_size, &file_size);
						set_response_header(resp, "Content-Length", file_size);
						free(file_size);

					}

					//set_response_header(resp, "Transfer-Encoding", "chunked");
					set_response_header(resp, "Connection", "Close"); /* TODO Keep-Alive friendly */

					resp->file_exists = TRUE;

					resp->file_path = malloc(strlen(file_path) + 1);
					memset(resp->file_path, 0, strlen(file_path) + 1);
					strncat(resp->file_path, file_path, strlen(file_path));

					free(file_path);

				} else if (s == 0 && (file_info.st_mode & S_IFDIR)) {

					/* requested resource is a directory */
					for (i = 0; i < sizeof(default_files) / sizeof(char *); i++) {

						length = strlen(conf.document_root)
							+ strlen(resource) 
							+ 1
							+ strlen(default_files[i]);
					
						file_path = malloc(length + 1);

						strncpy(file_path, conf.document_root, strlen(conf.document_root));
						strncat(file_path, resource, strlen(resource));
						strncat(file_path, "/", 1);
						strncat(file_path, default_files[i], strlen(default_files[i]));

						if (conf.output_level >= DEBUG) printf("DEBUG: looking for file %s\n", file_path);

						s = stat(file_path, &file_info);

						if (s == 0) {
							/* file exists */
							resp->file_exists = TRUE;
							break;
					
						} else if (s == -1) {
						
							if (conf.output_level >= DEBUG) printf("DEBUG: file not found %s\n", file_path);
					
						} else {

							memset(file_path, 0, length + 1);
							free(file_path);

						}

					}

					if (resp->file_exists) {

						set_response_status(resp, 200, "OK");

						/* Look for mime type */
						if (get_mime_type(strrchr(resource, '.'), &mime_type) == -1) {

							if (conf.output_level >= DEBUG) printf("DEBUG: default mime type %s\n", default_mime_type);
							mime_type = default_mime_type;

						} else {

							if (conf.output_level >= DEBUG) {
								printf("DEBUG: mime type %s\n", mime_type);
							}

						}

						set_response_header(resp, "Content-Type", mime_type);

						/* Append charset when mime type is text */
						if (strncmp(mime_type, "text", 4) == 0) {

							if (conf.output_level >= DEBUG) {
								printf("DEBUG: charset %s\n", conf.charset);
							}

							charset = malloc(strlen("charset=") + strlen(conf.charset) + 1);
							strncpy(charset, "charset=", strlen("charset="));
							strncat(charset, conf.charset, strlen(conf.charset));

							set_response_header(resp, "Content-Type", charset);	/* Append */

							free(charset);

						} else {

							/* Get the file length */
							integer_to_ascii(file_info.st_size, &file_size);

							set_response_header(resp, "Content-Length", file_size);

							free(file_size);

						}

						/* TODO Keep-Alive friendly */
						set_response_header(resp, "Connection", "Close");

						resp->file_path = malloc(strlen(file_path) + 1);
						memset(resp->file_path, 0, strlen(resp->file_path) + 1);
						strncat(resp->file_path, file_path, strlen(file_path));

						free(file_path);

					} else {

						/* default file not found */
						set_response_status(resp, 404, "Not Found");

					}

				} else {

					/* file not found */
					set_response_status(resp, 404, "Not Found");
					set_response_header(resp, "Connection", "Close");

				}

			}

			if (resource != NULL) free(resource);

			break;

		case HEAD:
			set_response_status(resp, 405, "Method Not Allowed");
			set_response_header(resp, "Connection", "Close");
			break;
		
		case POST:
			/* Look for "Content-Length" and "Expect" headers before anything else */
			set_response_status(resp, 405, "Method Not Allowed");
			set_response_header(resp, "Connection", "Close");
			break;
		
		case PUT:
			set_response_status(resp, 405, "Method Not Allowed");
			set_response_header(resp, "Connection", "Close");
			break;
		
		case DELETE:
			set_response_status(resp, 405, "Method Not Allowed");
			set_response_header(resp, "Connection", "Close");
			break;
		
		case TRACE:
			set_response_status(resp, 405, "Method Not Allowed");
			set_response_header(resp, "Connection", "Close");
			break;
		
		case CONNECT:
			set_response_status(resp, 405, "Method Not Allowed");
			set_response_header(resp, "Connection", "Close");
			break;
		
		default:
			set_response_status(resp, 405, "Method Not Allowed");
			set_response_header(resp, "Connection", "Close");
			break;
	}

	send_response(sockfd, req, resp);

}