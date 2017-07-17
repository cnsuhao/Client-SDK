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

#define NODE_NUM_MAX 100

static const struct table_entry {
    const char *extension;
    const char *content_type;
} content_type_table[] = {
{ "txt", "text/plain" },
{ "html", "text/html" },
{ "mp4", "video/mp4" },
{ NULL, NULL },
};

/* guess_content_type
 * 猜测请求的文件类型
 * path: 请求的文件路径
 * return: 返回文件类型
*/
const char * guess_content_type(const char *path);
/* send_file_cb
 * 响应对本地文件的请求的libevent的回调函数
 */
void send_file_cb(struct evhttp_request *req, void *arg);

/* header_cb
 * 从webrtc服务器下载视频文件时从response header中获取content length的回调函数
 * buffer: response header的内容
 * size: *
 * nitems: *
 * userdata: 用户传入的数据地址
 * return: size * nitems
*/
size_t header_cb(char *buffer, size_t size, size_t nitems, void *userdata);
/* write_file_cb
 * 从webrtc服务器下载视频文件时写入本地文件的回调函数
 * buffer: 从服务器读取到的视频文件数据
 * size: *
 * nitems: *
 * userdata: 文件指针
 * return: 是否写成功
*/
size_t write_file_cb(char *buffer, size_t size, size_t nitems, void *userdata);
/* get_file_range
 * 从webrtc服务器请求视频文件中部分数据的函数
 * remote_file_path: 服务器上的文件地址
 * local_file_path: 本地文件地址
 * permission: 文件的打开权限
 * filesize: 文件大小
 * pos: 文件读取的起始范围
 * range: 文件读取的范围
 * return: 请求是否成功
*/
int get_file_range(const char * remote_file_path, const char * local_file_path, const char * permission, long * filesize, long pos, long range);
/* get_file
 * 从webrtc服务器请求整个视频文件的函数
 * remote_file_paths: 返回的多个webrtc节点上的文件地址
 * local_file_path: 本地文件地址
 * node_num: 节点数量
 * range: 文件读取的范围
 * return: 请求是否成功
*/
int get_file(const char ** remote_file_paths, const char * local_file_path, size_t node_num, long range);
/* get_json_cb
 * 从webrtc服务器获取json数据的回调函数
 * buffer: 从服务器读取到的视频文件数据
 * size: *
 * nitems: *
 * userdata: 字符串数组
 * return: 1
*/
size_t get_json_cb(char *buffer, size_t size, size_t nitems, void *userdata);
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

/* vdn_proc
 * 从webrtc服务器请求视频文件的主函数
 * uri: 请求视频文件的uri
 * return: 请求是否成功
*/
int vdn_proc(const char * uri);


#endif
