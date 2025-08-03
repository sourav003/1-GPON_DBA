/*
 * onu.cc
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
#include "gtc_payload_m.h"

using namespace std;
using namespace omnetpp;

class ONU : public cSimpleModule
{
    private:
        cQueue queue_TC1;                       // queue for T-CONT 1 traffic: fixed bandwidth with guarantee
        cQueue queue_TC2;                       // queue for T-CONT 2 traffic: assured bandwidth with bound
        cQueue queue_TC3;                       // queue for T-CONT 3 traffic: assured bandwidth without guarantee
        double capacity;                        // buffer size = 100 MB
        double pending_buffer_TC1 = 0;          // pending data size in buffer
        double pending_buffer_TC2 = 0;
        double pending_buffer_TC3 = 0;
        double packet_drop_count = 0;
        double olt_onu_rtt = 0;
        double start_time_TC1 = 0;
        double onu_rx_grant_TC1 = 0;
        double onu_grant_TC1 = 0;
        double start_time_TC2 = 0;
        double onu_rx_grant_TC2 = 0;
        double onu_grant_TC2 = 0;
        double start_time_TC3 = 0;
        double onu_rx_grant_TC3 = 0;
        double onu_grant_TC3 = 0;
        double gtc_hdr_sz = 0;
        double gtc_pkt_sz = 0;

        simsignal_t latencySignal;

    protected:
        // The following redefined virtual function holds the algorithm.
        virtual void initialize() override;
        virtual void handleMessage(cMessage *msg) override;
};

Define_Module(ONU);

void ONU::initialize()
{
    latencySignal = registerSignal("latency");  // registering the signal

    queue_TC1.setName("queue_TC1");
    queue_TC2.setName("queue_TC2");
    queue_TC3.setName("queue_TC3");
    capacity = onu_buffer_capacity;

    gate("inSrc")->setDeliverImmediately(true);
    gate("SpltGate_i")->setDeliverImmediately(true);
}

void ONU::handleMessage(cMessage *msg)
{
    if(msg->isPacket() == true) {
        if(strcmp(msg->getName(),"bkg_data") == 0) {                    // background traffic is considered for T-CONT 3
            ethPacket *pkt = check_and_cast<ethPacket *>(msg);
            double buffer = pending_buffer_TC1 + pending_buffer_TC2 + pending_buffer_TC3 + pkt->getByteLength();      // future buffer size if current packet is queued
            if(buffer <= onu_buffer_capacity) {                         // queue the current packet if there is buffer capacity
                pkt->setOnuArrivalTime(simTime());
                pkt->setOnuId(getIndex());
                pkt->setTContId(3);             // for TC-3
                //EV << "[onu" << getIndex() << "] Packet arrived from source and being queued at ONU" << endl;
                queue_TC3.insert(pkt);
                pending_buffer_TC3 += pkt->getByteLength();
                //EV << "[onu" << getIndex() << "] Current TC3 queue length = " << queue_TC3.getLength() << " at ONU = " << getIndex() <<endl;
                //EV << "[onu" << getIndex() << "] Current buffer length = " << pending_buffer_TC3 << " at ONU = " << getIndex() <<endl;
            }
            //delete pkt;
        }
        else if(strcmp(msg->getName(),"gtc_hdr_dl") == 0) {
            gtc_header *pkt = check_and_cast<gtc_header *>(msg);
            double arr_time = pkt->getArrivalTime().dbl();
            EV << "[onu" << getIndex() << "] gtc_hdr_dl arrival time: " << arr_time << endl;

            olt_onu_rtt = pkt->getOlt_onu_rtt(getIndex());
            start_time_TC3 = pkt->getOnu_start_time_TC3(getIndex());
            onu_rx_grant_TC3 = pkt->getOnu_grant_TC3(getIndex());
            EV << "[onu" << getIndex() << "] olt_onu_rtt: " << olt_onu_rtt << ", start_time_TC3: " << start_time_TC3 << ", onu_grant_TC3: " << onu_rx_grant_TC3 << endl;

            double ul_tx_time = arr_time + 2*max_polling_cycle + start_time_TC3 - olt_onu_rtt;      // if RTT > 125/2 usec, then multiply by 2, else 1
            // - (pkt->getBitLength()/pon_link_datarate)
            cMessage *send_ul_header = new cMessage("send_ul_header");    // send uplink data
            scheduleAt(ul_tx_time, send_ul_header);
            EV << "[onu" << getIndex() << "] send_ul_header is scheduled at: " << ul_tx_time << endl;

            delete pkt;
        }
    }
    else {      // if not packet but a message
        if(strcmp(msg->getName(),"ping") == 0) {
            ping *png = check_and_cast<ping *>(msg);
            png->setONU_id(getIndex());
            send(png,"SpltGate_o");                   // immediately send the ping message back
            //EV << "[onu" << getIndex() << "] Sending ping response from ONU-" << getIndex() << endl;
        }
        else if(strcmp(msg->getName(),"send_ul_header") == 0) {
            cancelAndDelete(msg);         // delete the current instance of self-message

            gtc_hdr_sz = 3 + 1 + 1 + 5 + 8;                   // total size of GTC UL header: Preamble+Delim+BIP+PLOu_Header
            onu_grant_TC3 = onu_rx_grant_TC3 - gtc_hdr_sz;

            gtc_header *gtc_hdr_ul = new gtc_header("gtc_hdr_ul");
            gtc_hdr_ul->setByteLength(gtc_hdr_sz);
            gtc_hdr_ul->setUplink(true);
            gtc_hdr_ul->setOnuID(getIndex());

            EV << "[onu" << getIndex() << "] Sending gtc_hdr_ul from ONU-" << getIndex() << " at = " << simTime() << endl;
            send(gtc_hdr_ul,"SpltGate_o");

            simtime_t Txtime = (simtime_t)(gtc_hdr_ul->getBitLength()/pon_link_datarate);

            cMessage *send_ul_payload = new cMessage("send_ul_payload");            // send uplink data
            scheduleAt(gtc_hdr_ul->getSendingTime()+Txtime, send_ul_payload);

            //EV << "[onu" << getIndex() << "] latest pending_buffer_TC3: " << pending_buffer_TC3 << endl;
        }
        else if(strcmp(msg->getName(),"send_ul_payload") == 0) {
            if((onu_grant_TC3 > 0)&&(pending_buffer_TC3 > 0)) {
                ethPacket *front = (ethPacket *)queue_TC3.front();
                if(front->getByteLength() <= onu_grant_TC3) {                // check if the first packet can be sent now
                    ethPacket *data = (ethPacket *)queue_TC3.pop();          // pop and send the packet
                    onu_grant_TC3 -= data->getByteLength();
                    pending_buffer_TC3 -= data->getByteLength();

                    EV << "[onu" << getIndex() << "] Sending ul payload: " << data->getByteLength() << ", pending_buffer_TC3 = " << pending_buffer_TC3 << ", onu_grant_TC3 = " << onu_grant_TC3 << endl;
                    send(data,"SpltGate_o");
                    data->setOnuDepartureTime(data->getSendingTime());

                    double packet_latency = data->getOnuDepartureTime().dbl() - data->getOnuArrivalTime().dbl();
                    EV << "[onu" << getIndex() << "] packet_latency: " << packet_latency << endl;
                    emit(latencySignal,packet_latency);

                    // rescheduling send_ul_payload to send the consecutive queued packets
                    simtime_t Txtime = (simtime_t)(data->getBitLength()/pon_link_datarate);
                    scheduleAt(data->getSendingTime()+Txtime,msg);
                }
                else {      // if the remaining grant is insufficient to send the next packet
                    EV << "[onu" << getIndex() << "] ul transmission finished at: " << simTime() << endl;
                    cancelAndDelete(msg);   // cleaning up packetSend msg
                }
            }
            else {                      // either grant <= 0 or pending_buffer = 0
                EV << "[onu" << getIndex() << "] ul transmission finished at: " << simTime() << endl;
                cancelAndDelete(msg);   // cleaning up packetSend msg
            }
        }
    }
}


