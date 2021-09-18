/*
This code needs a configuration file, with name: ConfigurationFile.txt . This File must contain :
1 row IP 
2 row port
3 row RF for the formula

The process G must be compiled with the name "G"

signals must be sent to S process, and are the following:
SIGUSR1 : dump L process
SIGUSR2 : stop G and P process
SIGCONT : start G and P prosess
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
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

double newToken; // ?
double *recivedMsg;
char *timeString;
pid_t pid_S, pid_G, pid_L, pid_P;

char IP[20];
char port[20];
char RF[20];

char *signame[] = {"INVALID", "SIGHUP", "SIGINT", "SIGQUIT", "SIGILL", "SIGTRAP", "SIGABRT", "SIGBUS", "SIGFPE", "SIGKILL", "SIGUSR1", "SIGSEGV", "SIGUSR2", "SIGPIPE", "SIGALRM", "SIGTERM", "SIGSTKFLT", "SIGCHLD", "SIGCONT", "SIGSTOP", "SIGTSTP", "SIGTTIN", "SIGTTOU", "SIGURG", "SIGXCPU", "SIGXFSZ", "SIGVTALRM", "SIGPROF", "SIGWINCH", "SIGPOLL", "SIGPWR", "SIGSYS", NULL};

char *myfifo1 = "myfifo1"; 
char *myfifo2 = "myfifo2"; 
char *myfifo3 = "myfifo3"; 

void error(const char *msg)
{
	perror(msg);
	exit(0);
}

void logFile(pid_t process_id, double message)
{
	FILE *f;
	f = fopen("logFile.log", "a");
	time_t currentTime;
	currentTime = time(NULL);
	timeString = ctime(&currentTime);
	fprintf(f, "-%sPID: %d value:%.3f.\n", timeString, process_id, message);
	fprintf(f, "-%s%.3f.\n\n", timeString, newToken);

	fclose(f);
}

void sig_handler(int signo)
{
	if (signo == SIGUSR1) //dump log signal
	{
		printf("Received SIGUSR1\n");
		printf("%d\n", pid_S);
		logFile(pid_S, (double)signo);
		printf("-%sPID: %d value:%s.\n", timeString, pid_S, signame[(int)signo]);
		printf("-%s%.3f.\n\n", timeString, newToken);
	}
	else if (signo == SIGCONT)
	{
		printf("Received SIGCONT\n"); // start signal
		kill(pid_P, signo);
		kill(pid_G, signo);
	}
	else if (signo == SIGUSR2) // stop signal
	{
		printf("Received SIGUSR2\n");
		kill(pid_P, SIGSTOP);
		kill(pid_G, SIGSTOP);
	}
}

void readConfigurationFile(){

 FILE * fConfig;
    char * line = NULL;
    size_t len = 5;

 fConfig = fopen("ConfigurationFile.txt", "r");
    if (fConfig == NULL){

        exit(EXIT_FAILURE);
	perror("failure");
	}

    fscanf(fConfig, "%s", IP);
    fscanf(fConfig, "%s", port);
    //fscanf(fConfig, "%s", RF);

	
     fclose(fConfig);
    if (line)
        free(line);

}

//////////////////////////////////////////////////////////////////// S PROCESS Start ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[]) //This is the proces S which is the father of evry other processes
{
	int res; // ?
	char *argdata[4];  //process G data
	char *cmd = "./G"; //process G path

	readConfigurationFile();
	
	struct timeval tv; //time variable for the "select"

	float msg; // message from L
	int o; // ?

	argdata[0] = cmd;
	argdata[1] = port;
	argdata[2] = myfifo2;
	argdata[3] = NULL;

	/*-----------------------------------------Pipes Initialization---------------------------------------*/

	if (mkfifo(myfifo1, S_IRUSR | S_IWUSR) != 0) 
		perror("Cannot create fifo 1. Already existing?");

	if (mkfifo(myfifo2, S_IRUSR | S_IWUSR) != 0)
		perror("Cannot create fifo 2. Already existing?");

	if (mkfifo(myfifo3, S_IRUSR | S_IWUSR) != 0) 
		perror("Cannot create fifo 3. Already existing?");

	int fd1 = open(myfifo1, O_RDWR); //open the pipe1, dedicated to comunication with S

	if (fd1 == 0)
	{
		error("Cannot open fifo 1");
		unlink(myfifo1);
		exit(1);
	}

	int fd2 = open(myfifo2, O_RDWR); //open the pipe2, dedicated to comunication with G

	if (fd2 == 0)
	{
		perror("Cannot open fifo 2");
		unlink(myfifo2);
		exit(1);
	}

	int fd3 = open(myfifo3, O_RDWR); //open the pipe3, dedicated to comunication with L

	if (fd3 == 0)
	{
		perror("Cannot open fifo 3");
		unlink(myfifo3);
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

		float token;
		int w; // variable to check the "writes" 
		float q; // ?

		sleep(2);

		/*-------------------------------------Socket (client) initialization---------------------------*/
		int sockfd, portno;
		struct sockaddr_in serv_addr;
		struct hostent *server;
		portno = atoi(port); // port assignment

		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if (sockfd < 0)
			error("ERROR opening socket");
		
		server = gethostbyname(IP); // server assignment
		if (server == NULL)
			error("ERROR, no such host\n");
		bzero((char *)&serv_addr, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
		serv_addr.sin_port = htons(portno);
		if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
			error("ERROR connecting");

		w = write(sockfd, &token, sizeof(token));
		if (w < 0)
			error("ERROR writing to socket");

		while (1)
		{
			//SELECT initialization
			int retval, n;
			fd_set rfds; 
			int fd = fd1; // fd bigger, initialized as fd1

		
			FD_ZERO(&rfds);
			FD_SET(fd1, &rfds);
            FD_SET(fd2, &rfds);

			tv.tv_sec = 2;
			tv.tv_usec = 0;

            if(fd1<fd2) // check what fd is bigger
               fd = fd2;

			retval = select(fd + 1, &rfds, NULL, NULL, &tv);

			if (retval == -1)
				perror("select()");


			/////SWITCH SELECT
			switch (retval)
			{

			case 0:
				printf("no avaiable data\n");
				break;

			case 1:
				if (FD_ISSET(fd1,&rfds))
				{
					n = read(fd1, &q, sizeof(q));
					printf("From S recived message = %.3f \n", q);
					sleep((int)q);
				}
				else if (FD_ISSET(fd2,&rfds))
				{
					n = read(fd2, &token, sizeof(token));
				
					printf("From G recived token = %.3f \n", token);

					w = write(fd3, &token, sizeof(token));
					if (w < 0)
						error("ERROR writing to L");

                    //formula
					token += 1;

                    newToken = token;

                    w = write(fd3, &token, sizeof(token)); //rewrite in L the new token value
					if (w < 0)
						error("ERROR writing to L");

					w = write(sockfd, &token, sizeof(token)); //send the new token to G+1
					if (w < 0)
						error("ERROR writing to socket");
				}
				break;

			case 2: 
				n = read(fd1, &token, sizeof(token));
				printf("From S recived message = %.3f \n", token);
				break;

			default:
				printf("Select error!\n");
				break;
			}
            sleep(1);
		}

		wait(&res);

		//close all fifo 
		close(fd1);
		unlink(myfifo1);

		close(fd2);
		unlink(myfifo2);

		close(fd3);
		unlink(myfifo3);
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
				o = read(fd3, &msg, sizeof(msg));
				if (o < 0)
					error("ERROR reciving file to L");
				logFile(getpid(), msg);
			}
			close(fd3);
			unlink(myfifo3);
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



			while (1)
			{
            g = write(fd1, &t, sizeof(t));
			if (g < 0)
				error("size");
			sleep(10);

			};

			return 0;
		}
	}
}
