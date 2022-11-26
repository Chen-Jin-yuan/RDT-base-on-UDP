//receiver_1.cpp

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
	Receiver receiver(10001);
	receiver.start();
	string data = receiver.recv();
    printf("%s\n", data.c_str());
    WSACleanup();
	return 0;
}