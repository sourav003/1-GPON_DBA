/*
 * splitter.cc
 *
 *  Created on: 30 July 2025
 *      Author: mondals
 */

#include <string.h>
#include <math.h>
#include <omnetpp.h>
#include <numeric>   // Required for std::iota
#include <algorithm> // Required for std::sort

#include "sim_params.h"
#include "ethPacket_m.h"
#include "ping_m.h"
#include "gtc_header_m.h"

using namespace std;
using namespace omnetpp;

class Splitter : public cSimpleModule
{
    private:
        cQueue onu_queue;            // Queue for packets to be sent to ONUs
        cQueue olt_queue;            // Queue for packets to be sent to OLT
        double onu_queue_size;
        double olt_queue_size;

    protected:
       // The following redefined virtual function holds the algorithm.
       virtual void initialize() override;
        virtual void handleMessage(cMessage *msg) override;
};

Define_Module(Splitter);

void Splitter::initialize()
{
    onu_queue.setName("onu_queue");
    olt_queue.setName("olt_queue");
    olt_queue_size = 0;
    onu_queue_size = 0;
    // Make sure incoming message is delivered immediately
    gate("OltGate_i")->setDeliverImmediately(true);
    int n = gateSize("OnuGate_o");
    for (int k = 0; k < n; k++) {
        gate("OnuGate_i",k)->setDeliverImmediately(true);
    }
}

void Splitter::handleMessage(cMessage *msg)
{
    if(msg->isPacket() == true) {
        //EV << "[splt] input received as packet!" << endl;
        if(msg->arrivedOn("OltGate_i") == true) {     // any message arriving from OLT are broadcasted to all ONUs
            if(strcmp(msg->getName(),"gtc_hdr_dl") == 0) {
                gtc_header *pkt = check_and_cast<gtc_header *>(msg);
                //EV << "[splt] gtc_hdr_dl received at OltGate_i" << endl;
                int n = gateSize("OnuGate_o");
                for (int k = 0; k < n; k++) {
                    //EV << "[splt] sending packet to ONU-"<< k <<" at "<< simTime() << endl;
                    cGate *onu_gate = gate("OnuGate_o",k);
                    cChannel *onu_ch = onu_gate->getChannel();
                    if((onu_ch->isBusy() == false)&&(onu_queue.getLength() == 0)) {
                        gtc_header *copy = pkt->dup();       // creating a copy for all and sending immediately
                        send(copy,"OnuGate_o",k);
                    }
                    else {
                        gtc_header *copy = pkt->dup();
                        copy->setOnuID(k);                  // set the OnuID with the current value k
                        onu_queue.insert(copy);

                        cMessage *onu_tx = new cMessage("ONU_Tx_Delay");
                        scheduleAt(onu_ch->getTransmissionFinishTime()+(simtime_t)(onu_queue_size*8/pon_link_datarate),onu_tx);
                        onu_queue_size += copy->getByteLength();
                    }
                }
                delete pkt;
            }
        }
        else if(msg->arrivedOn("OnuGate_i") == true){
            cGate *olt_gate = gate("OltGate_o");
            cChannel *olt_ch = olt_gate->getChannel();
            if((olt_ch->isBusy() == false)&&(olt_queue.getLength() == 0)) {
                send(msg,"OltGate_o");
                //EV << "[splt] 76 sending packet to OLT at "<< simTime() << endl;
            }
            else {
                EV << "[splt] channel busy so queuing for OLT at "<< simTime() << endl;
                if (strcmp(msg->getName(), "gtc_hdr_ul") == 0) {
                    gtc_header *pkt = check_and_cast<gtc_header *>(msg);
                    olt_queue.insert(pkt);

                    cMessage *olt_tx = new cMessage("OLT_Tx_Delay");
                    scheduleAt(olt_ch->getTransmissionFinishTime()+(simtime_t)(olt_queue_size*8/pon_link_datarate),olt_tx);
                    olt_queue_size += pkt->getByteLength();
                    EV << "[splt] gtc_hdr_ul queued; OLT_Tx_Delay at: " << olt_ch->getTransmissionFinishTime()+(simtime_t)(olt_queue_size*8/pon_link_datarate) << ", Queue size = " << olt_queue_size << endl;
                }

                if (strcmp(msg->getName(), "bkg_data") == 0) {
                    ethPacket *pkt = check_and_cast<ethPacket *>(msg);
                    olt_queue.insert(pkt);

                    cMessage *olt_tx = new cMessage("OLT_Tx_Delay");
                    scheduleAt(olt_ch->getTransmissionFinishTime()+(simtime_t)(olt_queue_size*8/pon_link_datarate),olt_tx);
                    olt_queue_size += pkt->getByteLength();
                    EV << "[splt] bkg_data queued; OLT_Tx_Delay at: " << olt_ch->getTransmissionFinishTime()+(simtime_t)(olt_queue_size*8/pon_link_datarate) << ", Queue size = " << olt_queue_size << endl;
                }
            }
        }
    }
    else {
        if(msg->isSelfMessage()) {
            if(strcmp(msg->getName(),"OLT_Tx_Delay") == 0) {
                EV << "[splt] OLT_Tx_Delay detected!"<< endl;
                cancelAndDelete(msg);

                if(strcmp(olt_queue.front()->getName(),"gtc_hdr_ul") == 0) {
                    EV << "[splt] sending gtc_hdr_ul packet to OLT at "<< simTime() << endl;
                    gtc_header *pkt = (gtc_header *)olt_queue.pop();
                    send(pkt,"OltGate_o");
                    EV << "[splt] Sent delayed gtc_hdr_ul at: " << simTime() << endl;
                    olt_queue_size -= pkt->getByteLength();
                }
                if(strcmp(olt_queue.front()->getName(),"bkg_data") == 0) {
                    EV << "[splt] sending bkg_data packet to OLT at "<< simTime() << endl;
                    ethPacket *pkt = (ethPacket *)olt_queue.pop();
                    send(pkt,"OltGate_o");
                    EV << "[splt] Sent delayed bkg_data at: " << simTime() << endl;
                    olt_queue_size -= pkt->getByteLength();
                }
            }
        }

        //EV << "[splt] input received as message!" << endl;
        if(msg->arrivedOn("OltGate_i") == true) {     // any message arriving from OLT are broadcasted to all ONUs
            if(strcmp(msg->getName(),"ping") == 0) {
                ping *png = check_and_cast<ping *>(msg);
                //EV << "[splt] Ping received at OltGate_i" << endl;
                int n = gateSize("OnuGate_o");
                //EV << "[splt] OnuGate_o size = " << n << endl;
                for (int k = 0; k < n; k++) {
                    ping *copy = png->dup();       // creating a copy for all
                    send(copy,"OnuGate_o",k);
                    //EV << "[splt] Forwarding ping to ONU-" << k << endl;
                }
                delete png;
            }
        }
        else {                                      // any message arriving from any ONU are sent to the OLT
            send(msg,"OltGate_o");
            //EV << "[splt] Forwarding ping to OLT" << endl;
        }
    }
}


