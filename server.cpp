#include <iostream>
#include <vector>

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace std;

// typedef struct sockaddr_in  sockaddr_in;
// typedef struct sockaddr     sockaddr;

#define HOST_IP     "127.0.0.1"

int main(int argc, char *argv[]) {
    // Open 2 sockets (one for UDP, and one for TCP transmissions)
    int tcp_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_sock < 0) {
        cerr << "Cannot create a TCP socket\n";
        return -1;
    }

    int udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sock < 0) {
        cerr << "Cannot create an UDP socket\n";
        return -1;
    }

    // Bind the sockets to a specified port
    sockaddr_in serv_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(atoi(argv[1])),
        .sin_addr = {inet_addr(HOST_IP)}
    };

    if (bind(tcp_sock, (sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        cerr << "The port " << argv[1] << " is not available\n";
        return -1;
    }

    if (bind(udp_sock, (sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        cerr << "The port " << argv[1] << " is not available\n";
        return -1;
    }

    close(tcp_sock);
    close(udp_sock);

    return 0;
}
