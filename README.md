# BullyAlgorithm

This is an implementation of the Bully algorithm. It assumes a static group to simplify group management.
It uses a vector clock to determine event ordering. For an example, go to ShiViz and upload the ShiVizLog.dat event log.

Each node has a nodeId which is given by the port number it uses to receive messages on. The group will elect the node with the 
highest node ID to be the leader of the group (for whatever task).
