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

// Function to handle received data from cURL.
size_t write_data(void *ptr, size_t size, size_t nmemb, void *userdata) {
    return size * nmemb;
}

void *fetch_url(void *arg) {
    CrawlerParams *params = (CrawlerParams *)arg;
    URLQueue *queue = params->queue;
    int max_depth = params->max_depth;

    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "Error: Unable to initialize cURL\n");
        return NULL;
    }

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);

    while (true) {
        char *url = dequeue(queue);
        if (url == NULL) {
            break;
        }

        printf("Fetched URL: %s\n", url);

        if (max_depth > 0) {
            curl_easy_setopt(curl, CURLOPT_URL, url);

            // Perform HTTP request
            char response_buffer[4096];
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, response_buffer);
            CURLcode res = curl_easy_perform(curl);
            if (res != CURLE_OK) {
                fprintf(stderr, "Error: cURL request failed: %s\n", curl_easy_strerror(res));
                free(url);
                continue;
            }

            // Parse HTML response for URLs
            const char *start_tag = "<a href=\"";
            const char *end_tag = "\"";
            const char *ptr = response_buffer;
            while ((ptr = strstr(ptr, start_tag)) != NULL) {
                ptr += strlen(start_tag);
                const char *end_ptr = strstr(ptr, end_tag);
                if (end_ptr) {
                    char extracted_url[MAX_URL_LENGTH];
                    int len = end_ptr - ptr;
                    if (len < MAX_URL_LENGTH) {
                        strncpy(extracted_url, ptr, len);
                        extracted_url[len] = '\0';
                        printf("Extracted URL: %s\n", extracted_url); // Print extracted URL
                        enqueue(queue, extracted_url);
                    }
                    ptr = end_ptr;
                } else {
                    break;
                }
            }
        }

        free(url);
    }

    curl_easy_cleanup(curl);

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <starting-url|max-depth>\n", argv[0]);
        return EXIT_FAILURE;
    }

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

    enqueue(&queue, start_url);

    pthread_t threads[NUM_THREADS];

    CrawlerParams params = {&queue, max_depth};

    for (int i = 0; i < NUM_THREADS; i++) {
        if (pthread_create(&threads[i], NULL, fetch_url, (void *)&params) != 0) {
            perror("Error: Failed to create thread");
            return EXIT_FAILURE;
        }
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
            perror("Error: Failed to join thread");
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}
