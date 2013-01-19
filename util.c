/*
 * @author dhuertas
 * @email huertas.dani@gmail.com
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>

#include "constants.h"
#include "config.h"
#include "util.h"

extern config_t conf;

void integer_to_ascii(int number, char **result) {
	
	int tmp, count;

	tmp = number;

	while (tmp > 0) {
		
		tmp /= 10;
		count++;
	}
	
	*result = malloc(count + 1);
	sprintf(*result, "%d", number);
	
}

void get_date(char *buffer, char *format) {

	struct timeval tv;
	time_t curtime;

	gettimeofday(&tv, NULL);
	curtime = tv.tv_sec;

	strftime(buffer, MAX_DATE_SIZE, format, localtime(&curtime));

}

void send_file(int sockfd, char *file_path) {

	char buffer[MAX_BUFFER];

	int fd, r, w;

	if (conf.output_level >= DEBUG) printf("DEBUG: sending file %s\n", file_path);

	if ((fd = open(file_path, O_RDONLY, 0644)) < 0) {
		handle_error("sending_file: open");
	}

	memset(buffer, 0, sizeof(buffer));

	/* Send file */
	while ((r = read(fd, buffer, MAX_BUFFER)) > 0) {

		if ((w = send(sockfd, buffer, r, 0)) != r) {
			handle_error("sending_file: send");
		}

	}

	if (r < 0) {
		handle_error("sending_file: read");
	}

	close(fd);

}