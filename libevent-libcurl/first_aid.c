#include "server.h"

void sort_alive_nodes(struct send_file_ctx *sfinfo, double * download_speeds){

    for(int i = 1; i < sfinfo->alive_node_num; i++) {
        int j = i;
        while(j > 0 && download_speeds[j] > download_speeds[j-1]) {
            double tmp;
            struct node_info tmp_node;
            tmp = download_speeds[j];
            download_speeds[j] = download_speeds[j-1];
            download_speeds[j-1] = tmp;
            memcpy(&tmp_node, &sfinfo->alive_nodes[j], sizeof(struct node_info));
            memcpy(&sfinfo->alive_nodes[j], &sfinfo->alive_nodes[j-1], sizeof(struct node_info));
            memcpy(&sfinfo->alive_nodes[j-1], &tmp_node, sizeof(struct node_info));
            j--;
        }
    }

}
