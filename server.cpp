#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "utils.h"
#include "msg_transmission.h"

using namespace std;

#define HOST_IP     "127.0.0.1"

struct client_t {
    string id_client;
    bool active;
    int fd;

    client_t() {}
    client_t(string id) : id_client(id), active(true) {}
    client_t(string id, bool x, int fd) : id_client(id), active(x), fd(fd) {}
};

pair<string, uint8_t> decode_subscribe_msg(char *msg) {
    char cmd[MAX_CMD_LEN];
    char topic[BUF_LEN / 2];
    uint8_t sf;

    // The @msg containing the command will respect
    // the format and sscanf will be always successful,
    // because the @msg was formatted in the TCP client.
    sscanf(msg, "%s%s%hhd", cmd, topic, &sf);

    return {string(topic), sf};
}

string decode_unsubscribe_msg(char *msg) {
    char cmd[MAX_CMD_LEN];
    char topic[BUF_LEN / 2];

    // The @msg containing the command will respect
    // the format.
    sscanf(msg, "%s%s", cmd, topic);

    return string(topic);
}

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

    // map every message to the tcp clients to the messages that it has to receive
    // after reconnecting to the server
    unordered_map<message, vector<client_t>> queued_messages();

    // Map of id-s used by the clients that were
    // connected to the server at least once,
    // associated to their structure
    unordered_map<string, client_t> used_ids;

    // key:     string representing the topic name
    // value:   map, where the keys are the clients
    //          subscribed to a specific topic, and
    //          the value is 0/1, according to their
    //          sf status
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
    if (status < 0) {
        cerr << "Cannot deactivate the Nagle algorithm\n";
    }

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
    FD_SET(tcp_sock_fd, &read_fds);
    FD_SET(udp_sock_fd, &read_fds);
    FD_SET(STDIN_FILENO, &read_fds);
	fd_max = max(tcp_sock_fd, udp_sock_fd);

    while (true) {
        bool sig_exit = false;
        tmp_fds = read_fds;

        ret = select(fd_max + 1, &tmp_fds, NULL, NULL, NULL);
        DIE(ret < 0, "select() failed");

        // Iterate through file descriptors to see
        // which one received data
        for (int i = 0; i < fd_max + 1; ++i) {
            if (FD_ISSET(i, &tmp_fds)) {
                if (i == STDIN_FILENO) {
                    memset(buffer, 0, sizeof(buffer));
                    if (read(STDIN_FILENO, buffer, sizeof(buffer) - 1) < 0) {
                        cerr << "read() failed\n";
                    }

                    if (!strncmp(buffer, "exit", 4)) {
                        sig_exit = true;
                        break;
                    }
                } else if (i == udp_sock_fd) {  // message from a UDP client
                    memset(buffer, sizeof(buffer), 0);
                    ret = recvfrom(udp_sock_fd, buffer, sizeof(buffer) - 1, 0,
                                    (sockaddr *) &client_addr, &client_addr_size);

                    if (ret < 0) {
                        cerr << "Reading from UDP socket failed\n";
                        continue;
                    }

                    cout << "Received: " << buffer << '\n';

                    char topic[TOPIC_LEN + 1] = {0};
                    memcpy(topic, buffer, TOPIC_LEN);
                    uint8_t type;
                    memcpy(&type, buffer + TOPIC_LEN, 1);
                    cout << (int)type << '\n';

                    message received = message(client_addr.sin_addr.s_addr,
                                                client_addr.sin_port, type,
                                                topic);
                    cout << (int)received.type << '\n';

                    // Sent the message to the clients that are
                    // subscribed to the topic
                    for (auto &it : topics[topic]) {
                        if (used_ids[it.first].active == true) {
                            // Send the message to the client
                            ret = send_message(used_ids[it.first].fd, (char *) &received, sizeof(received));
                            cout << ret<< '\n';

                            if (ret < 0) {
                                cerr << "Sending message to client failed\n";
                            }
                        } else {
                            // Check if store & forward is activated
                            if (it.second == 1) {

                            }
                        }
                    }
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
                    ret = receive_message(new_sock_fd, buffer);
                    if (ret < 0) {
                        cerr << "Skipped new client, since the ID "
                                "wasn't received\n";
                        continue;
                    }

                    string client_id(buffer);

                    // Map the client to the file descriptor associated,
                    // only if the id is not currently used
                    if (used_ids.find(client_id) == used_ids.end() || used_ids[client_id].active == false) {
                        send_message(new_sock_fd, ACC, -1);

                        // Check if the client was ever connected to the server,
                        // and if not create a client_t structure
                        if (used_ids.find(client_id) == used_ids.end()) {
                            cout << "New client " << client_id << " connected from "
                                 << inet_ntoa(client_addr.sin_addr) << ":"
                                 << client_addr.sin_port << '\n';

                            used_ids[client_id] = client_t(client_id, true, new_sock_fd);
                        } else {
                            used_ids[client_id].active = true;
                            used_ids[client_id].fd = new_sock_fd;
                        }
                        
                        clients[new_sock_fd] = client_id;
                    } else {
                        cout << "Client " << client_id << " already connected.\n";
                        send_message(new_sock_fd, REJ, -1);

                        FD_CLR(new_sock_fd, &read_fds);
                        close(new_sock_fd);
                    }
                } else {                        // message from a TCP client
                    memset(buffer, 0, BUF_LEN);
                    ret = receive_message(i, buffer);

                    if (ret < 0) {
                        cerr << "Receiving data from TCP client failed\n";
                        continue;
                    }
                    
                    if (ret == 0) {
                        cout << "Client disconnected.\n";
                        FD_CLR(i, &read_fds);

                        used_ids[clients[i]].active = false;
                        clients.erase(i);
                        close(i);
                    } 
                    
                    if (ret != 0) {
                        if (!strncmp(buffer, "subscribe", 9)) {
                            pair<string, uint8_t> args = decode_subscribe_msg(buffer);

                            // Associate the new topic to the client
                            topics[args.first][clients[i]] = args.second;
                        } else if (!strncmp(buffer, "unsubscribe", 11)) {
                            string topic = decode_unsubscribe_msg(buffer);

                            // Delete the client from the topic's subscribers
                            topics[topic].erase(clients[i]);
                        }
                    }
                }
            }
        }

        if (sig_exit) {
            break;
        }
    }

    close(tcp_sock_fd);
    close(udp_sock_fd);

    return 0;
}
