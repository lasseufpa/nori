#include "E2-term-helper.h"

#include "ns3/antenna-module.h"
#include "ns3/config-store.h"
#include "ns3/core-module.h"
#include "ns3/eps-bearer-tag.h"
#include "ns3/grid-scenario-helper.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/log.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/nr-helper.h"
#include "ns3/nr-module.h"
#include "ns3/nr-point-to-point-epc-helper.h"
#include <ns3/E2-report.h>
#include <ns3/config.h>
#include <ns3/log.h>
#include <ns3/lte-enb-net-device.h>
#include <ns3/lte-enb-rrc.h>
#include <ns3/lte-pdcp.h>
#include <ns3/lte-rlc.h>
#include <ns3/nr-gnb-net-device.h>
#include <ns3/nstime.h>
#include <ns3/object-map.h>
#include <ns3/object.h>
#include <ns3/oran-interface.h>
#include <ns3/pointer.h>
#include <ns3/simulator.h>
#include <ns3/string.h>
#include <ns3/type-id.h>
#include <ns3/uinteger.h>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("E2TermHelper");
NS_OBJECT_ENSURE_REGISTERED(E2TermHelper);

E2TermHelper::E2TermHelper()
{
    NS_LOG_FUNCTION(this);
    m_e2ForceLog = false;
}

E2TermHelper::~E2TermHelper()
{
    NS_LOG_FUNCTION(this);
}

TypeId
E2TermHelper::GetTypeId()
{
    static TypeId tid = TypeId("ns3::E2TermHelper")
                            .SetParent<Object>()
                            .AddConstructor<E2TermHelper>()
                            .AddAttribute("E2Term",
                                          "E2 term creation, using the eNB/gNB net device; creates "
                                          "an instance of the E2 term.",
                                          PointerValue(),
                                          MakePointerAccessor(&E2TermHelper::m_e2Term),
                                          MakePointerChecker<E2Termination>())
                            .AddAttribute("E2TermIp",
                                          "The IP address of the RIC E2 termination",
                                          StringValue("10.107.233.133"),
                                          MakeStringAccessor(&E2TermHelper::m_e2ip),
                                          MakeStringChecker())
                            .AddAttribute("E2Port",
                                          "Port number for E2",
                                          UintegerValue(36422),
                                          MakeUintegerAccessor(&E2TermHelper::m_e2port),
                                          MakeUintegerChecker<uint16_t>())
                            .AddAttribute("E2LocalPort",
                                          "The first port number for the local bind",
                                          UintegerValue(38470),
                                          MakeUintegerAccessor(&E2TermHelper::m_e2localPort),
                                          MakeUintegerChecker<uint16_t>());
    return tid;
}

void
E2TermHelper::InstallE2Term(Ptr<NetDevice> NetDevice)
{
    NS_LOG_FUNCTION(this);

    // Create E2 messages scheduling
    auto e2Messages = CreateObject<E2Interface>(NetDevice);
    
    m_e2Report = e2Messages->GetE2DuCalculator();

    // NetDevice is a gNB or eNB
    auto nrGnbNetDev = DynamicCast<NrGnbNetDevice>(NetDevice);
    // auto lteEnbNetDev = DynamicCast<LteEnbNetDevice>(NetDevice);

    // Public Land Mobile Network Identifier or with abbreviated version PLMN is a combination of
    // MCC and MNC. It is unique value and globally used to identify the mobile network that a user
    // subscribed.
    std::string plmnId = "111";
    // node cell ID
    uint16_t cellId{0};
    // Client local port
    uint16_t localPort{0};

    // Enable E2 traces
    EnableE2PdcpTraces();
    EnableE2RlcTraces();

    if (nrGnbNetDev != nullptr)
    {
        cellId = nrGnbNetDev->GetCellId();
        NS_LOG_DEBUG("Cell ID: " << cellId);
        localPort = m_e2localPort + cellId;
        e2Messages->SetE2PdcpStatsCalculator(m_e2PdcpStats);
        e2Messages->SetE2RlcStatsCalculator(m_e2RlcStats);
    }
    // else if (lteEnbNetDev != nullptr)
    //{
    //     cellId = lteEnbNetDev->GetCellId();
    //     localPort = m_e2localPort + cellId;
    //     // e2Messages->SetE2PdcpStatsCalculator(m_e2LtePdcpStats);
    //     // e2Messages->SetE2RlcStatsCalculator(m_e2LteRlcStats);
    // }
    else
    {
        NS_ABORT_MSG("NetDevice is not a gNB or eNB");
    }
    // Assert that configuration was properly set
    NS_ASSERT(plmnId == "111" && cellId != 0 && localPort != 0);

    auto e2Term = CreateObject<E2Termination>(m_e2ip,
                                              m_e2port,
                                              m_e2localPort,
                                              std::to_string(cellId),
                                              plmnId);
    NetDevice->AggregateObject(e2Term);
    e2Messages->SetAttribute("E2Term", PointerValue(e2Term));

    // Connect PDU's packets to callback report, after UEs registration.
    // The schedule ensures that the connection is made after the UEs are registered.
    Simulator::Schedule(Seconds(0.2),
                        &E2TermHelper::ConnectPDUReports,
                        this,
                        NetDevice,
                        e2Messages);
    // Connect PHY traces
    ConnectPhyTraces();
    // Enable SINR traces
    EnableSinrTraces(e2Messages);

    // Connect E2 termination to E2 messages
    if (!m_e2ForceLog)
    {
        Ptr<KpmFunctionDescription> kpmFd = Create<KpmFunctionDescription>();
        e2Term->RegisterKpmCallbackToE2Sm(
            200,
            kpmFd,
            std::bind(&E2Interface::KpmSubscriptionCallback, e2Messages, std::placeholders::_1));

        Simulator::Schedule(MicroSeconds(0), &E2Termination::Start, e2Term);
    }
    else
    {
        ForceE2Log(NetDevice);
    }

    NetDevice->AggregateObject(e2Messages);
}

void
E2TermHelper::InstallE2Term(NetDeviceContainer& NetDevices)
{
    NS_LOG_FUNCTION(this);
    for (size_t i = 0; i < NetDevices.GetN(); i++)
    {
        InstallE2Term(NetDevices.Get(i));
    }
}

void
E2TermHelper::EnableE2PdcpTraces()
{
    NS_LOG_FUNCTION(this);
    // Assert that the E2 report is going to be called once
    // if (m_e2PdcpStats == nullptr && m_e2LtePdcpStats == nullptr)
    //{
    // Enable E2 PDCP traces
    m_e2PdcpStats = CreateObject<NrBearerStatsCalculator>("E2PDCP");
    m_e2PdcpStats->SetAttribute("DlPdcpOutputFilename", StringValue("DlE2PdcpStats.txt"));
    m_e2PdcpStats->SetAttribute("UlPdcpOutputFilename", StringValue("UlE2PdcpStats.txt"));
    m_e2StatsConnector.EnablePdcpStats(m_e2PdcpStats);

    // m_e2Report->EnableE2PdcpStats(m_e2PdcpStats);

    // Enable E2 PDCP traces for LTE
    // m_e2LtePdcpStats = CreateObject<NrBearerStatsCalculator>("E2PDCPLTE");
    // m_e2LtePdcpStats->SetAttribute("DlPdcpOutputFilename",
    // StringValue("DlE2PdcpStatsLte.txt"));
    // m_e2LtePdcpStats->SetAttribute("UlPdcpOutputFilename",
    // StringValue("UlE2PdcpStatsLte.txt")); m_e2LtePdcpStats->SetAttribute("EpochDuration",
    // TimeValue(Seconds(1))); m_e2Report->EnableE2PdcpStats(m_e2LtePdcpStats);
    //}
}

void
E2TermHelper::EnableE2RlcTraces()
{
    NS_LOG_FUNCTION(this);
    // Assert that the E2 report is going to be called once
    // if (m_e2RlcStats == nullptr && m_e2LteRlcStats == nullptr)
    //{
    // Enable E2 RLC traces
    m_e2RlcStats = CreateObject<NrBearerStatsCalculator>("E2RLC");
    m_e2RlcStats->SetAttribute("DlRlcOutputFilename", StringValue("DlE2RlcStats.txt"));
    m_e2RlcStats->SetAttribute("UlRlcOutputFilename", StringValue("UlE2RlcStats.txt"));
    m_e2StatsConnector.EnableRlcStats(m_e2RlcStats);

    // Enable E2 RLC traces for LTE
    // m_e2LteRlcStats = CreateObject<NrBearerStatsCalculator>("E2RLCLTE");
    // m_e2LteRlcStats->SetAttribute("DlRlcOutputFilename", StringValue("DlE2RlcStatsLte.txt"));
    // m_e2LteRlcStats->SetAttribute("UlRlcOutputFilename", StringValue("UlE2RlcStatsLte.txt"));
    // m_e2LteRlcStats->SetAttribute("EpochDuration", TimeValue(Seconds(1)));
    // m_e2Report->EnableE2RlcStats(m_e2LteRlcStats);
    //}
}

void
E2TermHelper::EnableSinrTraces(Ptr<E2Interface> e2Messages)
{
    NS_LOG_FUNCTION(this);
    Config::ConnectFailSafe("/NodeList/*/DeviceList/*/ComponentCarrierMapUe/*/NrUePhy/DlDataSinr",
                            MakeCallback(&E2Interface::RegisterNewSinrReadingCallback, e2Messages));
}

void
E2TermHelper::ForceE2Log(Ptr<NetDevice> nodeB)
{
    NS_FATAL_ERROR("Not implemented yet");

    /**
     *
        Simulator::Schedule(MicroSeconds(500),
                            &E2Interface::BuildAndSendReportMessage,
                            m_e2Interface,
                            E2Termination::RicSubscriptionRequest_rval_s{},
                            NetDevice);
     */
}

void
E2TermHelper::ConnectPDUReports([[maybe_unused]] Ptr<NetDevice> NetDevice,
                                [[maybe_unused]] Ptr<E2Interface> e2Message)
{
    NS_LOG_FUNCTION(this << "Connecting PDU reports in E2 interface");

    Config::ConnectWithoutContext(
        "/NodeList/*/DeviceList/*/LteEnbRrc/UeMap/*/DataRadioBearerMap/*/LteRlc/TxPDU",
        MakeCallback(&E2Interface::ReportTxPDU, e2Message));
}

void
E2TermHelper::ConnectPhyTraces()
{
    Config::Connect(
        "/NodeList/*/DeviceList/*/ComponentCarrierMapUe/*/NrUePhy/SpectrumPhy/RxPacketTraceUe",
        MakeCallback(&NoriE2Report::UpdateTraces, m_e2Report));
}

} // namespace ns3
