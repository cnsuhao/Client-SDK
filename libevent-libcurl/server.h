#ifndef __SERVER_H_
#define __SERVER_H_
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <sys/stat.h>
#include <sys/socket.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

#include <assert.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <event2/event.h>
#include <event2/http.h>
#include <event2/buffer.h>
#include <event2/util.h>
#include <event2/keyvalq_struct.h>
#include <event2/bufferevent.h>
#include <event2/listener.h>

#include <curl/curl.h>
#include <jansson.h>

#ifdef EVENT__HAVE_NETINET_IN_H
#include <netinet/in.h>
# ifdef _XOPEN_SOURCE_EXTENDED
#  include <arpa/inet.h>
# endif
#endif

static const struct table_entry {
    const char *extension;
    const char *content_type;
} content_type_table[] = {
{ "txt", "text/plain" },
{ "html", "text/html" },
{ "mp4", "video/mp4" },
{ NULL, NULL },
};


const char * guess_content_type(const char *path);
void send_file_cb(struct evhttp_request *req, void *arg);


size_t header_cb(char *buffer, size_t size, size_t nitems, void *userdata);
size_t write_file_cb(char *buffer, size_t size, size_t nitems, void *userdata);
int get_file_range_cb(CURL *curlhandle, long * filesize, long pos, long off);
int get_file_cb(const char * remote_file_path, const char * local_file_path, long range);
size_t parse_token_cb(char *buffer, size_t size, size_t nitems, void *userdata);
static int login_cb(const char * username, const char * password, char * token);
size_t parse_node_cb(char *buffer, size_t size, size_t nitems, void *userdata);
static int get_node_cb(char * client_ip, char * host, char * uri, char * md5, char * token, char * nodes);

static int vdn_proc(const char * uri);


#endif
