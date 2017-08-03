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
    timer++;
    printf("tick tick %d\n", timer);

    struct send_file_ctx *sfinfo = ctx;
    struct file_transfer_session_info thread_ftsi[THREAD_NUM_MAX];

    if (sfinfo->sent_chunk_pointer >= sfinfo->chunk_num) {
        event_free(sfinfo->tm_ev);
        evhttp_send_reply_end(sfinfo->req);
        for(int i = 0; i < sfinfo->chk_in_win_ct; i++)
            evbuffer_free(sfinfo->evb_array[i]);
        free(sfinfo);
        return;
    }

    if (sfinfo->sent_chunk_pointer <= 0){
        memset(thread_ftsi, 0, sizeof(thread_ftsi));
        if(!window_download(sfinfo, thread_ftsi)) {
            printf("getfile wrong\n");
            goto err;
        }
    }

    int t_ptr = sfinfo->thread_pointer;
    if (sfinfo->sent_chunk_pointer < sfinfo->chk_in_win_ct){
        /* 如果是第一个窗口，那么需要强行等待所有线程结束，以便测速 */
        pthread_join(sfinfo->thread_id[t_ptr], NULL);
        struct evbuffer *evb = sfinfo->evb_array[t_ptr];
        printf("sent_chunk: %d  evbuffer len: %ld   ",
               sfinfo->sent_chunk_pointer,
               evbuffer_get_length(evb));
        evhttp_send_reply_chunk(sfinfo->req, evb);
        sfinfo->sent_chunk_pointer++;
        sfinfo->thread_pointer++;
        printf("speed: %lf\n", thread_ftsi[t_ptr].download_speed);

        /* 如果第一个窗口全部下载完了，那么可以根据速度来对节点排序 */
        if (sfinfo->sent_chunk_pointer == sfinfo->chk_in_win_ct)
            sort_alive_nodes(sfinfo, thread_ftsi);
    } else if (sfinfo->thread_pointer < sfinfo->chk_in_win_ct) {
        struct evbuffer *evb = sfinfo->evb_array[t_ptr];
        size_t len = evbuffer_get_length(evb);
        /* 如果不是第一个窗口，则检查是否下载完，没下载完就不发 */
        if (len == sfinfo->chunk_size
                || len == sfinfo->filesize - (sfinfo->chunk_num-1)*sfinfo->chunk_size){
            printf("sent_chunk: %d  evbuffer len: %ld   ",
                   sfinfo->sent_chunk_pointer,
                   evbuffer_get_length(evb));
            evhttp_send_reply_chunk(sfinfo->req, evb);
            sfinfo->sent_chunk_pointer++;
            sfinfo->thread_pointer++;
            printf("speed: %lf\n", thread_ftsi[t_ptr].download_speed);
        }
    }

    /* 每隔5s进行一次窗口滑动 */
    if (timer >= 10) {
        timer = 0;
        /* 滑动窗口，从已经发送的最后一个chunk处作为起始chunk */
        memset(thread_ftsi, 0, sizeof(thread_ftsi));
        if(!window_download(sfinfo, thread_ftsi)) {
            printf("getfile wrong\n");
            goto err;
        }
    }
    event_add(sfinfo->tm_ev, &timeout);
    return;
err:
    evhttp_send_error(sfinfo->req, 404, "Shit!");
    event_free(sfinfo->tm_ev);
    evhttp_send_reply_end(sfinfo->req);
    for(int i = 0; i < sfinfo->chk_in_win_ct; i++){
        evbuffer_free(sfinfo->evb_array[i]);
    }
    free(sfinfo);
}


/*
 * if you're literally calling sleep(), or you're calling evhttp_send_reply_chunk() in a loop,
 * you can't do that in an event-based application. You need to schedule a callback to happen
 * later. Nothing works while your function is running, only in between your functions.
 * nothing will be sent during your loop, so
 * DO NOT USE ANT LOOP IN YOUR FUNCTION!
 * DO NOT USE ANT LOOP IN YOUR FUNCTION!
 * DO NOT USE ANT LOOP IN YOUR FUNCTION!
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
    memset(sfinfo->alive_nodes, 0, sizeof(sfinfo->alive_nodes));
    sfinfo->window_size = 10000000L;
    sfinfo->chunk_size = 1000000L;
    sfinfo->sent_chunk_pointer = 0;

    timer = 0;

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

