# 整体结构
根据Client-SDK/README.md可知，整体架构为
![client architecture](../fig/client_architecture.png)
对于local http server，这里的实现方式为使用libevent库和libcurl协作完成，其中Libevent为一个事件驱动的底层网络库，其主要逻辑为：
<img src="fig_for_doc/main.png" width = "400" height = "200"/>
libevent内部有一个事件监听循环，这里记为event loop，该循环会不断监听绑定的端口发送来的请求，如果收到请求，则调用请求相对的回调函数do_request_cb，这个回调函数主要功能如下：
<img src="fig_for_doc/do_request_cb.png" width = "200" height = "200"/>
do_request_cb最后一步为初始化发送数据的event，该event触发时会调用回调函数send_file_cb，该函数主要功能为：
<img src="fig_for_doc/send_file_cb.png" width = "300" height = "200"/>
综上，整体的运行流程是：<br>
1. 事件监听循环event_loop监听到请求，调用回调函数do_request_cb<br>
2. 回调函数do_request_cb执行，获取到节点并分别对节点测速，将信息保存在send_file_ctx结构体中，并将发送数据的event加入到事件监听循环中，触发事件的条件是隔1s触发<br>
3. 1s后，事件监听循环触发发送数据的event，调用事件相关的回调函数send_file_cb，此函数根据send_file_ctx中的信息判断现在发送到哪个数据chunk，并从线程池中找正在下载该chunk的线程，如果该chunk至少有一个thread下载完了，则发送该chunk，并将这个event再次加入事件监听循环<br>
4. 事件监听循环event_loop触发event，调用事件相关的回调函数send_file_cb，根据send_file_ctx中的信息发现已经发送完所有的chunk，则释放资源，结束请求<br>

# 编译方式及运行
## 编译方式
make clean<br>
make
## 使用方式
./server ~/test
第一个参数设置为服务器的根目录文件夹，端口号为60000，之后任意访问服务器下的某个文件，就能输出结果。

