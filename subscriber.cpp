#include <iostream>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>

#include "utils.h"

using namespace std;

// 4 bytes of size, then the message itselfs
struct message {
    uint32_t size;
    char payload[1500];
};

// Function that sends a given payload to a server
// (connection to the server should be established
//  before calling this function)
bool send_message(int sock_fd, const char *message) {
    bool status = true;
    int ret;

    char buffer[MAX_LEN];
    uint32_t message_len = strlen(message) + 1;

    memcpy(buffer, &message_len, MSG_LEN_SIZE);
    snprintf(buffer + MSG_LEN_SIZE, strlen(message) + 1, "%s", message);

    ret = send(sock_fd, buffer, MSG_LEN_SIZE + message_len, 0);
    if (ret < 0) {
        status = false;
    }
    
    return status;
}

// Message format: "subscribe <TOPIC> <SF>"
string parse_subscribe_msg(char *msg) {    
    char cmd[MAX_CMD_LEN];
    char topic[MAX_LEN / 2];
    int sf;

    char parsed_msg[MAX_LEN] = {0};

    if (sscanf(msg, "%s%s%d", cmd, topic, &sf) != 3) {
        cerr << "Subscribe format: \"subscribe <TOPIC> <SF>\"\n";
    } else {
        sprintf(parsed_msg, "%s %s %d", cmd, topic, sf);
    }

    return string(parsed_msg);
}

// Message format: "unsubscribe <TOPIC>"
string parse_unsubscribe_msg(char *msg) {
    char cmd[MAX_CMD_LEN];
    char topic[MAX_LEN / 2];

    char parsed_msg[MAX_LEN] = {0};

    if (sscanf(msg, "%s%s", cmd, topic) != 2) {
        cerr << "Unsubscribe format: \"unsubscribe <TOPIC>\"\n";
    } else {
        sprintf(parsed_msg, "%s %s", cmd, topic);
    }

    return string(parsed_msg);
}

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    if (argc < 4) {
        cerr << "Usage: " << argv[0] << " <ID_CLIENT> <IP_SERVER>"
                                        " <PORT_SERVER>\n";
        return ERR;
    }

    char *client_id = argv[1];

    int sock_fd;
    sockaddr_in serv_addr;
    char buffer[MAX_LEN];

    fd_set read_fds;
    fd_set tmp_fds;
    int fd_max;

    // protocolul meu:
    // creez din server si trimit chunk-uri de
    // 64 de bytes, iar pentru a finaliza un mesaj
    // trimit o fereastra de 64 de bytes formati
    // doar din biti de 1
    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        cerr << "Cannot create a TCP socket\n";
        return ERR;
    }

    // Deactivate the Nagle algorithm
    int flag = 1;
    int status = setsockopt(sock_fd, IPPROTO_TCP, TCP_NODELAY,
                            (char *) &flag, sizeof(flag));
    if (status < 0) {
        cerr << "Cannot deactivate the Nagle algorithm\n";
    }

    // Check if the provided port is a valid int
    if (atoi(argv[3]) == 0) {
        cerr << "atoi() failed\n";
        return ERR;
    }

    // Check the IP address
    if (inet_addr(argv[2]) == 0) {
        cerr << argv[2] << " is not a valid IP address\n";
        return ERR;
    }

    // Fill server info
    serv_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(atoi(argv[3])),
        .sin_addr = {inet_addr(argv[2])}
    };

    // Connect to the server
    if (connect(sock_fd, (sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        cerr << "Connection to the server at <" << argv[2] << "> failed\n";
        return ERR;
    }

    // Initialize the sets of file descriptors
    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);
    FD_SET(sock_fd, &read_fds);
    FD_SET(STDIN_FILENO, &read_fds);
    fd_max = sock_fd;

    // Send the client identification info to the server
    if (!send_message(sock_fd, client_id)) {
        cerr << "Sending client ID failed\n";
        return ERR;
    }

    while (true) {
        tmp_fds = read_fds;

        if (select(fd_max + 1, &tmp_fds, NULL, NULL, NULL) < 0) {
            cerr << "select() failed\n";
            return ERR;
        }

        // Either read from stdin (data provided by user),
        // or from socket (data provided by server)
        if (FD_ISSET(STDIN_FILENO, &tmp_fds)) {
            // fgets(buffer, MAX_LEN, stdin);

            memset(buffer, 0, sizeof(buffer));
            if (read(STDIN_FILENO, buffer, sizeof(buffer) - 1) < 0) {
                cerr << "read() failed\n";
            }

            if (!strncmp(buffer, "exit", 4)) {
                break;
            } else if (!strncmp(buffer, "subscribe ", 10)) {
                string msg = parse_subscribe_msg(buffer);

                if (msg.size() > 0) {
                    if (send_message(sock_fd, msg.c_str()) < 0) {
                        cerr << "send() failed in subscribe transmission\n";
                    } else {
                        cout << "Subscribed to topic.\n";
                    }
                }
            } else if (!strncmp(buffer, "unsubscribe ", 12)) {
                string msg = parse_unsubscribe_msg(buffer);

                if (msg.size() > 0) {
                    if (send_message(sock_fd, msg.c_str()) < 0) {
                        cerr << "send() failed in unsubscribe transmission\n";
                    } else {
                        cout << "Unsubscribed from topic.\n";
                    }
                }
            }
        }
    }

    close(sock_fd);

    return 0;
}
