/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2026 LASSE/UFPA - Universidade Federal do Pará
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#pragma once

#include "E2-report.h"
#include "encode_e2apv1.hpp"
#include "oran-interface.h"

#include "ns3/nr-bearer-stats-calculator.h"
#include "ns3/nr-gnb-net-device.h"
#include "ns3/nr-phy-rx-trace.h"
#include "ns3/nr-rl-mac-scheduler-ofdma.h"

#include <nlohmann/json.hpp>

#include "ns3/event-id.h"
#include "ns3/nstime.h"

#include <deque>
#include <mutex>

namespace ns3
{
class NoriE2Report;
class NrUeManager;

typedef std::pair<uint64_t, uint16_t> ImsiCellIdPair_t;

class E2Interface : public Object
{
  public:
    const static uint16_t E2SM_REPORT_MAX_NEIGH = 8; //<! Maximum number of neighbors

    /**
     * @brief Constructor
     */
    E2Interface();

    /**
     * @brief Constructor
     * @param netDev the net device of the nodeB
     */
    E2Interface(Ptr<NetDevice> netDev);

    /**
     * @brief Destructor
     */
    ~E2Interface() override
    {
    }

    /**
     * @brief Get the type ID.
     * @return the object TypeId
     */
    static TypeId GetTypeId();

    /**
     * @brief Function Service Subscription Request callback.
     * This function is triggered whenever a RIC Subscription Request is received.
     *
     * @param pdu request message
     */
    void FunctionServiceSubscriptionCallback(E2AP_PDU_t* sub_req_pdu);
    void CccSubscriptionCallback(E2AP_PDU_t* sub_req_pdu);
    //callback specific for the ccc service model, it will be removed in the future
    /**
     * @brief Register new SINR reading callback
     * @param path the path
     * @param cellId the cell identifier
     * @param rnti the Radio Network Temporary Identifier
     * @param avgSinr the average SINR
     * @param bwpId the Bandwidth Part Identifier
     */
    void RegisterNewSinrReadingCallback([[maybe_unused]] std::string path,
                                        uint16_t cellId,
                                        uint16_t rnti,
                                        double avgSinr,
                                        uint16_t bwpId);

    /**
     * @brief Register new SINR reading
     */
    void RegisterNewSinrReading(uint16_t imsi, uint16_t cellId, double avgSinr);

    /**
     * @brief Build and send report message
     * @param params subscription request parameters
     * @param nodeBNetdev the net device of the nodeB
     *
     */
    void BuildAndSendReportMessage(E2Termination::RicSubscriptionRequest_rval_s params);

    /**
     * @brief Report the number of TX PDU calls
     * @param rnti the current Radio network temporary identifier
     * @param lcid the current cell identifier
     * @param packetSize the size of the packet in bytes
     */
    void ReportTxPDU(uint16_t rnti, uint8_t lcid, uint32_t packetSize);

    /**
     * @brief Set E2 PDCP stats variable report
     */
    void SetE2PdcpStatsCalculator(Ptr<NrBearerStatsCalculator> e2PdcpStatsCalculator);

    /**
     * @brief Set E2 RLC stats variable report
     */
    void SetE2RlcStatsCalculator(Ptr<NrBearerStatsCalculator> e2RlcStatsCalculator);

    /**
     * @brief Control Message Received Callback: A handler that deals with the control message
     * received
     * @param sub_req_pdu the subscription request PDU
     */
    void ControlMessageReceivedCallback(E2AP_PDU_t* sub_req_pdu);

    /**
     * @brief Apply slicing control directive to the MAC scheduler
     * @param quotas slice PRB quotas
     */
    bool ApplySlicingControl(const std::vector<RicControlMessage::SlicePRBQuota>& quotas);
    void ApplyCccSlicingControl(const std::vector<SlicePRBQuota>& quotas);

    /**
     * @brief Process control message on the simulation thread
     * @param pdu control message PDU
     */
    void ProcessControlMessage(E2AP_PDU_t* pdu);

    /**
     * @brief Send RIC Control Acknowledge (success) or RIC Control Failure for an E2SM-RC request
     * @param ricRequestId RICrequestID of the original request
     * @param ranFunctionId RANfunctionID of the original request
     * @param success true for Acknowledge, false for Failure
     * @param ricRequestCause CauseRICrequest value used in the Failure
     */
    void SendRcControlOutcome(const RICrequestID_t& ricRequestId,
                              long ranFunctionId,
                              bool success,
                              long ricRequestCause);

    /**
     * @brief Start the periodic event that applies queued E2SM-RC controls.
     *
     * Must be scheduled from the simulation (main) thread, before or during Simulator::Run().
     * E2SM-RC controls arrive on the SCTP thread. With RealtimeSimulatorImpl, an event
     * scheduled from another thread gets a wall-clock timestamp; when the simulation runs
     * slower than real time, the event lands in the simulated future and the quota is
     * applied late (or never, within the run). The SCTP thread therefore only enqueues the
     * control, and this event applies it at the current simulation time.
     */
    void StartRcControlDrain();

    Ptr<NoriE2Report> GetE2DuCalculator();

    /**
     * @brief Cumulative PDCP TX bytes of a UE, summed over all its data radio bearers
     * @param ue UE manager in the gNB RRC
     * @param downlink true for DL, false for UL
     * @return cumulative bytes reported by the E2 PDCP statistics calculator
     */
    uint64_t GetUePdcpTxBytes(Ptr<NrUeManager> ue, bool downlink) const;
    
    void MLSliceInterface(double macPrb, uint64_t imsi);

  private:
    /**
     * @brief Build RIC Indication Header
     * @return the RIC Indication Header
     */
    Ptr<KpmIndicationHeader> BuildRicIndicationHeader() const;
    
    /**
     * @brief Get the IMSI string
     * @param imsi the IMSI
     */
    std::string GetImsiString(uint64_t imsi);

    Ptr<KpmIndicationMessage> BuildNodeLevelIndicationMessage(std::string plmId, uint16_t nrCellId);

    Ptr<KpmIndicationMessage> BuildUeLevelIndicationMessage(std::string plmId, uint16_t nrCellId);

    void BuildBwpReport(nlohmann::json& bwpList);
    void BuildCccCellDuIndication(nlohmann::json& payload);
    void BuildAndSendCccIndication(E2Termination::RicSubscriptionRequest_rval_s params);
    void ApplyCellDuControl(const nlohmann::json& cellDu);
    // bool m_applyPrbSlicing{true};   // example ccc-slicing-demo
    // bool m_applyMimoLayers{false};  // example mimo-demo

    /**
     * @brief Function to help us to flip the map
     * @param src the source map
     * @return the flipped map
     */
    std::multimap<long double, uint16_t> FlipMap(const std::map<uint16_t, long double>& src);

    /// E2SM-RC slicing control received on the SCTP thread, waiting to be applied
    struct PendingRcControl
    {
        std::vector<RicControlMessage::SlicePRBQuota> quotas;
        RICrequestID_t ricRequestId;
        long ranFunctionId;
    };

    /// Apply the queued E2SM-RC controls on the simulation thread and answer the RIC
    void DrainRcControlQueue();

    std::mutex m_rcControlMutex;                    //<! protects m_pendingRcControls
    std::deque<PendingRcControl> m_pendingRcControls; //<! controls queued by the SCTP thread
    Time m_rcControlDrainPeriod;                    //<! period of the drain event
    EventId m_rcControlDrainEvent;                  //<! drain event
    bool m_rcControlDrainStarted{false};            //<! drain event already started

    double m_e2Periodicity;                                          //<! E2 periodicity
    Ptr<NrGnbRrc> m_rrc;                                             //<! RRC object
    std::map<uint64_t, std::map<uint16_t, long double>> m_l3sinrMap; //<! L3 SINR map

    Ptr<E2Termination> m_e2term;                          //<! E2 termination object
    Ptr<NetDevice> m_netDev;                              //<! Net device of the nodeB
    std::map<uint32_t, uint32_t> m_txPDU;                 //<! Number of TX PDU calls
    std::map<uint32_t, uint64_t> m_txPDUBytes;            //<! Number of TX PDU in bytes
    Ptr<NrBearerStatsCalculator> m_e2PdcpStatsCalculator; //<! E2 PDCP stats calculator
    Ptr<NrBearerStatsCalculator> m_e2RlcStatsCalculator;  //<! E2 RLC stats calculator
    Ptr<NoriE2Report> m_e2DuCalculator;                   //<! E2 DU calculator
    uint16_t m_cellId{0};                                 //<! Cell ID
    double m_cellTxDlPackets = 0;                         //<! Number of DL packets
    //double m_cellTxBytes = 0;                             //<! Number of DL bytes
    std::map <uint64_t, double> m_cellTxBytes;                             //<! Cumulative DL volume already reported at node level, per IMSI
    std::map <uint64_t, double> m_ueTxBytes;                               //<! Cumulative DL volume already reported at UE level, per IMSI
    
    double m_cellRxBytes = 0;                             //<! Number of UL bytes
    uint64_t m_startTime = 0;                             //<! Start time
    std::map<uint64_t, uint32_t>
        m_drbThrDlPdcpBasedComputationUeid;      //<! DRB throughput DL PDCP in UE IMSI
    std::map<uint64_t, uint32_t> m_drbThrDlUeid; //<! DRB throughput DL in UE ID
    std::string m_duFileName;                    //<! DU file name
    double macPrb;
    std::map<uint64_t, double> m_previousDlTxData;
    std::map<uint64_t, double> m_previousUlTxData;
    std::map<uint64_t, double> m_previousTime;
};
} // namespace ns3
