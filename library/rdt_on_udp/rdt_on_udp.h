#ifndef RDT_ON_UDP_H
#define RDT_ON_UDP_H

#include <iostream>
#include <math.h>

//std
#include <string>
#include <vector>
#include <list>

#include <bitset> //manager bit flags 
#include <chrono> //manager time
#include <random> //random value

//multithread
#include <mutex> 
#include <thread>

//function and STL algorithm
#include <algorithm>
#include <functional>

//socket
#include <WinSock2.h>
#include <WS2tcpip.h>//inet_pton£¬inet_ntop

//something else
#include <Windows.h>

//Using UDP requires disabling errors
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12) 

#define DETECTTIME 6000
#define HANDSHAKETIME 30000
#define UDPSEGSIZE 1300 //DATA SIZE
#pragma comment(lib,"ws2_32.lib") //link dll

using namespace std;

class MyUdpSeg
{
public:
    using num_type = unsigned int; //32bits
    using off_type = size_t; //64bits or 32bits
    using win_type = size_t; //64bits or 32bits
    using len_type = unsigned short; //16bits
    using id_type = unsigned short; //16bits
    using tag_type = bitset<8>; //8bits£¬?|?|?|?|ack|syn|fin|rst
    static int maxHeadSize;

private: //data members
    num_type number;
    off_type offset;
    len_type length;
    win_type window;
    id_type id;
    tag_type tag;
    //save...
    string data;

public:
    //Constructor 1: Construct class objects based on data
    MyUdpSeg(const char* buf, num_type Number, off_type Offset,
        len_type Length, tag_type Tag, win_type Window = 0, id_type Id = 0);

    //Constructor 2: Construct class objects based on the complete UDP packet segment(string)
    MyUdpSeg(string& udpSeg);

    //Constructor 3: Retransmit, adjust the number
    MyUdpSeg(MyUdpSeg& udpSeg, num_type Number);

    //Copy constructor 
    MyUdpSeg(const MyUdpSeg& udpSeg);
    //Move constructor 
    MyUdpSeg(MyUdpSeg&& udpSeg) noexcept;
    //operator=, copy-swap
    MyUdpSeg& operator=(MyUdpSeg udpSeg);

private:
    //parse the string
    vector<string> parse(string& str);
    //swap for copy-swap
    void swap(MyUdpSeg& udpSeg);

public:
    //convert to string to send data
    string seg_to_string();

    //return a default obj
    static MyUdpSeg initSeg();

    //read members only
    num_type getNumber() { return number; }
    off_type getOffset() { return offset; }
    len_type getLength() { return length; }
    win_type getWindow() { return window; }
    id_type getId() { return id; }
    bool getTag(size_t i) { return tag[i]; }
    string& getData() { return data; }
};

struct timerNode
{
    MyUdpSeg::num_type number; //segment number
    MyUdpSeg::off_type offset; //data offset
    chrono::system_clock::time_point time; //start time
    bool timeout; //timeout flag
    timerNode(MyUdpSeg::num_type Number, MyUdpSeg::off_type Offset) :
        number(Number), offset(Offset), time(chrono::system_clock::now()), timeout(false) {}
    timerNode(const timerNode& node) :
        number(node.number), offset(node.offset), time(node.time), timeout(false) {}
    bool operator==(MyUdpSeg::num_type Number) { return number == Number; } //for find() function or remove()
};

class TimerList
{
public:
    using node_type = timerNode;
    using rtt_type = unsigned int;
    using rto_type = double;
    using timerIter = list<node_type>::iterator;

private:
    list<node_type> timerList;

public:
    TimerList() {}
    ~TimerList() {}
    //insert by node
    void insertTimer(const node_type& node);
    //insert by number
    void insertTimer(MyUdpSeg::num_type Number, MyUdpSeg::off_type Offset);

    //delete by number, return RTT/ms
    rtt_type deleteTimer(MyUdpSeg::num_type Number);

    //deal with timeout packets, return numbers
    vector<MyUdpSeg::num_type> tick(rto_type RTO);

    //return size
    size_t size() { return timerList.size(); }

private:
    //delete all by offset, call by "rtt_type deleteTimer(MyUdpSeg::num_type Number);"
    void deleteTimer_(MyUdpSeg::off_type Offset);
};

//buffer node
struct bufferNode
{
    MyUdpSeg udpSeg; //segment object
    bool isSent;

    //Constructor 1: construct by lvalue
    bufferNode(MyUdpSeg& UdpSeg) :udpSeg(UdpSeg), isSent(false) {}
    //Constructor 2: construct by rvalue 
    bufferNode(MyUdpSeg&& UdpSeg) noexcept :udpSeg(UdpSeg), isSent(false) {}
    //Constructor 3: Retransmit, adjust the number
    bufferNode(MyUdpSeg& UdpSeg, MyUdpSeg::num_type Number) :udpSeg(UdpSeg, Number), isSent(false) {}

    //copy constructor
    bufferNode(const bufferNode& node) :udpSeg(node.udpSeg), isSent(node.isSent) {}

    bool operator==(MyUdpSeg::num_type Number) { return udpSeg.getNumber() == Number; } //for find() function
};

class BufferList
{
public:
    using node_type = bufferNode;
    using bufferIter = list<node_type>::iterator;
    using node_return_type = pair<node_type&, bool>;

private:
    list<node_type> bufferList;

public:
    BufferList() {}
    ~BufferList() {}

    //insert node
    void insertNode(MyUdpSeg& UdpSeg);
    void insertNode(MyUdpSeg&& UdpSeg);
    void insertNode(MyUdpSeg& UdpSeg, MyUdpSeg::num_type Number);
    void sortInsertNode(MyUdpSeg& UdpSeg); //insert sorted by offset(used by recvbuf)

    //delete by number
    void deleteNode(MyUdpSeg::num_type Number);

    //get node by number, return pair: reference¡¢bool (to check node)
    node_return_type getNode(MyUdpSeg::num_type Number);
    /*
    * Note for getNode function:
    * For each return value, initialize it with a new variable
    * Do not assign a value to pair again after initialization, it will act on the initialized node
    */

    //return size
    size_t size() { return bufferList.size(); }

    bufferIter begin() { return bufferList.begin(); }
    bufferIter end() { return bufferList.end(); }
    void deleteFront() { bufferList.erase(begin()); }
    //return a default node to ues getNode function
    static node_type initNode();

private:
    void deleteNode_(MyUdpSeg::off_type Offset);
};

class SendWindow
{
private:
    mutex detectThreadMux;
    mutex windowMux;
    mutex rtoMux;
    mutex bufferListMux;
    mutex timerListMux;
    thread t;

    MyUdpSeg::num_type number;
    MyUdpSeg::off_type offset;
    MyUdpSeg::win_type window; //The number of bytes that the window can send
    MyUdpSeg::id_type id;
    MyUdpSeg::id_type myid;
    TimerList::rto_type rto;
    BufferList bufferList;
    TimerList timerList;

    SOCKET udpSock;
    FILE* fp;

    const unsigned int SegDataSize;
    const MyUdpSeg::win_type maxWindow; //The max number of bytes that the window can send

    TimerList::rto_type srtt; //Smoothed
    TimerList::rto_type drtt; //Deviation


    struct sockaddr_in socketaddr;
    bool handShakeOver;
    bool sendOver;
    chrono::system_clock::time_point lastAckTime; //for heart beat detect
    const unsigned int handShakeTime;
    const unsigned int detectTime;
    unsigned int milliseconds; //heart beat


public:
    SendWindow(SOCKET UdpSock, const char* ip, const int port);
    SendWindow(SOCKET UdpSock, const char* fileName, const char* ip, const int port);
    ~SendWindow();

    bool loadData(const char* data, size_t size, bool goOn = false);
    bool loadFile();
    void sendSeg();
    void reLoad_timeout_Seg();
    void recvAck();
    void handShake(); //update id and window
    bool sendIsOver();

    //Detect if the other party is alive, set 'sendOver'
    void heartBeatDetectStart();
    MyUdpSeg::off_type writeBytes() { return offset; }

private:
    //Window is the number of bytes that the receiver can continue to receive at the moment, 
    //and is 'the maximum number of windows for the receiver' - 'the maximum offset received'
    //Refer to the QUIC protocol
    void updateWindow(MyUdpSeg::win_type Window);
};

class RecvWindow
{
private:
    mutex detectThreadMux;
    mutex windowOffsetMux;
    mutex bufferListMux;
    thread t;

    MyUdpSeg::off_type maxOffset; //max offset
    MyUdpSeg::win_type window; //max bytes
    const MyUdpSeg::win_type fixWindow;
    MyUdpSeg::id_type id;
    MyUdpSeg::id_type myid;

    MyUdpSeg::off_type curOffset; //current offset
    MyUdpSeg::off_type overOffset; //ending offset

    BufferList bufferList;

    SOCKET udpSock;
    FILE* fp;
    const unsigned int SegDataSize;
    struct sockaddr_in socketaddr;
    bool peerNotExist; //for heart beat detect
    chrono::system_clock::time_point lastRecvTime; //for heart beat detect
    const unsigned int handShakeTime;
    const unsigned int detectTime;
    unsigned int milliseconds; //for heart beat


public:
    //window is byte stream, default 100KiB
    RecvWindow(SOCKET UdpSock, MyUdpSeg::win_type Window = 1024000);
    RecvWindow(SOCKET UdpSock, const char* fileName, MyUdpSeg::win_type Window = 1024000);
    ~RecvWindow();

    //window is the number of bytes that the receiver can continue to receive at the moment, 
    //and is 'the maximum number of windows for the receiver' - 'the maximum offset received'
    //Refer to the QUIC protocol

    string getData();
    size_t writeFile();
    void recvSeg();

    bool recvIsOver();
    void heartBeatDetectStart();

private:
    void handShakeReturn(); //return id and window, update socketaddr
    void updateWindow();
    void sendAck(MyUdpSeg::num_type Number);
};

class Sender
{
private:
    SOCKET udpSock;
    SendWindow sendWindow;
    const bool defaultSock;
    bool sendIsOver;
    string fileName;
    thread t;

public:
    Sender(SOCKET UdpSock, const char* ip, const int port);
    Sender(SOCKET UdpSock, const char* fileName, const char* ip, const int port);
    Sender(const char* ip, const int port);
    Sender(const char* fileName, const char* ip, const int port);
    ~Sender();

    void start();
    void sendFile();
    void send(const char* data, size_t size);

private:
    //Set non-blocking
    void setNonBlocking(SOCKET sockfd);

};

class Receiver
{
private:
    SOCKET udpSock;
    RecvWindow recvWindow;
    const bool defaultSock;
    bool recvIsOver;
    string fileName;
    thread t;

public:

    Receiver(SOCKET UdpSock, MyUdpSeg::win_type Window = 102400);
    Receiver(SOCKET UdpSock, const char* fileName, MyUdpSeg::win_type Window = 102400);
    Receiver(const int myPort, MyUdpSeg::win_type Window = 102400);
    Receiver(const int myPort, const char* fileName, MyUdpSeg::win_type Window = 102400);
    ~Receiver();

    void start();
    void recvFile();
    string recv();

private:
    //Set non-blocking
    void setNonBlocking(SOCKET sockfd);
};

#endif
