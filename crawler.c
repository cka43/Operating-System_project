#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

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

// Function to duplicate a string
char *my_strdup(const char *src) {
    size_t len = strlen(src) + 1; // Include space for null terminator
    char *dest = malloc(len);
    if (dest != NULL) {
        memcpy(dest, src, len);
    }
    return dest;
}

// Remove a URL from the queue.
char *dequeue(URLQueue *queue) {
    pthread_mutex_lock(&queue->lock);
    if (queue->head == NULL) {
        pthread_mutex_unlock(&queue->lock);
        return NULL;
    }

    URLQueueNode *temp = queue->head;
    char *url = my_strdup(temp->url);
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

// Function to parse HTML content and extract links
void parseHTML(const char *html_content, URLQueue *queue) {
    // Implement HTML parsing logic here
    // Extract links and enqueue them into the queue
    // Example:
    // enqueue(queue, "https://example.com/page1");
    // enqueue(queue, "https://example.com/page2");
}

// Function to fetch and process a URL.
void *fetch_url(void *arg) {
    CrawlerParams *params = (CrawlerParams *)arg;
    URLQueue *queue = params->queue;
    int max_depth = params->max_depth;

    while (true) {
        char *url = dequeue(queue);
        if (url == NULL) {
            // Queue is empty, exit thread
            break;
        }

        // TODO: Fetch the URL and process its content
        // Example: Fetch HTML content from the URL

        // Simulate parsing HTML content
        parseHTML("<html><body><a href='https://example.com/page1'>Page 1</a></body></html>", queue);

        free(url); // Free the URL after processing
    }

    return NULL;
}

// Main function to drive the web crawler.
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <starting-url>\n", argv[0]);
        return EXIT_FAILURE;
    }

    URLQueue queue;
    initQueue(&queue);
    enqueue(&queue, argv[1]);

    // Set up crawler parameters
    CrawlerParams params = {&queue, MAX_DEPTH};

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

    return EXIT_SUCCESS;
}

