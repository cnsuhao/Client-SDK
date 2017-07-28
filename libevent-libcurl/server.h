/* http://www.cnblogs.com/zlcxbb/p/6006861.html */

#ifndef __SERVER_H_
#define __SERVER_H_
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <sys/stat.h>
#include <sys/socket.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <pthread.h>
#include <regex.h>
#include <time.h>
#include <assert.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>


#include <event2/event.h>
#include <event2/http.h>
#include <event2/buffer.h>
#include <event2/util.h>
#include <event2/keyvalq_struct.h>
#include <event2/bufferevent.h>
#include <event2/listener.h>

#include <curl/curl.h>
#include <jansson.h>

#ifdef EVENT__HAVE_NETINET_IN_H
#include <netinet/in.h>
# ifdef _XOPEN_SOURCE_EXTENDED
#  include <arpa/inet.h>
# endif
#endif

#define URL_LENGTH_MAX 1024
#define NODE_NUM_MAX 100
#define THREAD_NUM_MAX 100

static const struct table_entry {
    const char *extension;
    const char *content_type;
} content_type_table[] = {
{ "txt", "text/plain" },
{ "html", "text/html" },
{ "mp4", "video/mp4" },
{ NULL, NULL },
};

struct node_info {
    char protocol[10];
    char remote_file_url[URL_LENGTH_MAX];
    long pos;
    long range;
    long filesize;

};

struct file_transfer_session_info {
    struct node_info ni;

    struct evbuffer *evb;
};

struct send_file_ctx {
    struct evhttp_request *req;
    struct event *tm_ev;

    char username[20];
    char password[20];
    char client_ip[15];
    char host[URL_LENGTH_MAX];
    char uri[50];
    char md5[50];

    char whole_path[URL_LENGTH_MAX];
    size_t alive_node_num;
    int alive_nodes_index[NODE_NUM_MAX];

    size_t thread_count;
    size_t completed_count;
    pthread_t thread_id[THREAD_NUM_MAX];

    struct evbuffer * evb_array[THREAD_NUM_MAX];

    long window_size;
    long chunk_size;
};

static struct timeval timeout = { 1, 0 };

struct event_base *base;
struct evhttp *http;
struct evhttp_bound_socket *handle;

/* guess_content_type
 * 猜测请求的文件类型
 * path: 请求的文件路径
 * return: 返回文件类型
*/
const char * guess_content_type(const char *path);
/* send_file_cb
 * 发送缓存文件的回调函数
 */
void send_file_cb(int fd, short events, void *ctx);

/* do_request_cb
 * 响应对本地文件的请求的libevent的回调函数
 */
void do_request_cb(struct evhttp_request *req, void *arg);


/* header_cb
 * 从webrtc服务器下载视频文件时从response header中获取content length的回调函数
 * buffer: response header的内容
 * size: *
 * nitems: *
 * userdata: 用户传入的数据地址
 * return: size * nitems
*/
size_t header_cb(char *buffer, size_t size, size_t nitems, void *userdata);
/* write_buffer_cb
 * 从webrtc服务器下载视频文件时写入内存的回调函数
 * buffer: 从服务器读取到的视频文件数据
 * size: *
 * nitems: *
 * userdata: evbuffer指针
 * return: 是否写成功
*/
size_t write_buffer_cb(char *buffer, size_t size, size_t nitems, void *userdata);
/* get_file_range
 * 从webrtc服务器请求视频文件中部分数据的函数
 * ftsi: 文件传输会话的具体信息
 * return: 请求是否成功
*/
int get_file_range(struct file_transfer_session_info * ftsi);
/* thread_run
 * 从webrtc服务器请求视频文件中部分数据的线程函数
 * ftsi: 文件传输会话的具体信息
*/
void *thread_run(void *ftsi);
/* get_file
 * 从webrtc服务器请求整个视频文件的函数
 * sfinfo: 本次发送的Context
 * ni_list: 节点信息
 * return: 请求是否成功
*/
int get_file(struct send_file_ctx * sfinfo, struct node_info * ni_list);
/* joint_string_cb
 * 从webrtc服务器获取json数据并拼接到userdata尾部的回调函数
 * buffer: 从服务器读取到的视频文件数据
 * size: *
 * nitems: *
 * userdata: 字符串数组
 * return: 1
*/
size_t joint_string_cb(char *buffer, size_t size, size_t nitems, void *userdata);
/* login
 * 登录webrtc服务器获取token
 * username: 用户名
 * password: 密码
 * token: 获取到的token
 * return: 请求是否成功
*/
int login(const char * username, const char * password, char * token);
/* get_node
 * 从webrtc服务器获取多个节点
 * client_ip: 用户端ip
 * host: host
 * uri: 请求视频文件的uri
 * md5: md5
 * token: login函数获得的token
 * nodes: 返回的节点字符串
 * return: 请求是否成功
*/
int get_node(char * client_ip, char * host, const char * uri, char * md5, const char * token, char * nodes);

/* get_node_alive
 * 获取多个节点中活着的节点
 * sfinfo: 本次发送的Context
 * ni_list: 节点信息
 * node_num: 节点数量
 * alive_node_num: 活着的节点数量的指针
 * return: 活着的节点数目
*/
int get_node_alive(struct node_info * ni_list, size_t node_num, int *alive_nodes_index);

/* preparation_process
 * 从webrtc服务器请求视频文件的准备工作，比如登录，获取节点以及检测可用节点
 * sfinfo: 本次发送的Context
 * ni_list: 节点信息
 * return: 请求是否成功
*/
int preparation_process(struct send_file_ctx * sfinfo, struct node_info * ni_list);


#endif
