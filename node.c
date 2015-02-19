
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <stdio.h>
#include <errno.h>

#include "msg.h"

struct clock vector_clock[MAX_NODES];
struct clock * me;

struct sockaddr_in members[MAX_NODES];

void usage(char * cmd) {
  printf("usage: %s  portNum groupFileList logFile timeoutValue averageAYATime failureProbability \n",
	 cmd);
}

int init_group(char * groupListFileName){
	FILE *group_f = fopen( groupListFileName, "r" );
  	if (group_f == 0 || group_f ==NULL){
		printf("Error opening group file.\n");
		return -1;
  	} 	
	char line[100];
	int i = 0;
	char * pch = line;
	while(fgets(line, sizeof(line), group_f)){
		printf("%s", line);
		//pch = strtok (line," \n");
		printf("%s\n", pch);
		pch = strtok(NULL, " \n");
		printf("%s\n", pch);
	}
	fclose(group_f);
	return 0;
}


int main(int argc, char ** argv) {

  // This is some sample code feel free to delete it
  
  unsigned long  port;
  char *         groupListFileName;
  char *         logFileName;
  unsigned long  timeoutValue;
  unsigned long  AYATime;
  unsigned long  myClock = 1;
  unsigned long  sendFailureProbability;
  if (argc != 7) {
    usage(argv[0]);
    return -1;
}
  


  char * end;
  int err = 0;

  port = strtoul(argv[1], &end, 10);
  if (argv[1] == end) {
    printf("Port conversion error\n");
    err++;
  }

  groupListFileName = argv[2];
  logFileName       = argv[3];

  timeoutValue      = strtoul(argv[4], &end, 10);
  if (argv[4] == end) {
    printf("Timeout value conversion error\n");
    err++;
  }

  AYATime  = strtoul(argv[5], &end, 10);
  if (argv[5] == end) {
    printf("AYATime conversion error\n");
    err++;
  }

  sendFailureProbability  = strtoul(argv[6], &end, 10);
  if (argv[5] == end) {
    printf("sendFailureProbability conversion error\n");
    err++;
  }

  printf("Port number:              %d\n", port);
  printf("Group list file name:     %s\n", groupListFileName);
  printf("Log file name:            %s\n", logFileName);
  printf("Timeout value:            %d\n", timeoutValue);  
  printf("AYATime:                  %d\n", AYATime);
  printf("Send failure probability: %d\n", sendFailureProbability);
  printf("Starting up Node %d\n", port);
  
  printf("N%d {\"N%d\" : %d }\n", port, port, myClock++);
  printf("Sending to Node 1\n");
  printf("N%d {\"N%d\" : %d }\n", port, port, myClock++);
  
  if (err) {
    printf("%d conversion error%sencountered, program exiting.\n",
	   err, err>1? "s were ": " was ");
    return -1;
  }
  
  if(init_group(groupListFileName)==-1){
	printf("could not initialize group\n");
	return -1;
  }

  // If you want to produce a repeatable sequence of "random" numbers
  // replace the call time() with an integer.
  srandom(time(NULL));
  int i;
  for (i = 0; i < 10; i++) {
    int rn;
    rn = random(); 

    // scale to number between 0 and the 2*AYA time so that 
    // the average value for the timeout is AYA time.

    int sc = rn % (2*AYATime);
    printf("Random number %d is: %d\n", i, sc);
  }

  return 0;
  
}
