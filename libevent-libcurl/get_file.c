#include "get_file.h"
#include "first_aid.h"

size_t header_cb(char *buffer, size_t size, size_t nitems, void *userdata) {
    long start, end, len = 0;
    int r = sscanf(buffer, "Content-Range: bytes %ld-%ld/%ld", &start, &end, &len);
    if (r)
        *((long *) userdata) = len;
    return size * nitems;
}

size_t write_buffer_cb(char *buffer, size_t size, size_t nitems, void *userdata) {
    struct evbuffer * evb = (struct evbuffer * )userdata;
    int ret = evbuffer_add(evb, buffer, size * nitems);
    if(!ret)
        return size * nitems;
    else
        return ret;
}

int get_file_range(struct file_transfer_session_info * ftsi) {
    CURL *curlhandle = NULL;
    char range_str[URL_LENGTH_MAX];

    if ( strlen(ftsi->ni.remote_file_url) <= 0 || ftsi->pos < 0
         || ftsi->range < 0 || ftsi->pos >= ftsi->filesize
         || ftsi->evb == NULL){
        printf("get_file_range param wrong\n");
        goto error;
    }
    curlhandle = curl_easy_init();
    if (curlhandle == NULL){
        printf("get_file_range curl wrong\n");
        goto error;
    }
    curl_easy_setopt(curlhandle, CURLOPT_URL, ftsi->ni.remote_file_url);
    curl_easy_setopt(curlhandle, CURLOPT_HEADERFUNCTION, header_cb);
    curl_easy_setopt(curlhandle, CURLOPT_HEADERDATA, &(ftsi->filesize));
    curl_easy_setopt(curlhandle, CURLOPT_WRITEDATA, ftsi->evb);
    curl_easy_setopt(curlhandle, CURLOPT_WRITEFUNCTION, write_buffer_cb);
    curl_easy_setopt(curlhandle, CURLOPT_TIMEOUT, ftsi->download_timeout);

    if ( ftsi->filesize >= ftsi->pos + ftsi->range - 1 ){
        sprintf(range_str, "%ld-%ld", ftsi->pos, ftsi->pos + ftsi->range - 1);
    } else {
        sprintf(range_str, "%ld-", ftsi->pos);
    }
    curl_easy_setopt(curlhandle, CURLOPT_RANGE, range_str);
    CURLcode r = curl_easy_perform(curlhandle);
    if (r != CURLE_OK){
        printf("get_file_range error %s     range: %s\n", ftsi->ni.remote_file_url, range_str);
        goto error;
    }else{
        r = curl_easy_getinfo(curlhandle, CURLINFO_SPEED_DOWNLOAD, &ftsi->download_speed);
        if (r != CURLE_OK)
            printf("get_file_range SPEED option not supported\n");
    }
    if (curlhandle)
        curl_easy_cleanup(curlhandle);
    return 1;
error:
    if (curlhandle)
        curl_easy_cleanup(curlhandle);
    if (ftsi->evb)
        evbuffer_drain(ftsi->evb, evbuffer_get_length(ftsi->evb));
    return 0;
}

void *thread_run(void *ftsi){
    struct file_transfer_session_info * ftsi_ptr = (struct file_transfer_session_info * )ftsi;
    int ret = get_file_range(ftsi_ptr);
}

int window_download(struct send_file_ctx *sfinfo){
    printf("window_download begin\n");
    size_t alive_node_num = sfinfo->alive_node_num;

    int win_total = sfinfo->tp.win_num * sfinfo->chk_in_win_ct;
    printf("win_num %d\n", sfinfo->tp.win_num);
    int sending_chunk_no = sfinfo->sent_chunk_num;
    for (int i = win_total; i < win_total + sfinfo->chk_in_win_ct; i++) {
        int node_index = (i - win_total) % alive_node_num;
        int thread_index = i % THREAD_NUM_MAX;
        if (check_download(sfinfo, sending_chunk_no)){
            sending_chunk_no++;
            i--;
            continue;
        }
        memcpy(&(sfinfo->tp.thread_ftsi[thread_index].ni), &(sfinfo->alive_nodes[node_index]), sizeof(struct node_info));
        sfinfo->tp.thread_ftsi[thread_index].pos = sending_chunk_no * sfinfo->chunk_size + sfinfo->pos;
        sfinfo->tp.thread_ftsi[thread_index].range = sfinfo->chunk_size;
        sfinfo->tp.thread_ftsi[thread_index].filesize = sfinfo->filesize;
        sfinfo->tp.thread_ftsi[thread_index].download_timeout = THREAD_NUM_MAX / sfinfo->chk_in_win_ct * win_slide.tv_sec - 1;
        evbuffer_drain(sfinfo->tp.thread_ftsi[thread_index].evb,
                       evbuffer_get_length(sfinfo->tp.thread_ftsi[thread_index].evb));
        pthread_create(&(sfinfo->tp.thread_id[thread_index]), NULL,
                       thread_run, (void *)&(sfinfo->tp.thread_ftsi[thread_index]));
        sfinfo->tp.sending_chunk_no[thread_index] = sending_chunk_no;
        sending_chunk_no++;
        printf("sending_chunk_no %d: %d at %s from %ld\n",
               thread_index, sfinfo->tp.sending_chunk_no[thread_index],
               sfinfo->tp.thread_ftsi[thread_index].ni.remote_file_url,
               sfinfo->tp.thread_ftsi[thread_index].pos);
    }
    sfinfo->tp.win_num++;
    printf("win_num %d\n", sfinfo->tp.win_num);
    printf("thread pool: <");
    for(int i = 0; i < THREAD_NUM_MAX; i++){
        printf("%d ", sfinfo->tp.sending_chunk_no[i]);
    }
    printf(">\n");
    printf("window_download end\n");
    return 1;
}

/* return value must be size of data which has been process */
size_t joint_string_cb(char *buffer, size_t size, size_t nitems, void *userdata) {
    strcat(userdata, buffer);
    return size * nitems;
}

int login(const char * username, const char * password, char * token){
    int ret = 1;
    char jsonData[URL_LENGTH_MAX];
    CURL *curlhandle = NULL;
    struct curl_slist *list = NULL;

    if (username == NULL || password == NULL){
        printf("login wrong\n");
        ret = 0;
        goto error;
    }
    sprintf(jsonData, "{\"user\":\"%s\",\"password\":\"%s\"}", username, password);
    curlhandle = curl_easy_init();
    if (curlhandle == NULL){
        printf("login wrong\n");
        ret = 0;
        goto error;
    }
    curl_easy_setopt(curlhandle, CURLOPT_URL, "https://api.webrtc.win:6601/v1/customer/login");
    curl_easy_setopt(curlhandle, CURLOPT_WRITEDATA, token);
    curl_easy_setopt(curlhandle, CURLOPT_WRITEFUNCTION, joint_string_cb);
    list = curl_slist_append(NULL, "Content-Type:application/json");
    curl_easy_setopt(curlhandle, CURLOPT_HTTPHEADER, list);
    curl_easy_setopt(curlhandle, CURLOPT_POSTFIELDS, jsonData);

    CURLcode r = curl_easy_perform(curlhandle);
    if (r != CURLE_OK){
        printf("login wrong\n");
        ret = 0;
    }
error:
    if (list)
        curl_slist_free_all(list);
    if (curlhandle)
        curl_easy_cleanup(curlhandle);
    return ret;
}

int get_node(char * client_ip, char * host, const char * uri, char * md5, const char * token, char * nodes){
    int ret = 1;
    char url[URL_LENGTH_MAX];
    char token_header[URL_LENGTH_MAX];
    CURL *curlhandle = NULL;
    struct curl_slist *list = NULL;


    if (client_ip == NULL || host == NULL || uri == NULL || md5 == NULL || token == NULL){
        printf("get_node wrong\n");
        ret = 0;
        goto error;
    }
    sprintf(url, "https://api.webrtc.win:6601/v1/customer/nodes?client_ip=%s&host=%s&uri=%s&md5=%s", client_ip, host, uri, md5);
    sprintf(token_header, "X-Pear-Token: %s", token);
    curlhandle = curl_easy_init();
    if (curlhandle == NULL){
        printf("get_node wrong\n");
        ret = 0;;
        goto error;
    }
    curl_easy_setopt(curlhandle, CURLOPT_URL, url);
    curl_easy_setopt(curlhandle, CURLOPT_WRITEDATA, nodes);
    curl_easy_setopt(curlhandle, CURLOPT_WRITEFUNCTION, joint_string_cb);
    list = curl_slist_append(NULL, token_header);
    list = curl_slist_append(list, "Content-Type:application/json");
    curl_easy_setopt(curlhandle, CURLOPT_HTTPHEADER, list);

    CURLcode r = curl_easy_perform(curlhandle);
    if (r != CURLE_OK){
        printf("get_node wrong\n");
        ret = 0;
    }
error:
    if (list)
        curl_slist_free_all(list);
    if (curlhandle)
        curl_easy_cleanup(curlhandle);
    return ret;
}

int get_node_alive(struct node_info * ni_list, size_t node_num, struct send_file_ctx * sfinfo){
    int alive_node_num = 0;
    pthread_t tid[NODE_NUM_MAX];
    struct file_transfer_session_info tmp_ftsis[NODE_NUM_MAX];
    double download_speeds[NODE_NUM_MAX];
    size_t test_range = 10000L;

    if (ni_list == NULL || node_num <= 0){
        printf("get_node_alive wrong\n");
        return 0;
    }

    /* find which node can be used for transmission */
    for(int i = 0; i < node_num; i++){
        if (strcmp(ni_list[i].protocol, "https") == 0 || strcmp(ni_list[i].protocol, "http") == 0){
            memcpy(&(tmp_ftsis[i].ni), &(ni_list[i]), sizeof(struct node_info));
            tmp_ftsis[i].pos = 0L;
            tmp_ftsis[i].range = test_range;
            tmp_ftsis[i].filesize = test_range;
            tmp_ftsis[i].evb = evbuffer_new();
            pthread_create(&tid[i], NULL, thread_run, (void *)&(tmp_ftsis[i]));
        } else {
            /* magnet or other protocol added here */

        }
    }

    for(int i = 0; i < node_num; i++){
        if (strcmp(ni_list[i].protocol, "https") == 0 || strcmp(ni_list[i].protocol, "http") == 0){
            pthread_join(tid[i], NULL);
            if ( tmp_ftsis[i].filesize >= test_range
                 && test_range == evbuffer_get_length(tmp_ftsis[i].evb) ) {
                printf("node alive: %s, speed %lf\n", tmp_ftsis[i].ni.remote_file_url, tmp_ftsis[i].download_speed);
                sfinfo->filesize = tmp_ftsis[i].filesize;
                download_speeds[alive_node_num] = tmp_ftsis[i].download_speed;
                memcpy(&(sfinfo->alive_nodes[alive_node_num]), &(ni_list[i]), sizeof(struct node_info));
                alive_node_num++;
            }else{
                printf("node dead: %s\n", tmp_ftsis[i].ni.remote_file_url);
            }
            evbuffer_free(tmp_ftsis[i].evb);
        } else {
            /* magnet or other protocol added here */

        }
    }

    sfinfo->alive_node_num = alive_node_num;

    sort_alive_nodes(sfinfo, download_speeds);

    //    for(int i = 0; i < alive_node_num; i++) {
    //        printf("node sorted: %s\n", sfinfo->alive_nodes[i].remote_file_url);
    //    }

    return alive_node_num;

}

int node_info_init(struct send_file_ctx *sfinfo, struct node_info * ni_list, char * nodes){
    json_error_t error;
    json_t *root = NULL;
    int node_num = 0;

    /* there are some BUG here!!!!!!!!!!! */
    char nodes_tmp[URL_LENGTH_MAX*10];
    for(int i = strlen(nodes)-1; i >= 0; i--){
        if(nodes[i] == '}'){
            strncpy(nodes_tmp, nodes, i+1);
            break;
        }
    }
    strcpy(nodes, nodes_tmp);
    root = json_loads(nodes, 0, &error);
    if (!root) {
        printf("node_info_init nodes wrong\n");
        goto error;
    }
    json_t *nodes_j = json_object_get(root, "nodes");
    if (!json_is_array(nodes_j)) {
        printf("node_info_init node_array wrong\n");
        goto error;
    }
    node_num = json_array_size(nodes_j);
    for(int i = 0; i < node_num; i++){
        json_t *node = json_array_get(json_object_get(root, "nodes"), i);
        json_t *protocol = json_object_get(node, "protocol");
        const char *protocol_str = json_string_value(protocol);
        strcpy(ni_list[i].protocol, protocol_str);
        if (strcmp(protocol_str, "https") == 0 || strcmp(protocol_str, "http") == 0){
            json_t *host = json_object_get(node, "host");
            const char *host_str = json_string_value(host);
            sprintf(ni_list[i].remote_file_url, "%s://%s/%s%s", protocol_str, host_str, sfinfo->host, sfinfo->uri);
        } else {
            json_t *magnet = json_object_get(node, "magnet_uri");
            const char *magnet_str = json_string_value(magnet);
            sprintf(ni_list[i].remote_file_url, "%s", magnet_str);
        }
    }
error:
    if (root)
        json_decref(root);
    return node_num;
}


int preparation_process(struct send_file_ctx *sfinfo, struct node_info * ni_list){
    int ret = 1;
    char token[URL_LENGTH_MAX];
    char nodes[URL_LENGTH_MAX*10];
    json_error_t error;
    json_t *root = NULL;
    int node_num = 0;

    if(sfinfo->window_size <= 0 || sfinfo->chunk_size <= 0 || sfinfo->window_size % sfinfo->chunk_size != 0){
        printf("preparation_process param wrong\n");
        ret = 0;
        goto error;
    }

    memset(token, 0, sizeof(token));
    memset(nodes, 0, sizeof(nodes));

    ret = login(sfinfo->username, sfinfo->password, token);
    root = json_loads(token, 0, &error);
    if (!root || !ret) {
        ret = 0;
        printf("preparation_process login wrong\n");
        goto error;
    }
    json_t *token_j = json_object_get(root, "token");
    const char *token_str = json_string_value(token_j);
    // printf("token: %s\n", token_str);

    ret = get_node(sfinfo->client_ip, sfinfo->host, sfinfo->uri, sfinfo->md5, token_str, nodes);
    if (!ret) {
        printf("preparation_process get_nodes wrong\n");
        goto error;
    }
    // printf("nodes: %s\n", nodes);

    node_num = node_info_init(sfinfo, ni_list, nodes);

    get_node_alive(ni_list, node_num, sfinfo);
    if (sfinfo->alive_node_num <= 0 || sfinfo->filesize <= 0) {
        ret = 0;
        printf("preparation_process alive_node_num wrong\n");
        goto error;
    }

    sfinfo->chk_in_win_ct = sfinfo->window_size / sfinfo->chunk_size;
    sfinfo->chunk_num = (sfinfo->filesize - sfinfo->pos) / sfinfo->chunk_size + 1;


    printf("node num %d    alive num: %d\n", node_num, sfinfo->alive_node_num);
    printf("preparation_process done\n");

error:
    if (root)
        json_decref(root);
    return ret;
}


int check_download(struct send_file_ctx * sfinfo, int sending_chunk_no){
    for(int i = 0; i < THREAD_NUM_MAX; i++)
        if( sfinfo->tp.sending_chunk_no[i] == sending_chunk_no ){
            struct evbuffer *evb = sfinfo->tp.thread_ftsi[i].evb;
            size_t len = evbuffer_get_length(evb);
            if (len == sfinfo->chunk_size
                    ||(sfinfo->filesize-sfinfo->pos-(sfinfo->chunk_num-1)*sfinfo->chunk_size == len
                       && sfinfo->chunk_num-1 == sending_chunk_no)){
                printf("chunk download over: %d\n", sending_chunk_no);
                return 1;
            }
        }
    return 0;
}

