# 整体结构
根据Client-SDK/README.md可知，整体架构为
![client architecture](../fig/client_architecture.png)
对于local http server，这里的实现方式为使用libevent库和libcurl协作完成，其中Libevent为一个事件驱动的底层网络库，主要作用是实现http server的对端口的监听，libcurl是一个网络传输协议库，其主要功能是传输数据。整个程序的主要逻辑为：
<img src="fig_for_doc/main.png" width = "500" height = "250"/>
libevent内部有一个事件监听循环，这里记为event loop，该循环会不断监听绑定的端口发送来的请求，如果收到请求，则调用请求相对的回调函数do_request_cb，这个回调函数主要功能如下：
<img src="fig_for_doc/do_request_cb.png" width = "300" height = "300"/>
do_request_cb最后一步为初始化发送数据的event，该event触发时会调用回调函数send_file_cb，该函数主要功能为：
<img src="fig_for_doc/send_file_cb.png" width = "500" height = "300"/>
综上，整体的运行流程是：<br>
* https://github.com/libevent/libevent/issues/536
1. 事件监听循环event_loop监听到请求，调用回调函数do_request_cb<br>
2. 回调函数do_request_cb执行，获取到节点并分别对节点测速，将信息保存在send_file_ctx结构体中，并将发送数据的event加入到事件监听循环中，触发事件的条件是隔1s触发<br>
3. 1s后，事件监听循环触发发送数据的event，调用事件相关的回调函数send_file_cb，此函数根据send_file_ctx中的信息判断现在发送到哪个数据chunk，并从线程池中找正在下载该chunk的线程，如果该chunk至少有一个thread下载完了，则发送该chunk，并将这个event再次加入事件监听循环<br>
4. 事件监听循环event_loop触发event，调用事件相关的回调函数send_file_cb，根据send_file_ctx中的信息发现已经发送完所有的chunk，则释放资源，结束请求<br>

# 节点获取及文件下载
## 节点获取
为了实现多源下载的目标，首先要从服务器获取token，然后使用token获得多个节点，获得多个节点后还需要对节点测速排序等，之后才能开始真正的下载，这一步在do_request_cb的【下载文件的准备】这一步中完成，这一步的接口函数为preparation_process，其流程为：
<img src="fig_for_doc/preparation_process.png" width = "500" height = "300"/>
上图中的功能对应的函数分别为：
1. 登录服务器返回token: login<br>
2. 获取node信息: get_node<br>
3. 解析节点数据: node_info_init<br>
4. 判断节点是否可用，测速和排序: get_node_alive<br>

## 文件下载
根据send_file_cb的流程图，每次滑动窗口时调用函数window_download，此函数都会开启新的线程，开启的线程数量根据预先配置的chunk大小和window大小决定，若chunk为1MB，window为10MB，则每次开启10个线程。下载的具体策略请见Client-SDK/README.md。<br>
直接下载文件的函数为get_file_range，此函数使用libcurl，支持http byte-range请求。

## 窗口滑动及下载流程
现在以图示表现下载过程，上方为要下载的文件，假设文件被划分为数个chunk，每个chunk的大小为chunk_size，每个chunk均有其编号。下方为滑动窗口的缓冲区，假设我们最多允许THREAD_NUM_MAX个线程同时下载，每次窗口滑动时都会新产生chk_in_win_ct个新的线程。此示例中，假设文件大小13.5MB，chunk_size为1MB，则一共划分为14个chunk，设置THREAD_NUM_MAX为9，chk_in_win_ct为3，滑动窗口间隔为win_slide秒，每个线程对应的chunk号如图所示。
<img src="fig_for_doc/0.png" width = "1000" height = "250"/>
一开始下载时，没有线程在下载数据，此时线程对应的chunk号均为-1。
<img src="fig_for_doc/1.png" width = "1000" height = "250"/>
do_request_cb执行完之后，触发第一次的send_file_cb，此时要发送的chunk编号为0，但是并没有任何一个线程正在下载chunk 0，因此开启chk_in_win_ct = 3个新线程，使其下载0,1,2号
<img src="fig_for_doc/2.png" width = "1000" height = "250"/>
经过win_slide秒之后，发现chunk 1, chunk 2下载完了，然而需要发送的chunk 0还没下载完，此时进行滑动窗口，新开启chk_in_win_ct个线程，优先下载没下载完的chunk 0，剩下2个线程顺序接着下载后面的数据，最终新开启的线程分别下载0,3,4号
<img src="fig_for_doc/3.png" width = "1000" height = "250"/>
经过win_slide秒之后，发现thread 0已经下载完了chunk 0，thread 4下载好了chunk 3，此时chunk 0-3都可以顺序发送，则发送数据，发送过的chunk以红色表示，此时进行滑动窗口，下一个需要发送的chunk 4还没下载完，优先下载没下载完的chunk 4，剩下2个线程顺序接着下载后面的数据，最终新开启的线程分别下载4,5,6号
<img src="fig_for_doc/4.png" width = "1000" height = "250"/>
经过win_slide秒之后，发现thread 5,6都下载好了chunk 4，取其中任意一个发送即可，此时进行滑动窗口，因为此时THREAD_NUM_MAX已经被占满了，因此重新复用前三块缓冲区，优先下载没下载完的chunk 5，最终新开启的线程分别下载5,7,8号。<br>
最后以此类推，最终下载并发送完全部数据即可。

# 编译方式及运行
## 编译方式
make clean<br>
make
## 使用方式
./server ~/test
第一个参数设置为服务器的根目录文件夹，端口号为60000，之后任意访问服务器下的某个文件，就能输出结果。

# 参考文献
libcurl: https://curl.haxx.se/libcurl/c/<br>
libevent: https://sourcecodebrowser.com/libevent/2.0.3-alpha/files.html<br>

