#include <iostream>
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
    int protocolChoice = 0;  // 0=UDP, 1=TCP

    char remoteIP[16]{};     // 마지막으로 수신한 상대 IP(UDP에서 필요)
    char localIP[16]{};
    char netbuf[BUFSIZE]{};
    char keybuf[BUFSIZE]{};

    // --- 프로토콜 선택 ---
    do {
        std::cout << "----- Chat Server -----\n"
            << "\nSelect Protocol\n"
            << "  0 = UDP\n"
            << "  1 = TCP\n\n"
            << "Choice : ";
        std::cin >> protocolChoice;
    } while (protocolChoice != 0 && protocolChoice != 1);
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    // --- 포트 입력 ---
    do {
        std::cout << "Enter port number (Use 0 for default 48161) : ";
        std::cin >> newPort;
    } while (newPort < 0 || newPort > 65535);
    if (newPort != 0) port = newPort;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    Protocol proto = (protocolChoice == 0) ? Protocol::UDP : Protocol::TCP;

    Net net;
    int err = net.createServer(port, proto);
    if (err != NET_OK) { std::cout << net.getError(err) << "\n"; return 1; }

    if (net.getLocalIP(localIP, sizeof(localIP)) == NET_OK)
        std::cout << "Server IP is : " << localIP << "\n";
    std::cout << "Server port is : " << port << "\n";

    while (true) {
        // 수신
        int rsz = BUFSIZE;
        int rc = net.readData(netbuf, rsz, remoteIP);
        if (rc == NET_OK && rsz > 0) {
            std::cout << "[" << (remoteIP[0] ? remoteIP : "Client") << "] ";
            std::cout.write(netbuf, rsz);
            std::cout << std::endl;
        }
        else if (rc != NET_OK) {
            std::cout << net.getError(rc) << std::endl;
            net.closeSocket();
            if (net.createServer(port, proto) != NET_OK) {
                std::cout << "recreate server failed\n";
                return 1;
            }
        }

        // 입력 → 전송
        if (_kbhit()) {
            std::cin.getline(keybuf, BUFSIZE);
            if (std::cin.fail()) {
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            }
            int sz = (int)std::strlen(keybuf);
            if (sz > 0) {
                if (proto == Protocol::UDP) {
                    if (remoteIP[0]) {
                        int tmp = sz;
                        int se = net.sendData(keybuf, tmp, remoteIP);
                        if (se != NET_OK) std::cout << net.getError(se) << "\n";
                    }
                }
                else {
                    int tmp = sz;
                    int se = net.sendData(keybuf, tmp); // TCP는 remoteIP 불필요
                    if (se != NET_OK) std::cout << net.getError(se) << "\n";
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}