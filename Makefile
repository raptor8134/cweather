CC=gcc
CFLAGS=-Wall -Wpedantic.
DEPS = stdio.h stdlib.h string.h curl/curl.h cjson/cJSON.h cjson/cJSON_Utils.h getopt.h
PREFIX = /usr/bin
DESTDIR=

weather:
	@$(CC) -o $(NAME) weather.c -lcurl -lcjson
test:
	@ for header in $(DEPS); do printf "found "; ls /usr/include/$$header; done
	@ echo "All dependencies satisfied." 
clean: 
	@rm $(NAME) 
install:
	@install ./cweather $(DESTDIR)/$(PREFIX)/$(NAME)
uninstall:
	@rm $(DESTDIR)/$(PREFIX)/$(NAME)
