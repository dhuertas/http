/*
 * @author dhuertas
 * @email huertas.dani@gmail.com
 */
/* 
 * PERFORMANCE
 */
#ifndef __CONSTANTS_H
#define __CONSTANTS_H

#define MAX_LISTEN		30
#define MAX_THREADS		10
#define MAX_DATE_SIZE 	64
#define MAX_BUFFER		1024

/* OUTPUT LEVEL */
#define SILENT			0
#define NORMAL			1
#define DEBUG			2

/*
 * BASICS
 */
#define TRUE			1
#define FALSE			0
#define ERROR			-1

/*
 * HTML METHODS
 */
#define GET				0
#define HEAD			1
#define POST			2
#define PUT				3
#define DELETE			4
#define TRACE			5
#define CONNECT			6

/*
 * FILES
 */
#define DS				'/' 		// Directory  Separator
#define IS_DS_LAST(str) (str[strlen(str) - 1] == DS)	

/*
 * CONNECTION
 */
#define SOCK_RD			0
#define SOCK_WR			1
#define TIME_OUT		10			// Keep Alive timeout 10 seconds
#define RECV_TIME_OUT	10

/*
 * ERRORS
 */
#define handle_error(msg) \
	do { \
		fprintf(stderr, "ERROR: file %s at line %d. Caller function %s says '%s' (%s).\n", \
			__FILE__, __LINE__, __func__, msg, strerror(errno)); \
		exit(EXIT_FAILURE);\
	} while (0)

/*
 * OUTPUT
 */
#define debug(o_level, ...) \
	if (o_level >= DEBUG) printf(__VA_ARGS__);
	

static char *methods[7] = {"GET", "HEAD", "POST", "PUT", "DELETE", "TRACE", "CONNECT"};

#endif