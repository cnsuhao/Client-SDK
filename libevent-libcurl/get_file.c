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

int get_file_range(const char * remote_file_path, const char * local_file_path, const char * permission, long * filesize, long pos, long range) {
    int ret = 1;
    if (remote_file_path == NULL || local_file_path == NULL || filesize == NULL || pos < 0 || range < 0){
        ret = 0;
        goto error;
    }
    FILE *local_file_ptr;
    local_file_ptr = fopen(local_file_path, permission);
    if (local_file_ptr == NULL) {
        ret = 0;
        goto error;
    }

    CURL *curlhandle = NULL;
    curl_global_init(CURL_GLOBAL_ALL);
    curlhandle = curl_easy_init();
    if (curlhandle == NULL){
        ret = 0;
        goto error;
    }
    curl_easy_setopt(curlhandle, CURLOPT_URL, remote_file_path);
    curl_easy_setopt(curlhandle, CURLOPT_HEADERFUNCTION, header_cb);
    curl_easy_setopt(curlhandle, CURLOPT_HEADERDATA, filesize);
    curl_easy_setopt(curlhandle, CURLOPT_WRITEDATA, local_file_ptr);
    curl_easy_setopt(curlhandle, CURLOPT_WRITEFUNCTION, write_file_cb);

    struct curl_slist *headers = NULL;
    char range_str[1024];
    if ( *(filesize) >= pos + range - 1 ){
        sprintf(range_str, "%ld-%ld", pos, pos + range - 1);
    } else {
        sprintf(range_str, "%ld-", pos);
    }
    curl_easy_setopt(curlhandle, CURLOPT_RANGE, range_str);
    CURLcode r = curl_easy_perform(curlhandle);
    if (r != CURLE_OK)
        ret = 0;
error:
    if (local_file_ptr) {
        fclose(local_file_ptr);
    }
    if (headers != NULL)
        curl_slist_free_all(headers);
    curl_easy_cleanup(curlhandle);
    curl_global_cleanup();
    return ret;
}

int get_file(const char ** remote_file_paths, const char * local_file_path, size_t node_num, long range){

    long filesize[NODE_NUM_MAX];
    for(size_t i = 0; i < node_num; i++){
        printf("node: %s\n", remote_file_paths[i]);
        filesize[i] = 0L;
        get_file_range(remote_file_paths[i], local_file_path, "w", &filesize[i], 0L, 1L);
        if( filesize[i] == 0L )
            printf("node: %s cant get data\n", remote_file_paths[i]);
    }
}

size_t get_json_cb(char *buffer, size_t size, size_t nitems, void *userdata) {
    printf("%s\n%ld\n%ld\n%s\n", buffer, size, nitems, (char *)userdata);
    strcpy(userdata, buffer);
    return 1;
}

int login(const char * username, const char * password, char * token){
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
    curl_easy_setopt(curlhandle, CURLOPT_WRITEFUNCTION, get_json_cb);
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

int get_node(char * client_ip, char * host, const char * uri, char * md5, const char * token, char * nodes){
    int ret = 1;
    if (client_ip == NULL || host == NULL || uri == NULL || md5 == NULL || token == NULL){
        ret = 0;
        goto error;
    }

    char url[1024];
    sprintf(url, "https://api.webrtc.win:6601/v1/customer/nodes?client_ip=%s&host=%s&uri=%s&md5=%s", client_ip, host, uri, md5);
    char token_header[1024];
    sprintf(token_header, "X-Pear-Token: %s", "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJwZWFyIjoicGVhci1yZXN0ZnVsLWFwaS0zMzQ0IiwiZXhwIjoxNTAwMjg2MDE4LCJqdGkiOiIxODMuNjAuNDAuMTA0OjE2MDEwIiwiaXNzIjoiZm9nLWxvZ2luLWFwaSIsInN1YiI6InRlc3QifQ.qqfPwhsFRd-HcxnAUebk5QUDZUAlMl2Ko5xLkZuP2Og");

    CURL *curlhandle = NULL;
    curl_global_init(CURL_GLOBAL_ALL);
    curlhandle = curl_easy_init();
    if (curlhandle == NULL){
        ret = 0;
        goto error;
    }

    curl_easy_setopt(curlhandle, CURLOPT_URL, url);
    curl_easy_setopt(curlhandle, CURLOPT_WRITEDATA, nodes);
    curl_easy_setopt(curlhandle, CURLOPT_WRITEFUNCTION, get_json_cb);
    struct curl_slist *list = curl_slist_append(NULL, token_header);
    list = curl_slist_append(list, "Content-Type:application/json");
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
    char token[1024*10];
    char nodes[1024*20];
    memset(nodes, 0, 1024*20);
    char remote_file_paths[NODE_NUM_MAX][1024];
//    login("test", "123456", token);
    json_error_t error;
    json_t *root = json_loads(token, 0, &error);
//    if (!root) {
//       login("test", "123456", token);
//       root = json_loads(token, 0, &error);
//    }
//    json_t *token_j = json_object_get(root, "token");
//    const char *token_str = json_string_value(token_j);

    get_node("127.0.0.1", "qq.webrtc.win", uri, "ab340d4befcf324a0a1466c166c10d1d", "", nodes);
    //printf("node: <%s>\n", nodes);
    //    root = json_loads(nodes, 0, &error);
    //    while (!json_is_array(root)) {
    //        get_node("127.0.0.1", "qq.webrtc.win", uri, "ab340d4befcf324a0a1466c166c10d1d", token_str, nodes);
    //        memset(nodes, 0, 1024*20);
    //        root = json_loads(nodes, 0, &error);
    //        break;
    //    }
    size_t node_num = json_array_size(json_object_get(root, "nodes"));
    for(size_t i = 0; i < node_num; i++){
        json_t *node = json_array_get(json_object_get(root, "nodes"), i);
        json_t *protocol = json_object_get(node, "protocol");
        json_t *host = json_object_get(node, "host");
        const char *host_str = json_string_value(host);
        const char *protocol_str = json_string_value(protocol);
        sprintf(remote_file_paths[i], "%s://%s%s", protocol_str, host_str, uri);
        printf("node%ld: %s\n", i, remote_file_paths[i]);
    }
    json_decref(root);
    printf("node num %ld\n", node_num);
    get_file((const char **)remote_file_paths, "1.mp4", node_num, 10L);
}
