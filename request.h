/*
 * @author dhuertas
 * @email huertas.dani@gmail.com
 */
#ifndef __REQUEST_H
#define __REQUEST_H

#define _REQUEST_ALLOC_SIZE		512			// allocate first 512 bytes for requests
#define _REQUEST_MAX_SIZE		8192		// 8 KB

#define _REQUEST_URI			0x01
#define _REQUEST_VERSION		0x02
#define _REQUEST_RESOURCE		0x04
#define _REQUEST_QUERY			0x08
#define _REQUEST_MESSAGE		0x10


typedef struct request {
	uint32_t _mask;
	// mask:
	// ........ ........ ........ .......x uri
	// ........ ........ ........ ......x. version
	// ........ ........ ........ .....x.. resource
	// ........ ........ ........ ....x... query
	// ........ ........ ........ ...x.... message body
	uint16_t num_headers;
	uint8_t method;
	char *uri;
	char *version;
	char *resource;
	char *query;
	char *message_body;
	header_t **headers;
} request_t;

int receive_request(int sockfd, char **data);
int receive_message_body(int sockfd, char **data);
uint16_t get_request_header(request_t *req, char *name, char *value);
int handle_request(int sockfd, request_t *req);
void free_request(request_t *req);

#endif
