#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>

#include "queue.h"

#define handle_error_en(en, msg) \
        do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

#define handle_error(msg) \
        do { perror(msg); exit(EXIT_FAILURE); } while (0)

typedef struct QueueStruct 
{
    uint rindex;            //buffer read index
    uint windex;            //buffer write index
    size_t size;            //buffer size
    void   **data;          //buffer data pointing to the file data
    sem_t read;             //read and write sems used to stop the buffer rindex getting ahead of the windex
    sem_t write;            //
	pthread_mutex_t lock;   //queue resource protection
} Queue;


Queue *queue_alloc(int size) 
{
	Queue* queue = malloc(sizeof(Queue));   //allocate some memory for the queue
    queue->rindex = 0;                      //initialisations
    queue->windex = 0;
    queue->size = (size_t) size;
    queue->data = (void **)calloc(size, sizeof(void **));
    sem_init(&queue->read, 0, 0);           //init to 0 as nothing to read at start, sems shared between threads!
    sem_init(&queue->write, 0, size);       //init to size of buffer as there is that much space available, sems shared between threads!
    pthread_mutex_init(&queue->lock, NULL); //init mutex to neither locked or unlock to avoid deadlock
    return queue;
}

void queue_free(Queue *queue) 
{
    //clean up
    queue->rindex = 0;  //resets
    queue->windex = 0;
    queue->size = 0;
    free(queue->data);  //free the data in the queue
    queue->data = NULL; //NULL the data (not necessary)
    free(queue);        //free the queue 
}

void queue_put(Queue *queue, void *item) 
{
    sem_wait(&queue->write);            //wait until a write signal is given (given at init stage)
    pthread_mutex_lock(&queue->lock);   //protect queue from race conditions with concurrent accesses
    
    queue->data[queue->windex] = item;  //put item in queue at current write index and increment
    queue->windex++;                    
    
    if (queue->windex >= queue->size)   //if we are at the end of the buffer, wrap back to the start!
    {
        queue->windex = 0;              //kinda the whole idea behind a circular buffer!
    }
    
    pthread_mutex_unlock(&queue->lock); //unlock the queue for program to do stuff
    sem_post(&queue->read);             //signal there is data available in the buffer
}

void *queue_get(Queue *queue) 
{
    sem_wait(&queue->read);                     //wait until read signal is given
    pthread_mutex_lock(&queue->lock);           //protect queue
                                
    void *data = queue->data[queue->rindex];    //get the data from buffer at current read index and increment
    queue->rindex++;                        
    
    if (queue->rindex >= queue->size)           //wrap around if at end of buffer
    {
        queue->rindex = 0;
    }
        
    pthread_mutex_unlock(&queue->lock);        //unlock the queue to be used again
    sem_post(&queue->write);                   //signal data has been read and available for overwriting
    return data;                        
}

