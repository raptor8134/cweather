/*              ___       _____        ___  __     __
 *  \        / |      /\    |   |   | |    |  \   /
 *   \  /\  /  |---  /__\   |   |---| |--- |__/  |
 *    \/  \/   |___ /    \  |   |   | |___ |  \ * \__
 *
 *  Licensed under the GPLv3
 *
 *    Dependencies are cjson and curl, both available on most distros' package managers,
 *  try `make test` to check if they are installed. Pass the --location or -l option if 
 *  you want to get the coordinates of your location, convinient for opening a weather.
 *  com forecast from a script, like in a polybar module. Credit to the curl team for 
 *  the example code I used (from here: https://curl.se/libcurl/c/getinmemory.html)
 *
 *  Sample module: 
 *  [module/weather]
 *  type = custom/script
 *  exec = cweather 2>/dev/null # Just incase curl fails, cutoff the long error message
 *  tail = true
 *  inteval = 60
 *  click-left = $your-browser-here https://weather.com/weather/tenday/$(cweather --location)?par=google&temp=f # Replace the 'f' with 'c' if you want metric units
 */    

//#define API_KEY "" // Uncomment and paste your api key between the quotes if desired

#include <stdio.h>              //
#include <stdlib.h>             //
#include <string.h>             //
#include <curl/curl.h>          // To download stuff
#include <cjson/cJSON.h>        // For json parsing
#include <cjson/cJSON_Utils.h>  // ^^
#include <getopt.h>             // Get command line options

// Macros to help with writing cJSON code
#define getjson cJSON_GetObjectItemCaseSensitive
#define printj cJSON_Print 
#define ISEMPTY(VAL) VAL ## 1

#ifdef API_KEY
    static int key_flag = 1;
#else
    char* API_KEY;
    static int key_flag = 0;
#endif

// 1st one for day, second for night
// id numbers to list map {1,2,3,4,9,10,11,12,13,50}, I did some ternary
// magic on the given icon id to make it fit this list
// Icons may not display properly if nerd fonts are not installed
char *icon_array[2][10] = {
    {"󰖙", "", "", "", "", "", "", "", "", ""},
    {"", "", "", "", "", "", "", "", "", ""}
};

void help(char* name) {
printf("\
usage: %s [options]\n\
  options:\n\
    -h | --help         Display this help message\n\
    -k | --key <apikey> Define api key (not necessary if compiled in)\n\
    -c | --celsius      Changes the temperature scale to celcius\n\
    -C | --city         Manually input a city name\n\
    -l | --location     Print latitude and longitude seperated by a comma\n\
    -s | --simple       Only use day/night icons instead of the full set\n\
", name);
}

// Gets the quotes off the json output 
char *dequote(char *input) {
    int length = strlen(input);
    char *p = (char*)malloc(length-1);
    strncpy(p, input+1, length-2);
    p[length-2] = '\0';
    return p;
}

// Escapes spaces in tricky city names (shoutout to St. Augustine for 'finding' this bug)
// May also need to escape other characters but I haven't encountered them yet
char *spacereplace(char *input) {
    int inlen  = strlen(input);
    int outlen = inlen + 1;
    char* output = (char*)malloc(outlen);
    for (int i = 0, j = 0; i<inlen; i++, j++) {
        if (input[i] == ' ') {
            outlen+=2;
            realloc(output, outlen);
            output[j] = '%';
            j+=1;
            output[j] = '2';
            j+=1;
            output[j] = '0';
        } else {
            output[j] = input[i];
        }
    }
    output[outlen-1] = '\0';
    return output;
}

// Get command-line options with getopt
char *city  = "";
static int help_flag=0, centigrade_flag=0, location_flag=0, icon_flag=0;
void getoptions(int argc, char** argv) {
    int c;
    for (;;) {
        static struct option long_options[] =
        {
            {"help",        no_argument,        0, 'h'},
            {"celsius",     no_argument,        0, 'c'},
            {"city",        required_argument,  0, 'C'},
            {"location",    no_argument,        0, 'l'},
            {"simple",      no_argument,        0, 's'},
            {"key",         required_argument,  0, 'k'},
        };

        int option_index = 0;
        c = getopt_long(argc, argv, "hcClsk:", long_options, &option_index);

        if (c == -1) { break; }

        switch(c) {
            case 0:        // do nothing
                break;
            case 'h':
                help_flag = 1;
                break;
            case 'c':
                centigrade_flag = 1;
                break;
            case 'C':
                city = spacereplace(optarg);
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

// XXX Functions from https://curl.se/libcurl/c/getinmemory.html
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
// XXX End copied functions

char *curl(char *url) {
    // getting a bunch of stuff ready
    CURL* curl_handle;
    CURLcode res;

    struct MemoryStruct chunk;
    chunk.memory = malloc(1);
    chunk.size = 0;

    // initiate the curl session
    curl_handle = curl_easy_init();
    // specify url
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    // send data to memory 
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    // what
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
    // add user-agent field to appease the internet gods
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    res = curl_easy_perform(curl_handle); // get the data

    // Now to see if we screwed up or not
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        return NULL;
    }
    else {
        curl_easy_cleanup(curl_handle);
        return chunk.memory;
    }
        
    //free(chunk.memory);
}

int main(int argc, char **argv) { 
    getoptions(argc, argv);
    curl_global_init(CURL_GLOBAL_ALL); // Initiates the global curl instance 

    // Get location from public api
    char *location_api_url = "http://ip-api.com/json/"; 
    char *raw_data_location = curl(location_api_url);
    cJSON *location_json = cJSON_Parse(raw_data_location);
    free(raw_data_location);
    if (strlen(city)==0) {
        char *dequoted = dequote(printj(getjson(location_json, "city")));
        city = spacereplace(dequoted);
        free(dequoted);
    }
    
    if (location_flag == 1) {
        float lat, lon;
            lat = atof(printj(getjson(location_json, "lat")));
            lon = atof(printj(getjson(location_json, "lon")));
        printf("%.2f,%.2f\n", lat, lon);
        curl_global_cleanup(); // Stops and cleans up the global curl instance 
        return 0;
    }

    if (help_flag == 1) {
        help(argv[0]);
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

    // Get json from openweathermap.org
    char weather_api_url[1024];
    sprintf(weather_api_url,
        "https://api.openweathermap.org/data/2.5/weather?q=%s&units=%s&appid=%s",
        city, units, API_KEY); 
    cJSON* weather_json = cJSON_Parse(curl(weather_api_url));

    // Stops and cleans up the global curl instance
    curl_global_cleanup();  
 
    // Declare some variables
    char *weather, *sky, *icon_id;
    float temperature;

    // Get the weather data out of the json and put it in some variables
    temperature = atof(printj(getjson(getjson(weather_json, "main"), "temp")));    
    weather = dequote(printj(getjson(
                cJSON_GetArrayItem(getjson(weather_json, "weather"), 0), "main")));
    icon_id = dequote(printj(getjson(
                cJSON_GetArrayItem(getjson(weather_json, "weather"), 0), "icon")));

    // Icon
    char *icon;
    int isdaytime = icon_id[2] == 'd';
    if (icon_flag) {
        if (isdaytime) icon = "☀";
        else icon = "☽";
    } else {
        icon_id[2] = '\0';
        int id = atoi(icon_id);
        id = id < 5 ? id : (id < 50 ? id - 5 : 10);
        icon = icon_array[isdaytime ? 0 : 1][id-1];
    }

    // Output
    printf("%s %.0f°%c %s\n", weather, temperature, degreechar, icon);

    // Cleanup cJSON pointers
    cJSON_Delete(weather_json);

    return 0;
}
