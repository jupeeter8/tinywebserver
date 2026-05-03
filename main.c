// Parse message body and store in the struct
// headers

#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>


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
    char* msg_body;
    RquestHeader headers[64];
} HTTPMessage ;

struct sockaddr_in addr  = {
    .sin_family = AF_INET,
    .sin_port = htons(9000),
    .sin_addr.s_addr = INADDR_ANY
};

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
    msg->content_len = 0;
    msg->body = 0;
    msg->header_count = 0;

    while(i < len) {


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
    int total = bytes+head;
    while(head <= total) {
        if (buff[head] == '\r' && buff[head + 1] == '\n') {
            if (buff[head + 2] == '\r' && buff[head +3] == '\n') {
                head += 4;
                return total-head;
            }
        }
        head++;
    }

    return -1;
}

int read_failed(int bytes, int client_fd) {

    if (bytes == -1) {
        switch (errno) {
            case EAGAIN:
                usleep(20000);
                return 1;
            default:
                perror("Failed to read from socket\n");
                close(client_fd);
                return 0;
        }
    }
    if (bytes == 0) {
        printf("The client has closed the connection\n");
        return 0;
    }
    return 1;
}


void recv_msg(char* buff, int client_fd, HTTPMessage* msg, size_t len) {

    int reader = 0;
    int msg_start = -1;
    int msg_flag = 0;
    int total_bytes = 0;

    while (reader <= len) {
        int bytes = recv(client_fd, buff + reader, len  - reader, 0);

        if (!read_failed(bytes, client_fd)) {
            break;
        }
        total_bytes += bytes;

        int header_chk = check_header_eol(reader, bytes, buff);
        if (header_chk == 0) {
            msg_flag = 1;
            reader += bytes;
            msg_start = reader + bytes - header_chk;
            printf("total bytes: %d\n", total_bytes);
            break;
        } else if (header_chk > 0) {
            msg_flag = 1;
            msg_start = reader + bytes - header_chk;
            reader += bytes;
            printf("total bytes: %d\n", total_bytes);
            break;
        }

        printf("total bytes: %d\n", total_bytes);
        reader += bytes;
    }

    int offset = parse_req_start_line(msg, buff, len);
    parse_headers(&offset, msg, buff, len);

    if (msg_flag) {
        while((reader - msg_start) < msg->content_len) {
            int bytes = recv(client_fd, buff + reader, len - reader, 0);
            if (!read_failed(bytes, client_fd)) {
                break;
            }
            total_bytes += bytes;
            reader += bytes;
            printf("total bytes: %d\n", total_bytes);
        }
        msg->msg_body = strndup(&buff[msg_start], msg->content_len);

    }
}

void print_req(HTTPMessage* msg) {
    printf("%s %s %s -", msg->method, msg->target, msg->httpVersion);
    for (int i=0; i < msg->header_count; i++) {
        printf(" %s: %s", msg->headers[i].key, msg->headers[i].value);
    }
    if (msg->body) {
        printf(" %s\n", msg->msg_body);
    }
    printf("\n");


}

int main() {

    char buff[4096];


    int opt = 1;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    int error = bind(fd, (struct sockaddr *)&addr, sizeof(addr));
    if (error < 0) {
        perror("bind failed");
        return 1;
    }
    listen(fd, 2);

    int con_count = 1;
    while (1) {
        HTTPMessage message;
        
        int client_fd = accept(fd, NULL, NULL);
        printf("accepted connection: %d on fd: %d\n", con_count, client_fd);
        con_count++;
        struct timespec t_start, t_end;
        clock_gettime(CLOCK_MONOTONIC, &t_start);
        recv_msg(buff, client_fd, &message, sizeof buff);
        int send_sts = send(client_fd, "HTTP/1.1 200 OK\r\n\r\n", sizeof "HTTP/1.1 200 OK\r\n\r\n", 0);
        if (send_sts < 1) {
            perror("Send failed: ");
        }
        clock_gettime(CLOCK_MONOTONIC, &t_end);
        long ms = (t_end.tv_sec - t_start.tv_sec) * 1000 + (t_end.tv_nsec - t_start.tv_nsec) / 1000000;
        printf("req %ld ms\n", ms);
        close(client_fd);
        free(message.msg_body);
        for(int i = 0; i < message.header_count; i++){
            free(message.headers[i].key);
            free(message.headers[i].value);
        }
    }
    close(fd);

}

