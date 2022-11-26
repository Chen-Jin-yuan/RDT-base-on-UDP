#include "rdt_on_udp.h"

//----------------------------------------MyUdpSeg----------------------------------------//

/*
* The headers are converted to a string to send
* The maximum number of digits in 32-bit decimal is 10 digits: 4,294,967,296
* The maximum number of digits in 64-bit decimal is 20 digits: 18,446,744,073,709,551,616
* The maximum number of digits in 16-bit decimal is 5  digits: 65,536
* There are 8 bits of identification
* There are also 6 spaces
*/
int MyUdpSeg::maxHeadSize = 10 + 20 + 20 + 5 + 5 + 8 + 6;

//Constructor 1: Construct class objects based on data
MyUdpSeg::MyUdpSeg(const char* buf, num_type Number, off_type Offset,
    len_type Length, tag_type Tag, win_type Window, id_type Id) :
    data(buf), number(Number), offset(Offset), window(Window),
    length(Length), id(Id), tag(Tag)
{
    string str;
    //char* to string, prevents truncation by '0'
    for (size_t i = 0; i < Length; i++)
        str += buf[i];
    data = str;
}

//Constructor 2: Construct class objects based on the complete UDP packet segment(string)
MyUdpSeg::MyUdpSeg(string& udpSeg)
{
    vector<string> vec = parse(udpSeg);
    // error packet segment
    if (vec.size() < 6)
    {
        length = 0; //Indicates that this is a useless package
        return;
    }
    /*
    * have not string to unsigned int or size_t function
    * but string to unsigned long is satisfiable
    * just make sure it doesn't overflow
    * use forced transformation to ignore warnings
    */
    number = num_type(stoul(vec[0]));
    offset = off_type(stoul(vec[1]));
    window = win_type(stoul(vec[2]));
    length = len_type(stoul(vec[3]));
    id = id_type(stoul(vec[4]));
    tag = tag_type(vec[5]);
    if (vec.size() > 6) //if carry data. ACK may not carry data
        data = vec[6];
}

//Constructor 3: Retransmit, adjust the number
MyUdpSeg::MyUdpSeg(MyUdpSeg& udpSeg, num_type Number) :
    data(udpSeg.data), number(Number), offset(udpSeg.offset), window(udpSeg.window),
    length(udpSeg.length), id(udpSeg.id), tag(udpSeg.tag)
{}

//Copy constructor 
MyUdpSeg::MyUdpSeg(const MyUdpSeg& udpSeg) :
    data(udpSeg.data), number(udpSeg.number), offset(udpSeg.offset), window(udpSeg.window),
    length(udpSeg.length), id(udpSeg.id), tag(udpSeg.tag)
{}

//Move constructor 
MyUdpSeg::MyUdpSeg(MyUdpSeg&& udpSeg) noexcept :
    data(udpSeg.data), number(udpSeg.number), offset(udpSeg.offset), window(udpSeg.window),
    length(udpSeg.length), id(udpSeg.id), tag(udpSeg.tag)
{}

//operator=, copy-swap
MyUdpSeg& MyUdpSeg::operator=(MyUdpSeg udpSeg)
{
    swap(udpSeg);
    return *this;
}
//swap for copy-swap
void MyUdpSeg::swap(MyUdpSeg& udpSeg)
{
    using std::swap;
    swap(this->number, udpSeg.number);
    swap(this->offset, udpSeg.offset);
    swap(this->length, udpSeg.length);
    swap(this->window, udpSeg.window);
    swap(this->id, udpSeg.id);
    swap(this->tag, udpSeg.tag);
    swap(this->data, udpSeg.data);
}

//parse the string
vector<string> MyUdpSeg::parse(string& str)
{
    //Note that there are also spaces in the data, so parse up to six times
    //Data should not be sliced

    int spaceNum = 6;

    vector<string> res;
    size_t pos = 0;
    size_t pos1;
    while ((pos1 = str.find(' ', pos)) != string::npos)
    {
        if (spaceNum-- == 0)
            break;

        res.push_back(str.substr(pos, pos1 - pos));
        //one space
        pos1++;
        pos = pos1;
    }
    //Get complete data
    string data = str.substr(pos);
    if (data != "")
        res.push_back(data);

    return res; //move construction
}

//convert to string to send data
string MyUdpSeg::seg_to_string()
{
    string res;
    res += to_string(number) + " ";
    res += to_string(offset) + " ";
    res += to_string(window) + " ";
    res += to_string(length) + " ";
    res += to_string(id) + " ";
    res += tag.to_string() + " "; //std::bitset::to_string()
    res += data;
    return res;
}

MyUdpSeg MyUdpSeg::initSeg()
{
    return MyUdpSeg("", 0, 0, 0, MyUdpSeg::tag_type(), 0, 0);
}


//----------------------------------------TimerList----------------------------------------//

void TimerList::insertTimer(const node_type& node)
{
    timerList.push_back(node); // call move or copy constructor function, not need to construct
}

void TimerList::insertTimer(MyUdpSeg::num_type Number, MyUdpSeg::off_type Offset)
{
    timerList.emplace_back(Number, Offset); //call constructor function
}

TimerList::rtt_type TimerList::deleteTimer(MyUdpSeg::num_type Number)
{
    //find the node
    timerIter iter = find(timerList.begin(), timerList.end(), Number); //O(n)
    if (iter == timerList.end()) //not found
        return 0; //zero used to error detect

    //sample RTT
    chrono::system_clock::time_point nowtime = chrono::system_clock::now();
    rtt_type RTT = rtt_type(chrono::duration_cast<chrono::milliseconds>(nowtime - (*iter).time).count());

    //get offset
    MyUdpSeg::off_type offset = (*iter).offset;

    //delete
    timerList.erase(iter); //O(1)

    //delete node with the same offset
    deleteTimer_(offset);

    return RTT;
}

void TimerList::deleteTimer_(MyUdpSeg::off_type Offset)
{
    for (timerIter iter = timerList.begin(); iter != timerList.end();)
    {
        if ((*iter).offset == Offset)
            iter = timerList.erase(iter); //erase return next iterator
        else
            iter++;
    }
}

vector<MyUdpSeg::num_type> TimerList::tick(rto_type RTO)
{
    if (RTO <= 0) return{};

    vector<MyUdpSeg::num_type> number_retrans;
    for (timerIter iter = timerList.begin(); iter != timerList.end(); iter++)
    {
        if ((*iter).timeout == true)
            continue;
        else
        {
            chrono::system_clock::time_point nowtime = chrono::system_clock::now();
            rtt_type interval = rtt_type(chrono::duration_cast<chrono::milliseconds>(nowtime - (*iter).time).count());
            if (interval > RTO) //need to retransmit
            {
                number_retrans.push_back((*iter).number);
                (*iter).timeout = true;
            }
        }
    }
    return number_retrans;
}


//----------------------------------------BufferList----------------------------------------//

void BufferList::insertNode(MyUdpSeg& UdpSeg)
{
    bufferList.emplace_back(UdpSeg);
}

void BufferList::insertNode(MyUdpSeg&& UdpSeg)
{
    bufferList.emplace_back(move(UdpSeg));
}

void BufferList::insertNode(MyUdpSeg& UdpSeg, MyUdpSeg::num_type Number)
{
    bufferList.emplace_back(UdpSeg, Number);
}

void BufferList::sortInsertNode(MyUdpSeg& UdpSeg)
{
    MyUdpSeg::off_type offset = UdpSeg.getOffset();
    bufferIter iter = bufferList.begin();
    for (; iter != bufferList.end(); iter++)
    {
        if ((*iter).udpSeg.getOffset() < offset)
            continue;
        else if ((*iter).udpSeg.getOffset() == offset) //A package with the same data
            return;
        else
            break;
    }
    bufferList.insert(iter, UdpSeg);

}

void BufferList::deleteNode(MyUdpSeg::num_type Number)
{
    //find the node
    bufferIter iter = find(bufferList.begin(), bufferList.end(), Number); //O(n)
    if (iter == bufferList.end()) //not found
    {
        //cout << "no delete" << endl;
        return; //do nothing
    }


    //get offset
    MyUdpSeg::off_type offset = (*iter).udpSeg.getOffset();

    //remove
    bufferList.erase(iter); //O(1)

    //remove all nodes with the same offset
    deleteNode_(offset);

}

void BufferList::deleteNode_(MyUdpSeg::off_type Offset)
{
    bufferList.remove_if(bind([](bufferNode& bufNode, MyUdpSeg::off_type offset) {
        return bufNode.udpSeg.getOffset() == offset; },
        placeholders::_1, Offset));

}

BufferList::node_return_type BufferList::getNode(MyUdpSeg::num_type Number)
{

    //find the node
    bufferIter iter = find(bufferList.begin(), bufferList.end(), Number); //O(n)
    if (iter == bufferList.end()) //not found
    {
        node_type node = initNode(); //must check bool value firstly, node may be inexistent 
        return node_return_type(node, false);
    }

    return node_return_type(*iter, true);
}

BufferList::node_type BufferList::initNode()
{
    return node_type(MyUdpSeg::initSeg());
}


//----------------------------------------SendWindow----------------------------------------//

SendWindow::SendWindow(SOCKET UdpSock, const char* ip, const int port) : bufferList(), timerList(),
udpSock(UdpSock), fp(NULL), SegDataSize(UDPSEGSIZE), maxWindow(102400), handShakeTime(HANDSHAKETIME), detectTime(DETECTTIME)
{
    number = 0;
    offset = 0;
    window = UDPSEGSIZE;
    id = 0;
    rto = 200;
    srtt = 0;
    drtt = 0;
    socketaddr.sin_family = AF_INET;//ipv4
    socketaddr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &socketaddr.sin_addr);

    //random choose myid
    random_device seed;//Hardware random number seed
    ranlux48 engine(seed());//Random number engine
    MyUdpSeg::id_type max = MyUdpSeg::id_type(pow(2, (sizeof(MyUdpSeg::id_type) * 8)) - 1);
    uniform_int_distribution<int> distrib(1, max);//uniform distribution

    myid = distrib(engine); //return random value

    handShakeOver = false;
    sendOver = false;
    lastAckTime = chrono::system_clock::now();
    milliseconds = handShakeTime; //30s for hand shake
}
SendWindow::SendWindow(SOCKET UdpSock, const char* fileName, const char* ip, const int port) : bufferList(), timerList(),
udpSock(UdpSock), SegDataSize(UDPSEGSIZE), maxWindow(102400), handShakeTime(HANDSHAKETIME), detectTime(DETECTTIME)
{
    if (fopen_s(&fp, fileName, "rb") != 0)
    {
        printf("[SendWindow::SendWindow] open file failed!\n");
        closesocket(udpSock);
        WSACleanup();
        exit(1);
    }

    number = 0;
    offset = 0;
    window = UDPSEGSIZE;
    id = 0;
    rto = 200;
    srtt = 0;
    drtt = 0;

    socketaddr.sin_family = AF_INET;//ipv4
    socketaddr.sin_port = htons(port);//×Ö½ÚÐò×ª»»
    inet_pton(AF_INET, ip, &socketaddr.sin_addr);

    //random choose myid
    random_device seed;//Hardware random number seed
    ranlux48 engine(seed());//Random number engine
    MyUdpSeg::id_type max = MyUdpSeg::id_type(pow(2, (sizeof(MyUdpSeg::id_type) * 8)) - 1);
    uniform_int_distribution<int> distrib(1, max);//uniform distribution

    myid = distrib(engine); //return random value

    handShakeOver = false;
    sendOver = false;
    lastAckTime = chrono::system_clock::now();
    milliseconds = handShakeTime; //30s for hand shake
}
SendWindow::~SendWindow()
{
    if (fp)
        fclose(fp);

    if (t.joinable())
        t.join();
}

bool SendWindow::sendIsOver()
{
    lock_guard<mutex> locker(detectThreadMux);
    return sendOver;
}

bool SendWindow::loadData(const char* data, size_t size, bool goOn)
{

    size_t nSeg = 0;
    if (size % SegDataSize == 0)
        nSeg = size / SegDataSize;
    else
        nSeg = size / SegDataSize + 1;
    {
        lock_guard<mutex> locker(windowMux);
        lock_guard<mutex> locker1(bufferListMux);
        if (nSeg + bufferList.size() > window)
            return false;
    }

    if (nSeg == 1)
    {
        number++;
        offset += size;
        MyUdpSeg::tag_type tag;
        if (!goOn)
        {
            tag.set(1, 1); //send over, set fin = 1
        }
        MyUdpSeg seg(data, number, offset, MyUdpSeg::len_type(size), tag, 0, id);
        lock_guard<mutex> locker(bufferListMux);
        bufferList.insertNode(seg);
    }
    else
    {
        char* buf = new char[SegDataSize + 1];
        for (int i = 0; i < nSeg - 1; i++)
        {
            lock_guard<mutex> locker(bufferListMux);
            strncpy_s(buf, SegDataSize + 1, data + i * SegDataSize, SegDataSize);
            buf[SegDataSize] = '\0';
            number++;
            offset += SegDataSize;
            MyUdpSeg::tag_type tag;
            MyUdpSeg seg(buf, number, offset, MyUdpSeg::len_type(SegDataSize), tag, 0, id);
            bufferList.insertNode(seg);
        }

        //last one
        size_t haveLoadSize = (nSeg - 1) * SegDataSize;
        number++;
        offset += size - haveLoadSize;
        memset(buf, 0, SegDataSize);
        strncpy_s(buf, SegDataSize + 1, data + haveLoadSize, size - haveLoadSize);
        buf[size - haveLoadSize] = '\0';
        MyUdpSeg::tag_type tag;
        if (!goOn)
        {
            tag.set(1, 1); //send over, set fin = 1
        }

        MyUdpSeg seg(buf, number, offset, MyUdpSeg::len_type(size - haveLoadSize), tag, 0, id);
        lock_guard<mutex> locker(bufferListMux);
        bufferList.insertNode(seg);

        delete[] buf;
    }
    return true;
}

bool SendWindow::loadFile()
{
    char* loadBuf = new char[SegDataSize + 1];
    //It cannot be judged by offset<window
    //Otherwise, if it retransmit too much, the window will be too bloated
    unique_lock<mutex> locker(windowMux);
    size_t bufListSize;
    {
        lock_guard<mutex> locker(bufferListMux);
        bufListSize = bufferList.size();
    }
    while (bufListSize * SegDataSize < window)
    {
        locker.unlock();
        size_t nRead = fread(loadBuf, 1, SegDataSize, fp);
        if (ferror(fp) != 0)
        {
            printf("[SendWindow::loadFile] failed to read file\n");
            closesocket(udpSock);
            WSACleanup();
            exit(1);
        }

        loadBuf[nRead] = '\0';
        if (feof(fp))//end
        {
            loadData(loadBuf, nRead);

            printf("success to read file\n");
            delete[] loadBuf;
            return true;
        }
        else
        {
            loadData(loadBuf, nRead, true);
        }
        locker.lock();
        bufListSize = bufferList.size();
    }
    locker.unlock();

    delete[] loadBuf;
    return false;
}

void SendWindow::sendSeg()
{
    lock_guard<mutex> locker(bufferListMux);
    for (BufferList::bufferIter iter = bufferList.begin(); iter != bufferList.end(); iter++)
    {
        if ((*iter).isSent)
            continue;
        else
        {
            string data = (*iter).udpSeg.seg_to_string();

            int nSend = sendto(udpSock, data.c_str(), int(data.size()), 0, (struct sockaddr*)&socketaddr, sizeof(socketaddr));

            if (nSend == SOCKET_ERROR) //error
            {
                int error = WSAGetLastError();
                if (error == WSAEWOULDBLOCK) //nothing to do, have not data
                {
                    continue;
                }
                printf("[SendWindow::sendSeg] send data error %d!\n", error);
                closesocket(udpSock);
                WSACleanup();
                exit(1);
            }
            (*iter).isSent = true;
            lock_guard<mutex> locker1(timerListMux);
            timerList.insertTimer((*iter).udpSeg.getNumber(), (*iter).udpSeg.getOffset());

            //printf("num:%d, offset:%zd\n", (*iter).udpSeg.getNumber(),
             //   (*iter).udpSeg.getOffset()); //debug
            //Sleep(1);
        }
    }
}

void SendWindow::reLoad_timeout_Seg()
{

    vector<MyUdpSeg::num_type> tickNumber;
    {
        lock_guard<mutex> locker(rtoMux);
        lock_guard<mutex> locker1(timerListMux);
        //100 is some processing time because it is performed serially
        tickNumber = timerList.tick(rto + 100); 
    }

    for (int i = 0; i < tickNumber.size(); i++)
    {
        lock_guard<mutex> locker(bufferListMux);
        BufferList::node_return_type pair = bufferList.getNode(tickNumber[i]);
        if (!pair.second) //don't exist
            continue;
        else
        {
            BufferList::node_type& node = pair.first;
            number++;
            bufferList.insertNode(node.udpSeg, number);
            //printf("------reload number%zd, offset%zd, window:%zd------\n", number, node.udpSeg.getOffset(), window);
        }
    }
}

void SendWindow::recvAck()
{
    char* recvbuf = new char[SegDataSize + MyUdpSeg::maxHeadSize + 1];
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    memset(&addr, 0, sizeof(addr));
    memset(recvbuf, 0, SegDataSize + MyUdpSeg::maxHeadSize + 1);
    int nRecv = recvfrom(udpSock, recvbuf, SegDataSize + MyUdpSeg::maxHeadSize + 1, 0, (struct sockaddr*)&addr, &addr_len);
    if (nRecv == SOCKET_ERROR)
    {
        int error = WSAGetLastError();
        if (error == WSAEWOULDBLOCK) //nothing to do, have not data
        {
            delete[] recvbuf;
            return;
        }

        printf("[SendWindow::recvAck] recv data error %d!\n", error);
        delete[] recvbuf;
        closesocket(udpSock);
        WSACleanup();
        exit(1);
    }
    string str(recvbuf);
    MyUdpSeg seg(str);

    if (seg.getId() != myid || seg.getTag(3) != 1) //illegal
    {
        delete[] recvbuf;
        return;
    }

    //syn+ack handShake
    if (seg.getTag(2) == 1)
    {
        handShakeOver = true;
        id = MyUdpSeg::id_type(stoul(seg.getData())); //get peer's id
    }
    updateWindow(seg.getWindow());

    TimerList::rtt_type rtt;

    {
        lock_guard<mutex> locker(bufferListMux);
        lock_guard<mutex> locker1(timerListMux);
        //delete node and timer
        bufferList.deleteNode(seg.getNumber());
        rtt = timerList.deleteTimer(seg.getNumber());
    }

    //calculate rto (RFC6289 recommendations)
    if (srtt == 0 and drtt == 0) //first measure
    {
        srtt = TimerList::rto_type(rtt);
        drtt = TimerList::rto_type(rtt) / 2;
    }
    else
    {
        srtt += 0.125 * (rtt - srtt);
        drtt = 0.75 * drtt + 0.25 * abs(rtt - srtt);
    }
    {
        lock_guard<mutex> locker(rtoMux);
        rto = 1 * srtt + 4 * drtt;
    }
    //printf("rto:%f\n", rto); //debug

    //update last receive ack timepoint
    {
        lock_guard<mutex> locker(detectThreadMux);
        lastAckTime = chrono::system_clock::now();
    }
    //printf("recv ack, packet number:%zd, current number:%zd\n", seg.getNumber(), number); //debug
    delete[] recvbuf;
}

void SendWindow::updateWindow(MyUdpSeg::win_type Window)
{
    lock_guard<mutex> locker(windowMux);
    //Approximately, the number of packets that can be sent
    window = min(maxWindow, MyUdpSeg::win_type(bufferList.size() * SegDataSize) + Window); //update
}

void SendWindow::handShake()
{
    rto = 1000; //1s
    MyUdpSeg::tag_type tag;
    tag.set(2, 1);
    string myId = to_string(myid);
    //An id of 0 indicates generic / initialize
    MyUdpSeg seg(myId.c_str(), 0, 0, MyUdpSeg::len_type(myId.size()), tag, 0, 0);
    bufferList.insertNode(seg);
    while (!handShakeOver and !sendOver) //may cause heart beat timeout
    {
        sendSeg();
        reLoad_timeout_Seg();
    }
    if (handShakeOver and !sendOver)
    {
        milliseconds = detectTime; //for detection
        printf("hand shake successfully\n");
        rto = 200;
    }
    else
    {
        printf("hand shake failed\n");
        closesocket(udpSock);
        WSACleanup();
        exit(1);
    }

}

void SendWindow::heartBeatDetectStart()
{
    t = thread([this]
        {
            chrono::system_clock::time_point nowtime;
            unsigned int tick = 0;
            while (!sendOver)
            {
                {
                    lock_guard<mutex> locker(detectThreadMux);
                    nowtime = chrono::system_clock::now();
                    tick = (unsigned int)(chrono::duration_cast<chrono::milliseconds>(nowtime - lastAckTime).count());
                }
                if (tick > milliseconds)
                {
                    {
                        lock_guard<mutex> locker(detectThreadMux);
                        sendOver = true;
                        printf("[SendWindow::heartBeatDetectStart] send is over...exit now\n");
                    }

                }
            }
        });
}


//----------------------------------------RecvWindow----------------------------------------//

RecvWindow::RecvWindow(SOCKET UdpSock, MyUdpSeg::win_type Window) :fixWindow(Window),
bufferList(), udpSock(UdpSock), fp(NULL), SegDataSize(UDPSEGSIZE), handShakeTime(HANDSHAKETIME), detectTime(DETECTTIME)
{
    maxOffset = 0;
    curOffset = 0;
    overOffset = -1;
    window = Window;
    id = 0;
    peerNotExist = false;
    //wait for ip and port
    socketaddr.sin_family = AF_INET;//ipv4

    //random choose myid
    random_device seed;//Hardware random number seed
    ranlux48 engine(seed());//Random number engine
    MyUdpSeg::id_type max = MyUdpSeg::id_type(pow(2, (sizeof(MyUdpSeg::id_type) * 8)) - 1);
    uniform_int_distribution<int> distrib(1, max);//uniform distribution

    myid = distrib(engine); //return random value

    lastRecvTime = chrono::system_clock::now();
    milliseconds = handShakeTime; //30s for hand shake
}
RecvWindow::RecvWindow(SOCKET UdpSock, const char* fileName, MyUdpSeg::win_type Window) :fixWindow(Window),
bufferList(), udpSock(UdpSock), SegDataSize(UDPSEGSIZE), handShakeTime(HANDSHAKETIME), detectTime(DETECTTIME)
{
    if (fopen_s(&fp, fileName, "wb") != 0)
    {
        printf("[RecvWindow::RecvWindow] open file failed!\n");
        closesocket(udpSock);
        WSACleanup();
        exit(1);
    }
    maxOffset = 0;
    curOffset = 0;
    overOffset = -1;
    window = Window;
    id = 0;
    peerNotExist = false;

    //wait for ip and port
    socketaddr.sin_family = AF_INET;//ipv4

    //random choose myid
    random_device seed;//Hardware random number seed
    ranlux48 engine(seed());//Random number engine
    MyUdpSeg::id_type max = MyUdpSeg::id_type(pow(2, (sizeof(MyUdpSeg::id_type) * 8)) - 1);
    uniform_int_distribution<int> distrib(1, max);//uniform distribution

    myid = distrib(engine); //return random value

    lastRecvTime = chrono::system_clock::now();
    milliseconds = handShakeTime; //30s for hand shake
}
RecvWindow::~RecvWindow()
{
    if (fp)
        fclose(fp);
    if (t.joinable())
        t.join();
}

bool RecvWindow::recvIsOver()
{
    lock_guard<mutex> locker1(detectThreadMux);
    lock_guard<mutex> locker2(windowOffsetMux);
    if (!peerNotExist and curOffset != overOffset)
    {
        return false;
    }
    else if (curOffset == overOffset)
    {
        printf("Receive successfully.\n");
        return true;
    }
    else //peerNotExist and curOffset != overOffset
    {
        printf("[RecvWindow::recvIsOver] Receive fails. The sender times out in heartbeat detection\n");
        if (milliseconds == handShakeTime)
            printf("[RecvWindow::recvIsOver] hand shake failed\n");
        return true;
    }

}

void RecvWindow::recvSeg()
{
    //window is the maximum number of bytes that can be received
    //{
        //lock_guard<mutex> locker(windowOffsetMux);
        //if ((window - maxOffset) < SegDataSize)
            //return;
    //}
    char* recvbuf = new char[SegDataSize + MyUdpSeg::maxHeadSize + 1];
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    //memset(&addr, 0, sizeof(addr));
    memset(recvbuf, 0, SegDataSize + MyUdpSeg::maxHeadSize + 1);
    //printf("recvfrom something\n"); //debug
    int nRecv = recvfrom(udpSock, recvbuf, SegDataSize + MyUdpSeg::maxHeadSize + 1, 0, (struct sockaddr*)&addr, &addr_len);

    if (nRecv == SOCKET_ERROR)
    {
        int error = WSAGetLastError();
        if (error == WSAEWOULDBLOCK) //nothing to do, have not data
        {
            delete[] recvbuf;
            return;
        }
        printf("[RecvWindow::recvSeg] recv data error %d!\n", error);
        delete[] recvbuf;
        closesocket(udpSock);
        WSACleanup();
        exit(1);
    }

    string str;
    //char* to string, prevents truncation by '0'
    for (size_t i = 0; i < nRecv; i++)
        str += recvbuf[i];

    MyUdpSeg seg(str);

    //syn handShake
    if (seg.getTag(2) == 1 and seg.getId() == 0)
    {
        id = MyUdpSeg::id_type(stoul(seg.getData())); //get peer's id
        socketaddr.sin_port = addr.sin_port;
        socketaddr.sin_addr = addr.sin_addr;
        handShakeReturn();
        //update
        {
            lock_guard<mutex> locker(detectThreadMux);
            lastRecvTime = chrono::system_clock::now();
        }
        delete[] recvbuf;
        return;
    }

    if (seg.getId() != myid)
    {
        delete[] recvbuf;
        return;
    }
    MyUdpSeg::off_type offset = seg.getOffset();
    //fin
    if (seg.getTag(1) == 1)
    {
        overOffset = offset;
        //printf("get 'fin' packet, offset is %zd\n", overOffset); //debug
    }
    sendAck(seg.getNumber());
    maxOffset = max(maxOffset, offset);
    //printf("recv packet number:%zd, offset:%zd, curoffset:%zd\n", seg.getNumber(), offset, curOffset); //debug
    {
        lock_guard<mutex> locker(windowOffsetMux);
        if (offset > curOffset)
        {
            lock_guard<mutex> locker(bufferListMux);
            bufferList.sortInsertNode(seg);
        }
    }


    //update
    {
        lock_guard<mutex> locker(detectThreadMux);
        lastRecvTime = chrono::system_clock::now();
    }
    if (milliseconds == handShakeTime)
    {
        milliseconds = detectTime; //for detection
        printf("hand shake successfully\n");
    }
    //printf("recv data: %s\n", seg.getData().c_str()); //debug
    delete[] recvbuf;
}

void RecvWindow::sendAck(MyUdpSeg::num_type Number)
{
    lock_guard<mutex> locker(windowOffsetMux);
    MyUdpSeg::tag_type tag;
    tag.set(3, 1); //set ack from 0 to 1

    MyUdpSeg::win_type myWindow = 0;
    if (window <= maxOffset)
        myWindow = 0;
    else
        myWindow = window - maxOffset;
    //printf("number:%zd, window:%zd, curoffset:%zd, maxoffset:%zd, overoffset:%zd\n",
        //Number, myWindow, curOffset, maxOffset, overOffset); //debug
    MyUdpSeg udpSeg("", Number, 0, 0, tag, myWindow, id);
    string data = udpSeg.seg_to_string();
    sendto(udpSock, data.c_str(), int(data.size()), 0, (struct sockaddr*)&socketaddr, sizeof(socketaddr));
}

string RecvWindow::getData()
{
    unique_lock<mutex> locker1(bufferListMux);
    BufferList::bufferIter iter = bufferList.begin();
    if (iter == bufferList.end())
        return "";
    locker1.unlock();

    MyUdpSeg::off_type offset = (*iter).udpSeg.getOffset();
    MyUdpSeg::len_type length = (*iter).udpSeg.getLength();


    unique_lock<mutex> locker(windowOffsetMux);
    if (curOffset != offset - length)
    {
        //printf("now offset:%zd, next offset:%zd, length:%zd\n", curOffset, offset, length); //debug
        return "";
    }


    locker.unlock();
    string data = (*iter).udpSeg.getData();
    //printf("get data: %s\n", data.c_str()); //debug

    locker1.lock();
    bufferList.deleteFront();
    locker1.unlock();

    locker.lock();
    curOffset = offset;
    updateWindow();

    return data;
}

size_t RecvWindow::writeFile()
{
    string data;
    size_t writeBytes = 0;
    while ((data = getData()) != "")
    {
        size_t nWrite = fwrite(data.c_str(), 1, data.size(), fp); //write data

        if (nWrite != data.size() || ferror(fp) != 0)
        {
            printf("[RecvWindow::writeFile] write file error\n");
            closesocket(udpSock);
            WSACleanup();
            exit(1);
        }
        writeBytes += nWrite;
        //printf("write file: %s\n", data.c_str()); //debug
    }
    return writeBytes;
}

void RecvWindow::updateWindow()
{
    window = fixWindow + curOffset;
}

void RecvWindow::handShakeReturn()
{
    MyUdpSeg::tag_type tag;
    //syn+ack
    tag.set(2, 1); //set syn from 0 to 1
    tag.set(3, 1); //set ack from 0 to 1
    string myId = to_string(myid);
    MyUdpSeg udpSeg(myId.c_str(), 0, 0, MyUdpSeg::len_type(myId.size()), tag, window - maxOffset, id);
    string data = udpSeg.seg_to_string();
    sendto(udpSock, data.c_str(), int(data.size()), 0, (struct sockaddr*)&socketaddr, sizeof(socketaddr));
}

void RecvWindow::heartBeatDetectStart()
{
    t = thread([this]
        {
            chrono::system_clock::time_point nowtime;
            int tick = 0;
            while (!peerNotExist and (curOffset != overOffset))
            {
                {
                    lock_guard<mutex> locker(detectThreadMux);
                    nowtime = chrono::system_clock::now();
                    tick = int(chrono::duration_cast<chrono::milliseconds>(nowtime - lastRecvTime).count());
                }
                if (tick > int(milliseconds))
                {
                    {
                        peerNotExist = true;
                    }

                }
            }
        });
}


//----------------------------------------Sender----------------------------------------//

Sender::Sender(SOCKET UdpSock, const char* ip, const int port) :
    udpSock(UdpSock), sendWindow(UdpSock, ip, port), defaultSock(false)
{
    //Using UDP requires disabling errors
    /*
    * If sending a datagram using the sendto function results in an "ICMP port unreachable" response and the select function is set for readfds,
    * the program returns 1 and the subsequent call to the recvfrom function does not work with a WSAECONNRESET (10054) error response.
    * In Microsoft Windows NT 4.0, this situation causes the select function to block or time out.
    */
    BOOL bEnalbeConnRestError = FALSE;
    DWORD dwBytesReturned = 0;
    WSAIoctl(udpSock, SIO_UDP_CONNRESET, &bEnalbeConnRestError, sizeof(bEnalbeConnRestError), \
        NULL, 0, &dwBytesReturned, NULL, NULL);

    setNonBlocking(udpSock);
    sendIsOver = false;
    fileName = "";
}
Sender::Sender(SOCKET UdpSock, const char* FileName, const char* ip, const int port) :
    udpSock(UdpSock), sendWindow(UdpSock, FileName, ip, port), defaultSock(false)
{
    //Using UDP requires disabling errors
    /*
    * If sending a datagram using the sendto function results in an "ICMP port unreachable" response and the select function is set for readfds,
    * the program returns 1 and the subsequent call to the recvfrom function does not work with a WSAECONNRESET (10054) error response.
    * In Microsoft Windows NT 4.0, this situation causes the select function to block or time out.
    */
    BOOL bEnalbeConnRestError = FALSE;
    DWORD dwBytesReturned = 0;
    WSAIoctl(udpSock, SIO_UDP_CONNRESET, &bEnalbeConnRestError, sizeof(bEnalbeConnRestError), \
        NULL, 0, &dwBytesReturned, NULL, NULL);

    setNonBlocking(udpSock);
    sendIsOver = false;
    fileName = FileName;
}
Sender::Sender(const char* ip, const int port) :
    udpSock(socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)), sendWindow(udpSock, ip, port), defaultSock(true)
{
    if (udpSock < 0)
    {
        printf("Sender::Sender create udp socket error\n");
        exit(1);
    }

    //Using UDP requires disabling errors
    /*
    * If sending a datagram using the sendto function results in an "ICMP port unreachable" response and the select function is set for readfds,
    * the program returns 1 and the subsequent call to the recvfrom function does not work with a WSAECONNRESET (10054) error response.
    * In Microsoft Windows NT 4.0, this situation causes the select function to block or time out.
    */
    BOOL bEnalbeConnRestError = FALSE;
    DWORD dwBytesReturned = 0;
    WSAIoctl(udpSock, SIO_UDP_CONNRESET, &bEnalbeConnRestError, sizeof(bEnalbeConnRestError), \
        NULL, 0, &dwBytesReturned, NULL, NULL);

    setNonBlocking(udpSock);
    sendIsOver = false;
    fileName = "";
}
Sender::Sender(const char* FileName, const char* ip, const int port) :
    udpSock(socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)), sendWindow(udpSock, FileName, ip, port), defaultSock(true)
{
    if (udpSock < 0)
    {
        printf("[Sender::Sender] create udp socket error\n");
        exit(1);
    }

    //Using UDP requires disabling errors
    /*
    * If sending a datagram using the sendto function results in an "ICMP port unreachable" response and the select function is set for readfds,
    * the program returns 1 and the subsequent call to the recvfrom function does not work with a WSAECONNRESET (10054) error response.
    * In Microsoft Windows NT 4.0, this situation causes the select function to block or time out.
    */
    BOOL bEnalbeConnRestError = FALSE;
    DWORD dwBytesReturned = 0;
    WSAIoctl(udpSock, SIO_UDP_CONNRESET, &bEnalbeConnRestError, sizeof(bEnalbeConnRestError), \
        NULL, 0, &dwBytesReturned, NULL, NULL);

    setNonBlocking(udpSock);
    sendIsOver = false;
    fileName = FileName;
}
Sender::~Sender()
{
    if (defaultSock)
        closesocket(udpSock);
    if (t.joinable())
        t.join();
}

void Sender::start()
{
    printf("sender start...\n");
    t = thread([this] //deal with receive
        {
            while (!sendIsOver and !sendWindow.sendIsOver())
                sendWindow.recvAck();
        });
    sendWindow.heartBeatDetectStart(); //deal with detect
    sendWindow.handShake();
}

//deal with send
void Sender::sendFile()
{
    if (fileName == "")
    {
        printf("[Sender::sendFile] There is no file name\n");
        return;
    }

    chrono::system_clock::time_point time1 = std::chrono::system_clock::now();
    //load -> send -> check timer

    //The entire file has not been loaded
    while (!sendWindow.loadFile())
    {
        if (sendWindow.sendIsOver()) //heart beat timeout
        {
            printf("[Sender::sendFile] heart beat timeout\n");
            sendIsOver = true;
            return;
        }
        sendWindow.sendSeg();
        sendWindow.reLoad_timeout_Seg();
    }
    //Now the file loading is complete
    while (!sendWindow.sendIsOver())
    {
        sendWindow.sendSeg();
        sendWindow.reLoad_timeout_Seg();
    }
    chrono::system_clock::time_point time2 = std::chrono::system_clock::now();
    size_t writeBytes = sendWindow.writeBytes();
    double MB = double(writeBytes) / 1000 / 1000;
    printf("receive %03f MB totally\n", MB);
    printf("speed: %03f MB/s\n", 1000 * MB / (chrono::duration_cast<std::chrono::milliseconds>(time2 - time1).count() - DETECTTIME));
}

void Sender::send(const char* data, size_t size)
{
    //load -> send -> check timer

    if (!sendWindow.loadData(data, size))
    {
        printf("[Sender::send] data is too large\n");
        sendIsOver = true;
        return;
    }
    while (!sendWindow.sendIsOver())
    {
        sendWindow.sendSeg();
        sendWindow.reLoad_timeout_Seg();
    }
}

void Sender::setNonBlocking(SOCKET sockfd)
{
    unsigned long on = 1;
    if (0 != ioctlsocket(sockfd, FIONBIO, &on))
    {
        closesocket(sockfd);
        WSACleanup();
        exit(1);
    }
}


//----------------------------------------Receiver----------------------------------------//

Receiver::Receiver(SOCKET UdpSock, MyUdpSeg::win_type Window) :
    udpSock(UdpSock), recvWindow(UdpSock, Window), defaultSock(false)
{
    //Using UDP requires disabling errors
    /*
    * If sending a datagram using the sendto function results in an "ICMP port unreachable" response and the select function is set for readfds,
    * the program returns 1 and the subsequent call to the recvfrom function does not work with a WSAECONNRESET (10054) error response.
    * In Microsoft Windows NT 4.0, this situation causes the select function to block or time out.
    */
    BOOL bEnalbeConnRestError = FALSE;
    DWORD dwBytesReturned = 0;
    WSAIoctl(udpSock, SIO_UDP_CONNRESET, &bEnalbeConnRestError, sizeof(bEnalbeConnRestError), \
        NULL, 0, &dwBytesReturned, NULL, NULL);

    setNonBlocking(udpSock);
    recvIsOver = false;
    fileName = "";
}
Receiver::Receiver(SOCKET UdpSock, const char* FileName, MyUdpSeg::win_type Window) :
    udpSock(UdpSock), recvWindow(UdpSock, FileName, Window), defaultSock(false)
{
    //Using UDP requires disabling errors
    /*
    * If sending a datagram using the sendto function results in an "ICMP port unreachable" response and the select function is set for readfds,
    * the program returns 1 and the subsequent call to the recvfrom function does not work with a WSAECONNRESET (10054) error response.
    * In Microsoft Windows NT 4.0, this situation causes the select function to block or time out.
    */
    BOOL bEnalbeConnRestError = FALSE;
    DWORD dwBytesReturned = 0;
    WSAIoctl(udpSock, SIO_UDP_CONNRESET, &bEnalbeConnRestError, sizeof(bEnalbeConnRestError), \
        NULL, 0, &dwBytesReturned, NULL, NULL);

    setNonBlocking(udpSock);
    recvIsOver = false;
    fileName = FileName;
}
Receiver::Receiver(const int myPort, MyUdpSeg::win_type Window) :
    udpSock(socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)), recvWindow(udpSock, Window), defaultSock(true)
{
    if (udpSock < 0)
    {
        printf("[Receiver::Receiver] create udp socket error:%d\n", WSAGetLastError());
        exit(1);
    }

    //need to bind
    struct sockaddr_in addr;
    addr.sin_family = AF_INET; //ipv4
    addr.sin_port = htons(myPort);
    addr.sin_addr.s_addr = htonl(INADDR_ANY); //Monitor all NICs


    //bind port
    if (::bind(udpSock, (struct sockaddr*)&addr, sizeof(addr)) == -1)
    {
        printf("[Receiver::Receiver] bind port:%d error:%d\n", myPort, WSAGetLastError());
        closesocket(udpSock);
        exit(1);
    }

    //Using UDP requires disabling errors
    /*
    * If sending a datagram using the sendto function results in an "ICMP port unreachable" response and the select function is set for readfds,
    * the program returns 1 and the subsequent call to the recvfrom function does not work with a WSAECONNRESET (10054) error response.
    * In Microsoft Windows NT 4.0, this situation causes the select function to block or time out.
    */
    BOOL bEnalbeConnRestError = FALSE;
    DWORD dwBytesReturned = 0;
    WSAIoctl(udpSock, SIO_UDP_CONNRESET, &bEnalbeConnRestError, sizeof(bEnalbeConnRestError), \
        NULL, 0, &dwBytesReturned, NULL, NULL);

    setNonBlocking(udpSock);
    recvIsOver = false;
    fileName = "";
}
Receiver::Receiver(const int myPort, const char* FileName, MyUdpSeg::win_type Window) :
    udpSock(socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)), recvWindow(udpSock, FileName, Window), defaultSock(true)
{
    if (udpSock < 0)
    {
        printf("[Receiver::Receiver] create udp socket error:%d\n", WSAGetLastError());
        exit(1);
    }

    //need to bind
    struct sockaddr_in addr;
    addr.sin_family = AF_INET; //ipv4
    addr.sin_port = htons(myPort);
    addr.sin_addr.s_addr = htonl(INADDR_ANY); //Monitor all NICs


    //bind port
    if (::bind(udpSock, (struct sockaddr*)&addr, sizeof(addr)) == -1)
    {
        printf("[Receiver::Receiver] bind port:%d error:%d\n", myPort, WSAGetLastError());
        closesocket(udpSock);
        exit(1);
    }

    //Using UDP requires disabling errors
    /*
    * If sending a datagram using the sendto function results in an "ICMP port unreachable" response and the select function is set for readfds,
    * the program returns 1 and the subsequent call to the recvfrom function does not work with a WSAECONNRESET (10054) error response.
    * In Microsoft Windows NT 4.0, this situation causes the select function to block or time out.
    */
    BOOL bEnalbeConnRestError = FALSE;
    DWORD dwBytesReturned = 0;
    WSAIoctl(udpSock, SIO_UDP_CONNRESET, &bEnalbeConnRestError, sizeof(bEnalbeConnRestError), \
        NULL, 0, &dwBytesReturned, NULL, NULL);

    setNonBlocking(udpSock);
    recvIsOver = false;
    fileName = FileName;
}
Receiver::~Receiver()
{
    if (defaultSock)
        closesocket(udpSock);
    if (t.joinable())
        t.join();
}

void Receiver::start()
{
    printf("receiver start...\n");
    t = thread([this] //deal with receive
        {
            while (!recvWindow.recvIsOver())
                recvWindow.recvSeg();
        });
    recvWindow.heartBeatDetectStart(); //deal with detect
}

//deal with receive
void Receiver::recvFile()
{
    if (fileName == "")
    {
        printf("[Receiver::recvFile] There is no file name\n");
        return;
    }
    size_t writeBytes = 0;
    chrono::system_clock::time_point time1 = std::chrono::system_clock::now();
    while (!recvWindow.recvIsOver())
        writeBytes += recvWindow.writeFile();

    //Tail writes
    writeBytes += recvWindow.writeFile();
    chrono::system_clock::time_point time2 = std::chrono::system_clock::now();
    double MB = double(writeBytes) / 1000 / 1000;
    printf("receive %03f MB totally\n", MB);
    printf("speed: %03f MB/s\n", 1000 * MB / chrono::duration_cast<std::chrono::milliseconds>(time2 - time1).count());

}

string Receiver::recv()
{
    string res;
    while (!recvWindow.recvIsOver())
        res += recvWindow.getData();
    return res;
}

void Receiver::setNonBlocking(SOCKET sockfd)
{
    unsigned long on = 1;
    if (0 != ioctlsocket(sockfd, FIONBIO, &on))
    {
        closesocket(sockfd);
        WSACleanup();
        exit(1);
    }
}