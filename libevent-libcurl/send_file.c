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

    printf("Thread %ld terminated\n", sfinfo->completed_count);
    pthread_join(sfinfo->thread_id[sfinfo->completed_count], NULL);

    /* This holds the content we're sending. */
    struct evbuffer *evb = sfinfo->evb_array[sfinfo->completed_count];
    evhttp_send_reply_chunk(sfinfo->req, evb);
    printf("evbuffer len: %ld\n", evbuffer_get_length(evb));
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
    struct node_info ni_list[NODE_NUM_MAX];

    memset(ni_list, 0, sizeof(ni_list));

    strcpy(sfinfo->username, "admin");
    strcpy(sfinfo->password, "123456");
    strcpy(sfinfo->client_ip, "127.0.0.1");
    strcpy(sfinfo->host, "qq.webrtc.win");
    strcpy(sfinfo->md5, "ab340d4befcf324a0a1466c166c10d1d");
    strcpy(sfinfo->uri, uri);

    sfinfo->req = req;
    sfinfo->tm_ev = event_new(base, -1, 0, send_file_cb, sfinfo);
    sfinfo->alive_node_num = 0L;
    memset(sfinfo->alive_nodes_index, 0, sizeof(sfinfo->alive_nodes_index));
    sfinfo->completed_count = 0;
    sfinfo->window_size = 5000000L;
    sfinfo->chunk_size = 1000000L;

    /* Must initialize libcurl before any threads are started */
    curl_global_init(CURL_GLOBAL_ALL);

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

    if(!preparation_process(sfinfo, ni_list))
        goto err;

    if(!get_file(sfinfo, ni_list)) {
        printf("getfile wrong\n");
        goto err;
    }

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

