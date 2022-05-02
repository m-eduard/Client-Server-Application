PORT = 7697
SERVER_IP = 127.0.0.1
ID = "client"

build: server subscriber

server: server.cpp
	g++ -std=c++11 server.cpp -o server

subscriber: subscriber.cpp
	g++ -std=c++11 subscriber.cpp -o subscriber

run_server:
	./server ${PORT}

run_subscriber:
	./subscriber ${ID} ${SERVER_IP} ${PORT} 

clean:
	rm -rf *.o server subscriber

.PHONY: clean
