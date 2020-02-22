## 一、原理与基础

- 图解SSL/TLS协议（阮一峰）

    http://www.ruanyifeng.com/blog/2014/09/illustration-ssl.html

- SSL/TLS协议运行机制的概述（阮一峰）

    http://www.ruanyifeng.com/blog/2014/02/ssl_tls.html

- 数字签名是什么？（阮一峰）

    http://www.ruanyifeng.com/blog/2011/08/what_is_a_digital_signature.html

- TLS 1.3 详解（RFC 8446解读）

    TLS 1.3 对 TLS 1.2 的重大优化，如果追求性能，可阅读之。

    - 简化了握手过程（1-RTT）
    - 0-RTT恢复（连接缓存复用）
    - 除了hello消息外，其余消息全部加密
    - 删除了不安全的算法

    https://razeencheng.com/post/detail-of-tls13

## 二、工具

https://github.com/cloudflare/cfssl/releases

## 三、工具的使用

### 1、一般使用过程

- 生成 `CA` 自签名证书
    - 生成私钥
    - 生成证书文件（包含公钥）
- 生成 `Server` 服务器证书
    - 生成私钥
    - 生成证书文件（包含公钥）
- 使用 `CA` 的私钥和证书签署 `Server`
- 把 `CA` 证书导入系统
    - 在 win/mac 上部部署把证书后缀改为 .crt 可图形化安装
- 服务器部署时使用 `Server` 的私钥和证书

### 2、需要的工具

- **cfssl**

    用于生成私钥与证书至标准输出

- **cfssljson**

    用于把 `cfssl` 的输出写入硬盘

- **快速结束**

    ```
    bash mk.sh
    ```

    **证书和私钥文件的名字不影响使用可任意更改**

### 3、创建 CA证书

```
cfssl genkey -loglevel 1 -initca ca.json | cfssljson -bare _ca
```

查看证书信息

```
openssl.exe x509 -in _ca.pem -text
```

ca.json 参数解释

```
{
    "key": {
        // 秘钥的加密算法 rsa
        "algo": "rsa",
        "size": 2048 | 4096
        // 如果使用 ecdsa
        // "algo": "ecdsa"
        // "size": 256 | 384 | 521
    },
    "names": [
        {
            // 组织名称
            "O":  "yangqs"
        }
    ],
    "ca": {
        // 证书过期时间，只支持h
        "expiry": "86400h"
    }
}
```

### 4、创建 Server证书并签名

```
cfssl gencert -loglevel 1 -ca _ca.pem -ca-key _ca-key.pem server.json | cfssljson -bare _server
```

server.json 参数解释

```
{
    // 公共名字，旧版本的 X509 用于验证域名
    // 但在 v3 版本中已经不使用该字段
    "CN": "server",
    // 证书绑定的域名
    "hosts": [
        "example.com",
        "www.example.com",
        "https://www.example.com",
        "*.example.com",
        "127.0.0.1",
        "localhost"
    ],
    "key": {
        "algo": "rsa",
        "size": 2048
    },
    "names": [
        {
            "O":  "yangqs"
        }
    ],
    "ca": {
        "expiry": "8640h"
    }
}
```

## 四、golang HTTP2 实际验证 :)

- 编译并运行
```
package main

import (
	"fmt"
	"log"
	"net/http"

	"golang.org/x/net/http2"
)

func main() {
	// 绑定路由
	mux := http.NewServeMux()
	mux.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
		fmt.Println(r.RemoteAddr)
		w.Write([]byte("<p>hello http2</p>"))
	})

	// 创建服务对象
	srv := http.Server{
		Addr:    `:8443`,
		Handler: mux,
	}

	// 绑定 HTTP2 帧处理
	http2.ConfigureServer(&srv, &http2.Server{})

	// 服务器证书与私钥文件
	tlsServer := `E:/NewWorks2/yangqs.tools/cfssl/_server.pem`
	tlsServerKey := `E:/NewWorks2/yangqs.tools/cfssl/_server-key.pem`

	// 启动服务
	log.Fatal(srv.ListenAndServeTLS(tlsServer, tlsServerKey))
}


- 导入 `CA证书` 至系统
- 浏览器访问 https://localhost:8443 或 https://127.0.0.1:8443 
- 浏览器调试可观察到`证书有效`、传输协议为 `"h2"` 页面数据为 `"hello http2"`
