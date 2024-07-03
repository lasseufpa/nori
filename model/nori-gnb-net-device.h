// mmwave-enb-net-device.h --> nori-gnb-net-device.h
#ifndef NORI_NR_GNB_NET_DEVICE_H_
#define NORI_NR_GNB_NET_DEVICE_H_

#include "nori-E2-report.h"
#include "nori-lte-rlc.h"
#include "ns3/nori-bearer-stats-calculator.h"

#include <ns3/bandwidth-part-gnb.h>
#include <ns3/event-id.h>
#include <ns3/lte-enb-rrc.h>
#include <ns3/lte-rlc.h>
//#include <ns3/nr-bearer-stats-calculator.h>
#include <ns3/nr-gnb-mac.h>
#include <ns3/nr-gnb-net-device.h>
#include <ns3/nr-gnb-phy.h>
#include <ns3/nr-mac-scheduler.h>
#include <ns3/nr-net-device.h>
#include <ns3/nr-phy-rx-trace.h>
#include <ns3/nr-phy.h>
#include <ns3/nstime.h>
#include <ns3/oran-interface.h>
#include <ns3/traced-callback.h>

#include <map>
#include <vector>

namespace ns3
{

typedef std::pair<uint64_t, uint16_t> ImsiCellIdPair_t;
class NrGnbNetDevice;
class NoriE2Report;

/**
 * \ingroup nori
 *
 * \brief The NoriGnbNetDevice class represents a network device for a Nori GNB (gNodeB).
 *
 * This class is derived from the NrGnbNetDevice class and provides additional functionality specific to the Nori model.
 * It implements methods for initializing the device, setting E2 termination, building and sending report messages,
 * handling subscription callbacks, receiving control messages, and updating configuration.
 *
 * The NoriGnbNetDevice class also contains private helper methods for building indication headers and messages,
 * retrieving IMSI strings, and getting RLC buffer occupancy.
 *
 * \see NrGnbNetDevice
 */
class NoriGnbNetDevice : public NrGnbNetDevice
{
  public:
    /**
     * Constructor
     */
    NoriGnbNetDevice();
    /**
     * TypeId
     */
    static TypeId GetTypeId();
    /**
     * Destructor
     */
    ~NoriGnbNetDevice() override;

    const static uint16_t E2SM_REPORT_MAX_NEIGH = 8;

    void DoInialize();
    void SetE2Termination(Ptr<E2Termination> e2term);

    Ptr<E2Termination> GetE2Termination() const;

    void BuildAndSendReportMessage(E2Termination::RicSubscriptionRequest_rval_s params);

    void KpmSubscriptionCallback(E2AP_PDU_t* sub_req_pdu);

    void ControlMessageReceivedCallback(E2AP_PDU_t* sub_req_pdu);

    void SetStartTime(uint64_t);

    void GetPeriodicPdcpStats();
    void UpdateConfig();
    Ptr<E2Termination> m_e2term;

  private:
    double m_e2Periodicity;

    Ptr<KpmIndicationHeader> BuildRicIndicationHeader(std::string plmId,
                                                      std::string gnbId,
                                                      uint16_t nrCellId) const;
    Ptr<KpmIndicationMessage> BuildRicIndicationMessageCuUp(std::string plmId);
    Ptr<KpmIndicationMessage> BuildRicIndicationMessageCuCp(std::string plmId);
    Ptr<KpmIndicationMessage> BuildRicIndicationMessageDu(std::string plmId, uint16_t nrCellId);
    std::string GetImsiString(uint64_t imsi);
    uint32_t GetRlcBufferOccupancy(Ptr<LteRlc> rlc) const;

    bool m_sendCuUp;
    bool m_sendCuCp;
    bool m_sendDu;

    void RegisterNewSinrReadingCallback(Ptr<NrGnbNetDevice> netDev,
                                        std::string context,
                                        uint64_t imsi,
                                        uint16_t cellId,
                                        long double sinr);
    void RegisterNewSinrReading(uint64_t imsi, uint16_t cellId, long double sinr);
    std::map<uint64_t, std::map<uint16_t, long double>> m_l3sinrMap;
    uint64_t m_startTime;
    std::map<uint64_t, uint32_t> m_drbThrDlPdcpBasedComputationUeid;
    std::map<uint64_t, uint32_t> m_drbThrDlUeid;
    bool m_isReportingEnabled{false}; //! true is KPM reporting cycle is active, false otherwise
    bool m_reducedPmValues{false};    //< if true use a reduced subset of pmvalues

    uint16_t m_basicCellId;

    bool m_forceE2FileLogging{false}; //< if true log PMs to files
    std::string m_cuUpFileName;
    std::string m_cuCpFileName;
    std::string m_duFileName;

    Ptr<NoriBearerStatsCalculator> m_e2PdcpStatsCalculator;
    Ptr<NoriBearerStatsCalculator> m_e2RlcStatsCalculator;
    Ptr<NoriE2Report> m_e2DuCalculator;
};
} // namespace ns3

#endif /* NORI_NR_GNB_NET_DEVICE_H_ */
