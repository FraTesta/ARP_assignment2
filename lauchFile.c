/*
This code needs a configuration file, with name: ConfigurationFile.txt . This File must contain :
1 row IP 
2 row port
3 row RF for the formula

The process G must be compiled with the name "G"
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

double newToken; // ?
double *recivedMsg;
char *timeString; // serve 
int tokenFlag; // ?
pid_t pid_S, pid_G, pid_L, pid_P; // serve

// non servono
//char IP[20];
//char port[20];
//char RF[20];

typedef struct{
	timeval time;
	double token;
}socket_msg;

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

void logFile(char pName, float tokenG_1, float tokenG)
{
	FILE *f;
	f = fopen("logFile.log", "a");
	time_t currentTime;
	currentTime = time(NULL);
	timeString = ctime(&currentTime);

	if (pName == 'S')
	{
		switch ((int)tokenG_1)
		{
		case 10://SIGUSR1
			fprintf(f, "\n-%s From %c action: stop.\n", timeString, pName);
			break;
		case 12://SIGUSR2
			fprintf(f, "\n-%s From %c action: start.\n", timeString, pName);
			break;
		case 18://SIGCONT
			fprintf(f, "\n-%s From %c action: dump.\n", timeString, pName);
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

// E' diverso da robbi 
void sig_handler(int signo)
{
	if (signo == SIGUSR1) // dump log 
	{
		printf("Received SIGUSR1\n");
		printf("%d\n", pid_S);
		//logFile(pid_S, (double)signo);
		write(fd_PS, &signo, sizeof(signo));
		printf("-%sPID: %d value:%s.\n", timeString, pid_S, signame[(int)signo]);
	}
	else if (signo == SIGCONT)
	{
		printf("Received SIGCONT\n"); // resume processes 
		kill(pid_P, signo);
		kill(pid_G, signo);
		write(fd_PS, &signo, sizeof(signo));
	}
	else if (signo == SIGUSR2) // stop processes
	{
		printf("Received SIGUSR2\n");
		write(fd_PS, &signo, sizeof(signo));
		kill(pid_P, SIGSTOP);
		kill(pid_G, SIGSTOP);
	}
}

void readConfigurationFile(char *ip, char *port, int *wt, int *rf){

 	FILE * fConfig;
    char * line = NULL;
    size_t len = 5;

 	fConfig = fopen("ConfigurationFile.txt", "r");
    if (fConfig == NULL){
		perror("failure");
        exit(EXIT_FAILURE);
	}

	fscanf(fp, "%s", ip);
	fscanf(fp, "%s", port);
	fscanf(fp, "%d", wt);
	fscanf(fp, "%d", rf);

	printf("//////////////////////// \n Configuration parameters: \n");
	printf("IP : %s\n", ip);
	printf("Port : %s\n", port);
	printf("Waiting time : %d\n", wt);
	printf("RF : %d\n", *rf);

    fclose(fConfig);
    if (line) // ?????????????????????????????
        free(line);

}

// functions for plotting the token values
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

	int res; // ???????
	char *argdata[4];  //data struct to pass to process G
	char *cmd = "./G"; //process G path

	readConfigurationFile(ip, port, &wt, &rf);
	
	struct timeval tv;  // ?????

	float msg; // ?
	int o; // ?

	argdata[0] = cmd;
	argdata[1] = port;
	argdata[2] = (char *)pipePG;
	argdata[3] = NULL;

	bool tokenFlag = true; // flag which represents the sign of the token function
	// tokenFlag = true   -->  increasing function 
	// tokenFlag = false   -->  decreasing function 

	//Initialize file for token values plotting 
	tokenFileInit(rf);
	/*-----------------------------------------Pipes Initialization---------------------------------------*/

	if (mkfifo(pipePS, S_IRUSR | S_IWUSR) != 0) // pipe P|S
		perror("Cannot create fifo 1. Already existing?"); 

	if (mkfifo(pipePG, S_IRUSR | S_IWUSR) != 0) // pipe P|G
		perror("Cannot create fifo 2. Already existing?"); 

	if (mkfifo(pipePL, S_IRUSR | S_IWUSR) != 0) // pipe P|L
		perror("Cannot create fifo 3. Already existing?");

	int fd_PS = open(pipePS, O_RDWR); //open the pipePS, dedicated to comunication with S

	if (fd_PS == 0)
	{
		error("Cannot open fifo 1");
		unlink(pipePS);
		exit(1);
	}

	int fd_PG = open(pipePG, O_RDWR); //open the pipePG, dedicated to comunication with G

	if (fd_PG == 0)
	{
		perror("Cannot open fifo 2");
		unlink(pipePG);
		exit(1);
	}

	int fd_PL = open(pipePL, O_RDWR); //open the pipePL, dedicated to comunication with L

	if (fd_PL == 0)
	{
		perror("Cannot open fifo 3");
		unlink(pipePL);
		exit(1);
	}

	printf("\n");

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

		float line;
		int m; // variable to check the writes and reads
		float q; // ?
		double old_tok, new_tok; // token values

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

		m = write(sockfd, &line, sizeof(line));
		if (m < 0)
			error("ERROR writing to socket");

		while (1)
		{
			//SELECT 1 : pipe with S
			int retval1, retval2, n;
			fd_set rfds1, rfds2; //come mi riferisco a un fd
			int fd;

			//SELECT 1
			FD_ZERO(&rfds1);
			FD_SET(fd_PS, &rfds1);

			tv.tv_sec = 2;
			tv.tv_usec = 0;
			retval1 = select(fd_PS + 1, &rfds1, NULL, NULL, &tv);

			if (retval1 == -1)
				perror("select()");

			//SELECT 2
			FD_ZERO(&rfds2);
			FD_SET(fd_PG, &rfds2);

			tv.tv_sec = 2;
			tv.tv_usec = 0;
			retval2 = select(fd_PG + 1, &rfds2, NULL, NULL, &tv);

			if (retval2 == -1)
				perror("select()");

			/////SWITCH
			switch (retval1 + retval2)
			{

			case 0:
				printf("no avaiable data\n");
				break;

			case 1:
				if (retval1 == 1)
				{
					n = read(fd_PS, &q, sizeof(q));
					printf("From S recivedMsg = %.3f \n", q);
					sleep((int)q);
				}
				else if (retval2 == 1)
				{
					n = read(fd_PG, &line, sizeof(line));
					//if (line < -1 || line > 1)
					//break;
					printf("From G recivedMsg = %.3f \n", line);

					m = write(fd_PL, &line, sizeof(line));
					if (m < 0)
						error("ERROR writing to L");

					line += 1;

					m = write(sockfd, &line, sizeof(line));
					if (m < 0)
						error("ERROR writing to socket");
				}
				break;

			case 2:
				n = read(fd_PS, &line, sizeof(line));
				printf("From S recivedMsg = %.3f \n", line);
				break;

			default:
				printf("Select error!!!!\n");
				break;
			}
		}

		wait(&res);

		//close all fifo 
		close(fd_PS);
		unlink(pipePS);

		close(fd_PG);
		unlink(pipePG);

		close(fd_PL);
		unlink(pipePL);
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
			while (1)
			{
				o = read(fd_PL, &msg, sizeof(msg));
				if (o < 0)
					error("ERROR reciving file to L");
				logFile(getpid(), msg);
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

			float t = 1;
			int g;
			sleep(20);

			g = write(fd_PS, &t, sizeof(t));
			if (g < 0)
				error("size");
			sleep(10);
/*
			while (1)
			{
			};
*/
			return 0;
		}
	}
}
