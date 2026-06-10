#include "parser.h"
#include <stdlib.h>
#include <string.h>

struct requestline {
    int start;
    int end;
    char method[8];
    char target[2048];
    char httpVersion[9];
    int SP_POS[2];
    int sp_count;
};

requestline *startline_create(void) {
    return malloc(sizeof(struct requestline));
}

void free_startline(struct requestline *rl) {
    free(rl);
}

void parse(char *buff, int head, size_t len, requestline *rl) {

    static int flag = 0;
    switch (flag)
    {
    case 0:
        // parse request line

        break;
    
    default:
        break;
    }
}

static requestline *parse_startline(char *buff, int head, size_t len, requestline *rl) {

    while (head <= MAX_REQ_URI_LEN) {
        if (!rl->start && buff[head] > 32){
            rl->start = head;
            continue; 
        }

        if (buff[head] == SP) {
            rl->SP_POS[rl->sp_count % 2] = head;
        }

        head++;
    }
}