//============================================================================
// Name        : server.cpp
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
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/time.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>  //socket(), protocol families(AF_INET = IPv4)
#include <sys/types.h>   //SOCK_STREAM
#include <netdb.h>       //DNS resolver

using namespace std;

bool parse(char **argv);
string form_string(char* input, int begin, int end);
string getFileName(char *buff);
void sigchld_handler(int s);
void *get_in_addr(struct sockaddr *sa);

typedef struct credentials {
    string speed;
    string port;
} Tcredentials;

Tcredentials destination;
const int BACKLOG = 20;



int main(int argc, char **argv) {
    int sockfd, new_fd, b, client, rv, yes=1;  //sockets
    socklen_t sin_size;
    struct sockaddr_storage their_addr;
    FILE* fd;
    struct sockaddr_in sa;
    struct addrinfo hints, *servinfo, *p;
    struct sigaction action;
    char s[INET6_ADDRSTRLEN];
    struct timeval tv = { 0 };
    string request;
    long int time, time_total;
    int bytes = 0, temp = 0;
    // Check arguments
    if(argc == 5) {
        if(parse(argv) == false) {
            cerr << "Error: arguments have bad format" << endl;
            return 2;
        }
    } else {
        cerr << "Wrong number of arguments" << endl;
        return 2;
    }

    cout << "Bandwidth: " << destination.speed << " Port: " << destination.port << endl;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(NULL, destination.port.c_str(), &hints, &servinfo)) != 0) {
        cerr << "getaddrinfo: " << gai_strerror(rv) << endl;
        return 2;
    }

    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            cerr << "socket: " << strerror(errno) << endl;
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            cerr << "setsockopt: " << strerror(errno) << endl;
            return 2;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            cerr << "bind: " << strerror(errno) << endl;
            continue;
        }
        break;
    }

    if (p == NULL)  {
        cerr << "server: failed to bind" << endl;
        return 2;
    }
    freeaddrinfo(servinfo);

    if (listen(sockfd, BACKLOG) == -1) {
        cerr << "listen: " << strerror(errno) << endl;
        return 2;
    }

    action.sa_handler = sigchld_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;

    if (sigaction(SIGCHLD, &action, NULL) == -1) {
        cerr << "sigaction: " << strerror(errno) << endl;
        return 2;
    }
    cout << "server: waiting for clients..." << endl;

    while(1) {
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            cerr << "accept: " << strerror(errno) << endl;
            continue;
        }

        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
        cout << "server: got connection from " << s << endl;

        if (!fork()) { // this is the child process
            close(sockfd); // child doesn't need the listener
            if (send(new_fd, "220 system ready\r\n", 19, 0) == -1) {
                cerr << "send: " << strerror(errno) << endl;
                close(new_fd);
                exit(2);
            }
            FILE *lf;
            char buff[1000];
            while(1) {
                if(read(new_fd, buff, 999) < 0) {
                    cerr << "read: " << strerror(errno) << endl;
                    close(new_fd);
                    exit(2);
                }
                if(strncmp(buff, "FILE", 4) == 0) {
                    request = getFileName(buff);
                    cout << "server: sending " << request << endl;

                    if((lf = fopen(request.c_str(), "r")) == NULL) {
                        cerr << "server: Cannot open file " << request << endl;
                        send(new_fd, "550 ERROR\n", 11, 0);
                        close(new_fd);
                        exit(2);
                    }
                    memset(&buff, 0, 1000);
                    int cycles = 0;
                    gettimeofday(&tv, NULL);
                    time_total = tv.tv_usec;
                    while((bytes = fread(buff, sizeof(char), 999, lf)) > 0) {
                        temp += bytes;
                        gettimeofday(&tv, NULL);
                        time = tv.tv_usec;
                        send(new_fd, buff, bytes, 0);
                        cycles++;
                        memset(&buff, 0, 1000);
                        gettimeofday(&tv, NULL);
                        if(cycles >= atoi(destination.speed.c_str())) {
                            if((time_total = tv.tv_usec - time) > 1000000)
                                time_total = 1000000;
                            usleep(1000000 - time_total);
                            cycles = 0;
                        }
                    }
                    gettimeofday(&tv, NULL);
                    cout << "server: bytes sent: " << temp << endl;
                    fflush(stdout);
                    break;
                } else {
                    cerr << "Error on the network occurred." << endl;
                    close(new_fd);
                    fclose(lf);
                    exit(2);
                }
            }
            fclose(lf);
            close(new_fd);
            exit(0);
        }
        close(new_fd);
    }

    return 0;
}

/*
 * Extract file name from answer
 */
string getFileName(char *buff) {
    string name = "";
    for(int i=5; buff[i]!='\n' && buff[i]!='\r'; ++i)
        name += buff[i];
    return name;
}

/*
 * Parse input argument.
 */
bool parse(char **argv) {
    bool speed = false, port = false;
    for(int i=1; i<4; ++i) {
        if(strcmp(argv[i], "-d") == 0) {
            destination.speed = form_string(argv[i+1], 0, strlen(argv[i+1]));
            speed = true;
        }
        if(strcmp(argv[i], "-p") == 0) {
            destination.port = form_string(argv[i+1], 0, strlen(argv[i+1]));
            port = true;
        }
    }
    return speed && port;
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

/*
 * Beej
 */
void sigchld_handler(int s)
{
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

/*
 * Beej
 */
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
