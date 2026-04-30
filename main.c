// Store headers in a hash map
// Today's goal. get tcp streaming working

#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>

typedef struct  {
    char key[100];
    char value[100];
} RquestHeader ;

typedef struct  {
	char method[8];
	char target[2048];
	char httpVersion[9];
    RquestHeader rqmap[100];
} HTTPMessage ;

void parse_req_start_line(HTTPMessage *msg, char *buff, size_t len) {

    int count = 0;
    int prv = 0;
    int i = 0;

    while (1) {
        printf("%c", buff[i]);
        i++;
    }

    // while (count < 3) {

    //     char *body_part;
    //     if (count == 0) {
    //         body_part = msg->method;
    //     } else if (count == 1){
    //         body_part = msg->target ;
    //     } else if (count == 2) {
    //         body_part = msg -> httpVersion;
    //     } 
        
    //     if (buff[i] == ' ') {

    //         int seq = prv;
    //         for (int ptr = prv; ptr < i; ptr++) {
    //             body_part[ptr-seq] = buff[ptr];
    //         }
    //         count++;
    //         prv = i + 1;
    //     }

    //     i++;

    //     if (buff[i] == '\r' && buff[i + 1] == '\n') {
    //         int seq = prv;
    //         for (int ptr = prv; ptr < i; ptr++) {
    //             body_part[ptr-seq] = buff[ptr];
    //         }
    //         count++;
    //         prv = i + 1;
    //         i += 2;
    //         if (buff[i] == '\r' && buff[i + 1] == '\n') {
    //             break;
    //         }
    //     }
    // }

}

void RequestParse() {}

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
    parse_req_start_line(&message, buff, sizeof buff);
    printf("Method: %s\n", message.method);
    printf("Target: %s\n", message.target);
    printf("Http Version: %s\n", message.httpVersion);

	write(client_fd, "Hello back", sizeof("Hello back"));
	close(client_fd);
    close(fd);
}

