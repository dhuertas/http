/*
 * @author dhuertas
 * @email huertas.dani@gmail.com
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "constants.h"
#include "config.h"
#include "mime.h"

extern config_t conf;

int get_mime_type(char *ext, char **mime_type) {

	int i, result;
	int n = sizeof(mime_types) / sizeof(mime_t);
	result = -1;

	if (ext != NULL) {

		if (ext[0] == '.') {
			ext += sizeof(char);
		}

		for (i = 0; i < n; i++) {

			if (strncmp(mime_types[i].ext, ext, strlen(ext)) == 0) {

				*mime_type = mime_types[i].type;
				break;

			}

		}

		result = i;

	} else {

		*mime_type = NULL;

	}
	
	return result;

}