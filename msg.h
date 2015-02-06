

#ifndef MSG_H
#define MSG_H 1

#define MAX_NODES 9



/* The following enum defines the types of messages that can be
   sent. The only unusual ones are IAA and AYA which stand for I AM
   Aive and Are You Alive, respectively.  Regular nodes are to
   periodically send AYA to the coordinator to simulate interaction
   with the coordinator. The coordinatior will respond with an IAA.
   If the coordinator does not respond within Timeout value seconds
   provided on the command line then an election is called. When an
   election is underway if an ELECT message does not receive an ANSWER
   message within Timeout value seconds then that node is considered
   dead. (Note it may come back later.) If after sending an ELECT
   message, and receiving an ANSWER message, if a COORD message isn't
   received within ((MAX_NODES + 1) timeout value) seconds then the
   election is to be abandoned and a new one called. When a node first
   starts it waits timeout value  seconds and if it hasn't received an ELECT or
   COORD message then it calls an election.
*/

enum msgKind {
        ELECT = 10,
        ANSWER = 11,
        COORD = 12,
        AYA = 20,
        IAA = 21,

};
typedef enum msgKind msgType;


/* A clock value consists of the node number (i.e. a node's port
   number) and the clock value. 
*/

struct clock {
  unsigned int nodeId;
  unsigned int time;
};

/* This is the message sent between nodes. Simply fill this structure
   and send it. So that we don't commit that distributed systems sin
   of assuming the network is homogeneous, all values in this message
   are to be sent in the network byte order of their respective
   types. The electionID value is to be selected by a node when it
   starts an election. Each election a particular node starts is to
   have a different election ID. Although it would be helpful if the
   election IDs were different across all nodes that start elections,
   it is not a requirement.  If a node is forwarding the election call
   then this value is left unchanged. The election ID has no role in
   the actual election and is to be used for logging purposes only. If
   a time value for a node is not known, it is set to 0. Initially a
   node only knows its own clock time, but it will learn the clock
   values at other nodes as messages are exchanged. If there are fewer
   nodes in the system than vectorClock slots then the node ID for the
   unused slots is to be 0. There is no requirement for the IDs to be
   stored in the array of vectorClocks in any particular order and it
   is possible and than a unused slots could be encountered anywhere
   in the array. For the AYA and IAA messages the electionID is to be
   set to the port number of the node sending the AYA
   request. (i.e. when the IAA response is sent it will contain the
   electionID value from the AYA request.)
*/

struct msg {
  msgType        msgID;
  unsigned int   electionID;
  struct clock   vectorClock[MAX_NODES];
};
  

#endif 
