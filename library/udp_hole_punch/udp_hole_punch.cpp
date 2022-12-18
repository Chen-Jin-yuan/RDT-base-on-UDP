#include "udp_hole_punch.h"

void initSocket()
{
    //init WSA
    WORD sockVersion = MAKEWORD(2, 2);
    WSADATA wsaData;

    if (WSAStartup(sockVersion, &wsaData) != 0)
    {
        std::cerr << "WSAStartup() error!" << std::endl;
        exit(1);
    }
}

//blocking socket
void initUdpSocket(SOCKET& listenfd, const int port)
{
    listenfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP); //UDP
    if (listenfd < 0)
    {
        printf("create listen socket error, port-%d\n", port);
        exit(1);
    }

    struct sockaddr_in socketaddr;
    socketaddr.sin_family = AF_INET;//ipv4
    socketaddr.sin_port = htons(port);
    socketaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    //bind port
    if (bind(listenfd, (struct sockaddr*)&socketaddr, sizeof(socketaddr)) == -1)
    {
        printf("bind port-%d error !\n", port);
        closesocket(listenfd);
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
    WSAIoctl(listenfd, SIO_UDP_CONNRESET, &bEnalbeConnRestError, sizeof(bEnalbeConnRestError), \
        NULL, 0, &dwBytesReturned, NULL, NULL);
}

vector<string> getIpList()
{
    vector<string> result;
    char name[256];

    int getNameRet = gethostname(name, sizeof(name));//get host name
    //get private ip list
    hostent* host = gethostbyname(name);//need to disable c4996 error

    if (NULL == host)
    {
        return result;
    }

    in_addr* pAddr = (in_addr*)*host->h_addr_list;

    for (int i = 0; host->h_addr_list[i] != 0; i++)
    {
        char ip[20] = { '\0' };

        inet_ntop(AF_INET, &pAddr[i], ip, 16);
        string addr = ip;
        result.push_back(addr);
    }

    return result;
}

//send udp segment to server
bool notifyServer(SOCKET& udpfd, const char* server_ip, const int serverPort, const int myPort)
{
    struct sockaddr_in socketaddr;
    socketaddr.sin_family = AF_INET;//ipv4
    socketaddr.sin_port = htons(serverPort);
    inet_pton(AF_INET, server_ip, &socketaddr.sin_addr);

    vector<string> myipList = getIpList();
    string myip = myipList[myipList.size() - 1]; //send the last one 
    string myip_port = myip + " " + to_string(myPort);
    //send the private ip
    size_t res = sendto(udpfd, myip_port.c_str(), myip_port.size(), 0, (struct sockaddr*)&socketaddr, sizeof(socketaddr));
    if (res == SOCKET_ERROR)
    {
        printf("udp sendto error!\n");
        return false;
    }
    return true;

}

//send one or more messages to the other party(punch a hole)
bool udpHolePunch(SOCKET& udpfd, const char* gateway_ip, const int gatewayPort)
{
    struct sockaddr_in gateway;
    gateway.sin_family = AF_INET;
    gateway.sin_port = htons(gatewayPort);
    inet_pton(AF_INET, gateway_ip, &gateway.sin_addr);


    char sendbuf[10] = "hello!";
    //send two messages
    int res = sendto(udpfd, sendbuf, strlen(sendbuf), 0, (struct sockaddr*)&gateway, sizeof(gateway));
    if (res == SOCKET_ERROR)
    {
        printf("udp sendto error1!\n");
        return false;
    }
    res = sendto(udpfd, sendbuf, strlen(sendbuf), 0, (struct sockaddr*)&gateway, sizeof(gateway));
    if (res == SOCKET_ERROR)
    {
        printf("udp sendto error2!\n");
        return false;
    }
    return true;
}

bool udpPunchSide(SOCKET& udpfd, const char* server_ip, const int serverPort, const int myPort)
{
    initSocket();

    initUdpSocket(udpfd, myPort);

    //send message to server
    if (!notifyServer(udpfd, server_ip, serverPort, myPort))
        return false;

    //recvfrom server
    struct sockaddr_in server;
    socklen_t addr_len = sizeof(server);
    memset(&server, 0, sizeof(server));
    char recvbuf[128];
    memset(recvbuf, 0, sizeof(recvbuf));

    size_t res = recvfrom(udpfd, recvbuf, 128, 0, (struct sockaddr*)&server, &addr_len);
    if (res == SOCKET_ERROR)
    {
        printf("recv from server error %d!\n", WSAGetLastError());
        return false;
    }
    string gateway_ip_port = recvbuf;
    size_t pos1 = gateway_ip_port.find(" ");
    string gateway_ip = gateway_ip_port.substr(0, pos1);
    string gateway_port = gateway_ip_port.substr(pos1 + 1);


    printf("client recv: ip: %s, port: %s\n", gateway_ip.c_str(), gateway_port.c_str()); //debug

    if (!udpHolePunch(udpfd, gateway_ip.c_str(), stoi(gateway_port)))
    {
        printf("udp hole punch error!\n");
        return false;
    }

    return true;
}

pair<string, int> udpPunchedSide(SOCKET& udpfd, const char* server_ip, const int serverPort, const int myPort)
{
    initSocket();

    initUdpSocket(udpfd, myPort);
    pair<string, int> pairRes;
    pairRes.first = "";
    pairRes.second = -1;
    //send message to server
    if (!notifyServer(udpfd, server_ip, serverPort, myPort))
        return pairRes;

    //recvfrom server
    struct sockaddr_in server;
    socklen_t addr_len = sizeof(server);
    memset(&server, 0, sizeof(server));
    char recvbuf[128];
    memset(recvbuf, 0, sizeof(recvbuf));

    size_t res = recvfrom(udpfd, recvbuf, 128, 0, (struct sockaddr*)&server, &addr_len);
    if (res == SOCKET_ERROR)
    {
        printf("recv from server error %d!\n", WSAGetLastError());
        return pairRes;
    }
    string gateway_ip_port = recvbuf;
    size_t pos1 = gateway_ip_port.find(" ");
    string gateway_ip = gateway_ip_port.substr(0, pos1);
    string gateway_port = gateway_ip_port.substr(pos1 + 1);


    printf("client recv: ip: %s, port: %s\n", gateway_ip.c_str(), gateway_port.c_str()); //debug
    pairRes.first = gateway_ip;
    pairRes.second = stoi(gateway_port);
    return pairRes;
}
