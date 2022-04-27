PORT = 7697

build: server subscriber

server: server.cpp
	g++ -std=c++11 server.cpp -o server

run_server:
	./server ${PORT}

subscriber: subscriber.cpp
	g++ -std=c++11 subscriber.cpp -o subscriber

clean:
	rm -rf *.o server subscriber

.PHONY: clean
