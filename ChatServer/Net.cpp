#include "Net.h"
#include <sstream>

Net::Net() {}
Net::~Net() { closeSocket(); }

int Net::initialize(int port, Protocol proto) {
    if (netInitialized) closeSocket();

    WSADATA wsa{};
    int ws = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (ws != 0) { lastWSAError_ = ws; return NET_ERROR; }
    netInitialized = true;

    proto_ = proto;
    mode = Mode::UNINITIALIZED;
    bound = false;
    type = (proto == Protocol::UDP) ? ConnType::UDP_TYPE : ConnType::UNCONNECTED_TCP;

    sock = (proto == Protocol::UDP)
        ? ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)
        : ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (sock == INVALID_SOCKET) { lastWSAError_ = WSAGetLastError(); return NET_ERROR; }

    // non-blocking
    u_long nb = 1;
    if (ioctlsocket(sock, FIONBIO, &nb) == SOCKET_ERROR) {
        lastWSAError_ = WSAGetLastError();
        return NET_ERROR;
    }

    std::memset(&localAddr, 0, sizeof(localAddr));
    std::memset(&remoteAddr, 0, sizeof(remoteAddr));

    localAddr.sin_family = AF_INET;
    localAddr.sin_port = htons(static_cast<u_short>(port));
    localAddr.sin_addr.s_addr = INADDR_ANY;

    remoteAddr.sin_family = AF_INET;
    remoteAddr.sin_port = htons(static_cast<u_short>(port));
    remoteAddr.sin_addr.s_addr = INADDR_ANY;

    return NET_OK;
}

int Net::createServer(int port, Protocol proto) {
    if (initialize(port, proto) != NET_OK) return NET_ERROR;

    if (::bind(sock, reinterpret_cast<sockaddr*>(&localAddr), sizeof(localAddr)) == SOCKET_ERROR) {
        lastWSAError_ = WSAGetLastError();
        return NET_ERROR;
    }

    bound = true;
    mode = Mode::SERVER;

    if (proto == Protocol::TCP) {
        if (::listen(sock, SOMAXCONN) == SOCKET_ERROR) {
            lastWSAError_ = WSAGetLastError();
            return NET_ERROR;
        }
        type = ConnType::UNCONNECTED_TCP; // 리스너
    }
    return NET_OK;
}

int Net::createClient(const char* server, int port, Protocol proto) {
    if (initialize(port, proto) != NET_OK) return NET_ERROR;

    // dotted-decimal이면 그대로, 아니면 DNS
    in_addr addr{};
    addr.s_addr = inet_addr(server);
    if (addr.s_addr == INADDR_NONE) {
        ADDRINFOA hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = (proto == Protocol::UDP) ? SOCK_DGRAM : SOCK_STREAM;
        hints.ai_protocol = (proto == Protocol::UDP) ? IPPROTO_UDP : IPPROTO_TCP;

        ADDRINFOA* res = nullptr;
        int ga = getaddrinfo(server, nullptr, &hints, &res);
        if (ga != 0 || !res) { lastWSAError_ = (ga != 0 ? ga : WSAGetLastError()); return NET_ERROR; }
        addr = reinterpret_cast<sockaddr_in*>(res->ai_addr)->sin_addr;
        freeaddrinfo(res);
    }
    remoteAddr = {};
    remoteAddr.sin_family = AF_INET;
    remoteAddr.sin_port = htons(port);
    remoteAddr.sin_addr = addr;

    mode = Mode::CLIENT;

    if (proto == Protocol::TCP) {
        if (::connect(sock, reinterpret_cast<sockaddr*>(&remoteAddr), sizeof(remoteAddr)) == SOCKET_ERROR) {
            lastWSAError_ = WSAGetLastError();
            return NET_ERROR;
        }
        type = ConnType::CONNECTED_TCP;
    }
    else {
        // UDP — connect()를 호출하면 send/recv 사용 가능
        if (::connect(sock, reinterpret_cast<sockaddr*>(&remoteAddr), sizeof(remoteAddr)) == SOCKET_ERROR) {
            // connect 실패해도 UDP는 sendto/recvfrom 사용하면 됨
        }
        type = ConnType::UDP_TYPE;
    }

    return NET_OK;
}

int Net::getLocalIP(char* out, size_t outSize) {
    char host[256]{};
    if (gethostname(host, sizeof(host)) == SOCKET_ERROR) {
        lastWSAError_ = WSAGetLastError();
        return NET_ERROR;
    }

    ADDRINFOA hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    ADDRINFOA* res = nullptr;
    int ga = getaddrinfo(host, nullptr, &hints, &res);
    if (ga != 0 || !res) {
        lastWSAError_ = (ga != 0 ? ga : WSAGetLastError());
        return NET_ERROR;
    }

    auto in = reinterpret_cast<sockaddr_in*>(res->ai_addr)->sin_addr;
    const char* ip = inet_ntoa(in);
    if (!ip) { freeaddrinfo(res); lastWSAError_ = WSAGetLastError(); return NET_ERROR; }

    strncpy_s(out, outSize, ip, _TRUNCATE);
    freeaddrinfo(res);
    return NET_OK;
}

int Net::sendData(const char* data, int& size, const char* remoteIP) {
    if (sock == INVALID_SOCKET || size <= 0) return NET_ERROR;

    if (proto_ == Protocol::UDP) {
        if (remoteIP) remoteAddr.sin_addr.s_addr = inet_addr(remoteIP);
        int sent = ::sendto(sock, data, size, 0,
            reinterpret_cast<sockaddr*>(&remoteAddr), sizeof(remoteAddr));
        if (sent == SOCKET_ERROR) {
            int e = WSAGetLastError(); lastWSAError_ = e;
            if (e == WSAEWOULDBLOCK) { size = 0; return NET_OK; }
            return NET_ERROR;
        }
        size = sent;
        return NET_OK;
    }

    // TCP
    if (type == ConnType::UNCONNECTED_TCP) {
        int r = ::connect(sock, reinterpret_cast<sockaddr*>(&remoteAddr), sizeof(remoteAddr));
        if (r == SOCKET_ERROR) {
            int e = WSAGetLastError(); lastWSAError_ = e;
            if (e == WSAEISCONN) {
                type = ConnType::CONNECTED_TCP;
            }
            else if (e == WSAEWOULDBLOCK || e == WSAEALREADY) {
                size = 0; // 아직 연결 중
                return NET_OK;
            }
            else {
                return NET_ERROR;
            }
        }
        else {
            type = ConnType::CONNECTED_TCP;
        }
    }

    int sent = ::send(sock, data, size, 0);
    if (sent == SOCKET_ERROR) {
        int e = WSAGetLastError(); lastWSAError_ = e;
        if (e == WSAEWOULDBLOCK) { size = 0; return NET_OK; }
        return NET_ERROR;
    }
    size = sent;
    return NET_OK;
}

int Net::readData(char* data, int& size, char* senderIP) {
    if (sock == INVALID_SOCKET || size <= 0) return NET_ERROR;

    // 서버 TCP: 아직 accept 안 했으면 비동기로 시도
    if (mode == Mode::SERVER && proto_ == Protocol::TCP && type == ConnType::UNCONNECTED_TCP) {
        sockaddr_in peer{};
        int peerLen = sizeof(peer);
        SOCKET acc = ::accept(sock, reinterpret_cast<sockaddr*>(&peer), &peerLen);
        if (acc == INVALID_SOCKET) {
            int e = WSAGetLastError();
            if (e == WSAEWOULDBLOCK) {
                size = 0;
                return NET_OK; // 아직 연결 안 됨
            }
            lastWSAError_ = e;
            return NET_ERROR;
        }
        // accept 성공 → 소켓 교체
        ::closesocket(sock);
        sock = acc;
        type = ConnType::CONNECTED_TCP;
    }

    if (proto_ == Protocol::UDP) {
        sockaddr_in from{};
        int fromLen = sizeof(from);
        int recvd = ::recvfrom(sock, data, size, 0,
            reinterpret_cast<sockaddr*>(&from), &fromLen);
        if (recvd == SOCKET_ERROR) {
            int e = WSAGetLastError(); lastWSAError_ = e;
            if (e == WSAEWOULDBLOCK) { size = 0; return NET_OK; }
            return NET_ERROR;
        }
        if (senderIP) {
            const char* ip = inet_ntoa(from.sin_addr);
            if (ip) strncpy_s(senderIP, 16, ip, _TRUNCATE);
        }
        size = recvd;
        return NET_OK;
    }
    else {
        int recvd = ::recv(sock, data, size, 0);
        if (recvd == SOCKET_ERROR) {
            int e = WSAGetLastError(); lastWSAError_ = e;
            if (e == WSAEWOULDBLOCK) { size = 0; return NET_OK; }
            return NET_ERROR;
        }
        if (recvd == 0) { // 원격 종료
            lastWSAError_ = WSAENOTCONN;
            return NET_ERROR;
        }
        size = recvd;
        return NET_OK;
    }
}

int Net::closeSocket() {
    if (sock != INVALID_SOCKET) {
        ::closesocket(sock);
        sock = INVALID_SOCKET;
    }
    bound = false;
    mode = Mode::UNINITIALIZED;
    type = ConnType::UDP_TYPE;

    if (netInitialized) {
        WSACleanup();
        netInitialized = false;
    }
    return NET_OK;
}

const char* Net::wsaErrorText(int code) {
    switch (code) {
    case WSAEWOULDBLOCK:  return "Operation would block";
    case WSAEALREADY:     return "Operation already in progress";
    case WSAEISCONN:      return "Socket is already connected";
    case WSAENOTCONN:     return "Socket is not connected";
    case WSAECONNREFUSED: return "Connection refused";
    case WSAETIMEDOUT:    return "Connection timed out";
    case WSAECONNRESET:   return "Connection reset by peer";
    case WSAENETUNREACH:  return "Network unreachable";
    case WSAEHOSTUNREACH: return "Host unreachable";
    case WSAEINVAL:       return "Invalid argument";
    case WSAENOTSOCK:     return "Not a socket";
    default:              return "Unknown";
    }
}

std::string Net::getError(int code) const {
    if (code == NET_OK) return "OK";
    std::ostringstream os;
    os << "NET_ERROR";
    if (lastWSAError_ != 0) {
        os << " (WSA=" << lastWSAError_ << ": " << wsaErrorText(lastWSAError_) << ")";
    }
    return os.str();
}