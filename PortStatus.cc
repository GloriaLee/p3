//
// Created by 李成蹊 on 2017/11/12.
//
#include "PortStatus.h"
#include <netinet/in.h>
#include <string.h>

using namespace std;

PortStatus :: PortStatus() {}

PortStatus :: ~PortStatus() {
    vector<PortInfo>().swap(ports);
}

void PortStatus :: setPortNumbers(unsigned short num) {
    portNum = num;
    for (int i = 0; i < num; i++) {
      ports.push_back(PortInfo(num, -1, -1, -1));
    }
}

void PortStatus :: setRouterId(unsigned short id) {
    routerId = id;
}

unsigned short PortStatus :: getSize() {
    return portNum;
}

bool PortStatus :: checkStates(vector<unsigned short>& invalidList) {
    bool res = false;
    for (int i = 0; i < portNum; i++) {
        if (ports[i].timeStamp > TIME_OUT) {
            invalidList.push_back(ports[i].routerId);
            ports[i].timeStamp = -1;
            res = true;
        }
    }
    return res;
}

void PortStatus :: incTime() {
    for (int i = 0 ; i < portNum; i++) {
        if (ports[i].timeStamp >= 0) {
            ports[i].timeStamp++;
        }
    }
}

// build the data packet
void* PortStatus :: buildPacket(unsigned int systemTime, unsigned short &packSize) {
    packSize = sizeof(unsigned int) + 4 * sizeof(unsigned short);
    unsigned short* pack = (unsigned short*)malloc(packSize);

    *(unsigned char*)pack = 1;
    *((unsigned short*)pack + 1) = (unsigned short)htons((unsigned short)packSize);
    *((unsigned short*)pack + 2) = (unsigned short)htons((unsigned short)routerId);
    *((unsigned int*)pack + 2) = (unsigned int)htonl((unsigned int)systemTime);

    return pack;
}

void* PortStatus :: processPing(unsigned short portId, void *pack, unsigned short size) {
    *((unsigned char*)pack) = (char)2;
    unsigned short destination = (unsigned short) ntohs(*(unsigned short*)pack + 2);
    *((unsigned short*)pack + 3) = (unsigned short) htons((unsigned short)destination);
    *((unsigned short*)pack + 2) = (unsigned short) htons((unsigned short)routerId);
    if (ports[portId].timeStamp > 0) {
        ports[portId].timeStamp = 0;
    }
    return pack;
}

bool PortStatus :: processPong(
    unsigned short portId, void *pack, unsigned int systemTime,
    unsigned short &sourceId, unsigned int &dly
) {
    unsigned short type = *((unsigned char*)pack);
    if (type != 2 || portId >= portNum ) {
        return false;
    }
    sourceId = (unsigned short) ntohs(*((unsigned short*)pack + 2));
    dly = (unsigned int) ntohl(*((unsigned int*)pack + 2));
    dly = systemTime - dly;
    if (ports[portId].timeStamp < 0 || ports[portId].delay != dly || ports[portId].routerId != sourceId) {
        ports[portId].portNum = portId;
        ports[portId].routerId = sourceId;
        ports[portId].delay = dly;
        ports[portId].timeStamp = 0;
        return true;
    }
    ports[portId].timeStamp = 0;
    return false;
}

bool PortStatus :: checkRouteIdFromPortNum(unsigned short num, unsigned short &id) {
    if (num >= portNum) {
        return false;
    }
    if (ports[num].timeStamp >= 0) {
        id = ports[num].routerId;
        return true;
    } else {
        return false;
    }
}

bool PortStatus :: checkPortNumFromRouteId(unsigned short id, unsigned short &num) {
    for (int i = 0; i < portNum; i++) {
        if (ports[i].timeStamp >= 0) {
            if (ports[i].routerId == id) {
                num = ports[i].portNum;
                return true;
            }
        }
    }
    return false;
}

bool PortStatus :: getDelay(unsigned short port, unsigned int &dly) {
    if (ports[port].timeStamp >= 0) {
        dly = ports[port].delay;
        return true;
    } else {
        return false;
    }
}


