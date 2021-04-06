/*                      ___       _____        ___  __     __
 *          \        / |      /\    |   |   | |    |  \   /
 *           \  /\  /  |---  /__\   |   |---| |--- |__/  |
 *            \/  \/   |___ /    \  |   |   | |___ |  \ * \__
 *
 *	Licensed under the GPLv3
 *
 *	Dependencies are cjson and curl, both available on most distros' package managers; try `make test` to check if they are installed.
 * 	Pass the `--location` option if you want to get the gps coordinates of your location, convinient for opening a weather.com forecast from a script, like in a polybar module.
 *
 *	Sample module: 
 *	[module/weather]
 *	type = custom/script
 *	exec = cweather 2>/dev/null # Just incase curl fails, cutoff the long error message
 *	tail = true
 *	inteval = 60
 *	click-left = $your-browser-here https://weather.com/weather/tenday/$(cweather --location)?par=google&temp=f # Replace the 'f' with 'c' if you want metric units
 */	

#include <stdio.h>				//
#include <stdlib.h>				//
#include <string.h>				//
#include <curl/curl.h>			// To download stuff
#include <cjson/cJSON.h>		// For json parsing
#include <cjson/cJSON_Utils.h>	// ^^
#include <math.h>				// To get the round() function

#define getjson cJSON_GetObjectItemCaseSensitive
#define printj cJSON_Print 

//#define API_KEY "" // Paste your api key between the quotes 

#ifndef API_KEY
	int n = 2;
	char* API_KEY;
#else
	int n = 1;
#endif

// Gets the quotes off the json output 
char *dequote(char *input) {
	char *p = input;
	p++;
	p[strlen(p)-1] = 0;
	return p;
}

// Functions from https://curl.se/libcurl/c/getinmemory.html
struct MemoryStruct {
  char *memory;
  size_t size;
};
 
static size_t
WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)userp;
 
  char *ptr = realloc(mem->memory, mem->size + realsize + 1);
  if(ptr == NULL) {
    // out of memory!
    printf("not enough memory (realloc returned NULL)\n");
    return 0;
  }
 
  mem->memory = ptr;
  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;
 
  return realsize;
}

char *curl(char *url) {
	// getting a bunch of stuff ready
	CURL* curl_handle;
	CURLcode res;

	struct MemoryStruct chunk;
	chunk.size = 0;

	curl_handle = curl_easy_init(); // initiate the curl session
	curl_easy_setopt(curl_handle, CURLOPT_URL, url); // specify url
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback); // send data to memory 
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk); // what
	curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0"); // add user-agent field to appease the internet gods

	res = curl_easy_perform(curl_handle); // ladies and gentlemen, we got 'em 

	// Now to see if we screwed up or not
	if (res != CURLE_OK) {
		fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
	}
	else {
		return chunk.memory;
	}
		
	curl_easy_cleanup(curl_handle);
	free(chunk.memory);
}

int main(int argc, char **argv) { 
	curl_global_init(CURL_GLOBAL_ALL); // Initiates the global curl instance 

	// Get location from public api
	char *location_api_url = "http://ip-api.com/json/"; 
	cJSON *location_json = cJSON_Parse(curl(location_api_url));
	char *city = printj(getjson(location_json, "city"));
	
	// command line arguments
	if (argc < 2) {
		printf("Error: Missing API key\n");
		return 1;
	} else if (argc > n) {
		if (strncmp(argv[n], "--location", 5) == 0) {
			float lat = atof(printj(getjson(location_json, "lat")));
			float lon = atof(printj(getjson(location_json, "lon")));
			printf("%.2f,%.2f", lat, lon);
			curl_global_cleanup(); // Stops and cleans up the global curl instance 
			return 0;
		} else {
			printf("Error: unrecognized option '%s'\n", argv[n]);
			return 1;
		}
	} 
	if (n == 2) { API_KEY = argv[1]; }

	// Get json from openweathermap.org
	char weather_api_url[256];
	snprintf(weather_api_url, 256, "https://api.openweathermap.org/data/2.5/weather?q=%s&units=imperial&appid=%s", dequote(city), API_KEY); 
	cJSON* weather_json	 = cJSON_Parse(curl(weather_api_url));

	// Stops and cleans up the global curl instance
	curl_global_cleanup();  
 
	// Get the weather data out of the json and put it in some variables
	char *weather, *sky;
	int temperature, sunrise, sunset;
	weather = dequote(printj(getjson(cJSON_GetArrayItem(getjson(weather_json, "weather"), 0), "main")));
	temperature = round(atof(printj(getjson(getjson(weather_json, "main"), "temp"))));	
	sunrise = atoi(printj(getjson(getjson(weather_json, "sys"), "sunrise")));
	sunset  = atoi(printj(getjson(getjson(weather_json, "sys"), "sunset")));
	int now = time(NULL);	
	if (sunrise < now <= sunset){
		sky = "☀";
	}
	else {
		sky = "☽";
	}

	// Output, can be customized
	printf("%s %i°F %s\n", weather, temperature, sky);

	// Cleanup cJSON pointers
	cJSON_Delete(weather_json);

	return 0;
}
