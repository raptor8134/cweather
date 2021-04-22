/*                      ___       _____        ___  __     __
 *          \        / |      /\    |   |   | |    |  \   /
 *           \  /\  /  |---  /__\   |   |---| |--- |__/  |
 *            \/  \/   |___ /    \  |   |   | |___ |  \ * \__
 *
 *	Licensed under the GPLv3
 *
 *	Dependencies are cjson and curl, both available on most distros' package managers; try `make test` to check if they are installed.
 * 	Pass the --location or -l option if you want to get the coordinates of your location, convinient for opening a weather.com forecast from a script, like in a polybar module.
 * 	Credit to the curl team for the example code I used (from here: https://curl.se/libcurl/c/getinmemory.html)
 *
 *	Sample module: 
 *	[module/weather]
 *	type = custom/script
 *	exec = cweather 2>/dev/null # Just incase curl fails, cutoff the long error message
 *	tail = true
 *	inteval = 60
 *	click-left = $your-browser-here https://weather.com/weather/tenday/$(cweather --location)?par=google&temp=f # Replace the 'f' with 'c' if you want metric units
 */	

/* TODO TODO TODO
 * Add a script in the PKGBUILD to compile in an api key
 * Flex on Gavin with cool new program
 * TODO TODO TODO 
 */

#include <stdio.h>				//
#include <stdlib.h>				//
#include <string.h>				//
#include <curl/curl.h>			// To download stuff
#include <cjson/cJSON.h>		// For json parsing
#include <cjson/cJSON_Utils.h>	// ^^
#include <getopt.h>				// Get command line options

#define getjson cJSON_GetObjectItemCaseSensitive
#define printj cJSON_Print 

//#define API_KEY "" // Uncomment and paste your api key between the quotes 


#ifdef API_KEY
	static int key_flag = 1;
#else
	char* API_KEY;
	static int key_flag = 0;
#endif

void help(char* name) {
printf("\
usage: %s [options]\n\
  options:\n\
    -h | --help		Display this help message\n\
    -k | --key <apikey>	Define api key (not necessary if compiled in)\n\
    -c | --celsius	Changes the temperature scale to celcius\n\
    -l | --location	Print latitude and longitude seperated by a comma\n\
    -s | --simple	Only use day/night icons instead of the full set\n\
", name);
}

// Gets the quotes off the json output 
char *dequote(char *input) {
	char *p = input;
	p++;
	p[strlen(p)-1] = 0;
	return p;
}

// Get command-line options with getopt
static int help_flag, centigrade_flag, location_flag, icon_flag;
void getoptions(int argc, char** argv) {
	int c;
	for (;;) {
		static struct option long_options[] = //TODO add `static int help_flag centigrade_flag location_flag` at the top
		{
			{"help"			, no_argument,		 0, 'h'},
			{"celcius"		, no_argument,		 0, 'c'},
			{"location"		, no_argument,		 0, 'l'},
			{"simple"		, no_argument,		 0, 's'},
			{"key"			, required_argument, 0, 'k'},
		};

		int option_index = 0;
		c = getopt_long(argc, argv, "hclsk:", long_options, &option_index);

		if (c == -1) { break; }

		switch(c) {
			case 0:		// do nothing
				break;
			case 'h':
				help_flag = 1;
				break;
			case 'c':
				centigrade_flag = 1;
				break;
			case 'l':
				location_flag = 1;
				break;
			case 's':
				icon_flag = 1;
				break;
			case 'k':
				#ifndef API_KEY
					API_KEY = optarg;
					key_flag = 1;
				#endif
				break;
			default:
				abort();
		}
	}
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
	chunk.memory = malloc(1);
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

	getoptions(argc, argv);
	
	if (location_flag == 1) {
		float lat, lon;
			lat = atof(printj(getjson(location_json, "lat")));
			lon = atof(printj(getjson(location_json, "lon")));
		printf("%.2f,%.2f", lat, lon);
		curl_global_cleanup(); // Stops and cleans up the global curl instance 
		return 0;
	}

	if (key_flag != 1) {
		printf("Error: missing api key\n");
		return 1;
	}

	char * units = "imperial", degreechar = 'F';
	if (centigrade_flag == 1) {
		units = "metric";
		degreechar = 'C';
	}

	if (help_flag == 1) {
		help(argv[0]);
		return 0;
	}

	// Get json from openweathermap.org
	char weather_api_url[256];
	//TODO figure out how to change this to celcius
	sprintf(weather_api_url, "https://api.openweathermap.org/data/2.5/weather?q=%s&units=%s&appid=%s", dequote(city), units, API_KEY); 
	cJSON* weather_json	 = cJSON_Parse(curl(weather_api_url));

	// Stops and cleans up the global curl instance
	curl_global_cleanup();  
 
	// Get the weather data out of the json and put it in some variables
	char *weather, *sky, *icon_id;
	float temperature;
	int sunrise, sunset;
	weather = dequote(printj(getjson(cJSON_GetArrayItem(getjson(weather_json, "weather"), 0), "main")));
	temperature = atof(printj(getjson(getjson(weather_json, "main"), "temp")));	
	icon_id = dequote(printj(getjson(cJSON_GetArrayItem(getjson(weather_json, "weather"), 0), "icon")));
	sunrise = atoi(printj(getjson(getjson(weather_json, "sys"), "sunrise")));
	sunset  = atoi(printj(getjson(getjson(weather_json, "sys"), "sunset")));
	//sprintf(icon);

	// convert the icon code from the json to a nerdfont icon
	char *night, *icons[50];

	// Icon
	char *icon;
	if (icon_flag == 1) {
		if (icon_id[2] == 'd'){
			icon = "☀";
		}
		else {
			icon = "☽";
		}
	} else {
		char icon_array[50];
		if (icon_id[2] == 'd') {
			icons[1] = "滛";
			icons[2] = "";
			icons[3] = "";
			icons[4] = "";
			icons[9] = "";
			icons[10] = "";
			icons[11] = "";
			icons[13] = "";
			icons[50] = "";
		} else {
			icons[1] = "望";
			icons[2] = "";
			icons[3] = "";
			icons[4] = "";
			icons[9] = "";
			icons[10] = "";
			icons[11] = "";
			icons[13] = "";
			icons[50] = "";
		}
		// Hacky BS 
		char icon_id_but_better[2];
		sprintf(icon_id_but_better, "%c%c", icon_id[0], icon_id[1]);
		icon = icons[atoi(icon_id_but_better)];
	}

	// Output
	printf("%s %.0f°%c %s\n", weather, temperature, degreechar, icon);

	// Cleanup cJSON pointers
	cJSON_Delete(weather_json);

	return 0;
}
