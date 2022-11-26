#ifndef UDP_HOLE_PUNCH_H
#define UDP_HOLE_PUNCH_H

#include <iostream>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <chrono>
#include <string>
#include <vector>
#pragma comment(lib,"ws2_32.lib")

//Using UDP requires disabling errors
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#pragma warning(disable: 4996)

using namespace std;

void initSocket(); //init WSA
void initUdpSocket(SOCKET& listenfd, const int port); //init blocking socket
vector<string> getIpList(); //get host private ip

//send udp segment to server
bool notifyServer(SOCKET& udpfd, const char* server_ip, const int serverPort, const int myPort);

//send one or more messages to the other party(punch a hole)
bool udpHolePunch(SOCKET& udpfd, const char* gateway_ip, const int gatewayPort);

//active, receiver
bool udpPunchSide(SOCKET& udpfd, const char* server_ip, const int serverPort, const int myPort);
//passive, sender, return gateway ip and port
pair<string, int> udpPunchedSide(SOCKET& udpfd, const char* server_ip, const int serverPort, const int myPort);

#endif