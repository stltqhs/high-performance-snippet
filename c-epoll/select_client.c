#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#define MAXDATASIZE 100
#define SERVERPORT 1234
#define MAXCLINE 1024

int main(int argc, char *argv[])
{
    int sockfd, sendbytes;

    char send[MAXCLINE];
    char buf[MAXDATASIZE];

    struct hostent *host;
    struct sockaddr_in serv_addr;

    if (argc < 2)
    {
        fprintf(stderr, "Please enter the server's hostname\n");
	exit(1);
    }

    if ((host = gethostbyname(argv[1])) == NULL)
    {
        perror("gethostbyname");
	exit(1);
    }

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket error\n");
	exit(1);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVERPORT);
    serv_addr.sin_addr = *((struct in_addr*) host->h_addr);
    bzero(&(serv_addr.sin_zero), 8);

    if (connect(sockfd, (struct sockaddr *) & serv_addr, sizeof(struct sockaddr)) == -1)
    {
        perror("connect\n");
	exit(1);
    }

    while (fgets(send, 1024, stdin) != NULL)
    {
        if ((sendbytes = write(sockfd, send, 100)) == -1)
	{
	    perror("send error\n");
	    exit(1);
	}
    }

    close(sockfd);
}
