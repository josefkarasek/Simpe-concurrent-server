//============================================================================
// Name        : server.cpp
// Author      : xkaras27
// Version     :
// Copyright   : http://beej.us/guide/bgnet/output/html/multipage/clientserver.html#simpleserver
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
#include <semaphore.h>
#include <sys/mman.h>   //semaphore memory mapping
#include <sys/shm.h>    //shared memory
#include <sys/ipc.h>
#include <sys/stat.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>  //socket(), protocol families(AF_INET = IPv4)
#include <sys/types.h>   //SOCK_STREAM
#include <netdb.h>       //DNS resolver

using namespace std;

bool parse(char **argv);
bool get_sem(void);
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
sem_t *mutex;  //shared memory access control



int main(int argc, char **argv) {
    int sockfd, new_fd, b, client, rv, yes=1;                                //sockets
    socklen_t sin_size;
    struct sockaddr_storage their_addr; // connector's address information
    FILE* fd;
    struct sockaddr_in sa;                            //IP address info
    struct addrinfo hints, *servinfo, *p;
    struct sigaction action;
    char s[INET6_ADDRSTRLEN];
    string request;

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
    hints.ai_flags = AI_PASSIVE; // use my IP

    if ((rv = getaddrinfo(NULL, destination.port.c_str(), &hints, &servinfo)) != 0) {
        cerr << "getaddrinfo: " << gai_strerror(rv) << endl;
        return 2;
    }

    // loop through all the results and bind to the first we can
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
    freeaddrinfo(servinfo); // all done with this structure

    if (listen(sockfd, BACKLOG) == -1) {
        cerr << "listen: " << strerror(errno) << endl;
        return 2;
    }

    action.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;

    if (sigaction(SIGCHLD, &action, NULL) == -1) {
        cerr << "sigaction: " << strerror(errno) << endl;
        return 2;
    }
//    get_sem();
    cout << "server: waiting for clients..." << endl;

    while(1) {  // main accept() loop
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
            if (send(new_fd, "220 system ready\r\n", 18, 0) == -1) {
                cerr << "send: " << strerror(errno) << endl;
                exit(0);
            }
            ifstream local_file;
            string line;
            FILE *lf;
            char buff[1000];
            while(1) {
                if(read(new_fd, buff, 999) < 0) {
                    cerr << "read: " << strerror(errno) << endl;
                    exit(0);
                }
                if(strncmp(buff, "FILE", 4) == 0) {
                    request = getFileName(buff);
                    cout << "server: sending " << request << endl;

//                    sem_wait(mutex);
//                    local_file.open(request.c_str(), ios::in);
                    lf = fopen(request.c_str(), "r");
                    if(lf == NULL) {
                        cerr << "Cannot open file " << request << endl;
                        exit(0);
                    }
                    while(fread(buff, sizeof(char),999, lf) > 0) {
                        cout << buff;
//                        memcpy(&buff, line.c_str(), strlen(line.c_str()));
                        send(new_fd, buff, 999, 0);
                        memset(&buff, 0, 1000);
                    }
                    local_file.close();
//                    sem_post(mutex);
                    break;
                }
//                else if(strncmp(buff, "QUIT", 4) == 0) {
//
//                }

            }

            close(new_fd);
            exit(0);
        }
        close(new_fd);  // parent doesn't need this
//        sem_destroy(mutex);
    }

    return 0;
}


//bool get_sem(void)
//{
//    if((mutex = (sem_t*) mmap(NULL, sizeof(sem_t), PROT_READ | \
//            PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0)) == MAP_FAILED)
//        return false;
//    if(sem_init(mutex, 1, 1) == -1)
//        return false;
//    return true;
//}

string getFileName(char *buff) {
    string name = "";
    for(int i=5; i!='\n' && i!='\r'; ++i)
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


void sigchld_handler(int s)
{
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
