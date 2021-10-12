#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstdlib>
#include <unistd.h>
#include <cstring>
#include <string>
#include <cstdio>
#include <vector>
#include <iostream>
#include <thread>
#include <csignal>


const static int MAX=1024;

struct epoll_tr
{
    int epoll_fd, nfds;
    epoll_event ev;
    epoll_event evlist[MAX];
};

struct sock_tr
{
    int socket_fd, connected_fd;
    sockaddr_in ssin, csin;
    socklen_t socklen = sizeof(sockaddr);
};
sock_tr sock;
void sigint_handler(int sig)
{
  close(sock.socket_fd);
  std::cout << "server finished job";
}
void err(int ret, const char str[])
{
    if(ret==-1)
    {
        std::perror(str);
        exit(1);
    }
}
std::string exec(const char* cmd) {
    std::array<char, 128> buffer{};
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

int main(int argc, char *argv[])
{

    signal(SIGINT, sigint_handler);
    int buflen;
    char buf[BUFSIZ];
    epoll_tr str{};

    if (argc != 2)
    {
        std::cout<<"usage: "<<argv[0]<<" [port]"<<"\n";
        exit(1);
    }
    memset(&sock.ssin, 0, sock.socklen);
    sock.ssin.sin_family = AF_INET;
    sock.ssin.sin_port = htons(std::atoi(argv[1]));
    sock.ssin.sin_addr.s_addr = htonl(INADDR_ANY);

    sock.socket_fd = socket(AF_INET, SOCK_STREAM|SOCK_NONBLOCK, 0);
    err(sock.socket_fd, "socket");
    err((bind(sock.socket_fd, (struct sockaddr *)&sock.ssin,
              sock.socklen)), "bind");
    err((listen(sock.socket_fd, 5)), "listen");
    std::cout << "server listen on " << argv[1] <<  " port"<<std::endl;
    str.epoll_fd = epoll_create1(0);
    err(str.epoll_fd, "epoll_create1");
    str.ev.events = EPOLLIN;
    str.ev.data.fd = sock.socket_fd;
    err((epoll_ctl(str.epoll_fd, EPOLL_CTL_ADD,
                   sock.socket_fd, &str.ev)), "epoll_ctl1");
    for (;;)
    {
        str.nfds = epoll_wait(str.epoll_fd, str.evlist, MAX, -1);
        err(str.nfds, "epoll_wait");
        for (int i = 0; i < str.nfds; i++)
        {
            if (str.evlist[i].data.fd == sock.socket_fd)
            {
                sock.connected_fd = accept(sock.socket_fd,
                                           (struct sockaddr *)&sock.csin, &sock.socklen);
                err(sock.connected_fd, "accept");
                std::cout<<"accepted client: "<< sock.connected_fd<<std::endl;


                str.ev.events = EPOLLIN;
                str.ev.data.fd = sock.connected_fd;
                err((epoll_ctl(str.epoll_fd, EPOLL_CTL_ADD,
                               sock.connected_fd, &str.ev)),"epoll_ctl2");
            }
            else if (str.evlist[i].data.fd == sock.connected_fd)
            {
                sock.connected_fd = str.evlist[i].data.fd;
                buflen = read(sock.connected_fd, buf, BUFSIZ-1);
                if (buflen <= 0)
                {
                    err((epoll_ctl(str.epoll_fd, EPOLL_CTL_DEL,
                                   sock.connected_fd, &str.evlist[i])),"epoll_ctl3");
                    std::cout<<"closed client: "<<sock.connected_fd <<std::endl;
                    close(sock.connected_fd);
                    err(sock.connected_fd, "close");


                }
                else {
                    std::cout <<"got shell command from:" <<
                     "from client "<<  sock.connected_fd<<": "<<buf <<std::endl;
                    std::string ans = exec(buf);
                    std::cout << "send message to client " << sock.connected_fd <<": " <<std::endl;
                    std::cout << ans<<std::endl;
                    ::send(sock.connected_fd,ans.c_str(),ans.size(),0);
                }

            }
        }
    }
}
