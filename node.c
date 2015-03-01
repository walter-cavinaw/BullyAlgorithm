
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <stdlib.h>
#include <fcntl.h>

#include "msg.h"

#define stdName "none"

struct clock vector_clock[MAX_NODES];
struct clock * my_clock = NULL;
struct addrinfo *my_addrinfo;
int total_nodes = 0;
struct addrinfo * nodes[MAX_NODES];
struct addrinfo * coordinator;
unsigned int electionID;
char * logFile;
int failure_rate;

void usage(char * cmd) {
  printf("usage: %s  portNum groupFileList logFile timeoutValue averageAYATime failureProbability \n",
	 cmd);
}

int should_send(){
	int rand_int = random();
	if ((rand_int % 100) < failure_rate){
		return 0;
	}
	return 1;
}

unsigned short get_in_port(struct sockaddr *sa){
    if (sa->sa_family == AF_INET) {
        return ntohs(((struct sockaddr_in*)sa)->sin_port);
    }

    return ntohs(((struct sockaddr_in6*)sa)->sin6_port);
}

void print_vector_clock(int fd){
	int i = 0;
	dprintf(fd,"N%d {", my_clock->nodeId);
	while (i<MAX_NODES-1){
		dprintf(fd, "\"N%d\": %d, ", vector_clock[i].nodeId, vector_clock[i].time);
		i++;
	}
	dprintf(fd, "\"N%d\":%d}\n", vector_clock[i].nodeId, vector_clock[i].time);
}

void set_vector_clock(struct msg * last_msg){
	int local_iter = MAX_NODES - 1;
	while (local_iter > -1){
		int remote_iter = 0;
		while (remote_iter < MAX_NODES){
			if(vector_clock[local_iter].nodeId == last_msg->vectorClock[remote_iter].nodeId){
				if(vector_clock[local_iter].time < last_msg->vectorClock[remote_iter].time){
					vector_clock[local_iter].time = last_msg->vectorClock[remote_iter].time;
				}
			}
			remote_iter++;
		}
		local_iter--;
	}
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

int init_logging(char * logFileName){
	logFile = logFileName;
	if (strcmp(logFile, "none")==0){
		printf("use stdout\n");
	} else {
		int logf = open(logFile, O_WRONLY|O_TRUNC|O_CREAT, 0666);
		close(logf);
	}
}

int log_msg(char * message){
	struct flock fl;
	int fd = 1;
	fl.l_type   = F_WRLCK;  /* F_RDLCK, F_WRLCK, F_UNLCK    */
	fl.l_whence = SEEK_SET; /* SEEK_SET, SEEK_CUR, SEEK_END */
	fl.l_start  = 0;        /* Offset from l_whence         */
	fl.l_len    = 0;        /* length, 0 = to EOF           */
	fl.l_pid    = getpid(); /* our PID                      */

	if (strcmp(logFile, stdName) != 0){
		fd = open(logFile, O_WRONLY |O_APPEND);
	}
	if (fcntl(fd, F_SETLKW, &fl) == -1){
		perror("fcntl");
	}
	if(dprintf(fd, "%s\n", message) < 0 ){
		perror("write failed");
	}
	print_vector_clock(fd);
	if (fd!= 1){
		close(fd);
	}
	fl.l_type   = F_UNLCK;  /* tell it to unlock the region */
	fcntl(fd, F_SETLK, &fl); /* set the region to unlocked */
	
	my_clock->time++;
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
		if (port == my_port){
			my_addrinfo = nodes[i];
		}
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
	my_clock->time++;
	return 0;
}

int i_am_coordinator(){
	if (coordinator != NULL){
		if (get_in_port(coordinator->ai_addr) == my_clock->nodeId){
			return 1;
		}
	}
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

void create_msg(struct msg * new_msg, msgType type,unsigned int prev_electID){
	new_msg->msgID = type;
	if (prev_electID == 0){
		new_msg->electionID = ++electionID;
	} else{
		new_msg->electionID = prev_electID;
	}
	if(type==ANSWER){
		printf("%d : about to send answer for %u\n", my_clock->nodeId, new_msg->electionID);
	} else if (type==ELECT){
		printf("%d : about to send elect for %u\n", my_clock->nodeId, new_msg->electionID);
	}
	memcpy(&(new_msg->vectorClock), &vector_clock, sizeof(vector_clock));
	host_to_net_msg(new_msg);
}

void send_iaa(int sockfd, struct sockaddr_in * sender){
	//we are given the sender and must respond with IAA
	struct msg new_msg;
	create_msg(&new_msg, IAA, get_in_port((struct sockaddr *)sender));
	if (should_send()){
		if (sendto(sockfd, &new_msg, sizeof(new_msg), 0, (struct sockaddr *) sender, sizeof(struct sockaddr))==-1){
			log_msg("could not send answer");
		}
		}
		log_msg("IAA has been sent");
}

void send_aya(int sockfd){
	//we know the coordinator so we send aya to him
	struct msg new_msg;
	create_msg(&new_msg, AYA, my_clock->nodeId);
	if (should_send()){
	if (sendto(sockfd, &new_msg, sizeof(new_msg), 0, (struct sockaddr *) coordinator->ai_addr, coordinator->ai_addrlen)==-1){
		log_msg("could not send AYA");
		return;
	}
	}
	log_msg("AYA has been sent");
}

void send_answer(int sockfd, unsigned int prev_electID, struct sockaddr_in * sender){
	//we know the sender and must give an answer
	//we need to respond with the correct electionId so he knows what election we
	//are referring to
	struct msg new_msg;
	create_msg(&new_msg, ANSWER, prev_electID);
	if (should_send()){
	if (sendto(sockfd, &new_msg, sizeof(new_msg), 0, (struct sockaddr *) sender, sizeof(struct sockaddr))==-1){
		log_msg("could not send answer");
		return;
	}
	}
	log_msg("ANSWER has been sent");

}

void test_net_to_host_msg(){
  struct msg tester;
  tester.msgID = htonl(ELECT);
  tester.electionID = htonl(5);
  memcpy(&tester, &vector_clock, sizeof(vector_clock));
  net_to_host_msg(&tester);
  host_to_net_msg(&tester);
}


//This function takes the sender and finds that sender in our group.
//Then we assign the coordinator pointer to that node. Important to remember that our
//nodes array are pointers to addrinfo structs from which we can get an sockaddr.
void set_coordinator(struct sockaddr_in * sender){
	int i = 0;
	unsigned int node_port= 0;
	unsigned int sender_port = get_in_port((struct sockaddr *) sender);
	int done = 0;
	while (i<total_nodes){
		node_port = get_in_port(nodes[i]->ai_addr);
		if (node_port == sender_port){
			//make this guy the coord
			log_msg("The coordinator has been set");
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

//Send an elect msg to all nodes that have a greater Id than me
int start_election(int sockfd, unsigned int prev_electID){
	struct msg new_msg;
	create_msg(&new_msg, ELECT, prev_electID);
	int i = 0;
	while (i<total_nodes){
		//send to all nodes above you.
		if (get_in_port(nodes[i]->ai_addr) > my_clock->nodeId){
			//send it to this person
			if (should_send()){
			if (sendto(sockfd, &new_msg, sizeof(new_msg), 0, nodes[i]->ai_addr, nodes[i]->ai_addrlen) == -1){
				log_msg("The elect could not be sent");
				return -1;
			}
			}
			log_msg("sent ELECT");
		}
		i++;
	}
	log_msg("done sending elects");
	return 0;
}

int wait_for_answer(int sockfd, unsigned int prev_electID){
	int elect_over = 0;
	struct msg last_msg;  //this will always store the last message we receive
	memset(&last_msg, 0, sizeof(last_msg)); //make sure it doesn't contain any funky values to start
	struct sockaddr_in sender;  //this is to store who sent the most recent message.
	memset(&sender, 0, sizeof(sender));
	socklen_t sender_len = sizeof(sender);
	while (!elect_over){
		if(recvfrom(sockfd, &last_msg, sizeof(last_msg), 0,(struct sockaddr *) &sender, &sender_len)<0){
			log_msg("timed out waiting for return messages");
			return 0;
		} else{
			//Check which message I got and act accordingly.
			net_to_host_msg(&last_msg);
			set_vector_clock(&last_msg);
			if (last_msg.msgID == ELECT){
				log_msg("received ELECT while waiting for answer");
				//we got an elect from someone so send back a msg
				send_answer(sockfd, last_msg.electionID, &sender);
			}
			else if (last_msg.msgID == AYA){
				log_msg("received AYA while waiting for answer");
				//someone wants to know if we are alive
				send_iaa(sockfd, &sender);
			}
			else if (last_msg.msgID == ANSWER){
				log_msg("received ANSWER while waiting for answer");
				//we got an answer so we can just call off the election.
				//make sure we check that the answer has the same election Id we sent
				printf("waiting on %u, but got %u\n", prev_electID, last_msg.electionID);
				if(prev_electID == last_msg.electionID){
					log_msg("the electionID's matched");
					elect_over = 1;
				}
			} else if(last_msg.msgID == IAA){
				log_msg("received IAA while waiting for answer");
				//we can ignore this
			} else if(last_msg.msgID == COORD){
				log_msg("received COORD while waiting for answer");
				//we can ignore this
			}
			else {
				log_msg("received NONSENSE while waiting for answer");
			}
		}
	}
	return 1;
}

int declare_coordinator(int sockfd, unsigned int prev_electID){
	log_msg("I am the coordinator");
	coordinator = my_addrinfo;
	struct msg new_msg;
	create_msg(&new_msg, COORD, prev_electID);	
	int i = 0;
	while (i<total_nodes){
		if (my_clock->nodeId != get_in_port(nodes[i]->ai_addr)){
			if (should_send()){
			if (sendto(sockfd, &new_msg, sizeof(new_msg), 0, nodes[i]->ai_addr, nodes[i]->ai_addrlen) == -1){
				log_msg("could not send COORD");
			}
			}
			log_msg("sent a COORD message"); 
		}
	i++;
	}
	log_msg("sent all the coord messages");
}

int wait_for_coord(int sockfd, int timeoutValue){
	//we must set the timer to a different value on the socket at the beginning
	struct timeval tv;
	tv.tv_sec = timeoutValue*(total_nodes+1);
	tv.tv_usec = 0;	
	if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
		printf("Could not set timeout on socket\n");
	}
	struct msg last_msg;  //this will always store the last message we receive
	memset(&last_msg, 0, sizeof(last_msg)); //make sure it doesn't contain any funky values to start
	struct sockaddr_in sender;  //this is to store who sent the most recent message.
	memset(&sender, 0, sizeof(sender));
	socklen_t sender_len = sizeof(sender);
	int found_coord = 0;
	while(!found_coord){
		if(recvfrom(sockfd, &last_msg, sizeof(last_msg), 0,(struct sockaddr *) &sender, &sender_len)<0){
			//we timed out so there is no coordinator. IS elect over? No
			return 0;
		} else{
			net_to_host_msg(&last_msg);
			set_vector_clock(&last_msg);
			if (last_msg.msgID == ELECT){
				log_msg("received ELECT while waiting for COORD");
				//we got an elect from someone so send back a msg
				send_answer(sockfd, last_msg.electionID, &sender);
			}
			else if (last_msg.msgID == AYA){
				log_msg("received AYA while waiting for COORD");
				//someone wants to know if we are alive
				send_iaa(sockfd, &sender);
			}
			else if (last_msg.msgID == ANSWER){
				log_msg("received ANSWER while waiting for COORD");
			}
			else if(last_msg.msgID == IAA){
				log_msg("received IAA while waiting for COORD");
			}
			else if(last_msg.msgID == COORD){
				log_msg("received COORD while waiting for COORD");
				set_coordinator(&sender);
				found_coord = 1;
			}
			else {
				log_msg("received NONSENSE while waiting for COORD");
			}
		}
	}
	//change the timeout on the socket back to its original
	tv.tv_sec = timeoutValue;
	tv.tv_usec = 0;	
	if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
		printf("Could not set timeout on socket\n");
	}
	return 1;
}

void elect_coordinator(int sockfd, int timeoutValue, unsigned int prev_electID){
	//here we refactor the loop that doesnt finish until we have a coordinator
	//This is important to maintain our loop invariant below: we must always know the coordinator.
	log_msg("start election process");
	int coord_found = 0;
	if (prev_electID == 0){
		prev_electID = ++electionID;
	}
	while(!coord_found){
		//Now we start an election.
		if(start_election(sockfd, prev_electID)==-1){
		printf("there was an error starting the election\n");
		}
		if(wait_for_answer(sockfd, prev_electID) == 0){
			//since 0 was returned I must have timed out waiting for a return msg.
			//So I am the coordinator now. Let everybody know.
			declare_coordinator(sockfd, electionID);
			coord_found = 1;
		} else {
			coord_found = wait_for_coord(sockfd, timeoutValue);
		}
	log_msg("finished election");
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

  init_logging(logFileName);

  printf("Port number:              %d\n", port);
  printf("Group list file name:     %s\n", groupListFileName);
  printf("Log file name:            %s\n", logFileName);
  printf("Timeout value:            %d\n", timeoutValue);  
  printf("AYATime:                  %d\n", AYATime);
  printf("Send failure probability: %d\n", sendFailureProbability);
  printf("First electionID:	    %d\n", electionID);
  printf("Starting up Node %d\n", port);
  failure_rate = sendFailureProbability;
  
  srandom(port);
  electionID = (unsigned int)((random() % 99)+ 1); 
  
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
  log_msg("started");

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

  struct msg last_msg;  //this will always store the last message we receive
  memset(&last_msg, 0, sizeof(last_msg)); //make sure it doesn't contain any funky values to start
  struct sockaddr_in sender;  //this is to store who sent the most recent message.
  memset(&sender, 0, sizeof(sender));
  socklen_t sender_len = sizeof(sender);
  //wait to receive a message and timeout if we don't.
  int num_loops;
  while (1){
	if(recvfrom(sockfd, &last_msg, sizeof(last_msg), 0,(struct sockaddr *) &sender, &sender_len)<0){
		if (!i_am_coordinator()){
			elect_coordinator(sockfd, timeoutValue, 0);
		}
	  }
	  else{
		//make sure the message is in the correct endianness
		net_to_host_msg(&last_msg);
		set_vector_clock(&last_msg);
		if (last_msg.msgID == COORD){
			log_msg("got coord message");
			if (get_in_port((struct sockaddr *)&sender) > my_clock->nodeId){
				set_coordinator(&sender);
			} else{
				elect_coordinator(sockfd, timeoutValue, 0);
			}
		}
		else if (last_msg.msgID == ELECT){
			log_msg("got elect msg");
			send_answer(sockfd, last_msg.electionID, &sender);
			elect_coordinator(sockfd, timeoutValue, last_msg.electionID);
		}
		else if (last_msg.msgID == AYA) {
			//I must be the coordinator
			log_msg("received aya message");
			send_iaa(sockfd, &sender);
		}
		else if (last_msg.msgID == IAA) {
			//The coordinator is alive.
			//we just go back to sending an AYA
			log_msg("received IAA");
		}
	  }
	  //after handling the most recent message we should have a coordinator, that is a loop invariant.
	  //therefore we should send an aya message to the coordinator, unless we are the coordinator
	  if (!i_am_coordinator()){	
		//wait some random amount of time and send an AYA
		int rn;
		rn = random();
		int sc = rn % (2*AYATime);
		printf(" wait for %d seconds\n", sc);
		sleep(sc);
		send_aya(sockfd);
	  }
	  num_loops++;
  }
  log_msg("Bully algorithm loop has completed");
  printf("done\n");
  close(sockfd);
  return 0;
  
}
