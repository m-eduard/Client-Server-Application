#include <iostream>
#include <vector>
#include <string>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "utils.h"

using namespace std;

#define MAX_CLIENTS 1000
#define HOST_IP     "127.0.0.1"

void run_server(int tcp_sock, int udp_sock) {
    long long bits_received = 0;

    // int status = select(100000, read, write, NULL, NULL);
}

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <PORT>\n";
        return ERR;
    }

    int udp_sock_fd, tcp_sock_fd, new_sock_fd;
    string buffer;
    sockaddr_in serv_addr;

    fd_set read_fds;
    fd_set tmp_fds;
    int fd_max;

    // Open 2 sockets (one for UDP transmissions,
    // and one for the TCP connections)
    if ((udp_sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        cerr << "Cannot create an UDP socket\n";
        return ERR;
    }

    if ((tcp_sock_fd = socket(AF_INET, SOCK_STREAM, 0))< 0) {
        cerr << "Cannot create a TCP socket\n";
        return ERR;
    }

    // Deactivate the Nagle algorithm
    int flag = 1;
    int status = setsockopt(tcp_sock_fd, IPPROTO_TCP, TCP_NODELAY,
                            (char *) &flag, sizeof(flag));
    if (status < 0) {
        cerr << "Cannot deactivate the Nagle algorithm\n";
    }

    // Check if the provided port is a valid int
    if (atoi(argv[1]) == 0) {
        cerr << "atoi() failed\n";
        return ERR;
    }

    // Fill server info
    serv_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(atoi(argv[1])),
        .sin_addr = {inet_addr(HOST_IP)}
    };

    // Bind the sockets to a specified port
    if (bind(udp_sock_fd, (sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        cerr << "The port " << argv[1] << " is not available\n";
        return ERR;
    }

    if (bind(tcp_sock_fd, (sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        cerr << "The port " << argv[1] << " is not available\n";
        return ERR;
    }

    // This socket is passive and it will listen to new connections
    if (listen(tcp_sock_fd, MAX_CLIENTS) < 0) {
        cerr << "listen() failed (maybe max number of "
                "simultaneous connect requests was exceeded)\n";
        return ERR;
    }

    // Initialize the sets of file descriptors
    // (current fd-s: tcp_sock_fd and stdin fd)
    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);
    FD_SET(tcp_sock_fd, &read_fds);
    FD_SET(STDIN_FILENO, &read_fds);
	fd_max = tcp_sock_fd;

    while (true) {
        tmp_fds = read_fds;

        if (select(fd_max + 1, &tmp_fds, NULL, NULL, NULL) < 0) {
            cerr << "select() failed\n";
            return ERR;
        }

        // Iterate through file descriptors to see
        // which one received data
        for (int i = 0; i < fd_max + 1; ++i) {
            if (FD_ISSET(i, &tmp_fds)) {
                if (i == STDIN_FILENO) {

                } else if (i == udp_sock_fd) {  // message from a UDP client

                } else if (i == tcp_sock_fd) {  // connection request

                } else {                        // message from a TCP client
                    
                }
            }
        }
    }

    close(tcp_sock_fd);
    close(udp_sock_fd);

    return 0;
}
