#ifndef FIRST_AID_H
#define FIRST_AID_H
#include "server.h"

/* sort_alive_nodes
 * 对node进行排序
 * sfinfo: 本次发送的Context
 * download_speeds: 节点下载速度的数组
 * return
*/
void sort_alive_nodes(struct send_file_ctx *sfinfo, double * download_speeds);

#endif // FIRST_AID_H
