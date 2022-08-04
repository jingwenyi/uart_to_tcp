/*
*
*   code for imx6 uavrs ground station link model
*   author: wenyi.jing
*   date: 2022.08
*
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/statfs.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <signal.h>
#include <termios.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <termios.h>

#define LISTEN_NUM 1
#define MAX(a,b)	(a > b ? a : b)

 static int tty_configure(char *tty_port, char *baudrate)
{
	int tty_fd;
	struct termios  oldtio = {0};
	struct termios new_cfg = {0};
	int rate;

	assert(tty_port != NULL && baudrate != NULL);

	tty_fd = open(tty_port, O_RDWR | O_NOCTTY );

	if(tty_fd < 0 )
	{
		printf("open %s error\n", tty_port);
		return -1;
	}
	
	if(tcgetattr(tty_fd, &oldtio) != 0)
	{
		printf("tcgettattr failed \r\n");
		close(tty_fd);
		return -1;
	}


	if(strcmp(baudrate, "38400") == 0)
	{
		rate = B38400;
	}
	else if(strcmp(baudrate, "57600") == 0)
	{
		rate = B57600;
	}
	else if(strcmp(baudrate, "115200") == 0)
	{
		rate = B115200;
	}
	else if(strcmp(baudrate, "230400") == 0)
	{
		rate = B230400;
	}
	else
	{
		printf("baud rate %s is not support!\n", baudrate);
		close(tty_fd);
		return -1;
	}

	//set the raw model
	cfmakeraw(&new_cfg);
	new_cfg.c_cflag |= CREAD;
	

	if(cfsetspeed(&new_cfg, rate) < 0)
	{
		printf("cfsetispeed error %s\n", baudrate);
		return -1;
	}

	new_cfg.c_cflag &= ~CSIZE;
	
	new_cfg.c_cflag |= CS8;
	
	new_cfg.c_cflag &= ~PARENB;
 	new_cfg.c_iflag &= ~INPCK;

	new_cfg.c_cflag &= ~CSTOPB;

	new_cfg.c_cc[VTIME] = 0;
 	new_cfg.c_cc[VMIN] = 0;

	if (tcflush(tty_fd, TCIOFLUSH) < 0) 
	{
 		printf("tcflush error \n");
 		return -1;
 	}
	

	if(tcsetattr(tty_fd, TCSANOW, &new_cfg) < 0)
	{
		printf("tcsetattr failed \n");
		close(tty_fd);
		return -1;
	}

	return tty_fd;
}

static int create_sock(char *sock_type, char *sock_port)
{
	int sock_fd;
	int type, port, ret;
	struct sockaddr_in server_addr;
	
	assert(sock_type != NULL && sock_port != NULL);

	port = atoi(sock_port);

	if(strcmp(sock_type, "tcp") == 0)
	{
		type = SOCK_STREAM;
	}
	else
	{
		printf("sock type is not tcp.\n");
		return -1;
	}

	sock_fd = socket(PF_INET, type, 0);

	if(sock_fd < 0)
	{
		perror("sock error\n");
		return -1;
	}

	bzero(&server_addr, sizeof(server_addr));
	server_addr.sin_family = PF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	ret = bind(sock_fd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr));

	if(ret < 0)
	{
		close(sock_fd);
		perror("bind error\n");
		return -1;
	}

	ret = listen(sock_fd, LISTEN_NUM);
	
	if(ret < 0)
	{
		close(sock_fd);
		perror("listen error\n");
		return -1;
	}

	return sock_fd;
}

int main(int argc, char** argv)
{
	int ret = -1;
	socklen_t addr_len;
	pthread_t tty_pthread;
	struct sockaddr_in client_addr;
	int maxfd;
	fd_set allset, readset;
	uint8_t buf[512];
	int tty_fd;
	int sock_fd;
	int connect_fd;

	if(argc < 5)
	{
		printf("please intput  form:./serial_to_network + (uart port) + (baud rate) + (tcp) + (bind port) \n");
		printf("example1:./serial_to_network  /dev/ttyUSB0 115200 tcp 8000 \n");
		return ret;
		
	}

	//1.configure uart port, if failed, return -1

	tty_fd = tty_configure(argv[1], argv[2]);
	
	if(tty_fd < 0)
	{
		printf("---open %s error----\n", argv[1]);
		return ret;
	}

	//2.start tcp or udp server

	sock_fd = create_sock(argv[3], argv[4]);
	
	if(sock_fd < 0)
	{
		printf("---create %s %s error", argv[3], argv[4]);
		close(tty_fd);
		return ret;
	}

	maxfd = -1;
	FD_ZERO(&allset);
	//add connect_fd , tty_fd into allset
	FD_SET(sock_fd,&allset);
	FD_SET(tty_fd, &allset);
	maxfd = MAX(sock_fd, tty_fd);
	connect_fd = -1;

	printf("wait for connect...\n");

	
	while(1)
	{
		readset = allset;
		ret = select(maxfd+1, &readset, NULL, NULL, NULL);

		if(ret < 0)
		{
			perror("accept error");
			break;
		}

		if(FD_ISSET(sock_fd, &readset))
		{

			if(connect_fd >= 0)
			{
				printf("too many client!");
			}
			else
			{
				bzero(&client_addr, sizeof(client_addr));
				addr_len = sizeof(struct sockaddr);

				connect_fd = accept(sock_fd, (struct sockaddr *)&client_addr, &addr_len);
				if(connect_fd < 0)
				{
					perror("accept error");
					break;
				}

				printf("%s,%d connected!\n", 
					inet_ntoa(client_addr.sin_addr),ntohs(client_addr.sin_port));

				FD_SET(connect_fd, &allset);
				maxfd = MAX(maxfd, connect_fd);
			}
		}

		if(FD_ISSET(tty_fd, &readset))
		{
			// receive tty data.
			bzero(buf, sizeof(buf));
			int len = read(tty_fd, buf, sizeof(buf));

			if(len < 0)
			{
				perror("read tty error\n");
				break;
			}

			//send tty rx data to network.
			if(len > 0 && connect_fd >= 0)
			{
				int ret;
				ret = send(connect_fd, buf, len, MSG_NOSIGNAL);
				if(ret < 0)
				{
						perror("send error\n");
						FD_CLR(connect_fd, &allset);
						close(connect_fd);
						connect_fd = -1;
				}
			}
		}

		if(FD_ISSET(connect_fd, &readset))
		{
			// receive data.
			bzero(buf, sizeof(buf));
			int len = recv(connect_fd, buf ,sizeof(buf), 0);

			if(len < 0)
			{
				perror("recv error \n");
				FD_CLR(connect_fd, &allset);
 				close(connect_fd);
				connect_fd = -1;
			}
			else
			{
				//send data to uart tx
				ret = write(tty_fd, buf, len);
				if(ret < 0)
				{
					perror("write tty error\n");
					break;
				}
			}
		}
	}

	printf("Game over!\n");
	close(tty_fd);
	close(sock_fd);
	close(connect_fd);

	return ret;
}
