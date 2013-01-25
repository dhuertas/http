/*
 * @author dhuertas
 * @email huertas.dani@gmail.com
 */
#ifndef __RESPONSE_H
#define __RESPONSE_H

#define _RESPONSE_REASON		0x01
#define _RESPONSE_FILE_PATH		0x02

typedef struct response {
	uint32_t _mask;
	// mask:
	// ........ ........ ........ .......x reason phrase
	// ........ ........ ........ ......x. file path
	uint16_t status_code;
	uint16_t num_headers;
	uint8_t file_exists;
	char *reason_phrase;
	char *file_path;
	header_t **headers;
} response_t;

void set_response_status(response_t *resp, int status_code, char *reason_phrase);
void write_response_header(response_t *resp, char *name, char *value);
void append_response_header(response_t *resp, char *name, char *value);
void send_response(int sockfd, response_t *resp);
void handle_response(int sockfd, request_t *req, response_t *resp);

int handle_get(request_t *req, response_t *resp);
int handle_post(request_t *req, response_t *resp);
int handle_head(request_t *req, response_t *resp);

void free_response(response_t *resp);

#endif