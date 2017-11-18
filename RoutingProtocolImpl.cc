#include "RoutingProtocolImpl.h"
#include "Node.h"
#include <string.h>
#include <limits.h>

RoutingProtocolImpl::RoutingProtocolImpl(Node *n) : RoutingProtocol(n) {
  sys = n;
  // add your own code
}

RoutingProtocolImpl::~RoutingProtocolImpl() {
  // add your own code (if needed)
}

// The initial method
void RoutingProtocolImpl::init(unsigned short num_ports, unsigned short router_id, eProtocolType protocol_type) {
    protocol = protocol_type;
    r_id = router_id;

    P_table.setPortNumbers(num_ports);
    P_table.setRouterId(router_id);
    F_table.set_routerId(router_id);
    F_table.set_protocol(protocol_type);

    // Set up PING alarm, use int 0
    int* alarm = (int*)malloc(sizeof(int));
    *alarm = 0;
    sys->set_alarm(this, 0, alarm);
    
    // int 1 represents 1 second check
    alarm = (int*)malloc(sizeof(int));
    *alarm = 1;
    sys->set_alarm(this, 1000, alarm);
    
    // int 2 represents DV check
    // int 3 represents LS check
    alarm = (int*)malloc(sizeof(int));
    *alarm = protocol_type == P_DV ? 2 : 3;
    sys->set_alarm(this, 30 * 1000, alarm);
}

// The function to handle alarm
void RoutingProtocolImpl::handle_alarm(void *data) {
    switch ((int)(*(int*)data)) {
      // 0 is PING alarm
        case 0: {
            sys->set_alarm(this, 10 * 1000, data);
            unsigned short packSize;
            char* packet;
	    
	    // Send PING packet to all the ports in the current router
            for(int i = 0; i < P_table.getSize(); i++) {
                packet = (char*)P_table.buildPacket(sys->time(), packSize);
                sys->send(i, packet, packSize);
            }
            break;
        }
	
	  // 1 is for one second state check
        case 1: {
            sys->set_alarm(this, 1 * 1000, data);
	    
	    // Invalidlist vector to store the entries to be removed
            vector<unsigned short> invalidList;
            int change = 0;
	    
	    // Increase the timestamp for every check
            F_table.incTime();
	    if (F_table.state_check() && protocol == P_DV) {
	      change = 1;
	    }
	    
	    // Increase the timestamp in the port status table
            P_table.incTime();
	    // Remove port entries in the invalide list
            if (P_table.checkStates(invalidList)) {
                vector<unsigned short>::iterator it_vec;
                for (it_vec = invalidList.begin(); it_vec != invalidList.end(); it_vec++) {
                    if (protocol == P_DV) {
                        if (F_table.update_DV_Table(*it_vec, USHRT_MAX, 0, *it_vec)) {
                            change = 1;
                        }
                    } else {
		      if (F_table.update_LS_Table(*it_vec, UINT_MAX)) {
			change = 1;
		      }  
                    }
                }
                invalidList.clear();
            }

            if (change == 1) {
	      // For the DV protocol
                if (protocol == P_DV) {
                    unsigned short packSize, target_ID;
                    char* packet;
		    
		    // If change detected, send updated DV packet
                    for (int i = 0; i < P_table.getSize(); i++) {
                        if (P_table.checkRouteIdFromPortNum(i, target_ID)) {
                            packet = (char*)F_table.make_DV_packet(target_ID, packSize);
                            sys->send(i, packet, packSize);
                        }
                    }
                } else {
		  // Send updated LS packet if the shortest path changed
		  if (F_table.update_path()) {
		    unsigned short packSize;
		    char* packet;
		    for (int i = 0; i < P_table.getSize(); i++) {
		      packet = (char*)F_table.make_LS_packet(packSize);
		      sys->send(i, packet, packSize);
		      F_table.incSeq();
		    }
		  }
                }
            }
            break;
        }
	  // 2 is for DV check alarm
        case 2: {
          sys->set_alarm(this, 30 * 1000, data);
          unsigned short packSize, target_ID;
          char* packet;
	  
	  // Send DV packet to all ports in the current router
          for (int i = 0; i < P_table.getSize(); i++) {
	    if (P_table.checkRouteIdFromPortNum(i, target_ID)) {
	      packet = (char*)F_table.make_DV_packet(target_ID, packSize);
	      sys->send(i, packet, packSize);
	    }
          }
          break;
        }

	  // 3 is for LS check alarm
        case 3: {
	  sys->set_alarm(this, 30 * 1000, data);
	  unsigned short packSize;
	  char* packet;
	  
	  // Send LS packet to all ports in the current router
	  for (int i = 0; i < P_table.getSize(); i++) {
	    packet = (char*)F_table.make_LS_packet(packSize);
	    sys->send(i, packet, packSize);
	    F_table.incSeq();
	  }
          break;
        }
    }
}

// The function to receive the 5 types of data packet
void RoutingProtocolImpl::recv(unsigned short port, void *packet, unsigned short size) {
    // add your own code
    unsigned short packet_type = *((unsigned char *) packet);
    
    // 0: DATA, 1: PING, 2: PONG, 3: DV, 4: LS
    switch (packet_type) {
        //Data
        case 0: {
            unsigned short target_id, target_port;
            if (F_table.parse_Data_packet(packet, size, target_id)) {
	      if (target_id == r_id) {
		// Reached destination
		free(packet);
	      } else if (P_table.checkPortNumFromRouteId(target_id, target_port)) {
		// Send to the corresponding target_id via target_port
		sys->send(target_port, packet, size);
	      } else {
		free(packet);
	      }
	    } else {
	      free(packet);
	    }
            break;
        }
            //PING
        case 1: {
            char *pack = (char*) P_table.processPing(port, packet, size);
            sys->send(port, pack, size);
            break;
        }
            //PONG
        case 2: {
	  unsigned short source_id;
	  unsigned int curr_delay, prev_delay;
	  
	  int change = 0;
	  
	  // Get the delay and compare with the previous delay to see if there is any update
	  if (P_table.getDelay(port, prev_delay)) {
	    P_table.processPong(port, packet, sys->time(), source_id, curr_delay);
	    if (curr_delay != prev_delay) {
	      change = 1;
	    }
	  } else {
	    P_table.processPong(port, packet, sys->time(), source_id, curr_delay);
	    prev_delay = curr_delay;
	    change = 1;
	  }
	  
	  // Send DV or LS packet if there is any delay change
	  if (change == 1) {
	    if (protocol == P_DV) {
	      // Send updated DV packet to neighbor routers
	      if (F_table.update_DV_Table(source_id, curr_delay, prev_delay, source_id)) {
		unsigned short packSize, target_id;
		char* pack;
	      
		for (int i = 0; i < P_table.getSize(); i++) {
		  if (P_table.checkRouteIdFromPortNum(i, target_id)) {
		    pack = (char*)F_table.make_DV_packet(target_id, packSize);
		    sys->send(i, pack, packSize);
		  }
		}
	      }
	    } else {
	      if (F_table.update_LS_Table(source_id, curr_delay)) {
		// Send updated LS packet to neighbor routers
		if (F_table.update_path()) {
		  unsigned short packSize;
		  char* pack;
		  for (int i = 0; i < P_table.getSize(); i++) {
		    pack = (char*)F_table.make_LS_packet(packSize);
		    sys->send(i, pack, packSize);
		    F_table.incSeq();
		  }
		}
	      }
	    }
	  }
	    
	  free(packet);
          break;
        }
            //DV
        case 3: {
            unsigned int delay;
            if (P_table.getDelay(port, delay)) {
                if (F_table.parse_DV_packet(packet, size, delay)) {
                    unsigned short packSize, target_id;
                    char *pack;
		    // Send updated DV packet to all ports in the router
                    for (int i = 0; i < P_table.getSize(); i++) {
                        if (P_table.checkRouteIdFromPortNum(i, target_id)) {
                            pack = (char *) F_table.make_DV_packet(target_id, packSize);
                            sys->send(i, pack, packSize);
                        }
                    }
                }
            }
	    free(packet);
            break;
        }
	  //LS
        case 4: {
	  if (F_table.parse_LS_packet(packet, size)) {
	    if (F_table.update_path()) {
	    char* pack;
	    // Send LS packet if the seq number has never been seen before
	    for (int i = 0; i < P_table.getSize(); i++) {
	      pack = (char*)malloc(size);
	      memcpy(pack, packet, size);
	      sys->send(i, pack, size);
	    }
	    }
	  }
	  free(packet);
          break;
        }
    }
}
// add more of your own code
