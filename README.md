http
====

A custom, multi-threaded http web server.

Warning! This project is for fun and learning purposes only.

## Compile
* Under http folder do: `gcc -o bin/httpd ./src/*.c` (you may need to add `-lpthread` or `-pthread`).

## Execute
* Setup the `httpd.conf` config file.
* Under http folder do `./bin/httpd -c ./config/httpd.conf`.