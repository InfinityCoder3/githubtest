#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <errno.h>
#include <ctype.h>

#define MAXLINE 8192
#define SERV_PORT 6666
#define OPEN_MAX 5000

int main(int argc, char *aargv[])
{
    int i, j;
    int lfd, cfd, sockfd;
    int n, num = 0;
    ssize_t nready, epfd, res;
    char buf[MAXLINE], str[INET_ADDRSTRLEN];
    socklen_t clielen;

    struct sockaddr_in caddr, saddr;
    struct epoll_event tep, ep[OPEN_MAX];

    lfd = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 0;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    bzero(&saddr, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    saddr.sin_port = htons(SERV_PORT);

    bind(lfd, (struct sockaddr *)&saddr, sizeof(saddr));
    listen(lfd, 128);

    /*---------------------------------*/
    epfd = epoll_create(OPEN_MAX);
    if (epfd == -1) {
        perror("epoll_create error");
        exit(-1);
    }

    tep.events = EPOLLIN;   //read;
    tep.data.fd = lfd;
    res = epoll_ctl(epfd, EPOLL_CTL_ADD, lfd, &tep);    //add a epoll event
    if (res == -1) {
        perror("epoll_ctl error");
        exit(-1);
    }

    while (1) {
        //epoll block listen
        nready = epoll_wait(epfd, ep, OPEN_MAX, -1); //-1: always block 
        if (nready == -1) {
            perror("epoll_wait error");
            exit(-1);
        }

        for (i = 0; i < nready; i++) {  //listen client
            if (!(ep[i].events & EPOLLIN))
                continue;

            if (ep[i].data.fd == lfd) {
                clielen = sizeof(caddr);
                cfd = accept(lfd, (struct sockaddr *)&caddr, &clielen);
                printf("reveive from %s at port %d\n",
                    inet_ntop(AF_INET, &caddr.sin_addr, str, sizeof(str)),
                    ntohs(caddr.sin_port));
                printf("cdf %d----client NO %d\n", cfd, ++num);

                tep.events = EPOLLIN;
                tep.data.fd = cfd;
                res = epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &tep);
                if (res == -1) {
                    perror("epoll_ctl error");
                    exit(-1);
                }
            } else { //not lfd, read events
                sockfd = ep[i].data.fd;
                n = read(sockfd, buf, MAXLINE);

                if (n == 0) {   //read 0 means that client has been closed
                    res = epoll_ctl(epfd, EPOLL_CTL_DEL, sockfd, NULL);
                    if (res == -1) {
                        perror("epoll_ctl error");
                        exit(-1);
                    }
                    close(sockfd);
                    printf("client[%d] closed connection\n", sockfd);
                } else if (n < 0) { // read < 0 error
                    perror("read n < 0error: ");
                    res = epoll_ctl(epfd, EPOLL_CTL_DEL, sockfd, NULL);
                    close(sockfd);
                } else {
                    for (j = 0; j < n; j++) {
                        buf[j] = toupper(buf[j]);
                    }
                    write(STDOUT_FILENO, buf, n);
                    write(sockfd, buf, n);
                }
            }
        }
        
    }

    close(lfd);
    close(epfd);


    return 0;
}