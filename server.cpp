#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "utils.h"

using namespace std;

#define MAX_CLIENTS 1000
#define HOST_IP     "127.0.0.1"

struct client_t {
    int name;
};

void run_server(int tcp_sock, int udp_sock) {
    long long bits_received = 0;

    // int status = select(100000, read, write, NULL, NULL);
}


bool receive_msg(int sock_fd, char *message) {
    bool status = true;

    char buffer[MAX_LEN] = {0};
    int sz = recv(sock_fd, buffer, sizeof(buffer) - 1, 0);

    if (sz == 0) {
        status = false;
    } else {
        uint32_t msg_len;
        memcpy(&msg_len, buffer, MSG_LEN_SIZE);

        memcpy(message, buffer + MSG_LEN_SIZE, msg_len);
    }
    
    return status;
}

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <PORT>\n";
        return ERR;
    }

    int udp_sock_fd, tcp_sock_fd, new_sock_fd;
    char buffer[MAX_LEN];
    sockaddr_in serv_addr;
    sockaddr_in client_addr;
    socklen_t client_addr_size;

    fd_set read_fds;
    fd_set tmp_fds;
    int fd_max;

    unordered_map<int, client_t> clients;

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
                    client_addr_size = sizeof(client_addr);
                    new_sock_fd = accept(tcp_sock_fd,
                                    (sockaddr *) &client_addr, &client_addr_size);

                    if (new_sock_fd < 0) {
                        cerr << "accept() failed\n";
                        continue;
                    }

                    // Add the new file descriptor to the used fd-s set
                    FD_SET(new_sock_fd, &read_fds);
                    fd_max = max(fd_max, new_sock_fd);

                    // Get the client ID
                    if (!receive_msg(new_sock_fd, buffer)) {
                        cerr << "skipped new client, since "
                                "ID wasn't received)\n";
                        continue;
                    }

                    // Map the client to the file descriptor associated
                    clients[new_sock_fd] = {1};

                    cout << "New client " << buffer << " connected from " << inet_ntoa(client_addr.sin_addr) << ":" << client_addr.sin_port << '\n';
                } else {                        // message from a TCP client
                    
                }
            }
        }
    }

    close(tcp_sock_fd);
    close(udp_sock_fd);

    return 0;
}
