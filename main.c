// Increase buff Size if recieved message is larger than what can be stored
// implement multi threading
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>


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

typedef struct {
    HTTPMessage *msg;
    int *client_fd;
    char *buff;
    int len;
} ThreadPaylod;

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

int check_header_eol(int head, size_t bytes, char *buff, int* CRLF_HEAD) {
    printf("Bytes recv: %zu\nHead: %d\nTotal: %zu\n", bytes, head, bytes+head);
    int total = bytes+head;

    while(head < total) {

        printf("%c", buff[head]);
        if (buff[head] == '\r' && *CRLF_HEAD % 2 == 1) {
            *CRLF_HEAD = 1;
            head++;
            continue;
        }

        if (*CRLF_HEAD < 4) {
            if ( *CRLF_HEAD % 2 == 0 && buff[head] == '\r') {
                (*CRLF_HEAD) ++;
            } else if (*CRLF_HEAD % 2 == 1 && buff[head] == '\n') {
                (*CRLF_HEAD)++;
            } else {
                *CRLF_HEAD = 0;
            }
        } 

        if (*CRLF_HEAD == 4) {
            return total - head - 1;
        }

        head++;
    }

    return -1;
}

int read_ok(int bytes, int client_fd) {

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


void *recv_msg(void *args) {

    ThreadPaylod p = *(ThreadPaylod *)args;
    int reader = 0;
    int msg_start = -1;
    int CRLF_HEAD = 0;

    while (reader < p.len) {
        int bytes = recv(*(p.client_fd), p.buff + reader, p.len  - reader, 0);

        if (!read_ok(bytes, *(p.client_fd))) {
            break;
        }

        int header_chk = check_header_eol(reader, bytes, p.buff, &CRLF_HEAD);
        printf("Bytes after header check: %d\n", header_chk);
        if (header_chk == 0) {
            reader += bytes;
            msg_start = reader + bytes - header_chk;
            break;
        } else if (header_chk > 0) {
            msg_start = reader + bytes - header_chk;
            reader += bytes;
            break;
        }

        reader += bytes;
    }

    int offset = parse_req_start_line(p.msg, p.buff, p.len);
    parse_headers(&offset, p.msg, p.buff, p.len);

    if (p.msg->body) {
        printf("Reading body\n");
        while((reader - msg_start) < p.msg->content_len) {
            int bytes = recv(*(p.client_fd), p.buff + reader, p.len - reader, 0);
            if (!read_ok(bytes, *(p.client_fd))) {
                break;
            }
            reader += bytes;
        }
        p.msg->msg_body = strndup(&(p.buff[msg_start]), p.msg->content_len);

    }


    int send_sts = send(*(p.client_fd), "HTTP/1.1 200 OK\r\n\r\n", sizeof "HTTP/1.1 200 OK\r\n\r\n", 0);
    close(*(p.client_fd));
    free(p.client_fd);
    if (send_sts < 1) {
        perror("Send failed: ");
    }
    printf("Request completed\n");

    free(p.buff);
    free(p.msg->msg_body);

    for(int count = 0; count < p.msg->header_count; count++) {
        free(p.msg->headers[count].key);
        free(p.msg->headers[count].value);
    }

    free(p.msg);
    free(args);

    return NULL;
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

    setvbuf(stdout, NULL, _IONBF, 0);

    int opt = 1;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    int error = bind(fd, (struct sockaddr *)&addr, sizeof(addr));
    if (error < 0) {
        perror("bind failed");
        return 1;
    }
    listen(fd, 100);

    while (1) {

        pthread_t t;
        int *client_fd = malloc(sizeof *client_fd);
        *client_fd = accept(fd, NULL, NULL);
        ThreadPaylod *params = malloc(sizeof *params);

        *params = (ThreadPaylod){
            .msg = malloc(sizeof(HTTPMessage)),
            .client_fd = client_fd,
            .buff = (char *)malloc(4096 * sizeof(char)),
            .len = 4096,
        };

        pthread_create(&t, NULL, recv_msg, params);
        pthread_detach(t);

    }
    close(fd);

}

