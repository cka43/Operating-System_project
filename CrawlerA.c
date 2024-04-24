#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <stdbool.h>
#include <curl/curl.h>

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
        perror("Error: Memory allocation failed");
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
        perror("Error: Memory allocation failed");
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

    CURL *curl = curl_easy_init();
    if (!curl) {
        perror("Error: Unable to initialize cURL");
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
            // Set the option to follow redirects
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

            // Perform the HTTP request
            CURLcode res = curl_easy_perform(curl);
            if (res != CURLE_OK) {
                fprintf(stderr, "cURL request failed: %s\n", curl_easy_strerror(res));
            }
        }

        free(url); // Free the URL after processing
    }

    // Cleanup cURL handle
    curl_easy_cleanup(curl);

    return NULL;
}

// Main function to drive the web crawler.
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <starting-url|max-depth>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Splitting the input argument into URL and maximum depth
    char *start_url = strtok(argv[1], "|");
    char *depth_str = strtok(NULL, "|");
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

    // Add starting URL to the queue
    enqueue(&queue, start_url);

    // Set up crawler parameters
    CrawlerParams params = {&queue, max_depth};

    pthread_t threads[NUM_THREADS];

    // Create worker threads
    for (int i = 0; i < NUM_THREADS; i++) {
        if (pthread_create(&threads[i], NULL, fetch_url, (void *)&params) != 0) {
            perror("Error: Failed to create thread");
            return EXIT_FAILURE;
        }
    }

    // Join threads after completion
    for (int i = 0; i < NUM_THREADS; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
            perror("Error: Failed to join thread");
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}
