/*
 * @author dhuertas
 * @email huertas.dani@gmail.com
 */
#ifndef __MIME_H
#define __MIME_H

 typedef struct mime {
 	char *ext;
 	char *type;
 } mime_t;
 
static mime_t mime_types[] = {
	{"bmp",		"image/bmp"},
	{"css", 	"text/css"},
	{"html", 	"text/html"},
	{"htm", 	"text/html"},
	{"jpg",  	"image/jpeg"},
	{"js", 		"text/javascript"},
	{"json",	"application/json"},
	{"log", 	"text/plain"},
	{"png", 	"image/png"},
	{"txt", 	"text/plain"}
};

int get_mime_type(char *ext, char **mime_type);

#endif