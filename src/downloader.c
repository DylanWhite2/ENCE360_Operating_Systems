#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>

#include "http.h"
#include "queue.h"

#define FILE_SIZE 256

typedef struct {
    char *url;
    Buffer *result;
}  Task;


typedef struct {
    Queue *todo;
    Queue *done;

    pthread_t *threads;
    int num_workers;

} Context;

void create_directory(const char *dir) {
    struct stat st = { 0 };

    if (stat(dir, &st) == -1) {
        int rc = mkdir(dir, 0700);
        if (rc == -1) {
            perror("mkdir");
            exit(EXIT_FAILURE);
        }
    }
}


void *worker_thread(void *arg) {
    Context *context = (Context *)arg;

    Task *task = (Task *)queue_get(context->todo);

    while (task) {
        task->result = http_url(task->url);

        queue_put(context->done, task);
        task = (Task *)queue_get(context->todo);
    }

    return NULL;
}


Context *spawn_workers(int num_workers) {
    Context *context = (Context*)malloc(sizeof(Context));

    context->todo = queue_alloc(num_workers * 2);
    context->done = queue_alloc(num_workers * 2);

    context->num_workers = num_workers;

    context->threads = (pthread_t*)malloc(sizeof(pthread_t) * num_workers);
    int i = 0;

    for (i = 0; i < num_workers; ++i) {
        if (pthread_create(&context->threads[i], NULL, worker_thread, context) != 0) {
            perror("pthread_create");
            exit(1);
        }
    }

    return context;
}

void free_workers(Context *context) {
    int num_workers = context->num_workers;
    int i = 0;

    for (i = 0; i < num_workers; ++i) {
        queue_put(context->todo, NULL);
    }

    for (i = 0; i < num_workers; ++i) {
        if (pthread_join(context->threads[i], NULL) != 0) {
            perror("pthread_join");
            exit(1);
        }
    }

    queue_free(context->todo);
    queue_free(context->done);

    free(context->threads);
    free(context);
}



Task *new_task(char *url) {
    Task *task = malloc(sizeof(Task));
    task->result = NULL;
    task->url = malloc(strlen(url) + 1);

    strcpy(task->url, url);

    return task;
}

void free_task(Task *task) {

    if (task->result) {
        free(task->result->data);
        free(task->result);
    }

    free(task->url);
    free(task);
}


void wait_task(const char *download_dir, Context *context) {
    char filename[FILE_SIZE], url_file[FILE_SIZE];
    Task *task = (Task*)queue_get(context->done);

    if (task->result) {

        strcpy(url_file, task->url);

        size_t len = strlen(url_file);
        for (int i = 0; i < len; ++i) {
            if (url_file[i] == '/') {
                url_file[i] = '|';
            }
        }


        snprintf(filename, FILE_SIZE, "%s/%s", download_dir, url_file);
        FILE *fp = fopen(filename, "w");

        if (fp == NULL) {
            fprintf(stderr, "error writing to: %s\n", filename);
            exit(EXIT_FAILURE);
        }

        char *data = http_get_content(task->result);
        if (data) {
            size_t length = task->result->length - (data - task->result->data);

            fwrite(data, 1, length, fp);
            fclose(fp);

            printf("downloaded %d bytes from %s\n", (int)length, task->url);
        }
        else {
            printf("error in response from %s\n", task->url);
        }

    }
    else {

        fprintf(stderr, "error downloading: %s\n", task->url);

    }

    free_task(task);
}

int main(int argc, char **argv) {


    if (argc != 4) {
        fprintf(stderr, "usage: ./downloader url_file num_workers download_dir\n");
        exit(1);
    }

    char *url_file = argv[1];
    int num_workers = atoi(argv[2]);
    char *download_dir = argv[3];

    create_directory(download_dir);
    FILE *fp = fopen(url_file, "r");
    char *line = NULL;
    size_t len = 0;

    if (fp == NULL) {
        exit(EXIT_FAILURE);
    }

    // spawn threads and create work queue(s)
    Context *context = spawn_workers(num_workers);       
    /* Wait for tasks to be added to the queue
     * Take the task off the queue and download into a buffer
     * Put the buffer onto another queue and repeat the process for every task
     */  

    int work = 0; //the number of tasks that need to be completed
    while ((len = getline(&line, &len, fp)) != -1) {         

    /* fp is the URL that we want to download from and is passed into the
     * line char* array which gets passed to each new_task to make a task
     * for the worker function. -1 checks that there is still URLs to pass
     * to the worker task.
     */                                                 

        //checking for newline char at end of line
        if (line[len - 1] == '\n') {  
            //set last char to null byte before putting on the queue
            line[len - 1] = '\0';    
        }   

        //increment the number of tasks available 
        ++work;  
        //put the task on the context TODO queue which will be downloaded.
        queue_put(context->todo, new_task(line));  

        //if we've filled the queue up enough, start getting results back
        //if all workers are busy - wait
        if (work >= num_workers) {   
            //take total tasks down to allow new tasks to be added as wait_task is about to start downloading                   
            --work;  
            //downloads the URL on the context TODO queue                                   
            wait_task(download_dir, context);          
        }
    }
    //out of main loop. while there is still final tasks to complete keep going
    while (work > 0) {   
        //decrement number of tasks                               
        --work;                      
         //same as above                   
        wait_task(download_dir, context);              
    }
    //cleanup
    fclose(fp);        //close the file pointer
    free(line);        //free allocated memory to avoid leaks.
    free_workers(context);
    return 0;
}
