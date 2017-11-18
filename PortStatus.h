//
// Created by 李成蹊 on 2017/11/12.
//

#ifndef PROJECT3_PORTSTATUS_H
#define PROJECT3_PORTSTATUS_H

#include "global.h"
#define TIME_OUT 15

using namespace std;

class PortStatus {
public:
    PortStatus();

    struct PortInfo {
        unsigned short portNum;
        unsigned short routerId;
        unsigned int delay;
        unsigned int timeStamp;

        PortInfo(
            unsigned short portNum,
            unsigned short routerId,
            unsigned int delay,
            unsigned int timeStamp
		 ) : portNum(portNum), routerId(routerId), delay(delay), timeStamp(-1) {};
    };

    void setPortNumbers(unsigned short ports);

    void setRouterId(unsigned short id);

    unsigned short getSize();

    bool checkStates(vector<unsigned short>& invalidList);

    void incTime();

    void* buildPacket(unsigned int systemTime, unsigned short& packSize);

    void* processPing(unsigned short portId, void* pack, unsigned short size);

    bool processPong(
        unsigned short portId,
        void* pack,
        unsigned int systemTime,
        unsigned short& sourceId,
        unsigned int &rtt
    );

    bool checkRouteIdFromPortNum(unsigned short num, unsigned short& id);

    bool checkPortNumFromRouteId(unsigned short id, unsigned short& num);

    bool getDelay(unsigned short port, unsigned int& dly);

    ~PortStatus();

private:
    unsigned short portNum;
    unsigned short routerId;
    vector<PortInfo> ports;
};

#endif //PROJECT3_PORTSTATUS_H
