## 基于HTTP/HTTPS多源P2P流媒体播放器

客户端整体架构及流程如下：通过一个本地的HTTP Server(127.0.0.1),所以播放器端所有的数据请求都经过本地，客制化的HTTP Server去后台请求数据数据返回给播放器，最大程度的解耦各个模块。
![client architecture](fig/client_architecture.png)


## API

1. 登录返回Token,请求节点的时候需要此Token做为安全验证。

```
r=`curl  -X POST https://api.webrtc.win:6601/v1/customer/login \
  -H "Content-Type:application/json" \
  -d '{
    "user": "admin",
    "password":  "123456"
   }'`
```

2. 请求节点列表，后台服务器会返回N个有效节点

请求节点：

```
r=`curl -X GET "https://api.webrtc.win:6601/v1/customer/nodes?client_ip=127.0.0.1&host=qq.webrtc.win&uri=${file}&md5=ab340d4befcf324a0a1466c166c10d1d" \
  -H "X-Pear-Token: ${token}" \
  -H "Content-Type:application/json"`
```

返回节点：

```
{
  "nodes": [
    {
      "protocol": "https",
      "host": "000c29d049f4.webrtc.win:65526",
      "type": "node"
    },
    {
      "protocol": "https",
      "host": "2076933cdd51.webrtc.win:34038",
      "type": "node"
    },
    {
      "protocol": "https",
      "host": "5254005d31ff.webrtc.win:4582",
      "type": "server"
    }
  ]
}
```
