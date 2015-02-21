
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
unsigned int electionID;

void usage(char * cmd) {
  printf("usage: %s  portNum groupFileList logFile timeoutValue averageAYATime failureProbability \n",
	 cmd);
}

//I put all of this in another method so that it would clean up the main function
//and so that I could reuse it if need be. But mostly because there is a lot in main.
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
		portstr = strtok(NULL, " \n");
		port = strtoul(portstr, NULL, 10);
		//Set the addrinfo for each member of the group
		//res = nodes[i];
		int err=getaddrinfo(hostname,portstr,&hints,&nodes[i]);
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
	//We may not have 9 members total, so here we initialize the remaining
	//spots in the vector_clock so that we know they don't exist.
	total_nodes = i;
	while (i<MAX_NODES){
		vector_clock[i].nodeId=0;
		vector_clock[i].time=0;
		i++;
	}
	fclose(group_f);
	return 0;
}

//we need this method to ensure that the message is in the correct endianness
//so we go through all the types and convert them
void net_to_host_msg(struct msg * last_msg){
	last_msg->msgID = ntohl(last_msg->msgID);
	last_msg->electionID = ntohl(last_msg->electionID);
	int i = 0;
	while(i< MAX_NODES){
		last_msg->vectorClock[i].nodeId = ntohl(last_msg->vectorClock[i].nodeId);
		last_msg->vectorClock[i].time = ntohl(last_msg->vectorClock[i].time);
		i++;
	}
}

void host_to_net_msg(struct msg * new_msg){
	new_msg->msgID = htonl(new_msg->msgID);
	new_msg->electionID = htonl(new_msg->electionID);
	int i = 0;
	while(i<MAX_NODES){
		new_msg->vectorClock[i].nodeId = htonl(new_msg->vectorClock[i].nodeId);
		new_msg->vectorClock[i].time = htonl(new_msg->vectorClock[i].time);
		i++;
	}
}

//This is for debugging purposes; I wanted to make sure the clocks were right
void print_vector_clock(){
	int i = 0;
	printf("{");
	while (i<MAX_NODES-1){
		printf(" N%d:%d,", vector_clock[i].nodeId, vector_clock[i].time);
		i++;
	}
	printf(" N%d:%d }\n", vector_clock[i].nodeId, vector_clock[i].time);
}

void test_net_to_host_msg(){
  struct msg tester;
  tester.msgID = htonl(ELECT);
  tester.electionID = htonl(5);
  memcpy(&tester, &vector_clock, sizeof(vector_clock));
  net_to_host_msg(&tester);
  host_to_net_msg(&tester);
}

unsigned short get_in_port(struct sockaddr *sa){
    if (sa->sa_family == AF_INET) {
        return ntohs(((struct sockaddr_in*)sa)->sin_port);
    }

    return ntohs(((struct sockaddr_in6*)sa)->sin6_port);
}

//This function takes the sender and finds that sender in our group.
//Then we assign the coordinator pointer to that node. Important to remember that our
//nodes array are pointers to addrinfo structs from which we can get an sockaddr.
void set_coordinator(struct sockaddr_in * sender){
	int i = 0;
	unsigned int node_port= 0;
	unsigned int sender_port = get_in_port((struct sockaddr *) sender);
	int done = 0;
	printf("sender port : %u\n", sender_port);
	while (i<total_nodes){
		node_port = get_in_port(nodes[i]->ai_addr);
		if (node_port == sender_port){
			//make this guy the coord
			printf("The coordinator has Id: %d\n", sender_port);
			coordinator = nodes[i];
			done = 1;
		}
		i++;
	}
	if (!done){
		printf("the person who sent this is not in the group\n");
	}
}

//Since I had no way of really testing if things worked I created this
//method to fake a sender and then set the sender.
void test_set_coordinator(){
	struct sockaddr_in sender;
	sender.sin_family = AF_INET;
	sender.sin_port = htons(9000);
	set_coordinator(&sender);
}

int start_election(int sockfd){
	struct msg new_msg;
	new_msg.msgID = ELECT;
	new_msg.electionID = electionID++;
	memcpy(&new_msg, &vector_clock, sizeof(vector_clock));
	int i = 0;
	while (i<total_nodes){
		//send to all nodes above you.
		if (get_in_port(nodes[i]->ai_addr) > my_clock->nodeId){
			//send it to this person
			if (sendto(sockfd, &new_msg, sizeof(new_msg), 0, nodes[i]->ai_addr, nodes[i]->ai_addrlen) == -1){
				printf("could not send\n");
				return -1;
			} else{
				printf("sent!\n");
			}
		}
		i++;
	}
}


int main(int argc, char ** argv) {

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
  printf("First electionID:	    %d\n", electionID);
  printf("Starting up Node %d\n", port);
  
  srandom(time(NULL));
  electionID = random() %100; 
  
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
  printf("N%d {\"N%d\" : %d }\n", port, port, my_clock->time++);
  
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
  struct timeval tv;
  tv.tv_sec = timeoutValue;
  tv.tv_usec = 0;
  if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
	printf("Could not set timeout on socket\n");
  }
  if (bind(sockfd, (struct sockaddr *)&si_me, sizeof(si_me))==-1) {
	close(sockfd);
	perror("failed to bind\n");
	return -1;
  }
  //testing
  //test_set_coordinator();

  //1. We send AYA to the coordinator on every cycle, getting back IAA messages.
  // If we don't get an IAA, then we send an elect to everyone above us.
  //2. If we get an elect we need to send elect messages to the nodes above us.
  //2a. We set timeouts for each of the nodes
  //2b. If we get a message back we go back to 1
  //2c If we don't get any messages back then we send out a coord to everyone below.
  //3. If the message is a coord we just set the coordinator variable to the new node.
  //4. If the message is an elect we send an answer.
  struct msg last_msg;  //this will always store the last message we receive
  memset(&last_msg, 0, sizeof(last_msg)); //make sure it doesn't contain any funky values to start
  struct sockaddr_in sender;  //this is to store who sent the most recent message.
  memset(&sender, 0, sizeof(sender));
  socklen_t sender_len = sizeof(sender);
  //wait to receive a message and timeout if we don't.
  if(recvfrom(sockfd, &last_msg, sizeof(last_msg), 0,(struct sockaddr *) &sender, &sender_len)<0){
  	printf("timed out on receive\n");
	//Now we start an election.
	start_election(sockfd);
  }
  else{
	//make sure the message is in the correct endianness
	net_to_host_msg(&last_msg);
	if (last_msg.msgID == COORD){
		set_coordinator(&sender);
	}
  }

  close(sockfd);

  // If you want to produce a repeatable sequence of "random" numbers
  // replace the call time() with an integer.
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
