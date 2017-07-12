/*
  A trivial static http webserver using Libevent's evhttp.

  This is not the best code in the world, and it does some fairly stupid stuff
  that you would never want to do in a production webserver. Caveat hackor!

 */


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

static const struct http_response_header {
    const char *content;
} http_response_headers[] = {
{ "Content-Range: bytes %ld-%ld/%ld" },
{ NULL },
};
#define CONTENT_RANGE 0

size_t header_cb(char *buffer, size_t size, size_t nitems, void *userdata) {
    long start, end, len = 0;
    int r = sscanf(buffer, http_response_headers[CONTENT_RANGE].content, &start, &end, &len);
    if (r)
        *((long *) userdata) = len;

    return size * nitems;
}

/* 保存下载文件 */
size_t wirte_cb(void *ptr, size_t size, size_t nmemb, void *stream) {
    int ret = fwrite(ptr, size, nmemb, stream);
    fflush(stream);
    return ret;
}

int get_file_range_cb(CURL *curlhandle, long * filesize, long pos, long off) {
    struct curl_slist *headers = NULL;
    /* 设置文件续传的位置给libcurl */
    char range_str[1024];
    if ( *(filesize) >= off ){
        sprintf(range_str, "%ld-%ld", pos, off);
    } else {
        return 0;
    }
    curl_easy_setopt(curlhandle, CURLOPT_RANGE, range_str);
    CURLcode r = CURLE_GOT_NOTHING;
    r = curl_easy_perform(curlhandle);
    if (headers != NULL)
        curl_slist_free_all(headers);
    if (r == CURLE_OK)
        return 1;
    else {
        return 0;
    }
}

static int get_file_cb(const char * remote_file_path, const char * local_file_path, long range){
    int ret = 1;
    if (range <= 0 || remote_file_path == NULL || local_file_path == NULL){
        ret = 0;
        goto error;
    }
    CURL *curlhandle = NULL;
    curl_global_init(CURL_GLOBAL_ALL);
    curlhandle = curl_easy_init();

    long filesize = 0;
    FILE *file_pointer;
    //采用追加方式打开文件，便于实现文件断点续传工作
    file_pointer = fopen(local_file_path, "ab+");
    if (file_pointer == NULL) {
        ret = 0;
        goto error;
    }

    if (curlhandle == NULL){
        ret = 0;
        goto error;
    }
    curl_easy_setopt(curlhandle, CURLOPT_URL, remote_file_path);
    /* 设置http 头部处理函数 */
    curl_easy_setopt(curlhandle, CURLOPT_HEADERFUNCTION, header_cb);
    curl_easy_setopt(curlhandle, CURLOPT_HEADERDATA, &filesize);
    curl_easy_setopt(curlhandle, CURLOPT_WRITEDATA, file_pointer);
    curl_easy_setopt(curlhandle, CURLOPT_WRITEFUNCTION, wirte_cb);
    curl_easy_setopt(curlhandle, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(curlhandle, CURLOPT_VERBOSE, 1L);


    get_file_range_cb(curlhandle, &filesize, 0L, 0L);
    for(long i = 1L; i < filesize; i = i + range){
        if( i + range - 1 < filesize )
            get_file_range_cb(curlhandle, &filesize, i, i + range - 1);
        else
            get_file_range_cb(curlhandle, &filesize, i, filesize - 1);
    }

error:
    if (file_pointer) {
        fclose(file_pointer);
    }
    curl_easy_cleanup(curlhandle);
    curl_global_cleanup();
    return ret;
}


static const char * guess_content_type(const char *path) {
    const char *last_period, *extension;
    const struct table_entry *ent;
    last_period = strrchr(path, '.');
    if (!last_period || strchr(last_period, '/'))
        return "application/misc";
    extension = last_period + 1;
    for (ent = &content_type_table[0]; ent->extension; ++ent) {
        if (!evutil_ascii_strcasecmp(ent->extension, extension))
            return ent->content_type;
    }
}

static void send_file_cb(struct evhttp_request *req, void *arg) {
    struct evbuffer *evb = NULL;
    const char *docroot = arg;
    const char *uri = evhttp_request_get_uri(req);
    struct evhttp_uri *decoded = NULL;
    const char *path;
    char *decoded_path;
    char *whole_path = NULL;
    size_t len;
    int fd = -1;
    struct stat st;

    if (evhttp_request_get_command(req) != EVHTTP_REQ_GET) {
        return;
    }
    printf("Got a GET request for <%s>\n",  uri);

    get_file_cb("http://183.60.40.104/qq/tv/pear001.mp4", "1.mp4", 10000000L);

    decoded = evhttp_uri_parse(uri);
    if (!decoded) {
        printf("It's not a good URI. Sending BADREQUEST\n");
        evhttp_send_error(req, HTTP_BADREQUEST, 0);
        return;
    }

    path = evhttp_uri_get_path(decoded);
    if (!path) path = "/";

    decoded_path = evhttp_uridecode(path, 0, NULL);
    if (decoded_path == NULL)
        goto err;

    if (strstr(decoded_path, ".."))
        goto err;

    len = strlen(decoded_path)+strlen(docroot)+2;
    if (!(whole_path = malloc(len))) {
        perror("malloc");
        goto err;
    }
    evutil_snprintf(whole_path, len, "%s/%s", docroot, decoded_path);

    if (stat(whole_path, &st)<0) {
        goto err;
    }

    /* This holds the content we're sending. */
    evb = evbuffer_new();

    if (S_ISDIR(st.st_mode)) {
        goto err;
    } else {
        /* it's a file; add it to the buffer to get sent via sendfile */
        const char *type = guess_content_type(decoded_path);
        if ((fd = open(whole_path, O_RDONLY)) < 0) {
            perror("open");
            goto err;
        }
        if (fstat(fd, &st)<0) {
            perror("fstat");
            goto err;
        }
        evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", type);
        evbuffer_add_file(evb, fd, 0, st.st_size);
    }

    evhttp_send_reply(req, 200, "OK", evb);
    goto done;
err:
    evhttp_send_error(req, 404, NULL);
    if (fd>=0)
        close(fd);
done:
    if (decoded)
        evhttp_uri_free(decoded);
    if (decoded_path)
        free(decoded_path);
    if (whole_path)
        free(whole_path);
    if (evb)
        evbuffer_free(evb);
}

static void syntax(void) {
    fprintf(stdout, "Syntax: http-server <docroot>\n");
}

int main(int argc, char **argv)
{
    struct event_base *base;
    struct evhttp *http;
    struct evhttp_bound_socket *handle;

    ev_uint16_t port = 60000;
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
        return (1);
    if (argc < 2) {
        syntax();
        return 1;
    }

    base = event_base_new();
    if (!base) {
        fprintf(stderr, "Couldn't create an event_base: exiting\n");
        return 1;
    }

    http = evhttp_new(base);
    if (!http) {
        fprintf(stderr, "couldn't create evhttp. Exiting.\n");
        return 1;
    }

    evhttp_set_gencb(http, send_file_cb, argv[1]);

    handle = evhttp_bind_socket_with_handle(http, "127.0.0.1", port);
    if (!handle) {
        fprintf(stderr, "couldn't bind to port %d. Exiting.\n",
                (int)port);
        return 1;
    }

    event_base_dispatch(base);

    return 0;
}
