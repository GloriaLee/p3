#ifndef FORWARDING_H
#define FORWARDING_H

#include "global.h"
#define REFRESH_THRESHOLD 45

struct F_Item {
  F_Item(){};
  F_Item(unsigned short dest, unsigned int cost, unsigned short next_hop, unsigned int seq_num): dest(dest), cost(cost), next_hop(next_hop), timestamp(0), seq_num(seq_num) {};
  unsigned short dest;
  unsigned int cost;
  unsigned short next_hop;
  short int timestamp;
  unsigned int seq_num;
};

struct Dist_Pair {
  Dist_Pair(unsigned short r_id, unsigned int dist): r_id(r_id), dist(dist) {};
  unsigned short r_id;
  unsigned int dist;
};

struct Compare {
  bool operator() (Dist_Pair p1, Dist_Pair p2) {
    return (p1.dist > p2.dist);
  }
};

class Forwarding {
  public:
    Forwarding();
    void set_routerId(unsigned short router_id);
    void set_protocol(eProtocolType protocol);
    bool state_check();
    void* make_DV_packet(unsigned short targetID, unsigned short& DV_packet_size);
    bool parse_DV_packet(void* packet, unsigned short size, unsigned int delay);
    void insert_entry(unsigned short dest, unsigned int cost, unsigned short next_hop);
    void incTime();
    bool update_DV_Table(unsigned short dest, unsigned int curr_cost, unsigned int prev_cost, unsigned short next_hop);
    bool parse_Data_packet(void* data_packet, unsigned short size, unsigned short& next_hop);
    int get_size();
    void* make_LS_packet(unsigned short& packSize);
    bool parse_LS_packet(void* packet, unsigned short size);
    bool update_LS_Table(unsigned short source_id, unsigned int cost);
    bool update_path();
    void incSeq();
  private:
    eProtocolType pr;
    unsigned short r_id;
    unsigned short seq_num;
    unordered_map<int, vector<F_Item>> F_Items;
};

#endif
