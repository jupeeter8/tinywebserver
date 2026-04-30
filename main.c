// Parse message body and store in the struct
// headers

#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

typedef struct  {
    char *key;
    char *value;
} RquestHeader ;
~~
typedef struct  {
	char method[8];
	char target[2048];
	char httpVersion[9];
    int content_len;
    int body;
    RquestHeader headers[64];
} HTTPMessage ;

int parse_req_start_line(HTTPMessage *msg, char *buff, size_t len) {

    int count = 0;
    int prv = 0;
    int i = 0;

    while (count < 3) {

        char *body_part;
        if (count == 0) {
            body_part = msg->method;
        } else if (count == 1){
            body_part = msg->target ;
        } else if (count == 2) {
            body_part = msg -> httpVersion;
        } 
        
        if (buff[i] == ' ') {

            int seq = prv;
            for (int ptr = prv; ptr < i; ptr++) {
                body_part[ptr-seq] = buff[ptr];
            }
            count++;
            body_part[i-seq] = '\0';
            prv = i + 1;
        }

        i++;

        if (buff[i] == '\r' && buff[i + 1] == '\n') {
            int seq = prv;
            for (int ptr = prv; ptr < i; ptr++) {
                body_part[ptr-seq] = buff[ptr];
            }
            body_part[i-seq] = '\0';
            count++;
            prv = i + 1;
            i = i + 2;
            if (buff[i] == '\r' && buff[i + 1] == '\n') {
                i = i + 2;
                return i;
            }
        }

    }
    return i;

}

void parse_headers(int *offset, HTTPMessage *msg, char *buff, size_t len) {

    int i = *offset;
    int header_count = 0;

    while(i < len - 1) {


        if(buff[i] == '\r' && buff[i + 1] == '\n') {
            break;
        }


        int key_start = i;
        while (i < len - 1 && buff[i] != ':') {
            i++;
        }
        buff[i] = '\0';
        int key_len = i - key_start + 1;

        msg->headers[header_count].key = strndup(&buff[key_start], key_len);

        i++;
        if(i < len - 1 && buff[i] == ' ') {
            i++;
        }

        int val_start = i;
        while(i < len -1 && !(buff[i] == '\r' && buff[i + 1] == '\n')) {
            i++;
        }
        
        buff[i] = '\0';
        int val_len = i - val_start + 1;
        msg->headers[header_count].value = strndup(&buff[val_start], val_len);

        if (strcmp("Content-Length", msg->headers[header_count].key) == 0 ) {
            msg->body = 1;
            msg->content_len = atoi(msg->headers[header_count].value);
        }

        printf("%s: %s\n", msg->headers[header_count].key, msg->headers[header_count].value);
        header_count += 1;
        i += 2;

    }


}

int main() {
	int fd = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in addr  = {
		.sin_family = AF_INET,
		.sin_port = htons(80),
		.sin_addr.s_addr = INADDR_ANY
	};

	int error = bind(fd, (struct sockaddr *)&addr, sizeof(addr));
	if (error < 0) {
		perror("bind failed");
		return 1;
	}
	listen(fd, 5);

	int client_fd = accept(fd, NULL, NULL);

	char buff[4096];
	recv(client_fd, buff, sizeof buff, 0);

    HTTPMessage message;
    int offset = parse_req_start_line(&message, buff, sizeof buff);
    parse_headers(&offset, &message, buff, sizeof buff);


	write(client_fd, "Hello back", sizeof("Hello back"));
	close(client_fd);
    close(fd);
}

