/*
 * @author dhuertas
 * @email huertas.dani@gmail.com
 */
#include <stdint.h>

typedef struct config {
	char *server_name;
	uint16_t listen_port;
	uint8_t output_level;
	char *document_root;
	char *http_version;
	char *charset;
	char **default_files;
} config_t;

static char *default_files[2] = {"index.html", "index.htm"};

void read_config(char *conf_file_name);