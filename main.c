// Parse message body and store in the struct
// headers

#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>


typedef struct  {
    char *key;
    char *value;
} RquestHeader ;

typedef struct  {
	char method[8];
	char target[2048];
	char httpVersion[9];
    int content_len;
    int body;
    int header_count;
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
    msg->header_count = 0;

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

        msg->headers[msg->header_count].key = strndup(&buff[key_start], key_len);

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
        msg->headers[msg->header_count].value = strndup(&buff[val_start], val_len);

        if (strcmp("Content-Length", msg->headers[msg->header_count].key) == 0 ) {
            msg->body = 1;
            msg->content_len = atoi(msg->headers[msg->header_count].value);
        }

        msg->header_count += 1;
        i += 2;

    }


}

int check_header_eol(int head, size_t bytes, char *buff) {
    while(head <= bytes) {
        if (buff[head] == '\r' && buff[head + 1] == '\n') {
            if (buff[head + 2] == '\r' && buff[head +3] == '\n') {
                head += 4;
                return bytes - head;
            }
        }
        head++;
    }

    return -1;
}

int main() {
    int opt = 1;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr  = {
        .sin_family = AF_INET,
        .sin_port = htons(9000),
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

    int read_offset = 0;
    int msg_start = -1;
    int msg_flag = 0;
    while (1) {
        printf("reading remaining data");
        int bytes = recv(client_fd, buff + read_offset, sizeof buff - read_offset, 0);
        if (bytes == -1) {
            switch (errno) {
                case EAGAIN:
                    usleep(20000);
                    break;
                default:
                    perror("Failed to read from socket\n");
                    close(client_fd);
                    break;
            }
            break;
        }
        if (bytes == 0) {
            printf("The client has closed the connection\n");
            break;
        }

        int header_chk = check_header_eol(read_offset, bytes, buff);
        if (header_chk == 0) {
            printf("Recieved all headers\n");
            msg_flag = 1;
            read_offset += bytes;
            break;
        } else if (header_chk > 0) {
            printf("Recieved all headers and body conent\n");
            msg_flag = 1;
            msg_start = read_offset + bytes - header_chk;
            read_offset += bytes;
            break;
        }

        read_offset += bytes;
    }

    HTTPMessage message;
    int offset = parse_req_start_line(&message, buff, sizeof buff);
    parse_headers(&offset, &message, buff, sizeof buff);

    if (msg_flag) {
        while((read_offset - msg_start) < message.content_len) {
            int bytes = recv(client_fd, buff + read_offset, sizeof(buff) - read_offset, 0);
            if (bytes == -1) {
                switch (errno) {
                    case EAGAIN:
                        usleep(20000);
                        break;
                    default:
                        perror("Failed to read from socket\n");
                        close(client_fd);
                        break;
                }
                break;
            }
            if (bytes == 0) {
                printf("The client has closed the connection\n");
                break;
            }
            read_offset += bytes;
        }

    }

    for(int j = msg_start; j <= msg_start + message.content_len; j++){
        printf("%c", buff[j]);
    }


    close(client_fd);
    close(fd);
    return 0;

    write(client_fd, "Hello back", sizeof("Hello back"));
    close(client_fd);
    close(fd);
}

