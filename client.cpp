//============================================================================
// Name        : client.cpp
// Author      : xkaras27
// Version     :
// Description : programming assignment 2: concurrent server
//============================================================================

#include <iostream>
#include <cstdio>
#include <cerrno>        //error codes
#include <cstring>       //memset()
#include <fstream>

#include <regex.h>
#include <unistd.h>      //sleep()
#include <stdlib.h>
#include  <stdio.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>  //socket(), protocol families(AF_INET = IPv4)
#include <sys/types.h>   //SOCK_STREAM
#include <netdb.h>       //DNS resolver

using namespace std;

bool parse(char *argv);
string form_string(char* input, int begin, int end);

typedef struct credentials {
    string host;
    string file;
    string port;
} Tcredentials;

Tcredentials destination;
char buff[1000];

int main(int argc, char **argv) {
    FILE *out_fd;
    int client_socket, bytes;                         //sockets
    struct sockaddr_in server_address;                //IP address info
    struct hostent *server_ip;                        //DNS resolver
    struct timeval timeout;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;

    // Check arguments
    if(argc == 2) {
        if(parse(argv[1]) == false) {
            cerr << "Error: argument has bad format" << endl;
            return 2;
        }
    } else {
        cerr << "Wrong number of arguments" << endl;
        return 2;
    }

    //setting server address (need to contact DNS)
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons ( atoi(destination.port.c_str()) );
    //contact DNS
    server_ip = gethostbyname(destination.host.c_str());
    if(server_ip == NULL) {
        cerr << "Error: DNS returned NULL" << endl;
        return 2;
    }
    memcpy(&server_address.sin_addr, server_ip->h_addr_list[0], \
            server_ip->h_length);

    //creating socket
    client_socket = socket(PF_INET, SOCK_STREAM, 0);
    if(client_socket < 0) {
        cerr << "socket: " << strerror(errno) << endl;
        return 2;
    }

    if(setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, \
            (char *)&timeout, sizeof(timeout)) < 0) {
        cerr << "setsockopt: " << strerror(errno) << endl;
        close(client_socket);
        return 2;
    }

    //establishing connection
    if((connect(client_socket, (struct sockaddr *)&server_address, \
            sizeof(server_address))) < 0) {
        cerr << "connect: " << strerror(errno) << endl;
        close(client_socket);
        return 2;
    }

    //Welcome message
    recv(client_socket, buff, 999, 0);
    cout << "client: " << buff;
    memset(&buff, 0, 1000);
    //Form&send command
    string command = "FILE " + destination.file + "\n";
    memcpy(&buff, command.c_str(), strlen(command.c_str()));
    send(client_socket, buff, 999, 0);
    memset(&buff, 0, 1000);
    //Expecting answer
    if((out_fd = fopen(destination.file.c_str(), "w")) == NULL) {
        cerr << "client: cannot open output file" << endl;
        close(client_socket);
        return 2;
    }
    while((bytes = recv(client_socket, buff, 999, 0)) > 0) {
        if(strncmp("550 ERROR", buff, 9) == 0) {
            cout << "client: server coldn't serve the file" << endl;
            close(client_socket);
            fclose(out_fd);
            unlink(destination.file.c_str());
            return 2;
        }
        fwrite(buff, sizeof(char), bytes, out_fd);
        memset(&buff, 0, 1000);
    }

    close(client_socket);
    fclose(out_fd);

    return 0;
}

/*
 * Parse input argument.
 */
bool parse(char *argv) {
    regex_t regex;
    size_t nmatch = 5;
    int reti;
    regmatch_t pmatch[5];

    if((reti = regcomp(&regex, "^(([a-zA-Z0-9_]\\.?)+):([0-9]{1,5})/([a-zA-Z"\
            "0-9_$-\\.\\+\\!\\*'\\(\\)]+)$", REG_EXTENDED)) != 0) {
        return false;
    }
    reti = regexec(&regex, argv, nmatch, pmatch, 0);

    if(reti == 0 && reti != REG_NOMATCH) {
        destination.host = form_string(argv, pmatch[1].rm_so, pmatch[1].rm_eo);
        destination.port = form_string(argv, pmatch[3].rm_so, pmatch[3].rm_eo);
        destination.file = form_string(argv, pmatch[4].rm_so, pmatch[4].rm_eo);

        return true;
    }
    return false;
}

/*
 * Get substring from input string
 */
string form_string(char* input, int begin, int end) {
    string output = "";
    if(begin == -1 || end == -1)
        return output;

    for(int i=begin; i<end; ++i)
        output += input[i];

    return output;
}



