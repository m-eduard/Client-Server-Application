#include "utils.h"

/* Function that sends a given payload to a server
 * (connection to the server should be established
 *  before calling this function), and adds a header
 * of 4 bytes length which contains the message size.
 * 
 * Also, before the header, a control byte is added,
 * which signals the fact that the message do have the
 * header (to differentiate simple datagrams from
 * data formated according to my protocol).
 *
 * @len - number of bytes that will be sent from @msg
 *        (-1 if it's the whole message, until '\0')
 */
int send_message(int sock_fd, const char *msg, int len) {
    char buffer[BUF_LEN];

    uint8_t control_byte = CONTROL_BYTE;
    uint32_t msg_len = (len < 0 ? strlen(msg) : len);

    memcpy(buffer, &control_byte, 1);
    memcpy(buffer + 1, &msg_len, MSG_LEN_SIZE);
    memcpy(buffer + 1 + MSG_LEN_SIZE, msg, msg_len);

    uint32_t current_sent = 0;
    uint32_t total_len = 1 + MSG_LEN_SIZE + msg_len;

    while (current_sent < total_len) {
        int ret = send(sock_fd, buffer + current_sent,
                        total_len - current_sent, 0);

        if (ret < 0)
            break;

        current_sent += ret;
    }

    return current_sent;
}


// Function that receives a message, strips it of the header
// which contains the length and the control byte, and returns
// the number of bytes read from the message received
int receive_message(int sock_fd, char *buffer) {
    int ret = recv(sock_fd, buffer, 1, 0);

    // ret < 0, when there is an error
    // ret = 0, when no data was sent through the socket
    //          (this is the case when a client disconnects
    //           from the server, and an empty string will be
    //           received to announce the end of the connection,
    //           therefore this message will have no header)
    if (ret <= 0)
        return ret;
    
    // The message should start with the control byte,
    // else it doesn't belong to the messages generated
    // using my protocol
    if (buffer[0] != CONTROL_BYTE)
        return -1;

    // Get the length of the message
    int32_t current_read = 0;
    while (current_read < MSG_LEN_SIZE) {
        ret = recv(sock_fd, buffer + current_read,
                    MSG_LEN_SIZE - current_read, 0);

        if (ret < 0)
            return ret;

        current_read += ret;
    }

    // Store the length of the message
    uint32_t message_len;
    memcpy(&message_len, buffer, MSG_LEN_SIZE);

    // Clear the buffer
    memset(buffer, 0, BUF_LEN);

    // Receive the rest of the messag
    current_read = 0;
    while (current_read < message_len) {
        ret = recv(sock_fd, buffer + current_read,
                    message_len - current_read, 0);

        if (ret < 0)
            break;

        current_read += ret;
    }

    // Return the length of the actual message
    return current_read;
}

// Format the message received from UDP clients
// and store it in @buffer, so it can be sent
// over to the TCP clients (return the size of
// the message after formatting)
int format_udp_msg(message_t *msg, char *buffer) {
    int new_len = sizeof(*msg);

    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, msg, sizeof(*msg));

    // Append the actual string to the end of the
    // struct, because the struct don't contain the
    // string itself, but a pointer to it
    if (msg->type == STRING) {
        int string_len = strlen(msg->data.string_t);

        memcpy(buffer + sizeof(*msg), msg->data.string_t, string_len);
        new_len += string_len;
    }

    return new_len;
}
