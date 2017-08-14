#include "send_file.h"
#include "get_file.h"

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

void close_connection_cb(struct evhttp_connection * evcon, void * ctx){
    struct send_file_ctx *sfinfo = ctx;
    sfinfo->connection_end = 1;
    printf("connection_end!\n");
}



void send_file_cb(int fd, short events, void *ctx) {
    struct send_file_ctx *sfinfo = ctx;

    sfinfo->timer++;
    printf("tick tick %d and sent: %d\n", sfinfo->timer, sfinfo->sent_chunk_num);

    if (sfinfo->connection_end || sfinfo->sent_chunk_num >= sfinfo->chunk_num) {
        printf("connection_end or finished sending?\n");
        event_free(sfinfo->tm_ev);
        evhttp_send_reply_end(sfinfo->req);
        for(int i = 0; i < THREAD_NUM_MAX; i++)
            if(-1 != sfinfo->tp.sending_chunk_no[i])
                pthread_join(sfinfo->tp.thread_id[i], NULL);
        for(int i = 0; i < THREAD_NUM_MAX; i++){
            evbuffer_free(sfinfo->tp.thread_ftsi[i].evb);
        }
        /* bug可能触发：如果发送完所有数据，释放了sfinfo，但此时由于时序问题，
        才调用到close_connection_cb，则会出现线程不安全的问题 */
        free(sfinfo);
        printf("connection_end or finished sending!\n");
        return;
    }

    if (sfinfo->tp.win_num <= 0){
        if(!window_download(sfinfo)) {
            printf("getfile wrong\n");
            goto err;
        }
    }

    int find_sign = 0;
    for(int i = 0; i < THREAD_NUM_MAX; i++)
        if( sfinfo->tp.sending_chunk_no[i] == sfinfo->sent_chunk_num ){
            find_sign = 1;
            struct evbuffer *evb = sfinfo->tp.thread_ftsi[i].evb;
            size_t len = evbuffer_get_length(evb);
            /* 如果不是第一个窗口，则检查是否下载完，没下载完就不发: 若缓冲区中数据长度为chunk_size，
             * 或者是最后一个chunk时，缓冲区中数据长度为最后一个chunk的长度 */
            if (len == sfinfo->chunk_size
                    ||(sfinfo->filesize-sfinfo->pos-(sfinfo->chunk_num-1)*sfinfo->chunk_size == len
                       && sfinfo->chunk_num-1 == sfinfo->tp.sending_chunk_no[i])){
                printf("sent_chunk: %d  evbuffer len: %ld   ",
                       sfinfo->sent_chunk_num,
                       evbuffer_get_length(evb));
                evhttp_send_reply_chunk(sfinfo->req, evb);
                sfinfo->sent_chunk_num++;
                printf("speed: %lf\n", sfinfo->tp.thread_ftsi[i].download_speed);
                break;
            }
        }
    if (!find_sign){
        printf("cant find sending chunk %d\n", sfinfo->sent_chunk_num);
        goto err;
    }


    /* 每隔5s进行一次窗口滑动 */
    if (sfinfo->timer >= 10) {
        sfinfo->timer = 0;
        /* 滑动窗口，从已经发送的最后一个chunk处作为起始chunk */
        if(!window_download(sfinfo)) {
            printf("getfile wrong\n");
            goto err;
        }
    }
    event_add(sfinfo->tm_ev, &timeout);
    return;
err:
    evhttp_send_error(sfinfo->req, 404, "Sayonara tomodachi!");
    event_free(sfinfo->tm_ev);
    evhttp_send_reply_end(sfinfo->req);
    for(int i = 0; i < THREAD_NUM_MAX; i++)
        if(-1 != sfinfo->tp.sending_chunk_no[i])
            pthread_join(sfinfo->tp.thread_id[i], NULL);
    for(int i = 0; i < THREAD_NUM_MAX; i++){
        evbuffer_free(sfinfo->tp.thread_ftsi[i].evb);
    }
    free(sfinfo);
}

void do_request_cb(struct evhttp_request *req, void *arg){
    const char *docroot = arg;
    const char *uri = evhttp_request_get_uri(req);
    struct evhttp_uri *decoded = NULL;
    const char *path;
    char *decoded_path;
    char content_length[100];
    char content_range[100];

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
    sfinfo->tp.win_num = 0;
    sfinfo->sent_chunk_num = 0;

    memset(sfinfo->tp.thread_ftsi, 0, sizeof(sfinfo->tp.thread_ftsi));
    for(int i = 0; i < THREAD_NUM_MAX; i++){
        sfinfo->tp.thread_ftsi[i].evb = evbuffer_new();
        sfinfo->tp.sending_chunk_no[i] = -1;
    }
    sfinfo->timer = 0;
    sfinfo->connection_end = 0;

    struct evhttp_connection* evcon = evhttp_request_get_connection(sfinfo->req);
    evhttp_connection_set_closecb(evcon, close_connection_cb, sfinfo);

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

    const char * range_header = evhttp_find_header(evhttp_request_get_input_headers(req), "Range");
    if (range_header){
        int r = sscanf(range_header, "bytes=%ld-", &sfinfo->pos);
        if (!r){
            printf("do_request_cb get Range %s wrong\n", range_header);
            goto err;
        }
    }else{
        sfinfo->pos = 0;
    }
    printf("Range: bytes=%ld-\n", sfinfo->pos);

    if(!preparation_process(sfinfo, ni_list))
        goto err;

    const char *type = guess_content_type(decoded_path);
    evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", type);
    evhttp_add_header(evhttp_request_get_output_headers(req), "Connection", "keep-alive");
    evhttp_add_header(evhttp_request_get_output_headers(req), "Access-Control-Allow-Origin", "*");
    evhttp_add_header(evhttp_request_get_output_headers(req), "Access-Control-Allow-Credentials", "true");
    evhttp_add_header(evhttp_request_get_output_headers(req), "Access-Control-Allow-Methods", "GET,POST,OPTIONS,HEAD,DELETE,PUT");
    evhttp_add_header(evhttp_request_get_output_headers(req), "Access-Control-Allow-Headers", "Range,Host");
    evhttp_add_header(evhttp_request_get_output_headers(req), "Access-Control-Expose-Headers", "X-PEAR-ARGS,Content-Length,Content-Range");
    /* 允许进度条拖曳的header */
    evhttp_add_header(evhttp_request_get_output_headers(req), "Accept-Ranges", "bytes");
    if (range_header){
        sprintf(content_length, "%ld", sfinfo->filesize-sfinfo->pos);
        sprintf(content_range, "bytes %ld-%ld/%ld", sfinfo->pos, sfinfo->filesize-1, sfinfo->filesize);
        evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Length", content_length);
        evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Range", content_range);
        evhttp_send_reply_start(req, 206, "OK");
    }else{
        sprintf(content_length, "%ld", sfinfo->filesize);
        evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Length", content_length);
        evhttp_send_reply_start(req, HTTP_OK, "OK");
    }

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

