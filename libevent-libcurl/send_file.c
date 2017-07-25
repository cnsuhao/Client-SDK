#include "server.h"

const char * guess_content_type(const char *path) {
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

void send_file_cb(int fd, short events, void *ctx) {
    struct send_file_ctx *sfinfo = ctx;
    int file_descriptor = -1;
    struct stat st;

    printf("Thread %ld terminated\n", sfinfo->completed_count);
    pthread_join(sfinfo->thread_id[sfinfo->completed_count], NULL);
    char file_slice[URL_LENGTH_MAX];
    memset(file_slice, 0, URL_LENGTH_MAX*sizeof(char));
    evutil_snprintf(file_slice, URL_LENGTH_MAX, "%s.%ld.slice.%ld", sfinfo->whole_path, sfinfo->stamp, sfinfo->completed_count);
    printf("file_slice: [%s]\n", file_slice);


    if (stat(file_slice, &st)<0) {
        goto err;
    }
    /* This holds the content we're sending. */
    struct evbuffer *evb = evbuffer_new();
    if (S_ISDIR(st.st_mode)) {
        goto err;
    } else {
        /* it's a file; add it to the buffer to get sent via sendfile */
        if ((file_descriptor = open(file_slice, O_RDONLY)) < 0) {
            perror("open");
            goto err;
        }
        if (fstat(file_descriptor, &st)<0) {
            perror("fstat");
            goto err;
        }
        evbuffer_add_file(evb, file_descriptor, 0, st.st_size);
    }
    evhttp_send_reply_chunk(sfinfo->req, evb);
    evbuffer_free(evb);

    sfinfo->completed_count++;
    if (sfinfo->completed_count >= sfinfo->thread_count) {
        event_free(sfinfo->tm_ev);
        evhttp_send_reply_end(sfinfo->req);
        free(sfinfo);
        return;
    }
    event_add(sfinfo->tm_ev, &timeout);
    return;
err:
    evhttp_send_error(sfinfo->req, 404, NULL);
    if (file_descriptor >= 0)
        close(file_descriptor);
}


/*
 * if you're literally calling sleep(), or you're calling evhttp_send_reply_chunk() in a loop,
 * you can't do that in an event-based application. You need to schedule a callback to happen
 * later. Nothing works while your function is running, only in between your functions.
 * https://github.com/libevent/libevent/issues/536
 */
void do_request_cb(struct evhttp_request *req, void *arg){
    const char *docroot = arg;
    const char *uri = evhttp_request_get_uri(req);
    struct evhttp_uri *decoded = NULL;
    const char *path;
    char *decoded_path;

    struct send_file_ctx *sfinfo = malloc(sizeof(struct send_file_ctx));

    strcpy(sfinfo->username, "admin");
    strcpy(sfinfo->password, "123456");
    strcpy(sfinfo->client_ip, "127.0.0.1");
    strcpy(sfinfo->host, "qq.webrtc.win");
    strcpy(sfinfo->md5, "ab340d4befcf324a0a1466c166c10d1d");
    strcpy(sfinfo->uri, uri);

    sfinfo->req = req;
    sfinfo->tm_ev = event_new(base, -1, 0, send_file_cb, sfinfo);
    sfinfo->completed_count = 0;
    sfinfo->stamp = 0;

    if (evhttp_request_get_command(req) != EVHTTP_REQ_GET) {
        return;
    }
    decoded = evhttp_uri_parse(uri);
    if (!decoded) {
        printf("It's not a good URI. Sending BADREQUEST\n");
        evhttp_send_error(req, HTTP_BADREQUEST, 0);
        return;
    }
    path = evhttp_uri_get_path(decoded);
    if (!path)
        path = "/";
    decoded_path = evhttp_uridecode(path, 0, NULL);
    if (decoded_path == NULL)
        goto err;
    if (strstr(decoded_path, ".."))
        goto err;
    sprintf(sfinfo->whole_path, "%s%s", docroot, decoded_path);

    if(!vdn_proc(sfinfo))
        goto err;

    const char *type = guess_content_type(decoded_path);
    evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", type);
    evhttp_send_reply_start(req, HTTP_OK, "OK");

    event_add(sfinfo->tm_ev, &timeout);
    goto done;
err:
    evhttp_send_error(req, 404, NULL);
done:
    if (decoded)
        evhttp_uri_free(decoded);
    if (decoded_path)
        free(decoded_path);
}

