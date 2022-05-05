#pragma once

#define BUF_LEN         	5000
#define MAX_CLIENTS     	50
#define MSG_MAX_LEN			1600
#define MSG_LEN_SIZE    	4           // number of bytes to represent the message length
#define MAX_CMD_LEN     	20
#define TOPIC_LEN	   		50
#define MAX_DATA_LEN 		1500
#define MAX_ID_LEN      	10
#define ERR             	-1

#define INT             	0
#define SHORT_REAL      	1
#define FLOAT           	2
#define STRING          	3

#define INTEGER_T_SZ		40			// Sizes of my custom data types
#define SHORT_REAL_T_SZ		8			// used in struct message
#define FLOAT_T_SZ 			48

#define ACC             	"ACC"
#define REJ             	"REJ"

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
				memcpy(tmp, data, INTEGER_T_SZ);

				memcpy(&this->data.integer_t.sign, tmp += 1, 1);
				memcpy(&this->data.integer_t.num, tmp, 4);

				break;
			case SHORT_REAL:
				memcpy(&this->data.short_real_t, data, SHORT_REAL_T_SZ);
				break;
			case FLOAT:
				memcpy(tmp, data, FLOAT_T_SZ);

				memcpy(&this->data.float_t.sign, tmp += 1, 1);
				memcpy(&this->data.float_t.decimal, tmp += 4, 4);
				memcpy(&this->data.float_t.fractional, tmp, 1);

				break;
			case STRING:
				memcpy(tmp, data, MAX_DATA_LEN);

				this->data.string_t = tmp;
				break;
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
