#ifndef PARSER_H
#define PARSER_H
#include <stdlib.h>

#define MAX_REQ_URI_LEN 200
#define SP 32

typedef struct requestline requestline;
void parse(char *, int head, size_t len);

#endif
