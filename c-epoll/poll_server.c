/**
 *
 * poll 只是解决了 select 的最大文件数量，但是依然存在：
 * 1).每次都需要将检查的 fd 从用户空间拷贝到内核空间；
 * 2).需要遍历 fd 列表来检查就绪的 fd；
 *
 * 参考：
 * 1.https://cloud.tencent.com/developer/article/1005481
 * 2.https://blog.csdn.net/dongdongbusi/article/details/43067689
 */ 

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <poll.h>

#define MAX_FD_NUM 20
#define MAXLEN 1024

int main(int argc, char* argv[])
{
    printf("server start up\n");

    if (argc <= 2)
    {
        printf("usage: %s ip port\n", argv[0]);
	return 1;
    }

    // IP 地址
    const char* ip = argv[1];
    // 端口号
    int port = atoi(argv[2]);

    // 内核监听队列的最大长度（完全连接的 socket）
    // int backlog = atoi(argv[3]);

    // 创建 socket TCP/IP 协议，流式 socket
    int server_sockfd = socket(PF_INET, SOCK_STREAM, 0);

    // TCP/IP 协议的 socket 地址结构体
    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    // 将IP地址字符串转换为整数
    inet_pton(AF_INET, ip, &server_addr.sin_addr);
    // 端口，host to net，将主机字节序（小端）转换为网络字节序（大端）
    server_addr.sin_port = htons(port);

    // 将文件描述符 sock 和 socket 地址关联，仅服务端需要，客户端自动绑定地址
    // 注意需要强制转换为 struct sockaddr*
    int ret = bind(server_sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    assert(ret != -1);

    // 监听
    ret = listen(server_sockfd, MAX_FD_NUM - 1);
    assert(ret != -1);

    // 等待客户端做些连接等相关工作
    sleep(3);

    // 客户端地址
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(struct sockaddr_in);

    // poll fds
    struct pollfd pollfdArray[MAX_FD_NUM];
    for (int i = 0; i < MAX_FD_NUM; i++)
    {
        pollfdArray[i].fd = -1;
    }

    // insert the server socket fd
    pollfdArray[0].fd = server_sockfd;
    pollfdArray[0].events = POLLIN;

    int cur_fd_num = 1;
    char buf[MAXLEN] = {0};

    while (1)
    {
        int nready = poll(pollfdArray, cur_fd_num, -1);

	// server socket fd
	if (pollfdArray[0].revents & POLLIN)
	{
	    if (cur_fd_num > MAX_FD_NUM)
	    {
	        printf("socket num too much\n");
	    }
	    else
	    {
	    	// 接受连接，并将被接受的远端 sock 地址信息保存在地二个参数中
		// 只是从监听的队列中取出连接，即使客户端已经断开网络，连接也会 accept 成功
		int client_sockfd = accept(server_sockfd, (struct sockaddr*) &client_addr, &client_addr_len);
		if (client_sockfd < 0)
		{
		    perror("accept");
		}
		else
		{
		    // inet_ntoa(struct addr_in) 将IP地址转换为字符串并返回
		    printf("accept client_addr %s\n", inet_ntoa(client_addr.sin_addr));

		    for (int i = 0; i < MAX_FD_NUM; i++)
		    {
		        if (pollfdArray[i].fd == -1)
			{
			    pollfdArray[i].fd = client_sockfd;
			    pollfdArray[i].events = POLLIN;
			    cur_fd_num++;
			    break;
			}
		    }
		}
	    }

	    if (--nready <= 0)
	    {
	    	continue;
	    }
	}

	for (int i = 1; i < MAX_FD_NUM; i++)
	{
	    if (pollfdArray[i].fd < 0)
	    {
	        continue;
	    }

	    if (pollfdArray[i].revents & (POLLIN | POLLERR))
	    {
	        int n = recv(pollfdArray[i].fd, buf, MAXLEN, 0);

		if (n < 0)
		{
		    if (ECONNRESET == errno)
		    {
		        close(pollfdArray[i].fd);
			pollfdArray[i].fd = -1;
			cur_fd_num--;
		    }
		    else
		    {
		        perror("recv");
		    }
		}
		else if (n == 0)
		{
		    close(pollfdArray[i].fd);
		    pollfdArray[i].fd = -1;
		    cur_fd_num--;
		}
		else
		{
		    printf("received %d:%s\n", n, buf);
		    if (!strcmp("close", buf) || !strcmp("close\r\n", buf))
		    {
		        close(pollfdArray[i].fd);
			pollfdArray[i].fd = -1;
			cur_fd_num--;
		    }
		}
	    }

	    if (nready--)
	    {
	       break;
	    }
	}
    }

    for (int i = 0; i < MAX_FD_NUM; i++)
    {
        if (pollfdArray[i].fd != -1)
	{
	    close(pollfdArray[i].fd);
	}
    }

    return 0;
}
