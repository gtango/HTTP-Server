#include <sys/socket.h>
#include <sys/stat.h>
#include <stdio.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <fcntl.h>
#include <unistd.h> // write
#include <string.h> // memset
#include <stdlib.h> // atoi
#include <stdbool.h> // true, false
#include <ctype.h>
#include <errno.h>

#define BUFFER_SIZE 4096
#define GOOD_HTTP_VERSION "HTTP/1.1"
#define CARRIAGE "\r\n"
#define DOUBLECAR "\r\n\r\n"
#define CONTENT_START "Content-Length:"
#define FILE_TOO_LARGE 27

struct httpObject {
    /*
        Create some object 'struct' to keep track of all
        the components related to a HTTP message
        NOTE: There may be more member variables you would want to add
    */
    char method[5];         // PUT, HEAD, GET
    char filename[28];      // what is the file we are worried about
    char httpversion[9];    // HTTP/1.1
    ssize_t content_length; // example: 13
    int status_code;
    uint8_t buffer[BUFFER_SIZE];
};

/*
    \brief 1. Want to read in the HTTP message/ data coming in from socket
    \param client_sockd - socket file descriptor
    \param message - object we want to 'fill in' as we read in the HTTP message
*/
void read_http_response(ssize_t client_sockd, struct httpObject* message) {

    //If there was an error beforehand, skip this part and send message
    if(message->status_code > 201){
        return;
    }

    uint8_t fileBuff[BUFFER_SIZE + 1];

    ssize_t read_socket_bytes = recv(client_sockd, fileBuff, BUFFER_SIZE, 0); //Read the message
    char* clfr = strstr((char*)fileBuff, DOUBLECAR);

    printf("%s\n", fileBuff);
    
    //Check if we have a double carriage. If not, either read till we do or we read too much
    while(clfr == NULL){

        //Error: No carriage and recieved too much data
        if(read_socket_bytes >= BUFFER_SIZE){
            message->status_code = 400;
            return;
        } else {
            read_socket_bytes = recv(client_sockd, fileBuff, BUFFER_SIZE, 0);
            fileBuff[read_socket_bytes] = '\0';
        }
        clfr = strstr((char*)fileBuff, DOUBLECAR);

    }

    //Parses the first line of the client message and puts the things into the correct message attributes
    char* header_buff = (char*)fileBuff;
    char* headers = strtok_r(header_buff, CARRIAGE, &header_buff);
    sscanf(headers, "%s %*[/]%s %s ", message->method, message->filename, message->httpversion);

    //Check if filename is too big
    if(strlen(message->filename) > FILE_TOO_LARGE){
        message->status_code = 400;
        return;
    }

    //Check if method passed is of correct form
    if(strcmp(message->method, "PUT") != 0){
        if (strcmp(message->method, "GET") != 0){
            if(strcmp(message->method, "HEAD") != 0){
                message->status_code = 400;
                return;
            }
        } 
    }

    //Checks if https version is allowed
    if(strcmp(message->httpversion, GOOD_HTTP_VERSION) != 0){
        message->status_code = 400;
        return;
    }

    //Parsing the rest of the headers
    while(STDOUT_FILENO){

        headers = strtok_r(header_buff, CARRIAGE, &header_buff);
        char* content_len = NULL;
        char* ignore_head = NULL;
    
        //Look for Content-Length Header and Regular Heads when we are not on the carriage 
        if(headers != NULL){
            content_len = strstr(headers, CONTENT_START);
            ignore_head = strstr(headers, ":");
        }
        
        if(headers == NULL){
            break;
        }

        //If the header we are working with is content-length, extract data. Otherwise, move on.
        if(content_len != NULL){
            //Get the amount of content client is trying to send
            int number_files_Filled = sscanf(headers, "Content-Length: %zd", &(message->content_length));
            if(number_files_Filled < 1){
                message->status_code = 400;
                return;
            }
        } else if (ignore_head != NULL){
            continue;
        } else {
            message->status_code = 400;
            return;
        }

    }
}

/*
    \brief 2. Want to process the message we just recieved
*/
void process_request(ssize_t client_sockd, struct httpObject* message) {

    int filename_length = strlen(message->filename);
    ssize_t read_file_bytes;
    ssize_t writtenBytes = 0;

    //If there was an error beforehand, skip this part and send message
    if(message->status_code > 201){
        return;
    }

    //Check if file has valid characters
    for(int i = 0; i < filename_length;i++){
        if (message->filename[i] < 'A' || message->filename[i] > 'Z'){
            if (message->filename[i] < 'a' || message->filename[i] > 'z'){
                if(message->filename[i] < 48 || message->filename[i] > 57){
                    if(message->filename[i] != 95 && message->filename[i] != 45){
                        message->status_code = 400;
                        return;
                    }
                }
            }
        }
    }

    //Handle PUT request
    //Need to open a file, 
    if(strcmp(message->method, "PUT") == 0){

        int fd = open(message->filename, O_CREAT|O_RDWR|O_TRUNC, 0666);

        //If there is an error, determine what error it is
        if(fd < 0){
            //Catch-All 400 Error
            if(errno == 1 || errno == 9){
                message->status_code = 400;
                return;
            }
    
            //File does not exist
            if(errno == 2){
                message->status_code = 404;
		        return;
	        }

            //File permissions are strict
            if(errno == 13){
                message->status_code = 403;
                return;
            }
        }

        //Write contents of header to new file
        while(writtenBytes != message->content_length){

                read_file_bytes = recv(client_sockd, message->buffer, sizeof(message->buffer), 0);

                if(read_file_bytes < 0){
                    message->status_code = 400;
			        return;
		        } else if (read_file_bytes == 0){
			        break;
		        }

                read_file_bytes = write(fd, message->buffer, read_file_bytes);
                writtenBytes += read_file_bytes;
        }
        
        message->status_code = 201;
        return;
    }

    //Handle GET request & HEAD requests since they act the same
    if(strcmp(message->method, "GET") == 0 || strcmp(message->method, "HEAD") == 0){

        int fd = open(message->filename, O_RDWR);
        struct stat statbuf;

        if(fd < 0){
            //Catch-All 400 Error
            if(errno == 1 || errno == 9){
                message->status_code = 400;
                return;
            }
    
            //File does not exist
            if(errno == 2){
                message->status_code = 404;
		        return;
	        }

            //File permissions are strict
            if(errno == 13){
                message->status_code = 403;
                return;
            }
        }

        fstat(fd, &statbuf);
        message->content_length = statbuf.st_size;

        //Read file
        while(writtenBytes != statbuf.st_size){

            read_file_bytes = read(fd, message->buffer, sizeof(message->buffer));

            if(read_file_bytes < 0){
                message->status_code = 400;
			    return;
		    } else if (read_file_bytes == 0){
			    break;
		    }

        }

        message->status_code = 200;
        return;

    }

    return;
}

/*
    \brief 3. Construct some response based on the HTTP request you recieved
*/
void construct_http_response(ssize_t client_sockd, struct httpObject* message) {

    int fd = open(message->filename, O_RDWR);
    ssize_t printContent;
    ssize_t writtenBytes = 0;
    char* correctOutput;
    char content_temp[50];
    sprintf(content_temp, "%ld", message->content_length);

    //200 OK - GOOD GET OR HEAD REQUESTS
    if(message->status_code == 200){

        correctOutput = (char*)"HTTP/1.1 200 OK\r\nContent-Length: ";

        if(strcmp("GET", message->method) == 0){
            send(client_sockd, correctOutput, strlen(correctOutput), 0);
            send(client_sockd, content_temp, strlen(content_temp),0);
            send(client_sockd, "\r\n\r\n", strlen("\r\n\r\n"), 0);

                //Read file
                while(writtenBytes != message->content_length){

                printContent = read(fd, message->buffer, sizeof(message->buffer));

                if (printContent == 0){
			        break;
		        }

                printContent = write(client_sockd, (char*)message->buffer, printContent);
                writtenBytes += printContent;
                }
        }

        if(strcmp(message->method, "HEAD") == 0){
            send(client_sockd, correctOutput, strlen(correctOutput), 0);
            send(client_sockd, content_temp, strlen(content_temp),0);
            send(client_sockd, "\r\n\r\n", strlen("\r\n\r\n"), 0);
        }

    }

    //201 CREATED - GOOD PUT REQUEST
    if(message->status_code == 201){
        correctOutput = (char*)"HTTP/1.1 201 Created\r\nContent-Length: 0\r\n\r\n";
        send(client_sockd, correctOutput, strlen(correctOutput), 0);
    }

    //403 FORBIDDEN REQUESTED
    if(message->status_code == 403){
        printf("We are here\n");
        correctOutput = (char*)"HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\n\r\n";
        send(client_sockd, correctOutput, strlen(correctOutput), 0);
    }

    //404 FILE REQUESTED NOT FOUND
    if(message->status_code == 404){
        correctOutput = (char*)"HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
        send(client_sockd, correctOutput, strlen(correctOutput), 0);
    }

    //400 FILE THAT WAS PROVIDED WAS CORRUPT
    if(message->status_code == 400){
        correctOutput = (char*)"HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
        send(client_sockd, correctOutput, strlen(correctOutput), 0);
    }

    //CLEAR ALL DATA
    message->content_length = 0;
    memset(message->buffer, 0, sizeof(message->buffer));
    memset(message->method, 0, sizeof(message->method));
    memset(message->httpversion, 0, sizeof(message->httpversion));
    message->status_code = 0;

    return;
}


int main(int argc, char* argv[]) {
    /*
        Create sockaddr_in with server information
    */
    char* port = argv[1];

    if(argc != 2){
        exit(EXIT_FAILURE);
    }

    if(strcmp(argv[1], "8000") < 0){
        exit(EXIT_FAILURE);
    }
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(port));
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    socklen_t addrlen = sizeof(server_addr);

    /*
        Create server socket
    */
    int server_sockd = socket(AF_INET, SOCK_STREAM, 0);

    // Need to check if server_sockd < 0, meaning an error
    if (server_sockd < 0) {
        perror("socket");
    }

    /*
        Configure server socket
    */
    int enable = 1;

    /*
        This allows you to avoid: 'Bind: Address Already in Use' error
    */
    int ret = setsockopt(server_sockd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));

    /*
        Bind server address to socket that is open
    */
    ret = bind(server_sockd, (struct sockaddr *) &server_addr, addrlen);

    /*
        Listen for incoming connections
    */
    ret = listen(server_sockd, 5); // 5 should be enough, if not use SOMAXCONN

    if (ret < 0) {
        return EXIT_FAILURE;
    }

    /*
        Connecting with a client
    */
    struct sockaddr client_addr;
    socklen_t client_addrlen;

    struct httpObject message;

    while (true) {
        /*
         * 1. Accept Connection
         */
        int client_sockd = accept(server_sockd, &client_addr, &client_addrlen);
        // Remember errors happen

        /*
         * 2. Read HTTP Message
         */
        read_http_response(client_sockd, &message);

        /*
         * 3. Process Request
         */
        process_request(client_sockd, &message);

        /*
         * 4. Construct Response
         */
        construct_http_response(client_sockd, &message);

        /*
         * 5. Send Response
         */
        close(client_sockd);
        continue;

    }

    close(server_sockd);

    return EXIT_SUCCESS;
}
