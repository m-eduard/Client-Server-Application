#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <stdlib.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "utils.h"
#include "msg_transmission.h"
#include "msg_parsing.h"

using namespace std;

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    if (argc < 4) {
        cerr << "Usage: " << argv[0] << " <ID_CLIENT> <IP_SERVER>"
                                        " <PORT_SERVER>\n";
        return ERR;
    }

    char *client_id = argv[1];
    DIE(strlen(client_id) > MAX_ID_LEN, "Too long id");

    int ret;
    int sock_fd;
    sockaddr_in serv_addr;
    char buffer[BUF_LEN];

    fd_set read_fds;
    fd_set tmp_fds;
    int fd_max;

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(sock_fd < 0, "Cannot create a TCP socket");

    // Deactivate the Nagle algorithm
    int flag = 1;
    int status = setsockopt(sock_fd, IPPROTO_TCP, TCP_NODELAY,
                            (char *) &flag, sizeof(flag));

    if (status < 0)
        cerr << "Cannot deactivate the Nagle algorithm\n";

    // Check if the provided port is a valid int
    DIE(atoi(argv[3]) == 0, "Invalid port");

    // Check the IP address
    DIE(inet_addr(argv[2]) == 0, "Invalid IP address");

    // Fill server info
    serv_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(atoi(argv[3])),
        .sin_addr = {inet_addr(argv[2])}
    };

    // Connect to the server
    ret = connect(sock_fd, (sockaddr *) &serv_addr, sizeof(serv_addr));
    DIE(ret < 0, "Connection to the server failed");

    // Initialize the sets of file descriptors
    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);
    FD_SET(STDIN_FILENO, &read_fds);
    FD_SET(sock_fd, &read_fds);
    fd_max = sock_fd;

    // Send the client identification info to the server
    ret = send_message(sock_fd, client_id, -1);
    DIE(ret < 0, "Sending client ID failed");

    // Receive the server response (accept or reject)
    ret = receive_message(sock_fd, buffer);
    DIE(ret < 0, "Receiving server confirmation failed");

    // Check if the server accepted the connection request
    // (if another client with the same id is already
    // connected, the server will yield a REJ message)
    if (strcmp(buffer, ACC)) {
        close(sock_fd);
        return 0;
    }

    while (true) {
        tmp_fds = read_fds;

        ret = select(fd_max + 1, &tmp_fds, NULL, NULL, NULL);
        DIE(ret < 0, "select() failed");

        if (FD_ISSET(STDIN_FILENO, &tmp_fds)) { // data from stdin
            memset(buffer, 0, sizeof(buffer));
            ret = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
            DIE(ret < 0, "read() failed");

            if (!strncmp(buffer, "exit", 4))
                break;

            // Check if the command is valid and send it to the
            bool valid = false;
            if (!strncmp(buffer, "subscribe ", 10)) {
                pair<string, uint8_t> args =
                    parse_subscribe_msg(buffer);

                if (args.first.size() > 0) {
                    ret = send_message(sock_fd, buffer, -1);
                    DIE(ret < 0, "Sending cmd to server failed");
                    valid = true;
                }

                cout << "Subscribed to topic.\n";
            } else if (!strncmp(buffer, "unsubscribe ", 12)) {
                string topic = parse_unsubscribe_msg(buffer);

                if (topic.size() > 0) {
                    ret = send_message(sock_fd, buffer, -1);
                    DIE(ret < 0, "Sending cmd to server failed");
                    valid = true;
                }

                cout << "Unsubscribed from topic.\n";
            }

            if (!valid)
                cerr << "Wrong format of command\n"
                     << "Subscribe format: \"subscribe <TOPIC> <SF>\"\n"
                     << "Unsubscribe format: \"unsubscribe <TOPIC>\"\n";
        }
        
        if (FD_ISSET(sock_fd, &tmp_fds)) {  // data provided by server
            memset(buffer, 0, BUF_LEN);
            ret = receive_message(sock_fd, buffer);
            DIE(ret < 0, "Receiving server message failed");

            // Connection was closed
            if (ret == 0) {
                cerr << "Server is down\n";
                break;
            }

            show_server_msg(buffer);
        }
    }

    close(sock_fd);

    return 0;
}
