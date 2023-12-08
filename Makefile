all: client server

client:
	g++ client.cpp main.cpp -o client

server:
	g++ server.cpp -o server

clean:
	$(shell rm client 2>/dev/null)
	$(shell rm server 2>/dev/null)