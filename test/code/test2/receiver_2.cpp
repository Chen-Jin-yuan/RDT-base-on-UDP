#include "rdt_on_udp.h"

//user need to init WSA and close
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
    Receiver receiver(10001, "GAMES101.pdf");
	receiver.start();
	receiver.recvFile();
    WSACleanup();
	return 0;
}