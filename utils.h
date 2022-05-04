#pragma once

#define BUF_LEN         5000
#define MAX_CLIENTS     50
#define MSG_MAX_LEN		1600
#define MSG_LEN_SIZE    4           // number of bytes to represent the message length
#define MAX_CMD_LEN     20
#define TOPIC_LEN	   	50
#define MAX_DATA_LEN 	1500
#define MAX_ID_LEN      10
#define ERR             -1

#define INT             0
#define SHORT_REAL      1
#define FLOAT           2
#define STRING          3

#define ACC             "ACC"
#define REJ             "REJ"
// #define END				"END"

struct message {
    uint32_t ip;
    uint16_t port;
    uint8_t type;
	char topic[TOPIC_LEN + 1];

	union {
		struct integer_t {
			int8_t sign;
			uint32_t num;
		};

		int16_t short_real_t;

		struct float_t {
			int8_t sign;
			uint32_t decimal;
			uint8_t fractional;
		};

		char *string_t;
	} data;

    message() {}
	message(uint32_t ip, uint16_t port, uint8_t type, char *topic) : ip(ip), port(port),
														type(type) {this->type = type;}
	// message(uint32_t ip, uint16_t port, uint8_t type,
	// 		)
	// {
	// 	message(ip, port, type);
	// 	memcpy(this->topic, topic, TOPIC_LEN);
	// }
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
