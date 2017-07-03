## 基于HTTP/HTTPS多源P2P流媒体播放器

![client architecture](fig/client_architecture.png)
![server monitor](fig/firstaid.png)

```
r=`curl  -X POST https://api.webrtc.win:6601/v1/customer/login \
  -H "Content-Type:application/json" \
  -d '{
    "user": "admin",
    "password":  "123456"
   }'`
```

## request nodes who cache this file
```
r=`curl -X GET "https://api.webrtc.win:6601/v1/customer/nodes?client_ip=127.0.0.1&host=qq.webrtc.win&uri=${file}&md5=ab340d4befcf324a0a1466c166c10d1d" \
  -H "X-Pear-Token: ${token}" \
  -H "Content-Type:application/json"`
```
## return node list
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
