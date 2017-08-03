#include "server.h"

int sort_alive_nodes(struct send_file_ctx *sfinfo, struct file_transfer_session_info * thread_ftsi){

    for(int i = 1; i < sfinfo->alive_node_num; i++) {
        int j = i;
        while(j > 0 && (thread_ftsi[j].download_speed > thread_ftsi[j-1].download_speed)) {
            struct file_transfer_session_info tmp;
            memcpy(&tmp, &thread_ftsi[j], sizeof(struct file_transfer_session_info));
            memcpy(&thread_ftsi[j], &thread_ftsi[j-1], sizeof(struct file_transfer_session_info));
            memcpy(&thread_ftsi[j-1], &tmp, sizeof(struct file_transfer_session_info));
            j--;
        }
    }

    for(int i = 0; i < sfinfo->alive_node_num; i++) {
        memcpy(&sfinfo->alive_nodes[i], &(thread_ftsi[i].ni), sizeof(struct node_info));
    }

}
