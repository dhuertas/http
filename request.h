/*
 * @author dhuertas
 * @email huertas.dani@gmail.com
 */
typedef struct request {
	uint8_t method;
	char *uri;
	char *query;
	char *version;
	char *message_body;
	uint32_t query_length;
	uint16_t num_headers;
	header_t **headers;
} request_t;

void receive_request(int sockfd, char **data);
uint16_t get_request_header(request_t *req, char *name, char *value);
void handle_request(int sockfd, request_t *req);