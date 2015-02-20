
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>

#include "msg.h"

struct clock vector_clock[MAX_NODES];
struct clock * my_clock = NULL;
int total_nodes = 0;
struct addrinfo * nodes[MAX_NODES];
struct addrinfo * coordinator;

void usage(char * cmd) {
  printf("usage: %s  portNum groupFileList logFile timeoutValue averageAYATime failureProbability \n",
	 cmd);
}

int init_addrhint(struct addrinfo * hints){
	memset(hints,0,sizeof(*hints));
	hints->ai_family=AF_INET;
	hints->ai_socktype=SOCK_DGRAM;
	hints->ai_protocol=0;
	hints->ai_flags=AI_ADDRCONFIG;

}

int init_group(char * groupListFileName, unsigned int my_port){
	FILE *group_f = fopen( groupListFileName, "r" );
  	if (group_f == 0 || group_f ==NULL){
		printf("Error opening group file.\n");
		return -1;
  	} 	
	char line[100];
	struct addrinfo hints;
	init_addrhint(&hints);
	char * portstr, *hostname;
	unsigned int port;
	struct addrinfo* res;
	int i = 0;
	while(fgets(line, sizeof(line), group_f)){
		// issues with tokenizing here
		hostname = strtok(line," \n");	
		printf("%s\n", hostname);
		portstr = strtok(NULL, " \n");
		port = strtoul(portstr, NULL, 10);
		printf("%d\n", port);

		//Set the addrinfo for each member of the group
		res = nodes[i];
		int err=getaddrinfo(hostname,portstr,&hints,&res);
		if(err!=0){
			printf("did not get the addrinfo for %s\n", hostname);
		}

		//Here we will initialise the vector clock.
		//first make sure this nodeId doesnt already exist
		int j = 0;
		while (j<i){
			if (vector_clock[j].nodeId == port){
				printf("This nodeId already exists\n");
				return -1;
			}
			j++;
		}
		vector_clock[i].nodeId = port;
		vector_clock[i].time = 0;
		
		//store a pointer to our own vector clock, fail if our nodeId isn't there
		if (port == my_port){
			my_clock = &vector_clock[i];
		}
		i++;
	}
	//We may not have 9 members total, so here we take care of the remaining
	//spots in the vecto_clock.
	total_nodes = i;
	while (i<MAX_NODES){
		vector_clock[i].nodeId=0;
		vector_clock[i].time=0;
		i++;
	}
	fclose(group_f);
	return 0;
}


void print_vector_clock(){
	int i = 0;
	printf("{");
	while (i<MAX_NODES-1){
		printf(" N%d:%d,", vector_clock[i].nodeId, vector_clock[i].time);
		i++;
	}
	printf(" N%d:%d }\n", vector_clock[i].nodeId, vector_clock[i].time);
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
  
  if(init_group(groupListFileName, port)==-1){
	printf("could not initialize group\n");
	return -1;
  }
  if (my_clock == NULL){
	printf("My nodeId was not in the list\n");
	return -1;
  }
  print_vector_clock();

  int sockfd;
  struct sockaddr_in si_me;
  memset((char *) &si_me, 0, sizeof(si_me));
  si_me.sin_family = AF_INET;
  si_me.sin_port = htons(port);
  si_me.sin_addr.s_addr = htonl(INADDR_ANY);
  if ((sockfd=socket(AF_INET, SOCK_DGRAM, 0))==-1){
	printf("failed to create socket\n");
	return -1;
  }
  if (bind(sockfd, (struct sockaddr *)&si_me, sizeof(si_me))==-1) {
	close(sockfd);
	perror("failed to bind");
	return -1;
  }

  //1. We send AYA to the coordinator on every cycle, getting back IAA messages.
  // If we don't get an IAA, then we send an elect to everyone above us.
  //2. If we get an elect we need to send elect messages to the nodes above us.
  //2a. We set timeouts for each of the nodes
  //2b. If we get a message back we go back to 1
  //2c If we don't get any messages back then we send out a coord to everyone below.
  //3. If the message is a coord we just set the coordinator variable to the new node.
  //4. If the message is an elect we send an answer.

  close(sockfd);

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
