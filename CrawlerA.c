#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <stdbool.h>
#include <curl/curl.h>
#include <regex.h>

#define MAX_URL_LENGTH 1024
#define NUM_THREADS 4

// Structure for queue elements.
typedef struct URLQueueNode {
    char url[MAX_URL_LENGTH];
    struct URLQueueNode *next;
} URLQueueNode;

// Structure for a thread-safe queue.
typedef struct {
    URLQueueNode *head, *tail;
    pthread_mutex_t lock;
} URLQueue;

// Structure to hold crawler parameters
typedef struct {
    URLQueue *queue;
    int max_depth;
    FILE *output_file; // Added file pointer
} CrawlerParams;

// Initialize a URL queue.
void initQueue(URLQueue *queue) {
    queue->head = queue->tail = NULL;
    pthread_mutex_init(&queue->lock, NULL);
}

// Add a URL to the queue.
void enqueue(URLQueue *queue, const char *url) {
    URLQueueNode *newNode = (URLQueueNode *)malloc(sizeof(URLQueueNode));
    if (!newNode) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    strncpy(newNode->url, url, MAX_URL_LENGTH - 1);
    newNode->url[MAX_URL_LENGTH - 1] = '\0';
    newNode->next = NULL;

    pthread_mutex_lock(&queue->lock);
    if (queue->tail) {
        queue->tail->next = newNode;
    } else {
        queue->head = newNode;
    }
    queue->tail = newNode;
    pthread_mutex_unlock(&queue->lock);
}

// Remove a URL from the queue.
char *dequeue(URLQueue *queue) {
    pthread_mutex_lock(&queue->lock);
    if (queue->head == NULL) {
        pthread_mutex_unlock(&queue->lock);
        return NULL;
    }

    URLQueueNode *temp = queue->head;
    char *url = strdup(temp->url);
    if (!url) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
    queue->head = queue->head->next;
    if (queue->head == NULL) {
        queue->tail = NULL;
    }
    free(temp);
    pthread_mutex_unlock(&queue->lock);
    return url;
}

// Function to fetch and process a URL using cURL.
void *fetch_url(void *arg) {
    CrawlerParams *params = (CrawlerParams *)arg;
    URLQueue *queue = params->queue;
    int max_depth = params->max_depth;
    FILE *output_file = params->output_file; // Retrieve file pointer

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

    while (true) {
        char *url = dequeue(queue);
        if (url == NULL) {
            // Queue is empty, exit thread
            break;
        }

        // Print fetched URL
        printf("Fetched URL: %s\n", url);

        // Process URL at current depth
        if (max_depth > 0) {
            // Perform cURL request
            curl_easy_setopt(curl, CURLOPT_URL, url);

            // Store response in a buffer
            char response_buffer[4096]; // Adjust buffer size as needed
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_buffer);

            // Perform cURL request
            CURLcode res = curl_easy_perform(curl);
            if (res != CURLE_OK) {
                fprintf(stderr, "Error: cURL request failed: %s\n", curl_easy_strerror(res));
                free(url);
                continue;
            }

            // Extract links from HTML content using regular expressions
            regmatch_t matches[2]; // We're interested in the captured group
            const char *ptr = response_buffer;
            while (regexec(&regex, ptr, 2, matches, 0) == 0) {
                // Extract the URL from the matched portion
                char extracted_url[MAX_URL_LENGTH];
                int len = matches[1].rm_eo - matches[1].rm_so;
                if (len < MAX_URL_LENGTH) {
                    strncpy(extracted_url, ptr + matches[1].rm_so, len);
                    extracted_url[len] = '\0';
                    enqueue(queue, extracted_url);
                }
                // Move to the next match
                ptr += matches[0].rm_eo;
            }

            // Write the URL to the output file
            fprintf(output_file, "%s\n", url);
            fflush(output_file); // Flush the output to ensure it's written immediately
        }

        free(url); // Free the URL after processing
    }

    // Clean up resources
    regfree(&regex);
    curl_easy_cleanup(curl);

    return NULL;
}

// Function to record errors into a text file.
void record_error(const char *error_message) {
    FILE *error_file = fopen("error_log.txt", "a"); // Open the error log file in append mode
    if (error_file == NULL) {
        fprintf(stderr, "Error: Unable to open error log file\n");
        return;
    }

    // Get current time
    time_t current_time;
    struct tm *local_time;
    char timestamp[20];
    time(&current_time);
    local_time = localtime(&current_time);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", local_time);

    // Write error message and timestamp to the error log file
    fprintf(error_file, "[%s] %s\n", timestamp, error_message);
    fclose(error_file);
}


// Main function to drive the web crawler.
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <starting-url:max-depth>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Splitting the input argument into URL and maximum depth
    char *start_url = strtok(argv[1], ":");
    char *depth_str = strtok(NULL, ":");
    if (start_url == NULL || depth_str == NULL) {
        fprintf(stderr, "Error: Invalid input format\n");
        return EXIT_FAILURE;
    }

    int max_depth = atoi(depth_str);
    if (max_depth <= 0) {
        fprintf(stderr, "Error: Maximum depth must be a positive integer\n");
        return EXIT_FAILURE;
    }

    URLQueue queue;
    initQueue(&queue);

    // Open output file for writing
    FILE *output_file = fopen("output.txt", "w");
    if (!output_file) {
        record_error("Unable to open output file");
        return EXIT_FAILURE;
    }

    // Set up crawler parameters
    CrawlerParams params = {&queue, max_depth, output_file};

    // Add starting URL to the queue
    enqueue(&queue, start_url);

    pthread_t threads[NUM_THREADS];

    // Create worker threads
    for (int i = 0; i < NUM_THREADS; i++) {
        if (pthread_create(&threads[i], NULL, fetch_url, (void *)&params) != 0) {
            record_error("Failed to create thread");
            return EXIT_FAILURE;
        }
    }

    // Join threads after completion
    for (int i = 0; i < NUM_THREADS; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
            record_error("Failed to join thread");
            return EXIT_FAILURE;
        }
    }

    // Close the output file
    fclose(output_file);

    return EXIT_SUCCESS;
}
