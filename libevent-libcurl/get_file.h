#ifndef GET_FILE_H
#define GET_FILE_H
#include "server.h"

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
void window_download(struct send_file_ctx *sfinfo);
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

/* check_download
 * 检查第i个thread是否下载完
 * sfinfo: 本次发送的Context
 * return: 是否下载完
*/
int check_download(struct send_file_ctx * sfinfo, int sending_chunk_no);

/* check_timeout
 * 检查是否出现下载超时
 * sfinfo: 本次发送的Context
 * return: 是否出现下载超时
*/
int check_timeout(struct send_file_ctx * sfinfo);


#endif // GET_FILE_H
