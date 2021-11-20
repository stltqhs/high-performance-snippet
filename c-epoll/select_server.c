/**
 *
 * 这个例子是学习 select 函数。
 * 参考：
 * 1.https://blog.csdn.net/fengel_cs/article/details/78645140
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
