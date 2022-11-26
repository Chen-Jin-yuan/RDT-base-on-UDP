//sender_1.cpp

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
	Sender sender("127.0.0.1", 10001);
	sender.start();
    string data;
    for (int i = 0; i < 10; i++)
    {
        for (int j = 0; j < 200; j++)
            data += "jy";
        data += "\n";
    }
        
	sender.send(data.c_str(), data.size());
    WSACleanup();
	return 0;
}