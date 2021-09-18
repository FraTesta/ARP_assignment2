/*
This code needs a configuration file, with name: ConfigurationFile.txt . This File must contain :
1 row IP 
2 row port
3 row RF for the formula

The process G must be compiled with the name "G"

The user can interract by sanding the following singlas:
- SIGUSR1 = to print the logs on the command window 
- SIGUSR2 = to stop P and G processes in order to stop tokens receaving/sending and computing
- SIGCONT = to resume P and G previously interrupted
*/

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <inttypes.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <math.h>
#include <netdb.h>

#define max(a, b) \
	({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

pid_t pid_S, pid_G, pid_L, pid_P; 

// file descriptor of all pipes
int fd_PS, fd_PG, fd_PL;

char fileTokenName[30] = "token_";

typedef struct{
	timeval time;
	double token;
}socket_msg;

socket_msg message; // ???????????????? serve o no 

typedef struct{
	char process;
	float tokenG_1; // token from G-1
	float tokenG; // current token computed by P process of this machine
}pipe_msg;

socket_msg socketMsg;

char *signame[] = {"INVALID", "SIGHUP", "SIGINT", "SIGQUIT", "SIGILL", "SIGTRAP", "SIGABRT", "SIGBUS", "SIGFPE", "SIGKILL", "SIGUSR1", "SIGSEGV", "SIGUSR2", "SIGPIPE", "SIGALRM", "SIGTERM", "SIGSTKFLT", "SIGCHLD", "SIGCONT", "SIGSTOP", "SIGTSTP", "SIGTTIN", "SIGTTOU", "SIGURG", "SIGXCPU", "SIGXFSZ", "SIGVTALRM", "SIGPROF", "SIGWINCH", "SIGPOLL", "SIGPWR", "SIGSYS", NULL};

char *pipePS = "pipePS"; 
char *pipePG = "pipePG"; 
char *pipePL = "pipePL"; 


void error(const char *msg)
{
	perror(msg);
	exit(0);
}

/// to write data in the log file 
void logFile(char pName, float tokenG_1, float tokenG)
{
	FILE *f;
	char *timeString;

	f = fopen("logFile.log", "a");
	time_t currentTime;
	currentTime = time(NULL);
	timeString = ctime(&currentTime);

	if (pName == 'S')
	{
		switch ((int)tokenG_1)
		{
		case 10://SIGUSR1
			fprintf(f, "\n-%s From %c action: dump.\n", timeString, pName);
			break;
		case 12://SIGUSR2
			fprintf(f, "\n-%s From %c action: stop.\n", timeString, pName);
			break;
		case 18://SIGCONT
			fprintf(f, "\n-%s From %c action: start.\n", timeString, pName);
			break;
		default:
			break;
		}
	}else if(pName == 'G'){
		fprintf(f, "\n-%s From %c value:%.3f.\n", timeString, pName, tokenG_1);
		fprintf(f, "-%s New value:%.3f.\n", timeString, tokenG);
	}else{

	}

	fclose(f);
}


void sig_handler(int signo)
{

	if (signo == SIGUSR1) // dump log 
	{
		char *timeString;
		time_t currentTime;
		currentTime = time(NULL);
		timeString = ctime(&currentTime);
		printf("Received SIGUSR1\n");
		printf("%d\n", pid_S);
		//logFile(pid_S, (double)signo);
		write(fd_PS, &signo, sizeof(signo));
		printf("-%sPID: %d value:%s.\n", timeString, pid_S, signame[(int)signo]);
	}
	else if (signo == SIGUSR2) // stop processes
	{
		printf("Received SIGUSR2\n");
		write(fd_PS, &signo, sizeof(signo));
		kill(pid_P, SIGSTOP);
		kill(pid_G, SIGSTOP);
	}
	else if (signo == SIGCONT) // resume processes 
	{
		printf("Received SIGCONT\n"); 
		kill(pid_P, signo);
		kill(pid_G, signo);
		write(fd_PS, &signo, sizeof(signo));
	}

}

/// read data from the configuration file
void readConfigurationFile(char *ip, char *port, int *wt, int *rf){

 	FILE * fConfig;

 	fConfig = fopen("ConfigurationFile.txt", "r");
    if (fConfig == NULL){
		perror("failure");
        exit(EXIT_FAILURE);
	}

	fscanf(fConfig, "%s", ip);
	fscanf(fConfig, "%s", port);
	fscanf(fConfig, "%d", wt);
	fscanf(fConfig, "%d", rf);

	printf("//////////////////////// \n Configuration parameters: \n");
	printf("IP : %s\n", ip);
	printf("Port : %s\n", port);
	printf("Waiting time : %d\n", wt);
	printf("RF : %d\n", *rf);

    fclose(fConfig);

}

/// functions for plotting the token values
void tokenFileInit(int rf)
{
 	FILE *tokenFile;

	char rfStr[10];
	sprintf(rfStr, "%d", rf);
	strcat(fileTokenName, rfStr);
	strcat(fileTokenName,".txt");
	tokenFile = fopen(fileTokenName, "w+");
}
void tokenFile(double token)
{
	FILE *tokenFile;
	tokenFile = fopen(fileTokenName, "a");
	fprintf(tokenFile, "%f\n",token);
	fclose(tokenFile);
}

//////////////////////////////////////////////////////////////////// S PROCESS Start ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[]) //This is the proces S which is the father of evry other processes
{
	char port[128];   //Socket port
	char ip[32];	  //IP address of the next student
	int wt;			  // Waiting time
	int rf;			  //Frequency of the sine wave

	char *argdata[4];  //data struct to pass to process G
	char *cmd = "./G"; //process G path

	readConfigurationFile(ip, port, &wt, &rf);

	float msg; // ?
	int o; // ?

	pipe_msg pipe_msg_G, pipe_msg_S;

	argdata[0] = cmd;
	argdata[1] = port;
	argdata[2] = (char *)pipePG;
	argdata[3] = NULL;

	int tokenFlag = 1; // flag which represents the sign of the token function
	// tokenFlag = 1   -->  increasing function 
	// tokenFlag = 0   -->  decreasing function 

	//Initialize file for token values plotting 
	tokenFileInit(rf);
	/*-----------------------------------------Pipes Initialization---------------------------------------*/

	if (mkfifo(pipePS, S_IRUSR | S_IWUSR) != 0) // pipe P|S
		perror("Cannot create fifo 1. Already existing?"); 

	if (mkfifo(pipePG, S_IRUSR | S_IWUSR) != 0) // pipe P|G
		perror("Cannot create fifo 2. Already existing?"); 

	if (mkfifo(pipePL, S_IRUSR | S_IWUSR) != 0) // pipe P|L
		perror("Cannot create fifo 3. Already existing?");

	fd_PS = open(pipePS, O_RDWR); //open the pipePS, dedicated to comunication with S

	if (fd_PS == 0)
	{
		error("Cannot open fifo 1");
		unlink(pipePS);
		exit(1);
	}

	fd_PG = open(pipePG, O_RDWR); //open the pipePG, dedicated to comunication with G

	if (fd_PG == 0)
	{
		perror("Cannot open fifo 2");
		unlink(pipePG);
		exit(1);
	}

	fd_PL = open(pipePL, O_RDWR); //open the pipePL, dedicated to comunication with L

	if (fd_PL == 0)
	{
		perror("Cannot open fifo 3");
		unlink(pipePL);
		exit(1);
	}

	printf("//////////////////////// \n");

	/*-------------------------------------------PROCESS P--------------------------------------*/

	pid_P = fork();
	if (pid_P < 0)
	{
		perror("Fork P");
		return -1;
	}

	if (pid_P == 0)
	{
		printf("Hey I'm P and my PID is : %d.\n", getpid());

		timeval current_time;
		float delay_time;
		struct timeval tv;  // timeout for pipe select 
		

		socket_msg line_G;
		int line_S; 
		fd_set rfds; // for pipe select 
		int retval, fd_select; // return value of select function and unified file descriptor for pipe select 

		int n; // variable to check the writes and reads
		float q; // ?
		float token_G_1; // token value coming from G-1 

		sleep(2);

		/*-------------------------------------Socket (client) initialization---------------------------*/
		int sockfd, portno;
		struct sockaddr_in serv_addr;
		struct hostent *server;
		portno = atoi(port); // port assignment

		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if (sockfd < 0)
			error("ERROR opening socket");
		
		server = gethostbyname(ip); // server assignment
		if (server == NULL)
			error("ERROR, no such host\n");
		bzero((char *)&serv_addr, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
		serv_addr.sin_port = htons(portno);
		if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
			error("ERROR connecting");

		n = write(sockfd, &line_G, sizeof(line_G));
		if (n < 0)
			error("ERROR writing to socket");

		// Pipe Select body 
		while (1)
		{
			FD_ZERO(&rfds);
			FD_SET(fd_PS, &rfds);
			FD_SET(fd_PG, &rfds);

			fd_select = max(fd_PS, fd_PG);

			tv.tv_sec = 5;
			tv.tv_usec = 0;

			retval = select(fd_select + 1, &rfds, NULL, NULL, &tv);

			switch (retval)
			{

			case 0:
			//Either no active pipes or the timeout has expired
				printf("No data avaiable.\n");
				break;

			case 1:
				// If only one pipe is active, check which is between S and G
				if (FD_ISSET(fd_PS, &rfds))
				{
					n = read(fd_PS, &line_S, sizeof(line_S));
					if (n < 0)
						error("ERROR reading from S");

					printf("From S recivedMsg = %s.\n", signame[line_S]);

					pipe_msg_S.process = 'S';
					pipe_msg_S.tokenG_1 = line_S;
					pipe_msg_S.tokenG = 0;

					n = write(fd_PL, &pipe_msg_S, sizeof(pipe_msg_S));
					if (n < 0)
						error("ERROR writing to L");		

				}
				else if (FD_ISSET(fd_PG, &rfds))
				{
					// If G, make the computation and log the results through L
					n = read(fd_PG, &line_G, sizeof(line_G));
					if (n < 0)
						error("ERROR reading from G");

					/* if (line_G < -1 || line_G > 1)
					{
						printf("Value should be between -1 and 1!.\n");
						break;
					} */

					printf("From G recived token = %.3f \n", line_G.token);

					// Creating message to send to L 
					pipe_msg_G.process = 'G';
					pipe_msg_G.tokenG_1 = line_G.token;

					/* n = write(fd_PL, &line_G, sizeof(line_G));
					if (n < 0)
						error("ERROR writing to L"); */

					// Get the current time 
					gettimeofday(&current_time, NULL);

					// Compute DT
					delay_time = (double)(current_time.tv_sec - message.time.tv_sec) + (double)(current_time.tv_usec - message.time.tv_usec)/(double)1000000;

					token_G_1 = line_G.token;
					//message.token = token_G_1 + delay_time * (1 - pow(token_G_1,2)/2 ) * 2 * 3.14 * rf;

					switch(tokenFlag)
						{
							case 0:
								message.token = token_G_1 * cos(2 * 3.14 * rf * delay_time) - sqrt(1 - pow(token_G_1,2)/2 ) * sin(2 * 3.14 * rf * delay_time);
								if (message.token < -1){
									message.token = -1;
									tokenFlag = 1;
								}
								break;
							case 1:
								message.token = token_G_1 * cos(2 * 3.14 * rf * delay_time) + sqrt(1 - pow(token_G_1,2)/2 ) * sin(2 * 3.14 * rf * delay_time);
								if (message.token > 1){
									message.token = 1;
									tokenFlag = 0;
								}
								break;
						}

					// save token values on the txt file
					tokenFile(message.token);
					
					message.time = current_time;
					//line_G += 1; 			////////////////////////////////////////////FORMULA////////////////////////////////////////////////
					//message.token = line_G;

					pipe_msg_G.tokenG = message.token;

					// Send new value to L
					n = write(fd_PL, &pipe_msg_G, sizeof(pipe_msg_G));
					if (n < 0)
						error("ERROR writing to L");

					// Write new value to the socket
					//n = write(sockfd, &line_G, sizeof(line_G));
					n = write(sockfd, &message, sizeof(message));
					if (n < 0)
						error("ERROR writing to socket");
					
					//clock_gettime(CLOCK_REALTIME,&prev_time);

					usleep(wt); 	//Simulate communication delay
				}

				sleep(1);
				break;

			case 2:
				// If two active pipes, give priority to S
				n = read(fd_PS, &line_S, sizeof(line_S));
					if (n < 0)
						error("ERROR reading from S");

					printf("From S recivedMsg = %s.\n", signame[line_S]);

					pipe_msg_S.process = 'S';
					pipe_msg_S.tokenG_1 = line_S;
					pipe_msg_S.tokenG = 0;

					n = write(fd_PL, &pipe_msg_S, sizeof(pipe_msg_S));
					if (n < 0)
						error("ERROR writing to L");	
				break;

			default:
				perror("You should not be here!");
				break;
			}
		}

		//close all fifo 
		close(fd_PS);
		unlink(pipePS);

		close(fd_PG);
		unlink(pipePG);

		close(fd_PL);
		unlink(pipePL);

		close(sockfd);
	}

	else
	{
//////////////////////////////////////////////////////////////////////// Process L /////////////////////////////////////////////////////////////////////////////////////////// 
		pid_L = fork();

		if (pid_L < 0)
		{
			perror("Fork L");
			return -1;
		}

		if (pid_L == 0) 
		{
			printf("Hey I'm L and my PID is : %d.\n", getpid());
			pipe_msg log_msg;
			while (1)
			{
				o = read(fd_PL, &log_msg, sizeof(log_msg));
				if (o < 0)
					error("ERROR reciving file to L");

				logFile(log_msg.process, log_msg.tokenG_1, log_msg.tokenG);
			}
			close(fd_PL);
			unlink(pipePL);
		}

		else
		{

//////////////////////////////////////////////////////////////////////// Process G ///////////////////////////////////////////////////////////////////////////////////////////

			pid_G = fork(); 
			if (pid_G < 0)
			{
				perror("Fork G");
				return -1;
			}

			if (pid_G == 0)
			{
				printf("Hey I'm G and my PID is : %d.\n", getpid());
				execvp(argdata[0], argdata);
				error("Exec fallita");
				return 0;
			}

//////////////////////////////////////////////////////////////////////// Process S End ///////////////////////////////////////////////////////////////////////////////////////////


			pid_S = getpid();

			printf("Hey I'm S and my PID is : %d.\n", pid_S);


			// signal management                        

			if (signal(SIGUSR1, sig_handler) == SIG_ERR)
				printf("Can't catch SIGUSR1\n");

			if (signal(SIGCONT, sig_handler) == SIG_ERR)
				printf("Can't catch SIGCONT\n");

			if (signal(SIGUSR2, sig_handler) == SIG_ERR)
				printf("Can't catch SIGUSER2\n");


			sleep(5);


			while (1)
			{
				// sleep ???????????????????????????????????????????????????????????????????????????????????????????????????????????????????????
			};

			close(fd_PS);
			unlink(pipePS);

			close(fd_PG);
			unlink(pipePG);

			close(fd_PL);
			unlink(pipePL);
			return 0;
		}
	}
}
