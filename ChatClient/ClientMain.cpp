#include <iostream>
#include <string>
#include <conio.h>
#include <limits>
#include <thread>
#include <chrono>
#include <cstring>
#include "Net.h"

int main() {

    constexpr int BUFSIZE = 256;
    int port = 48161;
    int newPort = 0;
    int protocolChoice = 0;                 // 0: UDP, 1: TCP

    char remoteIP[BUFSIZE]{};               // 서버 IP 또는 도메인
    char localIP[16]{};                     // nnn.nnn.nnn.nnn
    char netbuf[BUFSIZE]{};                 // 네트워크 수신 버퍼
    char keybuf[BUFSIZE]{};                 // 전송 버퍼

    Net net;

    // --- 프로토콜 선택 ---
    do {
        std::cout << "----- Chat Client -----\n"
            << "\nSelect Protocol\n"
            << "  0 = UDP\n"
            << "  1 = TCP\n\n"
            << "Choice : ";
        std::cin >> protocolChoice;
    } while (protocolChoice != 0 && protocolChoice != 1);
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    // --- 서버 주소 입력 ---
    std::cout << "Enter IP address or name of server : ";
    std::cin.getline(remoteIP, BUFSIZE);

    // --- 포트 입력 ---
    do {
        std::cout << "Enter port number (Use 0 for default 48161) : ";
        std::cin >> newPort;
    } while (newPort < 0 || newPort > 65535);
    if (newPort != 0) port = newPort;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    // --- 클라이언트 생성 ---
    Protocol proto = (protocolChoice == 0) ? Protocol::UDP : Protocol::TCP;
    int err = net.createClient(remoteIP, port, proto);
    if (err != NET_OK) {
        std::cout << net.getError(err) << std::endl;
        return 1;
    }

    std::cout << "Server address : " << remoteIP << "\n";
    if (net.getLocalIP(localIP, sizeof(localIP)) == NET_OK)
        std::cout << "Client IP : " << localIP << "\n";

    while (true) {
        // 입력 → 전송
        if (_kbhit()) {
            std::cin.getline(keybuf, BUFSIZE);
            int sz = (int)std::strlen(keybuf);
            if (sz > 0) {
                int tmp = sz;
                int rc = (proto == Protocol::UDP)
                    ? net.sendData(keybuf, tmp, remoteIP)
                    : net.sendData(keybuf, tmp); // TCP는 remoteIP 불필요
                if (rc != NET_OK) std::cout << net.getError(rc) << std::endl;
            }
        }

        // 수신(논블로킹)
        int rsz = BUFSIZE;
        char sender[16]{};
        int rr = net.readData(netbuf, rsz, sender);
        if (rr == NET_OK && rsz > 0) {
            std::cout << (sender[0] ? sender : "Server") << " : ";
            std::cout.write(netbuf, rsz);
            std::cout << '\n';
        }
        else if (rr != NET_OK) {
            std::cout << net.getError(rr) << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}