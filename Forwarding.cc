#include "Forwarding.h"
#include <netinet/in.h>
#include <string.h>
#include <limits.h>
#include <unordered_set>

Forwarding::Forwarding(){
    
}

// Set the protocol for the forwarding table
void Forwarding::set_protocol(eProtocolType protocol) {
  pr = protocol;
}

// Set the router_id for the table
void Forwarding::set_routerId(unsigned short router_id) {
  r_id = router_id;
}

// Do the one second state check to remove outdated entries
bool Forwarding::state_check() {
  int change = 0;
  vector<int> to_clear;
  
  unordered_map<int, vector<F_Item>>::iterator it_map;
  vector<int>::iterator it_vec;

  for(it_map = F_Items.begin(); it_map != F_Items.end(); it_map++){
    if (!it_map->second.empty()){
      if (it_map->second[0].timestamp > REFRESH_THRESHOLD) {
	      to_clear.push_back(it_map->first);
      }
    }
  }

  for(it_vec = to_clear.begin(); it_vec != to_clear.end(); it_vec++){
    if (*it_vec != r_id) {
      F_Items[*it_vec].clear();
      F_Items.erase(*it_vec);
    }
    change = 1;
  }
  
  to_clear.clear();

  return change == 1;
  // if (change != 0) {
  //   return true;
  // }

  // return false;
}

// Function to make DV packet
void* Forwarding::make_DV_packet(unsigned short targetID, unsigned short& DV_packet_size) {
  DV_packet_size = 4 * F_Items.size() + 8;

  short* DV_packet = (short*)malloc(DV_packet_size);

  *(unsigned char*)DV_packet = 3;
  *((unsigned short*)DV_packet + 1) = (unsigned short)htons((unsigned short)DV_packet_size);
  *((unsigned short*)DV_packet + 2) = (unsigned short)htons((unsigned short)r_id);
  *((unsigned short*)DV_packet + 3) = (unsigned short)htons((unsigned short)targetID);

  // offset for the header part is 4 bytes, so set offset to 4
  int offset = 4;
  unordered_map<int, vector<F_Item>>::iterator it_map;

  for (it_map = F_Items.begin(); it_map != F_Items.end(); it_map++, offset = offset + 2) {
    if (!it_map->second.empty()) {
      *((unsigned short*)DV_packet + offset) = (unsigned short)htons((unsigned short)it_map->second[0].dest);

      if (it_map->second[0].next_hop == targetID) {
	*((unsigned short*)DV_packet + offset + 1) = (unsigned short)htons((unsigned short)USHRT_MAX);
      } else {
	*((unsigned short*)DV_packet + offset + 1) = (unsigned short)htons((unsigned short)it_map->second[0].cost);
      }
    }
  }
  
  return (void*)DV_packet;
}

// Helper function to insert entries into the forwarding map
void Forwarding::insert_entry(unsigned short dest, unsigned int cost, unsigned short next_hop) {
  F_Items[dest][0].dest = dest;
  F_Items[dest][0].cost = cost;
  F_Items[dest][0].next_hop = next_hop;
  F_Items[dest][0].timestamp = 0;
}

// Function to parse the DV packet
bool Forwarding::parse_DV_packet(void* DV_packet, unsigned short size, unsigned int delay) {
  int change = 0;
  if (*((unsigned char*)DV_packet) != 3 || size < 8) {
    return false;
  }

  unsigned short sourceID = (unsigned short)ntohs(*((unsigned short*)DV_packet + 2));
  unsigned short num_entries = size / 4 - 2;

  unordered_set<int> valid_entries;

  int i;
  int offset = 4;
  for (i = 0; i < num_entries; i++) {
    unsigned short dest = (unsigned short)ntohs(*((unsigned short*)DV_packet + offset + 2 * i));
    unsigned short cost = (unsigned short)ntohs(*((unsigned short*)DV_packet + offset + 1 + 2 * i));
    if (dest == r_id || cost == USHRT_MAX) {
      continue;
    }

    valid_entries.insert(dest);

    if (F_Items.find(dest)==F_Items.end()) {
      if (F_Items[dest].empty()) {
	F_Items[dest].push_back(F_Item());
      }
      insert_entry(dest, delay + cost, sourceID);
      change = 1;
    } else {
      if (F_Items[dest].empty()) {
	F_Items[dest].push_back(F_Item());
	insert_entry(dest, delay + cost, sourceID);
	change = 1;
      } else if (delay + cost < F_Items[dest][0].cost) {
	insert_entry(dest, delay + cost, sourceID);
	change = 1;
      } else {
	if (F_Items[dest][0].next_hop == sourceID) {
	  F_Items[dest][0].timestamp = 0;
	  if (F_Items[dest][0].cost != delay + cost) {
	    change = 1;
	  }
	}
      }
    }    
  }

  unordered_map<int, vector<F_Item>>::iterator it_map;
  vector<int> to_clear;
  vector<int>::iterator it_vec;

  // Need to remove entries where nexthop is the source ID but does not contain paths to the destination router
  for (it_map = F_Items.begin(); it_map != F_Items.end(); it_map++) {
    if (!it_map->second.empty()) {
      if (it_map->second[0].next_hop == sourceID && it_map->second[0].next_hop != it_map->second[0].dest) {
	if (valid_entries.find(it_map->second[0].dest) != valid_entries.end()) {
	  break;
	} else {
	  to_clear.push_back(it_map->first);
	}
      }
    }
  }

  // Clear all of the invalid entries
  for(it_vec = to_clear.begin(); it_vec != to_clear.end(); it_vec++){
    if (*it_vec != r_id) {
      F_Items[*it_vec].clear();
      F_Items.erase(*it_vec);
    }
    change = 1;
  }

  to_clear.clear();
  if (change != 0) {
    return true;
  }

  return false;
}

// Update DV table when a Pong is received or any change in delay
bool Forwarding::update_DV_Table(
        unsigned short dest, unsigned int curr_cost, unsigned int prev_cost, unsigned short next_hop
) {
  int change = 0;
  unordered_map<int, vector<F_Item>>::iterator it_map;

  // Remove entries with outdated router ID
  if (curr_cost == USHRT_MAX) {
    vector<int> to_clear;

    for (it_map = F_Items.begin(); it_map != F_Items.end(); it_map++) {
      if (!it_map->second.empty() && it_map->second[0].next_hop == dest) {
	to_clear.push_back(it_map->first);
      } 
    }
    
    // Clear entries with outdated router ID
    vector<int>::iterator it_vec;
    for (it_vec = to_clear.begin(); it_vec != to_clear.end(); it_vec++) {
      if (*it_vec != r_id) {
	F_Items[*it_vec].clear();
	F_Items.erase(*it_vec);
      }
      change = 1;
    }

    to_clear.clear();
    if (change != 0) {
      return true;
    }

    return false;
  } else { // Update the entries if they also exists in the F_Items
    if (F_Items.find(dest) == F_Items.end()) {
      if (F_Items[dest].empty()) {
	F_Items[dest].push_back(F_Item());
      }
      insert_entry(dest, curr_cost, next_hop);
      change = 1;
    } else if (F_Items[dest].empty()) {
      F_Items[dest].push_back(F_Item());
      insert_entry(dest, curr_cost, next_hop);
      change = 1;
    } else if (F_Items[dest][0].next_hop != next_hop && curr_cost < F_Items[dest][0].cost) {
      insert_entry(dest, curr_cost, next_hop);
      change = 1;
    }
  }

  // Refresh all entries that have next hop the same as the next hop in the parameter
  for (it_map = F_Items.begin(); it_map != F_Items.end(); it_map++) {
    if (!it_map->second.empty() && it_map->second[0].next_hop == next_hop) {
      it_map->second[0].cost = it_map->second[0].cost + curr_cost - prev_cost;
      it_map->second[0].timestamp = 0;
      if (curr_cost != prev_cost) {
	change = 1;
      }
    }
  }

  if (change != 0) {
    return true;
  }

  return false;  
}

// Parse data packet function
bool Forwarding::parse_Data_packet(void* data_packet, unsigned short size, unsigned short& next_hop) {
  unsigned short dest = (unsigned short)ntohs(*((unsigned short*)data_packet + 3));

  // Reached destination so return true
  if (dest == r_id) {
    next_hop = r_id;
    return true;
  }
  
  if (F_Items.find(dest) != F_Items.end() && !F_Items[dest].empty()) {
    next_hop = F_Items[dest][0].next_hop;
    return true;
  }

  return false;
}

// Get the size of the F_Items table
int Forwarding::get_size() {
  return F_Items.size();
}

// Update the timestamp by one for one second check
void Forwarding::incTime() {
  unordered_map<int, vector<F_Item>>::iterator it_map;
  for (it_map = F_Items.begin(); it_map != F_Items.end(); it_map++) {
    if (!it_map->second.empty()) {
      it_map->second[0].timestamp++;
    }
  }
}

// The function to make LS packet
void* Forwarding::make_LS_packet(unsigned short& packSize) {
  char* packet;
  packSize = 4 * F_Items[r_id].size() + 12;
  packet = (char*)malloc(packSize);

  *(unsigned char*)packet = 4;
  *((unsigned short*)packet + 1) = (unsigned short)htons((unsigned short)packSize);
  *((unsigned short*)packet + 2) = (unsigned short)htons((unsigned short)r_id);
  *((unsigned int*)packet + 2) = (unsigned short)htonl((unsigned int)seq_num);

  // Offset is 6 in LS packet because it includes the seq number
  int offset = 6;
  for (unsigned int i = 0; i < F_Items[r_id].size(); i++) {
    *((unsigned short*)packet + offset + 2 * i) = (unsigned short)htons((unsigned short)F_Items[r_id][i].dest);
    *((unsigned short*)packet + offset + 1 + 2 * i) = (unsigned short)htons((unsigned short)F_Items[r_id][i].cost);
  }

  return (void*)packet;
}


// Function to parse the LS packet
bool Forwarding::parse_LS_packet(void* packet, unsigned short size) {
  int change = 0;
  if (*((unsigned char*)packet) != 4 || size < 12) {
    return false;
  }

  unsigned short source_id = (unsigned short)ntohs(*((unsigned short*)packet + 2));
  unsigned int seqNum = (unsigned int)ntohl(*((unsigned int*)packet + 2));

  if (F_Items.find(source_id) != F_Items.end() && !F_Items[source_id].empty() && F_Items[source_id][0].seq_num >= seqNum) {
    return false;
  }

  F_Items[source_id].clear();
  change = 1;
  
  // Offset is 6 in LS packet
  int offset = 6;

  // Fill in the entries from the received LS packet
  for (int i = 0; i < size / 4 - 3; i++) {
    unsigned short node_id = (unsigned short)ntohs(*((unsigned short*)packet + offset + 2 * i));
    unsigned short cost = (unsigned short)ntohs(*((unsigned short*)packet + offset + 1 + 2 * i));

    F_Items[node_id].push_back(F_Item(node_id, cost, -1, seqNum));
  }

  // return change == 1;
  return false;
}

// Update F_Items table when received a Pong or any delay change is detected
bool Forwarding::update_LS_Table(unsigned short source_id, unsigned int cost) {
  for (unsigned int i = 0; i < F_Items.size(); i++) {
    if (F_Items[r_id][i].dest == source_id) {
      // This means the port link is broken, so remove the corresponding entry
      if (cost == UINT_MAX) {
	F_Items[r_id].erase(F_Items[r_id].begin() + i);
	return true;
      }
      // Delay is changed, so change the corresponding delay in the table
      if (F_Items[r_id][i].cost != cost) {
	F_Items[r_id][i].cost = cost;
	return true;
      }
      // No change is detected
      return false;
    }
  }

  F_Items[r_id].push_back(F_Item(source_id, cost, source_id, 0));
  return true;
}

// Update the shortest path according to dijkstra's algorithm
bool Forwarding::update_path() {
  priority_queue<Dist_Pair, std::vector<Dist_Pair>, Compare> pq;
  unordered_set<int> visited;
  int max_id = 0;
  
  // Get the maximum number of router id in the table
  unordered_map<int, vector<F_Item>>::iterator it_map;
  for(it_map = F_Items.begin(); it_map != F_Items.end(); it_map++){
    max_id = max(max_id, it_map->first);
  }

  // Record the previous node id to get to the destination
  unsigned short parent[max_id + 1];

  // Record the distance to the router id
  unsigned int dist[max_id+1];

  // Initialize the two arrays
  for(int i = 0; i < max_id + 1; i++) {
    dist[i] = UINT_MAX;
    parent[i] = USHRT_MAX;
  }

  dist[r_id] = 0;
  pq.push(Dist_Pair(r_id, 0));

  while (!pq.empty()) {
    Dist_Pair curr = pq.top();
    pq.pop();
    unsigned short curr_id = curr.r_id;

    if (F_Items.find(curr_id) == F_Items.end() || F_Items[curr_id].empty()) {
      continue;
    }

    visited.insert(curr_id);
    for (unsigned int i = 0; i < F_Items[curr_id].size(); i++) {
      int next_id = F_Items[curr_id][i].dest;
      if (visited.find(next_id) == visited.end()) {
        if (dist[next_id] > dist[curr_id] + F_Items[curr_id][i].cost) {
          dist[next_id] = dist[curr_id] + F_Items[curr_id][i].cost;
          parent[next_id] = curr_id;
          pq.push(Dist_Pair(next_id, dist[next_id]));
	}
      }
    }
  }

  int change = 0;
  
  // Update the next hop for every destination starting from the current router
  for (it_map = F_Items.begin(); it_map != F_Items.end(); it_map++) {
    unsigned short target_id = it_map->first;
    while (target_id >= 0 && target_id <= max_id && parent[target_id] != r_id) {
      target_id = parent[target_id];
    }

    if (!it_map->second.empty() && it_map->second[0].next_hop != target_id) {
      it_map->second[0].next_hop = target_id;
      change = 1;
    }
  }

  return change == 1;
}

// Function to increase the sequence number
void Forwarding::incSeq() {
  seq_num++;
}
