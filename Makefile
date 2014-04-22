CC=g++
CFLAGS=-std=c++98 -lpthread

executable:
	$(CC) $(CFLAGS) client.cpp -o client
	$(CC) $(CFLAGS) server.cpp -o server
	
client:
	$(CC) $(CFLAGS) client.cpp -o client
	
server:
	$(CC) $(CFLAGS) server.cpp -o server
	
clean:
	rm -f client server