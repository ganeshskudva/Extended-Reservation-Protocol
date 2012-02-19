/*
 * This file implements a Extended Reservation Protocol used to verify a simple modification
 * of the 802.11 MAC protocol for Wireless Networks
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* Defines */

#define MAX_SLOT_SIZE           100000
#define MAX_PKT_SIZE            100
#define MAX_NODE_COUNT          10000
#define MAX_CW_SIZE             512
#define MAX_PKT_RESERVED        32 

#define SLOT_STATE_IDLE         0
#define SLOT_STATE_TRANSMISSION 1
#define SLOT_STATE_COLLISION    2
#define SLOT_STATE_ACK          3

#define NODE_TX_START           1
#define NODE_TX_END             0

#define INVALID_BACKOFF        -1

/* Globals */

typedef struct slot_ {
    int    state;
    int    tx_id;
} slot_t;

typedef struct node_ {
    int    node_id;
    int    backoff;
    int    cw_size;
    int    prev_state;
    int    hidden_terminal[2];
    int    tx_state;
    int    slot_count;
} node_t;

slot_t slots[MAX_SLOT_SIZE];
node_t nodes[MAX_NODE_COUNT];
int slot_size, pkt_size, node_count, cw_size;
int idle_slots, collision_slots, transmission_slots, packet_count;

/*
 * Main entry point
 */
int 
main (int argc, char *argv[])
{
    int collision_count, colliding_nodes[100]; 
    int status;
    int i, j, k,z,flag = 0,receiver,coll_slot,max,pkt_suc;
#if 0
    float cur_efficiency = 0.0, prev_efficiency = 0.000001;
    float cur_delta = 1.0, prev_delta = 1.0;
#endif

/*    if (argc != 4) {
        printf("syntax: ./Simulation <pkt-size> <node-count> <cw-size>\n");
        exit(0);
    } */

    /* Validate the inputs */
    pkt_size = 32;//atoi(argv[1]);
    node_count = 100;//atoi(argv[2]);
    cw_size = 32;//atoi(argv[3]);

    if ((pkt_size > MAX_PKT_SIZE) || (node_count > MAX_NODE_COUNT) || 
        (cw_size > MAX_CW_SIZE)) {
        printf("Error taking inputs!\n");
        exit(1);
    }

    /* 
     * Slot size can be infinite. For this program, we will assume that it
     * can't exceed one million slots.
     */
    slot_size = MAX_SLOT_SIZE;

    /* Initialize the data structures */
    for (i = 0; i < slot_size; i++) {
        slots[i].state = SLOT_STATE_IDLE;
    	slots[i].tx_id = -1;
    }

    for (i = 0; i < node_count; i++) {
        nodes[i].node_id = i;
	nodes[i].backoff = INVALID_BACKOFF;
        nodes[i].cw_size = cw_size;
        nodes[i].prev_state = SLOT_STATE_IDLE;
    	nodes[i].tx_state = -1;
	nodes[i].slot_count = 0;
   	nodes[i].hidden_terminal[0] = 9999; 
   }
    nodes[0].hidden_terminal[0] = 1;
    nodes[0].hidden_terminal[1] = 9999; 
    nodes[1].hidden_terminal[0] = 0;
    nodes[1].hidden_terminal[1] = 9999;
    nodes[2].hidden_terminal[0] = 9999;
    nodes[65].hidden_terminal[0] = 66;
    nodes[65].hidden_terminal[1] = 9999;
    nodes[66].hidden_terminal[0] = 65;
    nodes[66].hidden_terminal[1] = 9999;
    nodes[55].hidden_terminal[0] = 45;
    nodes[55].hidden_terminal[1] = 9999;
    nodes[45].hidden_terminal[0] = 55;
    nodes[45].hidden_terminal[1] = 9999; 
    receiver = node_count - 1;
//    printf("Reciever is %d\n",receiver);
    /* Initialize the random seed generator */
    srand(time(NULL));

    /* 
     * Main loop. For each slot, do the following:
     *
     * 1. For each node, check if the slot is free. If it is, decrement
     *    backoff. If backoff expires, transmit the packet.
     * 2. If the slot is not free, wait till it becomes free and then
     *    decrement backoff
     * 3. If 2 nodes simultaneoulsly expire their backoff, then there will
     *    be a collision for packet-size slots.
     * 4. In case of a collision, the colliding nodes double their CW size.
     */

    for (i = 0; i < slot_size; i++) {

        /* Reset the collision count */
        collision_count = 0;
	flag = 0;
//	printf("\nBEGIN OF LOOP slots[%d].state = %d\n",i,slots[i].state);
        /* For each node */
	for(j = 0; j < node_count; j++)
	{
		if(slots[i].state == SLOT_STATE_IDLE && nodes[j].tx_state == NODE_TX_START && nodes[j].backoff == 0)
		{		
			slots[i].state = SLOT_STATE_TRANSMISSION;
			slots[i].tx_id = nodes[j].node_id;
			colliding_nodes[collision_count++] = nodes[j].node_id;
			flag = 1;
		}
	}
        for (j = 0; j < node_count; j++) {
	    if(j != receiver){
            if (slots[i].state == SLOT_STATE_IDLE &&
                nodes[j].prev_state == SLOT_STATE_IDLE) {
                /*
                 * Both the current and previous slots are idle. Decrement
                 * backoff. Initialize backoff if we are coming here for the
                 * first time.
                 */
                if (nodes[j].backoff == INVALID_BACKOFF) { 
                    nodes[j].backoff = (rand() % nodes[j].cw_size) + 1;
//                    printf("Node %d backoff is %d\n",j,nodes[j].backoff);
		}

                nodes[j].backoff -= 1;
                if (nodes[j].backoff == 0) {
                    /* 
                     * Ready to transmit the packet. Make a note of all 
                     * such nodes whose backoff expires in the same slot. 
                     * This will help us determine if there was a 
                     * successful transmission in this slot.
                     */
                    colliding_nodes[collision_count++] = j;
                }
            } else if (slots[i].state == SLOT_STATE_IDLE &&
                       nodes[j].prev_state != SLOT_STATE_IDLE) {
                /*
                 * We need to sense an idle slot for the full slot duration.
                 * Hence this check. Once the previous state and the current
                 * state are both idle, we decrement backoff.
                 */
                nodes[j].prev_state = SLOT_STATE_IDLE;
            } else {
                /*
                 * When we are decrementing our backoff, some other node's
                 * backoff may expire and it may start transmtitting. In such
                 * a case,we should freeze our backoff counter till the other
                 * transmission is complete. To acheive this, we set the
                 * node's slot state accordingly.
                 */
                nodes[j].prev_state = slots[i].state;
            }
	     if(slots[i].state == SLOT_STATE_TRANSMISSION){
	    	if(j != slots[i].tx_id){
		for (z = 0; nodes[j].hidden_terminal[z] != 9999; z++)
		{
			if(slots[i].tx_id == nodes[j].hidden_terminal[z])
			{	
				nodes[j].backoff--;
//				printf("\n\nIn SLOT_STATE_TRANSMISSION if condition. Node %d backoff decremented to %d\n\n",j,nodes[j].backoff);
			}
		}
		/* If we r here for the 1st time while checking for a slot */
		if(!flag)
		{	
			 colliding_nodes[collision_count++] = slots[i].tx_id;
			 flag = 1;
		} 
		if(nodes[j].backoff == 0)
			colliding_nodes[collision_count++] = j;
//		printf("\nIn SLOT_STATE_TRANSMISSION if condition collision_count --> %d\n",collision_count);

	    }
	    }
	     if(slots[i].state == SLOT_STATE_ACK){
		collision_count = 1;
	     }
	    }
		
//          printf("nodes[%d].backoff --> %d \t",j,nodes[j].backoff);
	}
//	printf("\n");
        /*
         * We have processed all the nodes. Check the collision count. There 
         * can be 3 cases:
         *
         * 1. collision_count = 0: None of the nodes expired their backoff.
         *    The slot state will either be transmission, collision or idle.
         * 2. collision_count = 1: Only one guy expired the backoff. This
         *    indicates successful transmission.
         * 3. collision_count > 1: Since many guys transmit simultaneously,
         *    there will be a collision 
         */
        switch (collision_count) {
            case 0:
                /* 
                 * No change in slot state. It will already be set to  
                 * transmission, collision or idle. Nothing to update here.
                 */
                break;

            case 1:
                /* Successful transmission */
  //              if(slots[i].state == SLOT_STATE_IDLE)
//		{
//		printf("\nInitialized these slots to TRANS in case 1\n");
//		printf("\nIn Switch Case-1: nodes[colliding_nodes[0]].slot_count --> %d\n",nodes[colliding_nodes[0]].slot_count);
		if(((nodes[colliding_nodes[0]].slot_count % pkt_size) == 0) && ((nodes[colliding_nodes[0]].slot_count / pkt_size)!= MAX_PKT_RESERVED))
		{
		if(slots[i].state != SLOT_STATE_ACK)
		{
		for (k = i; k < (i + pkt_size); k++) {
                    slots[k].state = SLOT_STATE_TRANSMISSION;
                    slots[k].tx_id = nodes[colliding_nodes[0]].node_id;
//		    printf("k = %d",k);
		}
		    slots[k].state = SLOT_STATE_ACK;
	            slots[k].tx_id = nodes[colliding_nodes[0]].node_id;
		}
		}
		/* Update the packet count */
                packet_count++;
	
		if(slots[i].state != SLOT_STATE_ACK)
		{	
			nodes[colliding_nodes[0]].slot_count++;
			packet_count++;
		}
		nodes[colliding_nodes[0]].tx_state = NODE_TX_START;
                /* Reset the backoff counter only when the node has completed tranmitting MAX_PKT_RESERVED pkts*/
                if(nodes[colliding_nodes[0]].slot_count == MAX_PKT_RESERVED * pkt_size)
		{
			nodes[colliding_nodes[0]].backoff = INVALID_BACKOFF;
			nodes[colliding_nodes[0]].tx_state = NODE_TX_END;
			nodes[colliding_nodes[0]].slot_count = 0;
		}
//		slots[k].state = SLOT_STATE_ACK;
//		slots[k].tx_id = nodes[colliding_nodes[0]].node_id;
//		printf("\nSet slot %d to %d state \n",k,slots[k].state);
//		}
                break;

            default:
                /*
                 * Collision. First set the slot state for the affected slots.
                 * For each colliding node, double the CW size and reset the 
                 * backoff counter.
                 */
                pkt_suc = -1;
		for (k = i; k < (i + pkt_size); k++) {
                    slots[k].state = SLOT_STATE_COLLISION;
                    slots[k].tx_id = 0;
		}
		
		max = -1;
		for (k = 0; k < collision_count; k++)
		{
			if(nodes[colliding_nodes[k]].slot_count > max)
				max = nodes[colliding_nodes[k]].slot_count;
		}

		pkt_suc  = max/pkt_size;
		coll_slot = max - (pkt_suc * pkt_size);
//		printf("\nNo of slots which needs to be markes coll is %d from %d \n",coll_slot,i-coll_slot);
		for(k = i - coll_slot;k < i; k++ )
			slots[k].state = SLOT_STATE_COLLISION;

                for (k = 0; k < collision_count; k++) {
                    nodes[colliding_nodes[k]].backoff = INVALID_BACKOFF;
                    nodes[colliding_nodes[k]].cw_size *= 2;
                    nodes[colliding_nodes[k]].tx_state = -1;
		    nodes[colliding_nodes[k]].slot_count = 0;
		}

                break;
        }

        /* Collect Statistics */
/*        if (slots[i].state == SLOT_STATE_IDLE) {
            idle_slots++;
        } else if (slots[i].state == SLOT_STATE_TRANSMISSION) {
            transmission_slots++;
        } else if(slots[i].state == SLOT_STATE_COLLISION){
            collision_slots++;
        } */
//	printf("Slots[%d] --> %d Slots[%d].tx_id = %d\n",i,slots[i].state,i,slots[i].tx_id);
}
    for (i = 0; i < slot_size; i++)
    {
	if (slots[i].state == SLOT_STATE_IDLE) {
            idle_slots++;
        } else if (slots[i].state == SLOT_STATE_TRANSMISSION) {
            transmission_slots++;
        } else if(slots[i].state == SLOT_STATE_COLLISION){
            collision_slots++;
        }
//    	printf("\nSlot[%d].state-->%d\n",i,slots[i].state);
    }
	
    printf("Idle Slots: %d\n", idle_slots);
    printf("Transmission Slots: %d\n", transmission_slots);
    printf("Collision Slots: %d\n", collision_slots);
    printf("Packets successfully transmitted: %d\n", (packet_count/pkt_size));
    printf("Total slots used for simulation: %d\n", i);
    
    printf("Throughput: %f\n", (float)(packet_count/pkt_size)/ (float)i);
    printf(" %d %f\n", cw_size, (float)transmission_slots / (float)i);
    return 0;
}

/* End of File */
