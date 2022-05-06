#include <iostream>
#include <string>
#include <algorithm>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "utils.h"
#include "msg_transmission.h"

using namespace std;

// // 4 bytes of size, then the message itselfs
// struct message {
//     uint32_t size;
//     char payload[1500];
// };

// Message format: "subscribe <TOPIC> <SF>"
string parse_subscribe_cmd(char *msg) {    
    char cmd[MAX_CMD_LEN];
    char topic[BUF_LEN / 2];
    uint8_t sf;

    char parsed_msg[BUF_LEN] = {0};

    if (sscanf(msg, "%s%s%hhd", cmd, topic, &sf) != 3) {
        cerr << "Subscribe format: \"subscribe <TOPIC> <SF>\"\n";
    } else {
        sprintf(parsed_msg, "%s %s %hhd", cmd, topic, sf);
    }

    return string(parsed_msg);
}

// Message format: "unsubscribe <TOPIC>"
string parse_unsubscribe_cmd(char *msg) {
    char cmd[MAX_CMD_LEN];
    char topic[BUF_LEN / 2];

    char parsed_msg[BUF_LEN] = {0};

    if (sscanf(msg, "%s%s", cmd, topic) != 2) {
        cerr << "Unsubscribe format: \"unsubscribe <TOPIC>\"\n";
    } else {
        sprintf(parsed_msg, "%s %s", cmd, topic);
    }

    return string(parsed_msg);
}

// Function that prints the message reeceived
// from the server, in the desired format
void show_server_msg(char *buffer) {
    message_t *msg = (message_t *)buffer;

    printf("%s:%d - %s", inet_ntoa({msg->ip}), ntohs(msg->port), msg->topic);

    switch (msg->type) {
        case INT:
            printf(" - %s - %s%d %d", stringify(INT),
                        sign(msg->data.integer_t.sign),
                        ntohl(msg->data.integer_t.num),
                        msg->data.integer_t.sign);
            break;
        case SHORT_REAL:
            printf(" - %s - %d", stringify(SHORT_REAL),
                        ntohs(msg->data.short_real_t) / 100);
            
            // Check fractional part existence
            if (ntohs(msg->data.short_real_t) % 100 != 0)
                printf(".%02d", ntohs(msg->data.short_real_t) % 100);

            break;
        case FLOAT: {
            string num;
            uint32_t current = ntohl(msg->data.float_t.decimal);
            int8_t cnt = msg->data.float_t.fractional;

            // Create the float number as string
            while (current) {
                num.push_back('0' + current % 10);
                cnt -= 1;

                if (cnt == 0)
                    num.push_back('.');

                current /=  10;
            }

            while (cnt > 0) {
                num.push_back('0');
                cnt -= 1;

                if (cnt == 0) {
                    num.push_back('.');
                    num.push_back('0');
                }
            }

            reverse(num.begin(), num.end());

            printf(" - %s - %s%s", stringify(FLOAT),
                                   sign(msg->data.float_t.sign),
                                   num.c_str());
            break;}
        case STRING:
            printf(" - %s - %s", stringify(STRING), buffer + sizeof(*msg));
    }

    printf("\n");
}

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
    if (status < 0) {
        cerr << "Cannot deactivate the Nagle algorithm\n";
    }

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
    FD_SET(sock_fd, &read_fds);
    FD_SET(STDIN_FILENO, &read_fds);
    fd_max = sock_fd;

    // Send the client identification info to the server
    ret = send_message(sock_fd, client_id, -1);
    DIE(ret < 0, "Sending client ID failed");

    // Receive the server response (accept or reject)
    ret = receive_message(sock_fd, buffer);
    DIE(ret < 0, "Receiving server confirmation failed");

    if (strcmp(buffer, ACC)) {
        close(sock_fd);
        return 0;
    }

    while (true) {
        tmp_fds = read_fds;

        ret = select(fd_max + 1, &tmp_fds, NULL, NULL, NULL);
        DIE(ret < 0, "select() failed");

        // Read from stdin (data provided by user),
        // or from socket (data provided by server)
        if (FD_ISSET(STDIN_FILENO, &tmp_fds)) {
            memset(buffer, 0, sizeof(buffer));
            ret = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
            
            if (ret < 0) {
                cerr << "read() failed\n";
            }

            if (!strncmp(buffer, "exit", 4)) {
                if (ret < 0) {
                    cerr << "Sending end request to the server failed\n";
                }

                break;
            } else if (!strncmp(buffer, "subscribe ", 10)) {
                string msg = parse_subscribe_cmd(buffer);

                if (msg.size() > 0) {
                    ret = send_message(sock_fd, msg.c_str(), -1);

                    if (ret < 0) {
                        cerr << "Sending subscribe cmd to the server failed\n";
                    } else {
                        cout << "Subscribed to topic.\n";
                    }
                }
            } else if (!strncmp(buffer, "unsubscribe ", 12)) {
                string msg = parse_unsubscribe_cmd(buffer);

                if (msg.size() > 0) {
                    ret = send_message(sock_fd, msg.c_str(), -1);

                    if (ret < 0) {
                        cerr << "Sending unsubscribe cmd to the server failed";
                        cerr << '\n';
                    } else {
                        cout << "Unsubscribed from topic.\n";
                    }
                }
            }
        }
        
        if (FD_ISSET(sock_fd, &tmp_fds)) {
            memset(buffer, 0, BUF_LEN);
            ret = receive_message(sock_fd, buffer);
            DIE(ret < 0, "Receiving server message failed");
            
            if (ret == 0) {
                // Connection was closed
                cout << "Server is down\n";
                break;
            }

            show_server_msg(buffer);
        }
    }

    close(sock_fd);

    return 0;
}
