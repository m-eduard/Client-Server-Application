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

    client_t() {}
    client_t(string id) : id_client(id), active(true) {}
    client_t(string id, bool x) : id_client(id), active(x) {}
};


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
    // value:   data about the client
    unordered_map<int, client_t> clients;

    // map every message to the tcp clients to the messages that it has to receive
    // after reconnecting to the server
    unordered_map<message, vector<client_t>> queued_messages();

    // Set of id-s used by the clients that were
    // connected to the server at least once
    unordered_set<string> used_clients_ids;

    // key:     string representing the topic name
    // value:   2 lists of file descriptors associated to clients
    //          that are subscribed to the given topic (one for
    //          the clients with sf=0 on that topic, and one for
    //          those with sf=1)
    // unordered_map<string, pair<vector<int>, vector<int>>> topics;
    // unordered_map<string, >


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
    FD_SET(STDIN_FILENO, &read_fds);
	fd_max = tcp_sock_fd;

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
                    if (connected_clients_id.find(client_id)
                        == connected_clients_id.end())
                    {
                        cout << "New client " << client_id << " connected from "
                             << inet_ntoa(client_addr.sin_addr) << ":"
                             << client_addr.sin_port << '\n';
                        send_message(new_sock_fd, ACC);

                        connected_clients_id.insert(client_id);
                        clients[new_sock_fd] = client_t(client_id);
                    } else {
                        cout << "Client " << client_id << " already connected.\n";
                        send_message(new_sock_fd, REJ);
                    }
                } else {                        // message from a TCP client
                    memset(buffer, 0, BUF_LEN);

                    ret = receive_message(i, buffer);
                    cout << "Ret is: " << ret << '\n';
                    if (ret < 0) {
                        cerr << "Receiving data from TCP client failed\n";
                        continue;
                    }
                    
                    if (ret == 0) {
                        clients.erase(i);
                        cout << "Client disconnected";
                        FD_CLR(i, &read_fds);
                    } else {
                        ret = receive_message(i, buffer);

                        if (ret < 0) {
                            cerr << "receive_message() failed\n";
                            continue;
                        }

                        cout << buffer << '\n';;

                        // cout << parse_server_msg(buffer);
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
