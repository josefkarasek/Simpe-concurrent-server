CC=g++
CFLAGS=-std=c++98 -lpthread

executable: create_dirs
	$(CC) $(CFLAGS) client.cpp -o client_dir/client
	$(CC) $(CFLAGS) server.cpp -o server_dir/server
	
client:	create_dirs
	$(CC) $(CFLAGS) client.cpp -o client_dir/client
	
server: create_dirs
	$(CC) $(CFLAGS) server.cpp -o server_dir/server

create_dirs: clean
	mkdir server_dir client_dir	
clean:
	rm -rf client_dir server_dir 2> /dev/null
