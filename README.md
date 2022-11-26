# Note

> 环境基于VS 2022

一个基于UDP的RDT实现，主要用于udp打洞进行内网穿透后的文件收发，参考了**Quic**协议的设计，但只做到了流量控制，未做到拥塞控制。

Quic：[RFC 9000 - QUIC: A UDP-Based Multiplexed and Secure Transport (ietf.org)](https://datatracker.ietf.org/doc/html/rfc9000)

实现的功能如下：

* 重新设计的报文头部；
* 两次握手（only 1 RTT）；
* 基于ID标识（参考Quic协议）；
* 乱序确认，number递增（参考Quic协议）；
* 超时重传，基于number采样rtt（参考Quic协议）；
* 使用offset标识流而非number（参考Quic协议）；
* 基于maxoffset进行流量控制（参考Quic协议）；
* 基于心跳检测确认双方存活与退出。

-----

# 快速使用

**library/rdt_on_udp**文件夹中有头文件和源文件，**不需要编译，直接在项目中include即可使用**。

* API只需要查看Sender和Receiver两个类的实作。
* 提供了两种使用方式
  * 不提供socket，这可以在构造函数中直接创建一个非阻塞的udp socket，发送方需要指定目标ip和端口，接收方需要指定端口；
  * 提供socket，会设置成非阻塞，这是用于udp打洞后继续利用该socket通信。
* 两种模式
  * 单步发送和单步接收，调用send和recv函数即可；
  * 文件收发，调用sendFile和recvFile函数即可。
  * 注意在一次通信后该实例就无法继续使用，没有重置接口，需要创建新实例。

# 测试样例

* 在**test**文件夹中有example，有详细的测试介绍和代码，在不同内网的P2P通信可以达到2MB/s的速度，如果`UDPSEGSIZE`设置得大即一次发送的报文很大，可以达到10MB/s，但在高丢包率情况下会有意外情况。如果在相同内网中可以设置大一些。

# 设计过程

因为平时比较忙，项目设计时分了好几段时间来做，所以有些地方思路会断开，代码也会有不足的地方。可以在GitHub上issue或联系邮箱`chen_jy@sjtu.edu.cn`。

整个设计过程可以看我的博客：[RDT base on UDP | JySama](https://jysama.cn/2022/11/26/RDT_base_on_UDP/)

# 其他

在library文件夹中还有一个udp_hole_punch文件夹，该文件夹是用于udp打洞实现内网穿透的客户端源码。

项目在：[Chen-Jin-yuan/UDP-hole-punching (github.com)](https://github.com/Chen-Jin-yuan/UDP-hole-punching)可以找到；

设计过程可以看我的博客：[UDP hole punching | JySama](https://jysama.cn/2022/11/26/udp_hole_punching/)
