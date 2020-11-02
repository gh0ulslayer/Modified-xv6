#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"

#define ll volatile long long int
#define MAX 10000000

int main(int argc, char **argv)
{
	int status = 0;
	int wtime;
	int rtime;
	int pid;
	pid = fork();
	if (pid == 0)
	{
		if (argc == 1)
		{
			printf(1, "Timing default program\n");
			ll x;
			x = 0;
			while(x != MAX)
			{
				x++;
			}
			exit();
		}

		else
		{
			printf(1, "Timing %s\n", argv[1]);
			int doo = exec(argv[1], argv + 1); 
			if (doo < 0)
			{
				printf(2, "exec %s failed\n", argv[1]);
				exit();
			}
		}
	}
	else if (pid < 0)
	{
		printf(2, " Forking failed !!\n");
		exit();
	}
	else if (pid > 0)
	{
		status = waitx(&wtime, &rtime);
		
		printf(1, "-->%s has\n-->Wait time: %d\n-->Run time: %d\n-->Status %d\n\n",argv[1] , wtime, rtime, status);

		exit();
	}
}