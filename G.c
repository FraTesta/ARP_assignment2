/* A simple server in the internet domain using TCP
   The port number is passed as an argument */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

typedef struct{
	timeval time;
	double token;
}socket_msg;

void error(const char *msg)
{
     perror(msg);
     exit(1);
}

int main(int argc, char *argv[])
{
     printf("G process:\n");

     int sockfd, newsockfd, portno;
     socklen_t clilen;
     //float receave_buffer;
     socket_msg receave_buffer;
     struct sockaddr_in serv_addr, cli_addr;
     int n;
     if (argc < 2)
     {
          fprintf(stderr, "ERROR, not enougth parameters\n");
          exit(1);
     }
     sockfd = socket(AF_INET, SOCK_STREAM, 0);
     if (sockfd < 0)
          error("ERROR opening socket");

     bzero((char *)&serv_addr, sizeof(serv_addr));
     portno = atoi(argv[1]); // port number in the first position of argv
     char *pipePG = argv[2];

     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = INADDR_ANY;
     serv_addr.sin_port = htons(portno);
     if (bind(sockfd, (struct sockaddr *)&serv_addr,
              sizeof(serv_addr)) < 0)
          error("ERROR on binding");

     listen(sockfd, 5);
     clilen = sizeof(cli_addr);

     newsockfd = accept(sockfd,
                        (struct sockaddr *)&cli_addr,
                        &clilen);
     if (newsockfd < 0)
          error("ERROR on accept");
     bzero(&receave_buffer, sizeof(receave_buffer));

     int fd_PG = open(pipePG, O_RDWR); // apro la fifo
     while (1)
     {

          n = read(newsockfd, &receave_buffer, sizeof(receave_buffer)); //leggo dato socket
          if (n < 0)
               error("ERROR reading from socket");

          int nb2 = write(fd_PG, &receave_buffer, sizeof(receave_buffer)); //scrivo ciò che leggo nella fifo

          //printf("Here is the socket_msg: %.3f\n", receave_buffer);

          /* n = write(newsockfd, "I got your socket_msg", 18);
          if (n < 0)
               error("ERROR writing to socket"); */
     }

     close(fd_PG);
     unlink(pipePG);

     close(newsockfd);
     close(sockfd);
     
     return 0;
}
