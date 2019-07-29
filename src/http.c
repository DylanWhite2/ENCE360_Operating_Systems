#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

#include "http.h"

#define BUF_SIZE 1024
#define REQ_LEN 128         //to pad the req_string_len used in send()
#define FLAGS 0             //flags for send/recv calls

Buffer* init_buffer(size_t size) 
{
    Buffer* buffer = (Buffer *)malloc(sizeof(Buffer));  //allocate memory for the buffer and the data in the buffer
    buffer->data = (char *)calloc(size, sizeof(char));
    buffer->length = size;                              
    return buffer;
}

Buffer* http_query(char *host, char *page, int port)
{
    Buffer* file = init_buffer(BUF_SIZE);   //make the buffer
    uint recv_size = 0;                     //total size of recvd file, used to index array when recving data
    uint recv_bits = !0;                    //bits recved, 0 when file xfer completed
    uint request_string_len;                //length of the HTTP request string
    char* request_string = NULL;            //HTTP request sent to server
    int sockfd;                             //socket connection stuff
    char addr_port[20];                     
    struct addrinfo their_info;             
    struct addrinfo *their_addr = NULL;     

    sockfd = socket(AF_INET, SOCK_STREAM, 0); //socket descriptor

    if (sockfd == -1)   
    {
        perror("socket failed"); //whoops something went wrong!
        exit(1);
    }
    
    memset(&their_info, 0, sizeof(struct addrinfo));
    their_info.ai_family = AF_INET;
    their_info.ai_socktype = SOCK_STREAM;   //TCP
    sprintf(addr_port, "%d", port);
    
    if ((getaddrinfo(host, addr_port, &their_info, &their_addr)) != 0)  //get the IP info
    {
        perror("getaddrinfo fail"); //whoops something went wrong!
        exit(1);
    }

    int rc = connect(sockfd, their_addr->ai_addr, their_addr->ai_addrlen);  //connect to the server

    if (rc == -1)
    {
        perror("connect failed"); //whoops something went wrong!
        exit(1);
    }

    request_string_len = REQ_LEN + strlen(host) + strlen(page);         //len of request string, needed for send()
                                                                        //padded with REQ_LEN = 128 to avoid chopping anything off
                                                                        
    request_string = (char *)calloc(request_string_len, sizeof(char));  //allocate some zeroed memory for the get request string
    
    sprintf(request_string, "GET /%s HTTP/1.0\r\nHost: %s\r\nUser-Agent: getter\r\n\r\n", page, host);  //HTTP request string sent to server with arguments passed in
    send(sockfd, request_string, request_string_len, FLAGS);                                            //send the get request string to the server

    while(recv_bits > 0)    //loop until there is no more data to recv from server
    {
        if (recv_size + BUF_SIZE > file->length)    //if out of space for the data, add another 1024 bytes to the length.
        {
            file->length = file->length + BUF_SIZE; 
            file->data = realloc(file->data, file->length); //realloc to avoid having to copy buffer contents 
        }

        recv_bits = recv(sockfd, file->data + recv_size, BUF_SIZE, FLAGS);  //write the recv data into the data array starting at address 0
        recv_size += recv_bits;                                             //update the total recv size as used to index the array for storing data
    }

    file->length = recv_size;  //only update the file length once full file has transferred rather than every iteration in the loop for effeciency
    
    //cleaning up resources
    close(sockfd);
    free(request_string);
    freeaddrinfo(their_addr);
    
    return file;    //return the file!
}

// split http content from the response string
char* http_get_content(Buffer *response)
{
    char* header_end = strstr(response->data, "\r\n\r\n");

    if (header_end) {
        return header_end + 4;
    }
    else {
        return response->data;
    }
}

Buffer *http_url(const char *url)
{
    char host[BUF_SIZE];
    strncpy(host, url, BUF_SIZE);

    char *page = strstr(host, "/");
    if (page) {
        page[0] = '\0';

        ++page;
        return http_query(host, page, 80);
    }
    else {

        fprintf(stderr, "could not split url into host/page %s\n", url);
        return NULL;
    }
}

