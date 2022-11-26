#include "rdt_on_udp.h"
#include "udp_hole_punch.h"
const char* SERVER_IP = "101.34.2.129";
const int SERVERUDPPORT = 10000;
const int MYPORT = 22223;

int main()
{
	
	SOCKET udpfd;
	if (!udpPunchSide(udpfd, SERVER_IP, SERVERUDPPORT, MYPORT))
	{
		closesocket(udpfd);
		WSACleanup();
		return 0;
	}
    Receiver receiver(udpfd, "GAMES101.pdf");

	receiver.start();
	receiver.recvFile();
	getchar();
	closesocket(udpfd);
    WSACleanup();
	return 0;
}