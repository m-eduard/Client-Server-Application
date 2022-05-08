#include <iostream>
#include <string>
#include <algorithm>
#include <arpa/inet.h>

using namespace std;

#include "utils.h"

pair<string, uint8_t> parse_subscribe_msg(char *msg) {
    char cmd[MAX_CMD_LEN];
    char topic[TOPIC_LEN + 1];
    uint8_t sf;

    // Check the length of the topic
    int pos1 = find(msg, msg + strlen(msg), ' ') - msg;
    int pos2 = find(msg + pos1 + 1, msg + strlen(msg), ' ') - msg;
    if (pos2 > TOPIC_LEN)
        return {string(), -1};

    // Check if @msg, containing the command, respect
    // the standard format for a subscribe message
    if (sscanf(msg, "%s%s%hhd", cmd, topic, &sf) != 3 || sf > 1)
        return {string(), -1};

    return {string(topic), sf};
}

string parse_unsubscribe_msg(char *msg) {
    char cmd[MAX_CMD_LEN];
    char topic[TOPIC_LEN + 1];

    // Check the length of the topic
    int pos1 = find(msg, msg + strlen(msg), ' ') - msg;
    int pos2 = find(msg + pos1 + 1, msg + strlen(msg), ' ') - msg;
    if (pos2 > TOPIC_LEN)
        return string();

    // Check the format of @msg
    if (sscanf(msg, "%s%s", cmd, topic) != 2)
        return string();

    return string(topic);
}

// Function that prints the message received from the server,
// in the desired format (according to message @type)
void show_server_msg(char *buffer) {
    message_t *msg = (message_t *)buffer;

    printf("%s:%d - %s", inet_ntoa({msg->ip}), ntohs(msg->port), msg->topic);

    switch (msg->type) {
        case INT:
            printf(" - %s - %s%d", stringify(INT),
                        sign(msg->data.integer_t.sign),
                        ntohl(msg->data.integer_t.num));
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
