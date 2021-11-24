/**
 *
 * 一、使用
 * 1.files: select_server.c select_client.c
 * 2.compile:
 * 1).gcc -o select_server.bin select_server.c
 * 2).gcc -o select_client.bin select_client.c
 * 3.执行
 * select_server.bin 和 select_client.bin
 *
 * 二、介绍
 * 这个例子是学习 select 函数。
 *
 * select io 多路复用使用的函数只有使用 select，原型是
 *
 * int select(int nfds, fd_set *readfds, fd_set *writefds,
 *                fd_set *exceptfds, struct timeval *timeout);
 *
 * 重要的是要理解 nfds 和 fd_set 的概念。
 *
 * nfds 表示 select 函数需要检测多少个 fd，它是加入到 fd_set 的最大 fd + 1。
 * fd_set 使用"位"来表示需要检查的 fd，一般 fd_set 是 1024 位，这是由操作系统编译时决定的。
 *
 * 举个例子，假如 fd_set 是一个字节长度，也就是 8 为，初始时（也就是调用了 FD_ZERO）后是 00000000 。8个0
 * 如果使用 FD_SET 将 fd 为 3 的文件加入进来，则 fd_set 的表示就是 00010000，从左边开始数的第4位设置为 1。
 * 因为 fd 的计数是从 0 开始，所以 nfds 需要告诉 select 最多检查多少个 fd，也就是 3 + 1 = 4，这就是最大 fd + 1 的原因。
 *
 * 那么 select 如何实现没有关心的事件时阻塞进程，有事件就绪或者超时时进程又唤醒呢？
 * 睡眠唤醒机制：
 * 每个 socket 都有一个 sleep_list 睡眠队列，队列的元素是 wait_entry ，wait_entry 包括等待该 socket 事件就绪的进程 pid。
 * 当用户调用 select 函数时，内核需要遍历关心的 socket 事件是否就绪，如果没有就绪，就在这些 socket 的 sleep_list 加上 wait_entry 节点。
 * 然后调用 schedule_timeout 使当前进程让出CPU，该进程此时不在CPU调度队列。当：
 * 1)等待超时后，内核将该进程唤醒，加入到CPU调度队列，然后再次挨个检查所有的 scoket 是否有事件就绪；
 * 2)如果某个 socket 的事件就绪，会遍历它的 sleep_list 的所有 wait_entry 节点，调用 wait_entry 节点的 callback，它会做唤醒进程的操作；
 * 如果有关心的 scoket 事件就绪，本次就不用阻塞或者调用 schedule_timeout 让出 CPU。
 *
 * 使用 select 的弊端：
 * 1)fd_set 的长度受限，一般是 1024 个 fd，也就是 fd 最大为 1023，无法实现高并发，由编译时的内核决定；
 * 2)fd_set 需要从用户态拷贝到内核态，复制也会浪费 CPU 时间；
 * 3)有事件到来时不知道是哪个文件有事件，应用需要使用 FD_ISSET 来遍历所有的 fd 是否可读可写；
 *
 *
 * 三、参考：
 * 1.https://cloud.tencent.com/developer/article/1005481
 * 2.https://blog.csdn.net/fengel_cs/article/details/78645140
 * 3.https://blog.csdn.net/zhougb3/article/details/79792089
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// 连接时使用的端口
#define MYPORT 1234

// 连接队列中的个数
#define MAXCLINE 5

// 接收缓存区大小
#define BUF_SIZE 200

int fd[MAXCLINE];

// 当前的连接数
int conn_amount;


void showclient()
{
    int i;
    printf("client amount:%d\n", conn_amount);
    for (i = 0; i < MAXCLINE; i++)
    {
        printf("[%d]:%d", i, fd[i]);
    }
}


int main(void)
{
    // 监听的fd 连接的fd
    int sock_fd, new_fd;
    
    // 服务端地址
    struct sockaddr_in server_addr;
    
    // 客户端地址
    struct sockaddr_in client_addr;

    socklen_t sin_size;
    int yes = 1;
    char buf[BUF_SIZE];
    int ret;
    int i;

    // 建立 sock_fd 
    // https://man7.org/linux/man-pages/man2/socket.2.html
    // 函数原型： int socket(int domain, int type, int protocol);
    // AF_INET 表示 IPv4
    // SOCK_STREAM 表所使用 TCP 
    if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("setsockopt\n");
	exit(1);
    }

    // https://man7.org/linux/man-pages/man3/setsockopt.3p.html
    // int setsockopt(int socket, int level, int option_name,
    //       const void *option_value, socklen_t option_len);
    // 设置套接字的选项 SO_REUSEADDR 允许在同一个端口启动服务的多个实例/进程
    //
    // setsockopt 的第二个参数 SOL_SOCKET 指定系统中解释选项的级别 普通套接字
    
    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
    {
        perror("setsockopt error\n");
	exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(MYPORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    memset(server_addr.sin_zero, '\0', sizeof(server_addr.sin_zero));

    if (bind(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("bind error!\n");
	exit(1);
    }

    if (listen(sock_fd, MAXCLINE) == -1)
    {
        perror("listen error!\n");
	exit(1);
    }

    printf("listen port %d\n", MYPORT);

    // 文件描述符集的定义
    // https://blog.csdn.net/zhougb3/article/details/79792089
    fd_set fdsr;

    int maxsock;
    struct timeval tv;

    conn_amount = 0;
    sin_size = sizeof(client_addr);
    maxsock = sock_fd;

    while(1)
    {
              FD_ZERO(&fdsr);
	      FD_SET(sock_fd, &fdsr);

	      tv.tv_sec = 30;
	      tv.tv_usec = 0;

	      for (i = 0; i < MAXCLINE; i++)
	      {
	          if (fd[i] != 0)
		  {
		      FD_SET(fd[i], &fdsr);
		  }
	      }

	      ret = select(maxsock + 1, &fdsr, NULL, NULL, &tv);

	      if (ret < 0)
	      {
	         // 没有找到有效的连接，失败
		 perror("select error!\n");
		 break;
	      }
	      else if (ret == 0)
	      {
	          // 指定的时间到
		  printf("timeout \n");
		  continue;
	      }

	      // 循环判断有效的连接是否有数据到达
	      for (i = 0; i < conn_amount; i++)
	      {
	          if (FD_ISSET(fd[i], &fdsr))
		  {
		      ret = recv(fd[i], buf, sizeof(buf), 0);

		      if (ret <= 0)
		      {
		          // 客户端连接关闭，清除文件描述符集中的相应的位
			  printf("client[%d] close\n", i);
			  close(fd[i]);
			  FD_CLR(fd[i], &fdsr);
			  fd[i] = 0;
			  conn_amount--;
		      }
		      else
		      {
		          // 否则有相应的数据发送过来，进行相应的处理
			  if (ret < BUF_SIZE)
			  {
			      memset(&buf[ret], '\0', 1);
			  }
			  printf("client[%d] send:%s\n", i, buf);
		      }
		  }
	      }

	      if (FD_ISSET(sock_fd, &fdsr))
	      {
	          new_fd = accept(sock_fd, (struct sockaddr *)&client_addr, &sin_size);
		  if (new_fd < 0)
		  {
		      perror("accept error\n");
		      continue;
		  }

		  // 添加新的 fd 到数组中，判断有效的连接数是否小于最大的连接数，如果小于的话
		  // 就把新的连接套接字加入集合
		  if (conn_amount < MAXCLINE)
		  {
		      for (i = 0; i < MAXCLINE; i++)
		      {
		          if (fd[i] == 0)
			  {
			      fd[i] = new_fd;
			      break;
			  }
		      }
		      conn_amount++;
		      printf("new connection client[%d]%s:%d\n", conn_amount, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
		      if (new_fd > maxsock)
		      {
		          maxsock = new_fd;
		      }
		  }
		  else
		  {
		      printf("max connections arrive, exit\n");
		      send(new_fd, "bye", 4, 0);
		      close(new_fd);
		      continue;
		  }
		  showclient();
	      }

    }

    for (i = 0; i < MAXCLINE; i++)
    {
        if (fd[i] != 0)
	{
	    close(fd[i]);
	}
    }
    exit(0);
}
