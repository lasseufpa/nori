#pragma once

#include "E2-report.h"
#include "encode_e2apv1.hpp"
#include "oran-interface.h"

#include <ns3/nr-bearer-stats-calculator.h>
#include <ns3/nr-gnb-net-device.h>
#include <ns3/nr-phy-rx-trace.h>

namespace ns3
{
class NoriE2Report;

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
    ~E2Interface() override;

    /**
     * @brief Get the type ID.
     * @return the object TypeId
     */
    static TypeId GetTypeId();

    /**void DoInitialize() override;*/

    /**
     * @brief KPM Subscription Request callback.
     * This function is triggered whenever a RIC Subscription Request for
     * the KPM RAN Function is received.
     *
     * @param pdu request message
     * @param e2Term E2 termination object
     */
    void KpmSubscriptionCallback(E2AP_PDU_t* sub_req_pdu);

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

    bool m_useSinrTraces; //<! Flag to enable SINR traces
    Ptr<NoriE2Report> GetE2DuCalculator();
    void MLSliceInterface(double macPrb, uint64_t imsi);

  private:
    /**
     * @brief Build RIC Indication Header
     * @param plmId PLMN ID
     * @param gnbId gNB ID
     * @param CellId NR cell ID
     * @return the RIC Indication Header
     */

    Ptr<KpmIndicationHeader> BuildRicIndicationHeader(std::string plmId,
                                                      std::string gnbId,
                                                      uint16_t CellId) const;

    /**
     * @brief Get the IMSI string
     * @param imsi the IMSI
     */
    std::string GetImsiString(uint64_t imsi);

    /**
     * @brief Build RIC Indication Message for CU-UP
     * @param plmId PLMN ID
     * @return the RIC Indication Message
     */

    Ptr<KpmIndicationMessage> BuildRicIndicationMessageCuUp(std::string plmId);

    /**
     * @brief Build RIC Indication Message for CU-CP
     * @param plmId PLMN ID
     * @return the RIC Indication Message
     *
     */
    Ptr<KpmIndicationMessage> BuildRicIndicationMessageCuCp(std::string plmId);

    /**
     * @brief Build RIC Indication Message for DU
     * @param plmId PLMN ID
     * @param nrCellId NR cell ID
     * @return the RIC Indication Message
     */
    Ptr<KpmIndicationMessage> BuildRicIndicationMessageDu(std::string plmId, uint16_t nrCellId);

    /**
     * @brief Function to help us to flip the map
     * @param src the source map
     * @return the flipped map
     */
    std::multimap<long double, uint16_t> FlipMap(const std::map<uint16_t, long double>& src);

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
    double m_cellTxBytes = 0;                             //<! Number of DL bytes
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
