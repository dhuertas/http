/*
 * @author dhuertas
 * @email huertas.dani@gmail.com
 */
typedef struct response {
	uint16_t status_code;
	char *reason_phrase;
	uint8_t file_exists;
	char *file_path;
	uint16_t num_headers;
	header_t **headers;
} response_t;

void set_response_status(response_t *resp, int status_code, char *reason_phrase);
void write_response_header(response_t *resp, char *name, char *value);
void append_response_header(response_t *resp, char *name, char *value);
void send_response(int sockfd, request_t *req, response_t *resp);
void handle_response(int sockfd, request_t *req, response_t *resp);

int handle_get(request_t *req, response_t *resp);
int handle_post(request_t *req, response_t *resp);
int handle_head(request_t *req, response_t *resp);