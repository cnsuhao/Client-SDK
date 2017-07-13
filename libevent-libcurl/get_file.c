#include "server.h"

size_t header_cb(char *buffer, size_t size, size_t nitems, void *userdata) {
    long start, end, len = 0;
    int r = sscanf(buffer, "Content-Range: bytes %ld-%ld/%ld", &start, &end, &len);
    if (r)
        *((long *) userdata) = len;
    return size * nitems;
}

/* 保存下载文件 */
size_t write_file_cb(char *buffer, size_t size, size_t nitems, void *userdata) {
    int ret = fwrite(buffer, size, nitems, userdata);
    fflush(userdata);
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

int get_file_cb(const char * remote_file_path, const char * local_file_path, long range){
    int ret = 1;
    if (range <= 0 || remote_file_path == NULL || local_file_path == NULL){
        ret = 0;
        goto error;
    }
    CURL *curlhandle = NULL;
    curl_global_init(CURL_GLOBAL_ALL);
    curlhandle = curl_easy_init();

    long filesize = 0;
    FILE *local_file_ptr;
    //采用追加方式打开文件，便于实现文件断点续传工作
    local_file_ptr = fopen(local_file_path, "ab+");
    if (local_file_ptr == NULL) {
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
    curl_easy_setopt(curlhandle, CURLOPT_WRITEDATA, local_file_ptr);
    curl_easy_setopt(curlhandle, CURLOPT_WRITEFUNCTION, write_file_cb);

    get_file_range_cb(curlhandle, &filesize, 0L, 0L);
    for(long i = 1L; i < filesize; i = i + range){
        if( i + range - 1 < filesize )
            get_file_range_cb(curlhandle, &filesize, i, i + range - 1);
        else
            get_file_range_cb(curlhandle, &filesize, i, filesize - 1);
    }

error:
    if (local_file_ptr) {
        fclose(local_file_ptr);
    }
    curl_easy_cleanup(curlhandle);
    curl_global_cleanup();
    return ret;
}

size_t parse_token_cb(char *buffer, size_t size, size_t nitems, void *userdata) {
    json_t *root;
    json_error_t error;
    root = json_loads(buffer, 0, &error);
    if (!root) {
        return 0;
    }
    json_t *info_str_json = json_object_get(root, "token");
    const char *info_str = json_string_value(info_str_json);
    strcpy(userdata, info_str);
    json_decref(root);
    return 1;
}

int login_cb(const char * username, const char * password, char * token){
    int ret = 1;
    if (username == NULL || password == NULL){
        ret = 0;
        goto error;
    }
    char jsonData[1024];
    sprintf(jsonData, "{\"user\":\"%s\",\"password\":\"%s\"}", username, password);

    CURL *curlhandle = NULL;
    curl_global_init(CURL_GLOBAL_ALL);
    curlhandle = curl_easy_init();
    if (curlhandle == NULL){
        ret = 0;
        goto error;
    }
    curl_easy_setopt(curlhandle, CURLOPT_URL, "https://api.webrtc.win:6601/v1/customer/login");
    curl_easy_setopt(curlhandle, CURLOPT_WRITEDATA, token);
    curl_easy_setopt(curlhandle, CURLOPT_WRITEFUNCTION, parse_token_cb);
    struct curl_slist *list = curl_slist_append(NULL, "Content-Type:application/json");
    curl_easy_setopt(curlhandle, CURLOPT_HTTPHEADER, list);
    curl_easy_setopt(curlhandle, CURLOPT_POSTFIELDS, jsonData);

    CURLcode res = curl_easy_perform(curlhandle);
    if(res != CURLE_OK)
        ret = 0;
error:
    if (list)
        curl_slist_free_all(list);
    curl_easy_cleanup(curlhandle);
    curl_global_cleanup();
    return ret;
}

size_t parse_node_cb(char *buffer, size_t size, size_t nitems, void *userdata) {
    strcpy(userdata, buffer);
    return 1;
}

int get_node_cb(char * client_ip, char * host, char * uri, char * md5, char * token, char * nodes){
    int ret = 1;
    if (client_ip == NULL || host == NULL || uri == NULL || md5 == NULL || token == NULL){
        ret = 0;
        goto error;
    }

    char url[1024];
    sprintf(url, "https://api.webrtc.win:6601/v1/customer/nodes?client_ip=%s&host=%s&uri=%s&md5=%s", client_ip, host, uri, md5);
    char token_header[1024];
    sprintf(token_header, "X-Pear-Token: %s", token);
    printf("url: %s\n", url);


    CURL *curlhandle = NULL;
    curl_global_init(CURL_GLOBAL_ALL);
    curlhandle = curl_easy_init();
    if (curlhandle == NULL){
        ret = 0;
        goto error;
    }

    curl_easy_setopt(curlhandle, CURLOPT_URL, url);
    curl_easy_setopt(curlhandle, CURLOPT_WRITEDATA, nodes);
    curl_easy_setopt(curlhandle, CURLOPT_WRITEFUNCTION, parse_node_cb);
    struct curl_slist *list = curl_slist_append(NULL, "Content-Type:application/json");
    list = curl_slist_append(list, token_header);
    curl_easy_setopt(curlhandle, CURLOPT_HTTPHEADER, list);

    CURLcode res = curl_easy_perform(curlhandle);
    if(res != CURLE_OK)
        ret = 0;
error:
    if (list)
        curl_slist_free_all(list);
    curl_easy_cleanup(curlhandle);
    curl_global_cleanup();
    return ret;
}

int vdn_proc(const char * uri){
    char token[1024];
    char nodes[2048];
    login_cb("test", "123456", token);
    printf("token: %s\n", token);
    get_node_cb("127.0.0.1", "qq.webrtc.win", "/tv/pear001.mp4", "ab340d4befcf324a0a1466c166c10d1d", token, nodes);
//    json_t *root;
//    json_error_t error;
//    root = json_loads(buffer, 0, &error);
//    if (!root) {
//        return 0;
//    }
//    json_t *info_str_json = json_object_get(root, "token");
//    const char *info_str = json_string_value(info_str_json);
//    strcpy(userdata, info_str);
//    json_decref(root);

    printf("nodes: %s\n", nodes);

    get_file_cb("http://183.60.40.104/qq/tv/1.txt", "1.txt", 10L);
}
