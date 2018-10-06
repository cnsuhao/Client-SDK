#ifndef CONSTANT_H
#define CONSTANT_H

#define CONNECTION_END (1 << 0)
#define SEND_FILE_END (1 << 1)
#define WINDOW_SLIDE_END (1 << 2)
#define TRANSMISSION_END (SEND_FILE_END | WINDOW_SLIDE_END)
#define CONTEXT_END (CONNECTION_END | TRANSMISSION_END)


#define URL_LENGTH_MAX 1024
#define NODE_NUM_MAX 50
#define THREAD_NUM_MAX 50

static struct timeval send_timeout = { 0, 100 };
static struct timeval win_slide = { 10, 0 };

#endif // CONSTANT_H
