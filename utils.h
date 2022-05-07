#include <string>
using namespace std;

#pragma once

#define BUF_LEN         	5000
#define MAX_CLIENTS     	50
#define MSG_MAX_LEN			1600
#define MSG_LEN_SIZE    	4    	// #bytes to represent the message length
#define MAX_CMD_LEN     	20
#define TOPIC_LEN	   		50
#define MAX_DATA_LEN 		1500
#define MAX_ID_LEN      	10
#define CONTROL_BYTE		(int8_t)0b10011101	// Random control byte
#define ERR             	-1

#define INT             	0
#define SHORT_REAL      	1
#define FLOAT           	2
#define STRING          	3

#define ACC             	"ACC"
#define REJ             	"REJ"

#define SUCC_SUBSCRIBE		"SUB"
#define SUCC_UNSUBSCRIBE	"UNSUB"
#define FAIL				"FAIL"

#define HOST_IP     		"127.0.0.1"

// get the name of the variable
#define stringify(x)		#x

// 0 -> ''
// 1 -> '-'
#define sign(x)				((((x) & 0xff) == (1 & 0xff)) ? "-" : "")


struct client_t {
    string id_client;
    bool active;
    int fd;

    client_t() {}
    client_t(string id, bool x, int fd) : id_client(id), active(x), fd(fd) {}
};

struct message_t {
    uint32_t ip;
    uint16_t port;
    uint8_t type;
	char topic[TOPIC_LEN + 1];

	union {
		struct __attribute__ ((__packed__)) {
			int8_t sign;
			uint32_t num;
		} integer_t;

		int16_t short_real_t;

		struct __attribute__ ((__packed__)) {
			int8_t sign;
			uint32_t decimal;
			uint8_t fractional;
		} float_t;

		char *string_t;
	} data;

    message_t() {}
	message_t(uint32_t ip, uint16_t port, uint8_t type) :
		ip(ip), port(port), type(type) {}
	message_t(uint32_t ip, uint16_t port, uint8_t type,
			char *topic, char *data) : message_t(ip, port, type)
	{
		memcpy(this->topic, topic, TOPIC_LEN);
		char *tmp = (char *)calloc(MAX_DATA_LEN + 1, sizeof(char));

		switch(type) {
			case INT:
				memcpy(&this->data.integer_t, data,
						sizeof(this->data.integer_t));
				break;
			case SHORT_REAL:
				memcpy(&this->data.short_real_t, data,
						sizeof(this->data.short_real_t));
				break;
			case FLOAT:
				memcpy(&this->data.float_t, data,
						sizeof(this->data.float_t));
				break;
			case STRING:
				memcpy(tmp, data, MAX_DATA_LEN);
				this->data.string_t = tmp;
		}
	}
};

#define DIE(assertion, call_description)	\
	do {									\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",	\
					__FILE__, __LINE__);	\
			perror(call_description);		\
			exit(EXIT_FAILURE);				\
		}									\
	} while(0)
