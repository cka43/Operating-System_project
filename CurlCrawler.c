#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <stdbool.h>
#include <curl/curl.h>
#include <libxml/HTMLparser.h>

#define MAX_URL_LENGTH 1024
#define MAX_DEPTH 10
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
    int current_depth;
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
    int current_depth = params->current_depth;
    FILE *output_file = params->output_file; // Retrieve file pointer

    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Error: Unable to initialize cURL\n");
        return NULL;
    }

    while (true) {
        char *url = dequeue(queue);
        if (url == NULL) {
            // Queue is empty, exit thread
            break;
        }

        // Process URL at current depth
        if (current_depth < max_depth) {
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

            // Parse HTML content
            htmlDocPtr doc = htmlReadMemory(response_buffer, strlen(response_buffer), NULL, NULL, HTML_PARSE_RECOVER | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
            if (doc == NULL) {
                fprintf(stderr, "Error: Unable to parse HTML content\n");
                free(url);
                continue;
            }

            // Find anchor tags (links)
            xmlNodePtr cur = xmlDocGetRootElement(doc);
            for (cur = cur->children; cur != NULL; cur = cur->next) {
                if (cur->type == XML_ELEMENT_NODE && xmlStrcmp(cur->name, (const xmlChar *)"a") == 0) {
                    xmlChar *href = xmlGetProp(cur, (const xmlChar *)"href");
                    if (href != NULL) {
                        char *new_url = (char *)href;
                        enqueue(queue, new_url);
                        xmlFree(href);
                    }
                }
            }

            // Clean up libxml2 resources
            xmlFreeDoc(doc);
            xmlCleanupParser();

            // Write the URL to the output file
            fprintf(output_file, "%s\n", url);
            fflush(output_file); // Flush the output to ensure it's written immediately

            // Create new parameters for deeper level crawling
            CrawlerParams deeper_params = {queue, max_depth, current_depth + 1, output_file};
            // Continue crawling with deeper level parameters
            fetch_url((void *)&deeper_params);
        }

        free(url); // Free the URL after processing
    }

    curl_easy_cleanup(curl);

    return NULL;
}

// Main function to drive the
int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <starting-url> <max-depth>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *start_url = argv[1];
    int max_depth = atoi(argv[2]);
    if (max_depth <= 0) {
        fprintf(stderr, "Error: Maximum depth must be a positive integer\n");
        return EXIT_FAILURE;
    }

    URLQueue queue;
    initQueue(&queue);

    // Open output file for writing
    FILE *output_file = fopen("OPCrwaler.txt", "w");
    if (!output_file) {
        fprintf(stderr, "Error: Unable to open output file\n");
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
            fprintf(stderr, "Error: Failed to create thread %d\n", i);
            return EXIT_FAILURE;
        }
    }

    // Join threads after completion
    for (int i = 0; i < NUM_THREADS; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
            fprintf(stderr, "Error: Failed to join thread %d\n", i);
            return EXIT_FAILURE;
        }
    }

    // Close the output file
    fclose(output_file);

    return EXIT_SUCCESS;
}
