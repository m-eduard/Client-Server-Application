#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <unordered_map>
#include <queue>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

#include <unistd.h>

#include "utils.h"
#include "msg_transmission.h"
#include "msg_parsing.h"

using namespace std;

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <PORT>\n";
        return ERR;
    }

    int ret;
    int udp_sock_fd, tcp_sock_fd, new_sock_fd;
    char buffer[BUF_LEN];
    sockaddr_in serv_addr;
    sockaddr_in client_addr;
    socklen_t client_addr_size;

    fd_set read_fds;
    fd_set tmp_fds;
    int fd_max;

    // key:     socket file descriptor associated to client
    // value:   client ID
    unordered_map<int, string> clients;

    // key:     client ID
    // value:   messages a client has to receive
    //          after reconnecting to the server
    unordered_map<string, queue<message_t>> queued_messages;

    // key:     client ID  (for all the clients that were
    //                      connected at least once)
    // value:   client_t struct containing data about a client
    unordered_map<string, client_t> used_ids;

    // key:     topic name
    // value:   map, where the keys are the clients
    //          subscribed to a specific topic, and
    //          the value is 0/1, according to their
    //          SF status
    unordered_map<string, unordered_map<string, bool>> topics;

    // Open 2 sockets (one for UDP transmissions,
    // and one for the TCP connections)
    udp_sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(udp_sock_fd < 0, "Cannot create an UDP socket");
    tcp_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(tcp_sock_fd < 0, "Cannot create a TCP socket");

    // Deactivate the Nagle algorithm
    int flag = 1;
    int status = setsockopt(tcp_sock_fd, IPPROTO_TCP, TCP_NODELAY,
                            (char *) &flag, sizeof(flag));
    if (status < 0)
        cerr << "Cannot deactivate the Nagle algorithm\n";

    // Check if the provided port is a valid int
    DIE(atoi(argv[1]) == 0, "atoi() failed");

    // Fill server info
    serv_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(atoi(argv[1])),
        .sin_addr = {inet_addr(HOST_IP)}
    };

    // Bind the sockets to a specified port
    ret = bind(udp_sock_fd, (sockaddr *) &serv_addr, sizeof(serv_addr));
    DIE(ret < 0, "bind() failed");
    ret = bind(tcp_sock_fd, (sockaddr *) &serv_addr, sizeof(serv_addr));
    DIE(ret < 0, "bind() failed");

    // This socket is passive and it will listen to new connections
    ret = listen(tcp_sock_fd, MAX_CLIENTS);
    DIE(ret < 0, "listen() failed (maybe max number of "
                 "simultaneous connect requests was exceeded)");

    // Initialize the sets of file descriptors
    // (current fd-s: tcp_sock_fd and stdin fd)
    FD_ZERO(&read_fds);
    FD_ZERO(&tmp_fds);
    FD_SET(STDIN_FILENO, &read_fds);
    FD_SET(udp_sock_fd, &read_fds);
    FD_SET(tcp_sock_fd, &read_fds);
    
	fd_max = max(tcp_sock_fd, udp_sock_fd);

    while (true) {
        bool sig_exit = false;
        tmp_fds = read_fds;

        ret = select(fd_max + 1, &tmp_fds, NULL, NULL, NULL);
        DIE(ret < 0, "select() failed");

        // Iterate through file descriptors to see
        // which one received data
        for (int i = 0; i <= fd_max; ++i) {
            if (FD_ISSET(i, &tmp_fds)) {
                memset(buffer, 0, sizeof(buffer));

                if (i == STDIN_FILENO) {        // message from stdin
                    ret = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
                    DIE(ret < 0, "read() failed");

                    if (!strncmp(buffer, "exit", 4)) {
                        sig_exit = true;
                        break;
                    }
                } else if (i == udp_sock_fd) {  // message from a UDP client
                    ret = recvfrom(udp_sock_fd, buffer, sizeof(buffer) - 1, 0,
                                (sockaddr *) &client_addr, &client_addr_size);
                    DIE(ret < 0, "Reading from UDP socket failed");

                    // Get the content of the UDP message
                    char topic[TOPIC_LEN + 1] = {0};
                    memcpy(topic, buffer, TOPIC_LEN);
                    uint8_t type;
                    memcpy(&type, buffer + TOPIC_LEN, 1);
                    char data[MAX_DATA_LEN + 1] = {0};  // data + ending '\0'
                    memcpy(data, buffer + TOPIC_LEN + 1,
                            ret - (TOPIC_LEN + 1));

                    // Build the message that will be sent over TCP
                    message_t udp_msg = message_t(client_addr.sin_addr.s_addr,
                                                    client_addr.sin_port, type,
                                                    topic, data);

                    // Send the message to the clients that are
                    // subscribed to the topic
                    for (auto &it : topics[topic]) {
                        if (used_ids[it.first].active == true) {
                            if (udp_msg.type != STRING) {
                                // Cast the struct containing the message
                                // to (char *) and send only the structure
                                // to the TCP client
                                ret = send_message(used_ids[it.first].fd,
                                        (char *) &udp_msg, sizeof(udp_msg));
                            } else {
                                int new_len = format_udp_msg(&udp_msg, buffer);
                                ret = send_message(used_ids[it.first].fd,
                                                    buffer, new_len);
                            }

                            DIE(ret < 0, "send_message() failed");
                        } else {
                            // Check if store & forward is activated
                            if (it.second == 1)
                                queued_messages[it.first].push(udp_msg);
                        }
                    }
                } else if (i == tcp_sock_fd) {  // connection request
                    client_addr_size = sizeof(client_addr);
                    new_sock_fd = accept(tcp_sock_fd,
                                        (sockaddr *) &client_addr,
                                        &client_addr_size);
                    DIE(new_sock_fd < 0, "accept() failed");

                    // Add the new file descriptor to the used fd-s set
                    FD_SET(new_sock_fd, &read_fds);
                    fd_max = max(fd_max, new_sock_fd);

                    // Get the client ID
                    ret = receive_message(new_sock_fd, buffer);
                    DIE(ret < 0, "receive_message() failed");

                    string client_id(buffer);

                    // Map the client to the file descriptor associated,
                    // only if the id is not currently used
                    if (used_ids[client_id].active == false) {
                        send_message(new_sock_fd, ACC, -1);

                        cout << "New client " << client_id << " connected from"
                             << " " << inet_ntoa(client_addr.sin_addr) << ":"
                             << client_addr.sin_port << '\n';

                        // Check if the client was ever connected to the
                        // server, and if not create a client_t structure
                        if (used_ids.find(client_id) == used_ids.end()) {
                            used_ids[client_id] = client_t(client_id, true,
                                                            new_sock_fd);
                        } else {
                            used_ids[client_id].active = true;
                            used_ids[client_id].fd = new_sock_fd;

                            // Forward the messages that were queued
                            // for this client, while it was disconnected
                            queue<message_t> to_forward =
                                    queued_messages[client_id];

                            while (!to_forward.empty()) {
                                message_t current = to_forward.front();
                                to_forward.pop();

                                int new_len = format_udp_msg(&current, buffer);
                                ret = send_message(used_ids[client_id].fd,
                                                    buffer, new_len);
                                DIE(ret < 0, "send_message() failed");
                            }
                        }
                        
                        clients[new_sock_fd] = client_id;
                    } else {
                        send_message(new_sock_fd, REJ, -1);

                        cout << "Client " << client_id << " already connected."
                                "\n";

                        FD_CLR(new_sock_fd, &read_fds);
                        close(new_sock_fd);
                    }
                } else {                        // message from a TCP client
                    ret = receive_message(i, buffer);
                    DIE(ret < 0, "Receiving data from TCP client failed");

                    if (ret == 0) {
                        cout << "Client " << clients[i] << " disconnected.\n";
                        FD_CLR(i, &read_fds);

                        used_ids[clients[i]].active = false;
                        clients.erase(i);
                        close(i);

                        continue;
                    } 

                    bool valid_cmd = false;
                    if (!strncmp(buffer, "subscribe ", 10)) {
                        pair<string, uint8_t> args =
                            parse_subscribe_msg(buffer);

                        if (args.first.size() > 0) {
                            // Associate the new topic to the client
                            topics[args.first][clients[i]] = args.second;

                            valid_cmd = true;
                        }
                    } else if (!strncmp(buffer, "unsubscribe ", 12)) {
                        string topic = parse_unsubscribe_msg(buffer);

                        if (topic.size() > 0) {
                            // Delete the client from the topic's subscribers
                            topics[topic].erase(clients[i]);

                            valid_cmd = true;
                        }
                    }

                    // Send to the client the command execution status
                    if (valid_cmd && !strncmp(buffer, "subscribe ", 10))
                        ret = send_message(i, SUCC_SUBSCRIBE, -1);
                    else if (valid_cmd && !strncmp(buffer, "unsubscribe ", 12))
                        ret = send_message(i, SUCC_UNSUBSCRIBE, -1);
                    else
                        ret = send_message(i, FAIL, -1);

                    DIE(ret < 0, "send_message() failed");
                }
            }
        }

        if (sig_exit)
            break;
    }

    for (int i = 0; i <= fd_max; ++i)
        if (FD_ISSET(i, &read_fds))
            close(i);

    return 0;
}
