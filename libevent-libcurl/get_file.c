#include "server.h"

size_t header_cb(char *buffer, size_t size, size_t nitems, void *userdata) {
    long start, end, len = 0;
    int r = sscanf(buffer, "Content-Range: bytes %ld-%ld/%ld", &start, &end, &len);
    if (r)
        *((long *) userdata) = len;
    return size * nitems;
}

size_t write_file_cb(char *buffer, size_t size, size_t nitems, void *userdata) {
    int ret = fwrite(buffer, size, nitems, userdata);
    fflush(userdata);
    return ret;
}

int get_file_range(struct file_transfer_session_info * ftsi) {
    int ret = 1;
    FILE *local_file_ptr;
    CURL *curlhandle = NULL;
    char range_str[URL_LENGTH_MAX];

    if ( strlen(ftsi->remote_file_url) <= 0 || strlen(ftsi->local_file_path) <= 0
         || strlen(ftsi->permission) <= 0 || ftsi->pos < 0 || ftsi->range < 0){
        ret = 0;
        goto error;
    }
    local_file_ptr = fopen(ftsi->local_file_path, ftsi->permission);
    if (local_file_ptr == NULL) {
        ret = 0;
        goto error;
    }
    curlhandle = curl_easy_init();
    if (curlhandle == NULL){
        ret = 0;
        goto error;
    }
    curl_easy_setopt(curlhandle, CURLOPT_URL, ftsi->remote_file_url);
    curl_easy_setopt(curlhandle, CURLOPT_HEADERFUNCTION, header_cb);
    curl_easy_setopt(curlhandle, CURLOPT_HEADERDATA, &(ftsi->filesize));
    curl_easy_setopt(curlhandle, CURLOPT_WRITEDATA, local_file_ptr);
    curl_easy_setopt(curlhandle, CURLOPT_WRITEFUNCTION, write_file_cb);

    if ( ftsi->filesize >= ftsi->pos + ftsi->range - 1 ){
        sprintf(range_str, "%ld-%ld", ftsi->pos, ftsi->pos + ftsi->range - 1);
    } else {
        sprintf(range_str, "%ld-", ftsi->pos);
    }
    curl_easy_setopt(curlhandle, CURLOPT_RANGE, range_str);
    CURLcode r = curl_easy_perform(curlhandle);
    if (r != CURLE_OK)
        ret = 0;
error:
    if (local_file_ptr)
        fclose(local_file_ptr);
    if (curlhandle)
        curl_easy_cleanup(curlhandle);
    return ret;
}

void *thread_run(void *ftsi){
    struct file_transfer_session_info tmp_ftsi = *((struct file_transfer_session_info *)ftsi);
//    printf("thread run:<\n remote: %s\n local: %s\n permit: %s\n pos: %ld\n range: %ld\n fsize: %ld>\n",
//           tmp_ftsi.remote_file_url, tmp_ftsi.local_file_path, tmp_ftsi.permission,
//           tmp_ftsi.pos, tmp_ftsi.range, tmp_ftsi.filesize);
    int ret = get_file_range(&tmp_ftsi);
}

int get_file(struct file_transfer_session_info * node_ftsi, size_t node_num, size_t * thread_count,
             pthread_t * thread_id, long stamp, long range){
    if (node_ftsi == NULL || node_num <= 0 || thread_count == NULL
            || thread_id == NULL || range <= 0L){
        return 0;
    }
    long alive_nodes[NODE_NUM_MAX];
    size_t alive_node_num = 0L;
    long file_size = 0L;
    char local_file_path[URL_LENGTH_MAX];
    struct file_transfer_session_info thread_ftsi[THREAD_NUM_MAX];
    memset(thread_ftsi, 0, THREAD_NUM_MAX*sizeof(struct file_transfer_session_info));
    memset(alive_nodes, 0, NODE_NUM_MAX*sizeof(int));

    /* find which node can be used for transmission */
    for(size_t i = 0; i < node_num; i++){
        if (strcmp(node_ftsi[i].protocol, "https") == 0 || strcmp(node_ftsi[i].protocol, "http") == 0){
            strcpy(node_ftsi[i].permission, "w");
            node_ftsi[i].pos = 0L;
            node_ftsi[i].range = 1L;
            get_file_range(&node_ftsi[i]);
            if ( node_ftsi[i].filesize != 0L ) {
                printf("node alive: %s\n", node_ftsi[i].remote_file_url);
                alive_nodes[alive_node_num] = i;
                alive_node_num++;
                /* if filesize and local file path havent been updated */
                if( file_size == 0 ){
                    file_size = node_ftsi[i].filesize;
                    /* all the local_file_path is the same path */
                    strcpy(local_file_path, node_ftsi[i].local_file_path);
                }
            }
        } else {
            /* magnet or other protocol added here */

        }
    }

    /* clean the tmp file for testing the available node */
    remove(node_ftsi[0].local_file_path);

    if (alive_node_num <= 0)
        return 0;

    size_t t_ct = 0;
    while (file_size > t_ct*range) {
        long index = alive_nodes[t_ct%alive_node_num];
        strcpy(thread_ftsi[t_ct].permission, "w");
        strcpy(thread_ftsi[t_ct].remote_file_url, node_ftsi[index].remote_file_url);
        sprintf(thread_ftsi[t_ct].local_file_path, "%s.%ld.slice.%ld", local_file_path, stamp, t_ct);
        thread_ftsi[t_ct].pos = t_ct * range;;
        thread_ftsi[t_ct].range = range;
        thread_ftsi[t_ct].filesize = file_size;
//        pthread_create(&thread_id[t_ct], NULL, thread_run, (void *)&(thread_ftsi[t_ct]));
        t_ct++;
    }
    *thread_count = t_ct;
    return 1;
}

size_t get_json_cb(char *buffer, size_t size, size_t nitems, void *userdata) {
    strcpy(userdata, buffer);
    return size * nitems;
}

int login(const char * username, const char * password, char * token){
    int ret = 1;
    char jsonData[URL_LENGTH_MAX];
    CURL *curlhandle = NULL;
    struct curl_slist *list = NULL;

    if (username == NULL || password == NULL){
        ret = 0;
        goto error;
    }
    sprintf(jsonData, "{\"user\":\"%s\",\"password\":\"%s\"}", username, password);
    curlhandle = curl_easy_init();
    if (curlhandle == NULL){
        ret = 0;
        goto error;
    }
    curl_easy_setopt(curlhandle, CURLOPT_URL, "https://api.webrtc.win:6601/v1/customer/login");
    curl_easy_setopt(curlhandle, CURLOPT_WRITEDATA, token);
    curl_easy_setopt(curlhandle, CURLOPT_WRITEFUNCTION, get_json_cb);
    list = curl_slist_append(NULL, "Content-Type:application/json");
    curl_easy_setopt(curlhandle, CURLOPT_HTTPHEADER, list);
    curl_easy_setopt(curlhandle, CURLOPT_POSTFIELDS, jsonData);

    CURLcode res = curl_easy_perform(curlhandle);
    if(res != CURLE_OK)
        ret = 0;
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
        ret = 0;
        goto error;
    }
    sprintf(url, "https://api.webrtc.win:6601/v1/customer/nodes?client_ip=%s&host=%s&uri=%s&md5=%s", client_ip, host, uri, md5);
    sprintf(token_header, "X-Pear-Token: %s", token);
    curlhandle = curl_easy_init();
    if (curlhandle == NULL){
        ret = 0;
        goto error;
    }
    curl_easy_setopt(curlhandle, CURLOPT_URL, url);
    curl_easy_setopt(curlhandle, CURLOPT_WRITEDATA, nodes);
    curl_easy_setopt(curlhandle, CURLOPT_WRITEFUNCTION, get_json_cb);
    list = curl_slist_append(NULL, token_header);
    list = curl_slist_append(list, "Content-Type:application/json");
    curl_easy_setopt(curlhandle, CURLOPT_HTTPHEADER, list);

    CURLcode res = curl_easy_perform(curlhandle);
    if(res != CURLE_OK)
        ret = 0;
error:
    if (list)
        curl_slist_free_all(list);
    if (curlhandle)
        curl_easy_cleanup(curlhandle);
    return ret;
}

int vdn_proc(const char * uri, size_t *thread_count, pthread_t *thread_id, long stamp){
    char token[URL_LENGTH_MAX*10];
    char nodes[URL_LENGTH_MAX*20];
    struct file_transfer_session_info ftsi_list[NODE_NUM_MAX];
    json_error_t error;
    json_t *root = NULL;
    size_t node_num = 0L;

    memset(nodes, 0, URL_LENGTH_MAX*sizeof(char));
    memset(ftsi_list, 0, NODE_NUM_MAX*sizeof(struct file_transfer_session_info));

    /* Must initialize libcurl before any threads are started */
    curl_global_init(CURL_GLOBAL_ALL);

    login("test", "123456", token);
    root = json_loads(token, 0, &error);
    while (!root) {
        login("test", "123456", token);
        root = json_loads(token, 0, &error);
    }
    json_t *token_j = json_object_get(root, "token");
    const char *token_str = json_string_value(token_j);

    get_node("127.0.0.1", "qq.webrtc.win", uri, "ab340d4befcf324a0a1466c166c10d1d", token_str, nodes);
//    printf("node: <%s>\n", nodes);
    /* use json to get node */
//    root = json_loads(nodes, 0, &error);
//    json_t *nodes_j = json_object_get(root, "nodes");
//    while (!json_is_array(nodes_j)) {
//        get_node("127.0.0.1", "qq.webrtc.win", uri, "ab340d4befcf324a0a1466c166c10d1d", token_str, nodes);
//        memset(nodes, 0, URL_LENGTH_MAX*sizeof(char));
//        root = json_loads(nodes, 0, &error);
//        nodes_j = json_object_get(root, "nodes");
//    }
//    node_num = json_array_size(json_object_get(root, "nodes"));
//    for(size_t i = 0; i < node_num; i++){
//        json_t *node = json_array_get(json_object_get(root, "nodes"), i);
//        json_t *protocol = json_object_get(node, "protocol");
//        const char *protocol_str = json_string_value(protocol);
//        strcpy(ftsi_list[i].protocol, protocol_str);
//        strcpy(ftsi_list[i].local_file_path, "tmp/1.mp4");
//        if (strcmp(protocol_str, "https") == 0 || strcmp(protocol_str, "http") == 0){
//            json_t *host = json_object_get(node, "host");
//            const char *host_str = json_string_value(host);
//            sprintf(ftsi_list[i].remote_file_url, "%s://%s/qq.webrtc.win/%s", protocol_str, host_str, uri);
//        } else {
//            json_t *magnet = json_object_get(node, "magnet_uri");
//            const char *magnet_str = json_string_value(magnet);
//            sprintf(ftsi_list[i].remote_file_url, "%s\n", magnet_str);
//        }
//    }
//    json_decref(root);



//    char *pattern = "\"host\": \"(.{28,29})\"";


    node_num = 2L;
    strcpy(ftsi_list[0].protocol, "https");
    strcpy(ftsi_list[1].protocol, "http");
    strcpy(ftsi_list[0].remote_file_url, "https://qq.webrtc.win/tv/pear001.mp4");
    strcpy(ftsi_list[1].remote_file_url, "https://2076933d87ad.webrtc.win:22371/qq.webrtc.win/tv/pear001.mp4");
    strcpy(ftsi_list[0].local_file_path, "tmp/1.mp4");
    strcpy(ftsi_list[1].local_file_path, "tmp/1.mp4");
    printf("node num %ld\n", node_num);
    get_file(ftsi_list, node_num, thread_count, thread_id, stamp, download_file_range);
    printf("vdn proc done\n");
}
