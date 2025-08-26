#pragma once
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")

#include <string>
#include <cstring>

enum class Protocol { UDP, TCP };
enum class Mode { UNINITIALIZED, SERVER, CLIENT };
enum class ConnType { UDP_TYPE, UNCONNECTED_TCP, CONNECTED_TCP };

constexpr int NET_OK = 0;
constexpr int NET_ERROR = -1;

class Net {
public:
    Net();
    ~Net();

    int initialize(int port, Protocol proto);
    int createServer(int port, Protocol proto);
    int createClient(const char* server, int port, Protocol proto);

    int getLocalIP(char* out, size_t outSize);

    // size�� in/out: ���� ũ�� ���� �� ���� ����/���ŵ� ũ�� ��ȯ
    int sendData(const char* data, int& size, const char* remoteIP = nullptr);
    int readData(char* data, int& size, char* senderIP = nullptr);

    int closeSocket();

    // ���� ���ڿ� (������ WSA ���� �ڵ� ����)
    std::string getError(int code) const;

private:
    SOCKET      sock{ INVALID_SOCKET };
    Mode        mode{ Mode::UNINITIALIZED };
    ConnType    type{ ConnType::UDP_TYPE };
    Protocol    proto_{ Protocol::UDP };
    bool        bound{ false };
    bool        netInitialized{ false };

    int         lastWSAError_{ 0 }; // ������ WSA ���� ����

    sockaddr_in localAddr{};
    sockaddr_in remoteAddr{};

    static const char* wsaErrorText(int code);
};