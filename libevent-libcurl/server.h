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
#define NODE_NUM_MAX 50
#define THREAD_NUM_MAX 50

static const struct table_entry {
    const char *extension;
    const char *content_type;
} content_type_table[] = {
{ "txt", "text/plain" },
{ "html", "text/html" },
{ "mp4", "video/mp4" },
{ NULL, NULL },
};

/**
 * @节点信息 The node_info struct
 */
struct node_info {
    /**
     * @brief 该节点支持的协议
     */
    char protocol[10];
    /**
     * @brief 该节点的url
     */
    char remote_file_url[URL_LENGTH_MAX];
    /**
     * @brief 节点的类型，服务器或普通节点
     */
    char type[10];
    /**
     * @brief 节点的打分，用于节点的排序(暂时未用到)
     */
    double score;
};

/**
 * @brief 某个线程开启下载所需要的信息
 */
struct file_transfer_session_info {
    /**
     * @brief 该线程下载文件所使用的节点的信息
     */
    struct node_info ni;
    /**
     * @brief 下载文件的起始位置，单位字节
     */
    size_t pos;
    /**
     * @brief 下载文件的长度，单位字节
     */
    size_t range;
    /**
     * @brief 文件的总长度
     */
    size_t filesize;
    /**
     * @brief 该线程下载的平均速度
     */
    double download_speed;
    /**
     * @brief 该线程下载时保存数据的缓冲区，由libevent提供
     */
    struct evbuffer *evb;
};

/**
 * @brief 线程池
 */
struct thread_pool {
    /**
     * @brief 当前线程池中的窗口数量
     */
    int win_num;
    /**
     * @brief 当前线程池中每个线程正在下载的chunk的编号
     */
    int sending_chunk_no[THREAD_NUM_MAX];
    /**
     * @brief 每个线程开启下载所对应的信息
     */
    struct file_transfer_session_info thread_ftsi[THREAD_NUM_MAX];
    /**
     * @brief 线程ID池
     */
    pthread_t thread_id[THREAD_NUM_MAX];
};

/**
 * @brief 每个请求的Context
 */
struct send_file_ctx {
    /**
     * @brief 请求
     */
    struct evhttp_request *req;
    /**
     * @brief 下载文件的事件
     */
    struct event *tm_ev;
    /**
     * @brief 登录的用户名
     */
    char username[20];
    /**
     * @brief 密码
     */
    char password[20];
    /**
     * @brief 客户端ip
     */
    char client_ip[15];
    /**
     * @brief 主机名称
     */
    char host[URL_LENGTH_MAX];
    /**
     * @brief 请求的文件的uri
     */
    char uri[50];
    /**
     * @brief 文件的md5
     */
    char md5[50];
    /**
     * @brief 本地对应的文件路径，日后做文件缓存时用到(现在未使用)
     */
    char whole_path[URL_LENGTH_MAX];
    /**
     * @brief 可用节点数
     */
    int alive_node_num;
    /**
     * @brief 可用节点的信息
     */
    struct node_info alive_nodes[NODE_NUM_MAX];

    /**
     * @brief 线程池
     */
    struct thread_pool tp;
    /**
     * @brief 发送了的chunk的数量
     */
    int sent_chunk_num;
    /**
     * @brief 文件大小
     */
    size_t filesize;
    /**
     * @brief 窗口的大小
     */
    size_t window_size;
    /**
     * @brief chunk的总数量
     */
    int chunk_num;
    /**
     * @brief 每个chunk的大小
     */
    size_t chunk_size;
    /**
     * @brief 每个win中的chunk数
     */
    int chk_in_win_ct;

    /**
     * @brief 计时器
     */
    int timer;
};

static struct timeval timeout = { 1, 0 };

struct event_base *base;
struct evhttp *http;
struct evhttp_bound_socket *handle;


/* send_file.c */

/* guess_content_type
 * 猜测请求的文件类型
 * path: 请求的文件路径
 * return: 返回文件类型
*/
const char * guess_content_type(const char *path);
/* send_file_cb
 * 发送缓存文件的回调函数
 * return
 */
void send_file_cb(int fd, short events, void *ctx);

/* do_request_cb
 * 响应对本地文件的请求的libevent的回调函数
 * return
 */
void do_request_cb(struct evhttp_request *req, void *arg);












/* get_file.c */

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
/* window_download
 * 从webrtc服务器请求整个视频文件的函数
 * sfinfo: 本次发送的Context
 * return: 请求是否成功
*/
int window_download(struct send_file_ctx *sfinfo);
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
 * ni_list: 节点信息
 * node_num: 节点数量
 * sfinfo: 本次发送的Context
 * return: 请求是否成功
*/
int get_node_alive(struct node_info * ni_list, size_t node_num, struct send_file_ctx *sfinfo);

/* node_info_init
 * 根据获得的json初始化node列表
 * sfinfo: 本次发送的Context
 * ni_list: 节点信息
 * nodes: json字符串
 * return: 节点数目
*/
int node_info_init(struct send_file_ctx *sfinfo, struct node_info * ni_list, char * nodes);

/* preparation_process
 * 从webrtc服务器请求视频文件的准备工作，比如登录，获取节点以及检测可用节点
 * sfinfo: 本次发送的Context
 * ni_list: 节点信息
 * return: 请求是否成功
*/
int preparation_process(struct send_file_ctx * sfinfo, struct node_info * ni_list);













/* first_aid.c */

/* sort_alive_nodes
 * 对node进行排序
 * sfinfo: 本次发送的Context
 * download_speeds: 节点下载速度的数组
 * return
*/
void sort_alive_nodes(struct send_file_ctx *sfinfo, double * download_speeds);

#endif
