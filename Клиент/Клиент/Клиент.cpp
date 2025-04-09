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
const string SECRET_KEY = "MySecretKey123";

void xor_decrypt(vector<char>& data) {
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] ^= SECRET_KEY[i % SECRET_KEY.size()];
    }
}

int main(int argc, char* argv[]) {
    setlocale(LC_ALL, "Russian");
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <server_ip> <filename.txt>\n";
        return 1;
    }

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "WSAStartup failed\n";
        return 1;
    }

    SOCKET clientSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (clientSocket == INVALID_SOCKET) {
        cerr << "Socket creation failed: " << WSAGetLastError() << "\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(DEFAULT_PORT);
    inet_pton(AF_INET, argv[1], &serverAddr.sin_addr);

    string filename(argv[2]);
    if (sendto(clientSocket, filename.c_str(), filename.size(), 0,
        (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cerr << "sendto failed: " << WSAGetLastError() << "\n";
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    uint32_t encryptedSize;
    int bytesReceived = recvfrom(clientSocket, (char*)&encryptedSize, sizeof(encryptedSize), 0, nullptr, nullptr);
    if (bytesReceived != sizeof(encryptedSize)) {
        cerr << "Failed to receive file size\n";
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }
    encryptedSize = ntohl(encryptedSize);

    vector<char> encryptedData(encryptedSize);
    size_t totalReceived = 0;
    while (totalReceived < encryptedSize) {
        char buffer[BUFFER_SIZE];
        int chunkSize = recvfrom(clientSocket, buffer, BUFFER_SIZE, 0, nullptr, nullptr);
        if (chunkSize <= 0) break;
        copy(buffer, buffer + chunkSize, encryptedData.begin() + totalReceived);
        totalReceived += chunkSize;
    }

    size_t dot_pos = filename.find_last_of(".");
    string base_name = (dot_pos == string::npos) ? filename : filename.substr(0, dot_pos);

    string encryptedFilename = base_name + "_encrypted.txt";
    ofstream encryptedFile(encryptedFilename, ios::binary);
    encryptedFile.write(encryptedData.data(), encryptedData.size());
    encryptedFile.close();
    cout << "Encrypted file saved as: " << encryptedFilename << "\n";

    xor_decrypt(encryptedData);
    string decryptedFilename = base_name + "_decrypted.txt";
    ofstream decryptedFile(decryptedFilename, ios::binary);
    decryptedFile.write(encryptedData.data(), encryptedData.size());
    decryptedFile.close();
    cout << "Decrypted file saved as: " << decryptedFilename << "\n";

    closesocket(clientSocket);
    WSACleanup();
    return 0;
}