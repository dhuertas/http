/*
 * @author dhuertas
 * @email huertas.dani@gmail.com
 */
#ifndef __CONFIG_H
#define __CONFIG_H

typedef struct error_document {
	int status_code;
	char *file_path;
} error_document_t;

typedef struct config {
	uint8_t output_level;
	uint16_t listen_port;
	uint16_t keep_alive_timeout;
	uint16_t request_timeout;
	uint32_t max_keep_alive_requests;
	uint32_t thread_pool_size;
	char *server_name;
	char *server_root;
	char *document_root;
	char *http_version;
	char *charset;
	char *default_type;
	char **directory_index;
	uint16_t directory_index_count;
	error_document_t **error_documents;
	uint16_t error_documents_count;
} config_t;

void read_config(char *conf_file_name);

#endif