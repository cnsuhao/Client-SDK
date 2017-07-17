# 编译方式
make clean<br>
make
# 使用方式
./server ~/test
第一个参数设置为服务器的根目录文件夹，端口号为60000，之后任意访问服务器下的某个文件，就能输出结果。假设test文件夹下有1.txt，则在浏览器中输入http://localhost:60000/1.txt之后就会调用get_node_cb函数，在终端中输出node节点

# 代码架构
主要分为三个文件，get_file.c，send_file.c，server.c，其中server.c文件中只有主函数，send_file.c文件中主要是使用libevent响应对本地文件的请求，get_file.c文件中主要是使用libcurl访问webrtc服务器获取token，节点和文件的函数。<br>
