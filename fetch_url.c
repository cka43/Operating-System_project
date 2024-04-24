#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <stdbool.h>
#include <curl/curl.h>
#include <regex.h>

#define MAX_URL_LENGTH 1024

// Structure to hold crawler parameters
typedef struct {
    int max_depth;
} CrawlerParams;

// Function to handle received data from cURL.
size_t write_data(void *ptr, size_t size, size_t nmemb, void *userdata) {
    // Here you can process the received data if needed.
    return size * nmemb;
}

void *fetch_url(void *arg) {
    CrawlerParams *params = (CrawlerParams *)arg;
    int max_depth = params->max_depth;

    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Error: Unable to initialize cURL\n");
        return NULL;
    }

    // Regular expression for extracting links from HTML content
    regex_t regex;
    const char *pattern = "<a\\s+href=\"([^\"]+)\"";
    if (regcomp(&regex, pattern, REG_EXTENDED) != 0) {
        fprintf(stderr, "Error: Unable to compile regular expression\n");
        curl_easy_cleanup(curl);
        return NULL;
    }

    // Set the write callback function
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);

    // URL to fetch (replace google.com with the desired URL)
    const char *url = "http://google.com";

    curl_easy_setopt(curl, CURLOPT_URL, url);

    char response_buffer[4096];
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response_buffer);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "Error: cURL request failed: %s\n", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        return NULL;
    }

    regmatch_t matches[2];
    const char *ptr = response_buffer;
    while (regexec(&regex, ptr, 2, matches, 0) == 0) {
        char extracted_url[MAX_URL_LENGTH];
        int len = matches[1].rm_eo - matches[1].rm_so;
        if (len < MAX_URL_LENGTH) {
            strncpy(extracted_url, ptr + matches[1].rm_so, len);
            extracted_url[len] = '\0';
            printf("Extracted URL: %s\n", extracted_url);
        }
        ptr += matches[0].rm_eo;
    }

    regfree(&regex);
    curl_easy_cleanup(curl);

    return NULL;
}

int main() {
    // Set up crawler parameters
    CrawlerParams params = { .max_depth = 3 }; // Example max_depth
    fetch_url(&params);
    return 0;
}
