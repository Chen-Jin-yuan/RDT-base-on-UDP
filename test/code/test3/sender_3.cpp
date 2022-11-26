#include "rdt_on_udp.h"
#include "udp_hole_punch.h"
const char* SERVER_IP = "101.34.2.129";
const int SERVERUDPPORT = 10000;
const int MYPORT = 22222;
int main()
{
	
	SOCKET udpfd;
	pair<string, int> gateway = udpPunchedSide(udpfd, SERVER_IP, SERVERUDPPORT, MYPORT);
	if (gateway.first == "")
	{
		closesocket(udpfd);
		WSACleanup();
		return 0;
	}
	const char* gateway_ip = gateway.first.c_str();
	const int gateway_port = gateway.second;

	Sender sender(udpfd,"GAMES101.pdf" , gateway_ip, gateway_port);

	sender.start();
        
	sender.sendFile();

	printf("enter anything to exit...\n");
	char x = getchar();
	closesocket(udpfd);
    WSACleanup();
	return 0;
}