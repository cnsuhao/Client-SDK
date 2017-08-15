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
    sfinfo->context_end |= CONNECTION_END;
    printf("connection_end!\n");
}



void send_file_cb(int fd, short events, void *ctx) {
    struct send_file_ctx *sfinfo = ctx;

    if ((sfinfo->context_end & CONNECTION_END) || sfinfo->sent_chunk_num >= sfinfo->chunk_num) {
        sfinfo->context_end |= SEND_FILE_END;
        evhttp_send_reply_end(sfinfo->req);
        printf("finished sending file!\n");
        return;
    }

    for(int i = 0; i < THREAD_NUM_MAX; i++)
        if( sfinfo->tp.sending_chunk_no[i] == sfinfo->sent_chunk_num ){
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

    event_add(sfinfo->send_ev, &send_timeout);
    return;
}

void window_slide_cb(int fd, short events, void *ctx){
    struct send_file_ctx *sfinfo = ctx;

    sfinfo->timer++;
    printf("tick tick %d and sent: %d\n", sfinfo->timer, sfinfo->sent_chunk_num);

    /* 回收资源 */
    if ((sfinfo->context_end == CONTEXT_END)
            || (sfinfo->context_end == TRANSMISSION_END)) {
        clock_t start, finish;
        start = clock();

        event_free(sfinfo->send_ev);
        event_free(sfinfo->win_ev);
        for(int i = 0; i < THREAD_NUM_MAX; i++)
            if(-1 != sfinfo->tp.sending_chunk_no[i])
                pthread_join(sfinfo->tp.thread_id[i], NULL);
        for(int i = 0; i < THREAD_NUM_MAX; i++){
            evbuffer_free(sfinfo->tp.thread_ftsi[i].evb);
        }
        free(sfinfo);
        printf("finished context!\n");

        finish = clock();
        printf("finished %lf seconds\n", (double)(finish - start)/CLOCKS_PER_SEC);

        return;
    }
    

    if ((sfinfo->context_end & CONNECTION_END) || sfinfo->sent_chunk_num >= sfinfo->chunk_num) {
        sfinfo->context_end |= WINDOW_SLIDE_END;
        printf("finished window sliding\n");
    }

    if (sfinfo->tp.win_num <= 0){
        window_download(sfinfo);
    }

    /* 每隔win_slide进行一次窗口滑动 */
    if (sfinfo->timer >= win_slide.tv_sec
            && !(sfinfo->context_end & WINDOW_SLIDE_END)) {
        sfinfo->timer = 0;
        /* 滑动窗口，从已经发送的最后一个chunk处作为起始chunk */
        window_download(sfinfo);
    }
    struct timeval timeout = { 1, 0 };
    event_add(sfinfo->win_ev, &timeout);
    return;
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
    struct evhttp_connection* evcon = evhttp_request_get_connection(req);
    struct node_info ni_list[NODE_NUM_MAX];

    memset(ni_list, 0, sizeof(ni_list));

    strcpy(sfinfo->username, "admin");
    strcpy(sfinfo->password, "123456");
    strcpy(sfinfo->client_ip, "127.0.0.1");
    strcpy(sfinfo->host, "qq.webrtc.win");
    strcpy(sfinfo->md5, "ab340d4befcf324a0a1466c166c10d1d");
    strcpy(sfinfo->uri, uri);

    sfinfo->req = req;
    sfinfo->send_ev = event_new(base, -1, 0, send_file_cb, sfinfo);
    sfinfo->win_ev = event_new(base, -1, 0, window_slide_cb, sfinfo);
    evhttp_connection_set_closecb(evcon, close_connection_cb, sfinfo);
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
    sfinfo->context_end = 0;


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

    clock_t start, finish;
    start = clock();
    if(!preparation_process(sfinfo, ni_list))
        goto err;
    finish = clock();
    printf("preparation %lf seconds\n", (double)(finish - start)/CLOCKS_PER_SEC);


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

    event_add(sfinfo->send_ev, &send_timeout);
    event_add(sfinfo->win_ev, &send_timeout);
    goto done;
err:
    evhttp_send_error(req, 404, NULL);
done:
    if (decoded)
        evhttp_uri_free(decoded);
    if (decoded_path)
        free(decoded_path);
}

