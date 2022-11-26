#include "rdt_on_udp.h"

void initSocket()
{
    WORD sockVersion = MAKEWORD(2, 2);
    WSADATA wsaData;

    if (WSAStartup(sockVersion, &wsaData) != 0)
    {
        std::cerr << "WSAStartup() error!" << std::endl;
        exit(1);
    }
}
int main()
{
    initSocket();
	Sender sender("GAMES101.pdf" ,"127.0.0.1", 10001);
	sender.start();
        
	sender.sendFile();
    WSACleanup();
	return 0;
}