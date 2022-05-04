#include <iostream>
#include <string.h>
#include <netinet/in.h>

#include "utils.h"


// Function that sends a given payload to a server
// (connection to the server should be established
//  before calling this function)
int send_message(int sock_fd, const char *message) {
    char buffer[BUF_LEN];
    uint32_t message_len = strlen(message) + 1;

    memcpy(buffer, &message_len, MSG_LEN_SIZE);
    snprintf(buffer + MSG_LEN_SIZE, message_len, "%s", message);

    uint32_t current_sent = 0;
    uint32_t total_len = MSG_LEN_SIZE + message_len;
    while (current_sent < message_len) {
        int ret = send(sock_fd, buffer + current_sent,
                        total_len - current_sent, 0);

        if (ret < 0) {
            break;
        }

        current_sent += ret;
    }

    return current_sent;
}


// Returns the length of the actual message received
int receive_message(int sock_fd, char *buffer) {
    int ret = recv(sock_fd, buffer, MSG_LEN_SIZE, 0);

    if (!(ret < 0)) {
        // Get the length of the message
        uint32_t message_len;
        memcpy(&message_len, buffer, MSG_LEN_SIZE);

        // Clear the buffer
        memset(buffer, 0, BUF_LEN);

        // Receive the rest of the message
        uint32_t current_len = 0;
        while (current_len < message_len) {
            ret = recv(sock_fd, buffer + current_len, message_len, 0);

            if (ret < 0) {
                break;
            }

            current_len += ret;
            ret = current_len;
        }
    }

    return ret;
}