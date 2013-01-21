/*
 * @author dhuertas
 * @email huertas.dani@gmail.com
 */
void integer_to_ascii(int number, char **result);
void get_date(char *buffer, char *format);
void send_file(int sockfd, char *file_path);
int is_dir(char *path);
int directory_index_lookup(char *dir_path, char **file_path);
int resource_path(char *resource, char **path);