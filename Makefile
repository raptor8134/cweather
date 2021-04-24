CC=gcc
CFLAGS=-Wall -Wpedantic.
DEPS = stdio.h stdlib.h string.h curl/curl.h cjson/cJSON.h cjson/cJSON_Utils.h getopt.h

weather: weather.c
	@$(CC) -o cweather weather.c -lcurl -lcjson
test:
	@ for header in $(DEPS); do printf "found "; ls /usr/include/$$header; done
	@ echo "All dependencies satisfied." 
clean: 
	@rm cweather
install:
	@install ./cweather /usr/bin/cweather
uninstall:
	@rm /usr/bin/cweather
