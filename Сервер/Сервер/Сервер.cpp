#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <fstream>
#include <vector>
#include <string>

using namespace std;

#pragma comment(lib, "ws2_32.lib")

const int BUFFER_SIZE = 1024;
const int DEFAULT_PORT = 12345;

void xor_encrypt(vector<char>& data, const string& key) {
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] ^= key[i % key.size()];
    }
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "WSAStartup failed\n";
        return 1;
    }

    SOCKET serverSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (serverSocket == INVALID_SOCKET) {
        cerr << "Socket creation failed: " << WSAGetLastError() << "\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(DEFAULT_PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cerr << "Bind failed: " << WSAGetLastError() << "\n";
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    cout << "UDP Server is running on port " << DEFAULT_PORT << "\n";
    cout << "Waiting for client requests...\n";

    const string SECRET_KEY = "MySecretKey123";

    while (true) {
        sockaddr_in clientAddr;
        int clientAddrLen = sizeof(clientAddr);
        char buffer[BUFFER_SIZE];

        int bytesReceived = recvfrom(serverSocket, buffer, BUFFER_SIZE, 0,
            (sockaddr*)&clientAddr, &clientAddrLen);

        if (bytesReceived == SOCKET_ERROR) {
            cerr << "recvfrom error: " << WSAGetLastError() << "\n";
            continue;
        }

        string filename(buffer, bytesReceived);
        cout << "Received request for file: " << filename << "\n";

        try {
            size_t dot_pos = filename.find_last_of(".");
            if (dot_pos == string::npos || filename.substr(dot_pos) != ".txt") {
                string error = "Error: Only .txt files are supported";
                sendto(serverSocket, error.c_str(), error.size(), 0,
                    (sockaddr*)&clientAddr, clientAddrLen);
                continue;
            }

            ifstream file(filename, ios::binary | ios::ate);
            if (!file.is_open()) {
                throw runtime_error("Cannot open file: " + filename);
            }

            streamsize size = file.tellg();
            file.seekg(0, ios::beg);

            vector<char> fileData(size);
            if (!file.read(fileData.data(), size)) {
                throw runtime_error("Failed to read file");
            }
            file.close();

            xor_encrypt(fileData, SECRET_KEY);

            uint32_t encryptedSize = static_cast<uint32_t>(fileData.size());
            uint32_t netSize = htonl(encryptedSize);
            sendto(serverSocket, (char*)&netSize, sizeof(netSize), 0,
                (sockaddr*)&clientAddr, clientAddrLen);

            sendto(serverSocket, fileData.data(), fileData.size(), 0,
                (sockaddr*)&clientAddr, clientAddrLen);

            cout << "File encrypted and sent successfully ("
                << size << " bytes -> " << encryptedSize << " encrypted bytes)\n";
        }
        catch (const exception& e) {
            string error = "Error: " + string(e.what());
            sendto(serverSocket, error.c_str(), error.size(), 0,
                (sockaddr*)&clientAddr, clientAddrLen);
        }
    }

    closesocket(serverSocket);
    WSACleanup();
    return 0;
}