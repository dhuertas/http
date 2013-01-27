/*
 * @author dhuertas
 * @email huertas.dani@gmail.com
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdint.h>
#include <errno.h>

/* local header files */
#include "constants.h"
#include "config.h"
#include "util.h"

extern config_t conf;
/*
 * Reads the httpd.conf file for configuration options.
 *
 * @param conf_file_name: path to the config file
 */
void read_config(char *file_path) {

	char buf[1], *line, *tmp;
	char *value;

	int fd;
	int length, line_length, readed;
	int n, i, size, count, file_size;
	int white_space, last_comma;

	length = 0;
	line_length = 0;
	readed = 0;

	n = 0;
	i = 0;
	size = 0;
	count = 0;
	file_size = 0;

	conf.http_version = malloc(strlen("HTTP/1.1") + 1);

	memset(conf.http_version, 0, strlen("HTTP/1.1") + 1);
	strncat(conf.http_version, "HTTP/1.1", strlen("HTTP/1.1"));

	conf.directory_index_count = 0;

	if ((fd = open(file_path, O_RDONLY, 0644)) < 0) {
		handle_error("server_config: open");
	}
	
	file_size = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);
	
	line_length = 0;

	while ((n = read(fd, buf, 1)) > 0) {

		readed++;

		if (readed == file_size) {
			/*
			 * If the last line has no new-line character, append the readed char and 
			 * continue. Once the line is parsed, next read() call will exit the while
			 * loop.
			 */
			line_length++;

			tmp = realloc(line, line_length + 1);
			if (tmp != NULL) line = tmp;

			strncat(line, buf, 1);
		}

		if (line_length == 0 && strncmp(buf, "\n", 1) != 0) {

			line = malloc(2);
			memset(line, 0, 2);
			strncat(line, buf, 1);

			line_length++;

		} else if (line_length == 0 && strncmp(buf, "\n", 1) == 0) {

			/* empty line */

		} else if (line_length > 0 && (strncmp(buf, "\n", 1) == 0 || readed == file_size)) {

			value = strchr(line, ' ') + sizeof(char);

			if (strncmp(&line[0], "#", 1) == 0) {

				/* ignore line */
				
			} else if (strncmp(line, "ServerName ", strlen("ServerName ")) == 0) {

				length = line_length - strlen("ServerName ");

				conf.server_name = malloc(length + 1);
				memset(conf.server_name, 0, length + 1);
				strncat(conf.server_name, value, strlen(value));

			} else if (strncmp(line, "DocumentRoot ", strlen("DocumentRoot ")) == 0) {

				length = line_length - strlen("DocumentRoot ");

				if (strncmp(&line[line_length - 1], "/", 1) == 0) {

					conf.document_root = malloc(length);
					memset(conf.document_root, 0, length);
					strncat(conf.document_root, value, length - 1);

				} else {

					conf.document_root = malloc(length + 1);
					memset(conf.document_root, 0, length + 1);
					strncat(conf.document_root, value, length);

				}

			} else if (strncmp(line, "ListenPort ", strlen("ListenPort ")) == 0) {

				conf.listen_port = atoi(strchr(line, ' ') + sizeof(char));

			} else if (strncmp(line, "Charset ", strlen("Charset ")) == 0) {

				length = line_length - strlen("Charset ");

				conf.charset = malloc(length + 1);
				memset(conf.charset, 0, length + 1);
				strncat(conf.charset, value, length);

			} else if (strncmp(line, "OutputLevel ", strlen("OutputLevel ")) == 0) {

				conf.output_level = atoi((strchr(line, ' ') + sizeof(char)));

			} else if (strncmp(line, "DirectoryIndex ", strlen("DirectoryIndex ")) == 0) {

				length = 0;
				count = 0;
				conf.directory_index_count = 0;

				while (value[i] != '\0') {
					if (strncmp(&value[i], ",", 1) == 0) {
						/* comma found */
						conf.directory_index = realloc(conf.directory_index, (count + 1)*sizeof(char *));
						conf.directory_index[count] = malloc(length + 1);
						
						memset(conf.directory_index[count], 0, length + 1);
						strncat(conf.directory_index[count], &value[i-length], length);

						length = 0;
						conf.directory_index_count++;
						count++;

					} else if (strncmp(&value[i], " ", 1) == 0) {
						/* whitespace found */
					} else {
						length++;
					}

					i++;
				}

				/* reached end of line */
				if (value[i-1] != ',') {

					conf.directory_index = realloc(conf.directory_index, (count + 1)*sizeof(char *));
					conf.directory_index[count] = malloc(length + 1);
					memset(conf.directory_index[count], 0, length + 1);
					strncat(conf.directory_index[count], &value[i-length], length);
					conf.directory_index_count++;
				}

			} else if (strncmp(line, "KeepAliveTimeout ", strlen("KeepAliveTimeout ")) == 0) {

				conf.keep_alive_timeout = atoi((strchr(line, ' ') + sizeof(char)));

			} else {

				/* do nothing */

			}

			memset(line, 0, line_length);
			free(line);
			line_length = 0;

		} else {

			line_length++;

			tmp = realloc(line, line_length + 1);
			if (tmp != NULL) line = tmp;

			strncat(line, buf, 1);

		}

	}

	n = sizeof(conf.directory_index) / sizeof(conf.directory_index[0]);

	if (conf.output_level >= DEBUG) {

		printf("Server configuration:\n");
		printf("  Document root: %s\n", conf.document_root);
		printf("  Listen port: %d\n", conf.listen_port);
		printf("  Default charset: %s\n", conf.charset);
		printf("  Output level: %d\n", conf.output_level);
		printf("  Directory index: ");
	
		for (i = 0; i < conf.directory_index_count; i++) {
			printf("%s ", conf.directory_index[i]);
		}
		
		printf("\n");
		printf("  Keep alive timeout: %d\n", conf.keep_alive_timeout);

	}

	close(fd);

}