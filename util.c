/*
 * @author dhuertas
 * @email huertas.dani@gmail.com
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>

#include "constants.h"
#include "config.h"
#include "util.h"

extern config_t conf;

/*
 * Converts an "unknown" integer to a string buffer
 *
 * @param string
 * @param c
 */
void integer_to_ascii(int number, char **result) {
	
	int tmp, count;

	tmp = number;

	count = 0;

	while (tmp > 0) {
		tmp /= 10;
		count++;
	}

	*result = malloc(count + 1);
	memset(*result, 0, count);
	snprintf(*result, count + 1, "%d", number);

}

/*
 * Gets the current date using the format specified in the argument
 * 
 * @param buffer: the address to store the formated date
 * @param format: the date format
 *
 * More info: man 2 date
 */
void get_date(char *buffer, char *format) {

	struct timeval tv;
	time_t curtime;

	gettimeofday(&tv, NULL);
	curtime = tv.tv_sec;

	strftime(buffer, MAX_DATE_SIZE, format, localtime(&curtime));

}

/*
 * Sends a file through a socket stream
 * 
 * @param sockfd: Socket file descriptor
 * @param file_path: absolute path to file
 */
void send_file(int sockfd, char *file_path) {

	char buffer[MAX_BUFFER];

	int fd, r, w;

	if ((fd = open(file_path, O_RDONLY, 0644)) < 0) {
		handle_error("open");
	}

	memset(buffer, 0, sizeof(buffer));

	/* Send file */
	while ((r = read(fd, buffer, MAX_BUFFER)) > 0) {

		if ((w = send(sockfd, buffer, r, 0)) != r) {
			handle_error("send");
		}

	}

	if (r < 0) {
		handle_error("read");
	}

	close(fd);

}

/*
 * Checks whether the path is a directory
 *
 * @param path: absolute path
 * @return: true when path is a directory
 */
int is_dir(char *path) {

	int s;

	struct stat info;
	
	s = stat(path, &info);

	return (s == 0 && (info.st_mode & S_IFDIR));

}

/*
 * Checks whether the path is a file
 *
 * @param path: absolute path
 * @return: true when path is a regular file
 */
int is_file(char *path) {

	int s;

	struct stat info;

	s = stat(path, &info);

	return (s == 0 && (info.st_mode & S_IFREG));

}

/*
 * Constructs the file path to the requested resource using the
 * configured root directory.
 *
 * @param resource: string of the requested resource
 * @param path: string to store the full path
 *
 * WARNING: this function allocates memory. Remember to free it when not
 * in use.
 */
int resource_path(char *resource, char **path) {

	int string_length;

	if (resource == NULL) {
		return -1;
	}

	if (resource[0] == '/') {

		string_length = strlen(conf.document_root)
			+ strlen(resource);

		*path = malloc(string_length + 1);

		memset(*path, 0, string_length + 1);
		strncpy(*path, conf.document_root, strlen(conf.document_root));
		strncat(*path, resource, strlen(resource));

	} else {

		string_length = strlen(conf.document_root) + 1
			+ strlen(resource);

		*path = malloc(string_length + 1);

		memset(*path, 0, string_length + 1);
		strncpy(*path, conf.document_root, strlen(conf.document_root));
		strncat(*path, "/", 1);
		strncat(*path, resource, strlen(resource));

	}

	return 0;

}

/*
 * Looks whether the directory contains any of the default index files.
 *
 * @param dir_path: path to the directory
 * @param file_path: contains the path to the file when found
 * 
 * WARNING: this function allocates memory. Remember to free it when not
 * in use.
 */
int directory_index_lookup(char *dir_path, char **file_path) {

	int i, s, string_length;

	struct stat file_info;

	string_length = 0;

	if (dir_path == NULL) {
		return -1;
	}

	for (i = 0; i < conf.directory_index_count; i++) {

		string_length = strlen(dir_path) + ( ! IS_DS_LAST(dir_path) ? 1 : 0)
			+ strlen(conf.directory_index[i]);

		*file_path = malloc(string_length + 1);
		memset(*file_path, 0, string_length + 1);

		strncat(*file_path, dir_path, strlen(dir_path));

		if ( ! IS_DS_LAST(dir_path)) strncat(*file_path, "/", 1);

		strncat(*file_path, conf.directory_index[i], strlen(conf.directory_index[i]));

		s = stat(*file_path, &file_info);

		if (s == 0) {

			return i;

		} else {

			memset(*file_path, 0, string_length + 1);
			free(*file_path);

		}

	}

	return -1;

}

/*
 * Locates a char in the string and returns its position (starting from 0)
 *
 * @param string: the haystack
 * @param c: the character to be found
 */
int chrpos(char *string, int c) {

	int i = 0;

	while (&string[i] != '\0') {
		if (string[i] == c) return i;
		i++;
	}

	return -1;
}

/*
 * Sets allocated memory to 0 before freeing it
 *
 * @param s: string
 */
void paranoid_free_string(char *s) {

	memset(s, 0, strlen(s) + 1);
	free(s);

}