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
    char range_str[1024];

    if ( strlen(ftsi->remote_file_path) <= 0 || strlen(ftsi->local_file_path) <= 0
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
    curl_easy_setopt(curlhandle, CURLOPT_URL, ftsi->remote_file_path);
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

int get_file(struct file_transfer_session_info * ftsi_list, size_t node_num){
    pthread_t tid[THREAD_NUM_MAX];
    int good_node_sign[NODE_NUM_MAX];

    for(size_t i = 0; i < node_num; i++){
        strcpy(ftsi_list[i].permission, "w");
        ftsi_list[i].pos = 0L;
        ftsi_list[i].range = 1L;
        get_file_range(&ftsi_list[i]);
        if( ftsi_list[i].filesize == 0L ){
            good_node_sign[i] = 0;
            printf("node: %s cant get data\n", ftsi_list[i].remote_file_path);
        }
        else{
            good_node_sign[i] = 1;
            printf("node: %s can get data\n", ftsi_list[i].remote_file_path);
        }
    }

    //    for(int i=0; i< NUMT; i++) {
    //        int error = pthread_create(&tid[i],
    //                               NULL, /* default attributes please */
    //                               pull_one_url,
    //                               (void *)urls[i]);
    //        if(0 != error)
    //            fprintf(stderr, "Couldn't run thread number %d, errno %d\n", i, error);
    //        else
    //            fprintf(stderr, "Thread %d, gets %s\n", i, urls[i]);
    //    }

    //    /* now wait for all threads to terminate */
    //    for(i=0; i< NUMT; i++) {
    //        error = pthread_join(tid[i], NULL);
    //        fprintf(stderr, "Thread %d terminated\n", i);
    //    }
}

size_t get_json_cb(char *buffer, size_t size, size_t nitems, void *userdata) {
    strcpy(userdata, buffer);
    return 1;
}

int login(const char * username, const char * password, char * token){
    int ret = 1;
    char jsonData[1024];
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
    char url[1024];
    char token_header[1024];
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

int vdn_proc(const char * uri){
    char token[1024*10];
    char nodes[1024*20];
    struct file_transfer_session_info ftsi_list[NODE_NUM_MAX];
    json_error_t error;
    json_t *root = NULL;
    size_t node_num = 0L;

    /* Must initialize libcurl before any threads are started */
    curl_global_init(CURL_GLOBAL_ALL);
    memset(nodes, 0, 1024*20);
    memset(ftsi_list, 0, NODE_NUM_MAX*sizeof(struct file_transfer_session_info));
    login("test", "123456", token);
    root = json_loads(token, 0, &error);
    while (!root) {
        login("test", "123456", token);
        root = json_loads(token, 0, &error);
    }
    json_t *token_j = json_object_get(root, "token");
    const char *token_str = json_string_value(token_j);

    get_node("127.0.0.1", "qq.webrtc.win", uri, "ab340d4befcf324a0a1466c166c10d1d", token_str, nodes);
    printf("node: <%s>\n", nodes);
    //    root = json_loads(nodes, 0, &error);
    //    while (!json_is_array(root)) {
    //        get_node("127.0.0.1", "qq.webrtc.win", uri, "ab340d4befcf324a0a1466c166c10d1d", token_str, nodes);
    //        memset(nodes, 0, 1024*20);
    //        root = json_loads(nodes, 0, &error);
    //        break;
    //    }
    //    node_num = json_array_size(json_object_get(root, "nodes"));
    node_num = 2L;
    for(size_t i = 0; i < node_num; i++){
        //        json_t *node = json_array_get(json_object_get(root, "nodes"), i);
        //        json_t *protocol = json_object_get(node, "protocol");
        //        json_t *host = json_object_get(node, "host");
        //        const char *host_str = json_string_value(host);
        //        const char *protocol_str = json_string_value(protocol);
        //        sprintf(ftsi_list[i].remote_file_path, "%s://%s/qq.webrtc.win/%s", protocol_str, host_str, uri);
        strcpy(ftsi_list[i].local_file_path, "1.mp4");
    }
    json_decref(root);
    strcpy(ftsi_list[0].remote_file_path, "https://qq.webrtc.win/tv/pear001.mp4");
    strcpy(ftsi_list[1].remote_file_path, "https://000c29d049f4.webrtc.win:61530/qq.webrtc.win/tv/pear001.mp4");
    ftsi_list[0].range = 10L;
    ftsi_list[1].range = 10L;
    printf("node num %ld\n", node_num);
    get_file(ftsi_list, node_num);
    printf("vdn proc done\n");
}
