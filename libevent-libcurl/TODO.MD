## 未能回收非正常结束的请求的资源
### 触发原因
请求被中断的时候，ffplay或浏览器会发送rst包，如何在这个rst包出现的时候终止请求呢？
### 解决方案
struct evhttp_connection* evcon = evhttp_request_get_connection(sfinfo->req);<br>
evhttp_connection_set_closecb(evcon, close_connection_cb, sfinfo);<br>
设置连接关闭时的函数
## 滑动窗口时会有线程不安全的问题
### 触发原因
产生线程不安全的缘故在于，现在的线程池里每个线程对应一个线程下载信息file_transfer_session_info，这个结构体中有个缓存evbuffer，此buffer是该线程下载文件时存储数据的地方，假设线程池满了，新的线程会从线程池的头部重新使用线程id，此时若线程池的开头的线程还没结束，就会造成新的线程会和原本的线程写同一块的evbuffer，因此出现线程不安全。
### 解决方案 
此处有两个选择，一是线程的读写锁，二是设置超时时间让以前的线程直接结束，这里选择第二种，因为第一种情况会造成后续文件下载变慢。假设每次窗口滑动会产生n个新线程，线程池大小m*n个，窗口滑动周期为s秒，只要我们保证每个线程的超时时间少于m*s就可以保证evbuffer不会被多个线程重用。
## Assertion buf->last_with_datap == &buf->first failed in evbuffer_chain_insert的bug
### 触发原因
请求结束后，如果线程还没结束，在释放evbuffer之后，会发生向已经释放的evbuffer中写数据的情况，因此出现bug
### 解决方案 
请求结束之前，先等待所有的线程结束
## 资源重复下载
### 触发原因
窗口滑动之后会重复下载
### 解决方案 
排除重复下载的部分，窗口滑动之后只下载没下载的部分
## 如果到达超时时间，应该发送bug
### 触发原因
达到超时时间，说明视频中有一部分无法下载，视频有错，不能播放
### 解决方案 
判断是否超时，超时则直接结束请求，发送错误
## 二次播放会出现Segmentation fault
### 触发原因
检查是否下载完成的函数check_download中下标越界
### 解决方案 
使用core dump排查，参考资料http://www.cnblogs.com/Anker/p/6079580.html<br>
开启core dump功能之后，如果出bug，会生成一个core文件，使用gdb ./program ./core就可以看core文件，此时输入bt命令，就可以获得出现bug的语句的调用的trace，根据这个trace就可以知道那个函数的什么地方出现bug，据此可以猜测到为什么出现bug 
