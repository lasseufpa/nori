#include "E2-interface.h"

#include "E2-report.h"

#include <ns3/attribute.h>
#include <ns3/bandwidth-part-gnb.h>
#include <ns3/config.h>
#include <ns3/double.h>
#include <ns3/kpm-indication.h>
#include <ns3/log.h>
#include <ns3/lte-enb-net-device.h>
#include <ns3/lte-enb-rrc.h>
#include <ns3/lte-rlc-am.h>
#include <ns3/lte-rlc.h>
#include <ns3/mmwave-indication-message-helper.h>
#include <ns3/nr-gnb-mac.h>
#include <ns3/nr-gnb-net-device.h>
#include <ns3/nr-mac-sched-sap.h>
#include <ns3/nstime.h>
#include <ns3/object-map.h>
#include <ns3/object.h>
#include <ns3/oran-interface.h>
#include <ns3/pointer.h>
#include <ns3/string.h>
#include <ns3/type-id.h>
#include <ns3/uinteger.h>

#include <encode_e2apv1.hpp>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("E2Interface");
NS_OBJECT_ENSURE_REGISTERED(E2Interface);

E2Interface::E2Interface()
{
    NS_LOG_FUNCTION(this);
    NS_FATAL_ERROR("E2Interface must be created with a net device");
}

E2Interface::E2Interface(Ptr<NetDevice> netDev)
{
    NS_LOG_FUNCTION(this);
    m_netDev = netDev;
    m_rrc = m_netDev->GetObject<NrGnbNetDevice>()->GetRrc();
    m_e2DuCalculator = CreateObject<NoriE2Report>();
}

E2Interface::~E2Interface()
{
    NS_LOG_FUNCTION(this);
}

TypeId
E2Interface::GetTypeId()
{
    static TypeId tid = TypeId("ns3::E2Interface")
                            .SetParent<Object>()
                            .AddConstructor<E2Interface>()
                            .AddAttribute("E2Term",
                                          "E2 term creation, instance of the E2 term.",
                                          PointerValue(),
                                          MakePointerAccessor(&E2Interface::m_e2term),
                                          MakePointerChecker<E2Termination>())
                            .AddAttribute("nodeBNetDevice",
                                          "The net device of the nodeB",
                                          PointerValue(),
                                          MakePointerAccessor(&E2Interface::m_netDev),
                                          MakePointerChecker<NetDevice>())
                            .AddAttribute("E2Periodicity",
                                          "The periodicity of the E2 report messages",
                                          DoubleValue(0.5),
                                          MakeDoubleAccessor(&E2Interface::m_e2Periodicity),
                                          MakeDoubleChecker<double>())
                            .AddAttribute("StartTime",
                                          "The start time of the E2 report messages",
                                          DoubleValue(0),
                                          MakeDoubleAccessor(&E2Interface::m_startTime),
                                          MakeDoubleChecker<double>());
    return tid;
}

void
E2Interface::RegisterNewSinrReadingCallback([[maybe_unused]] std::string path,
                                            uint16_t cellId,
                                            uint16_t rnti,
                                            double avgSinr,
                                            uint16_t bwpId)
{
    NS_LOG_FUNCTION(this);
    double sinrDb = 10 * log10(avgSinr);
    NS_LOG_DEBUG("Registering new SINR reading for cellId: " << cellId << " RNTI: " << rnti
                                                             << " avgSinr: " << sinrDb);
    auto gnbNode = DynamicCast<NrGnbNetDevice>(m_netDev);
    auto enbNode = DynamicCast<LteEnbNetDevice>(m_netDev);
    for (auto id : gnbNode->GetCellIds())
    {
        NS_LOG_DEBUG("CellId: " << cellId << " gNB cellId: " << id);
        if (gnbNode)
        {
            if (id == cellId)
            {
                m_cellId = id;
                // Get the current gNB RRC instance
                PointerValue rrc;
                gnbNode->GetAttribute("LteEnbRrc", rrc);
                auto rrcPtr = rrc.Get<LteEnbRrc>();
                // Using the current RRC, get the UE map
                ObjectMapValue ueMap;
                rrcPtr->GetAttribute("UeMap", ueMap);
                // Get the ue based on the c-rnti
                NS_LOG_DEBUG("ue C-RNTI:" << rnti);
                auto ue = DynamicCast<UeManager>(ueMap.Get(rnti));
                auto ueRnti = ue->GetRnti();
                NS_ASSERT(ueRnti == rnti);
                // Use in dB
                m_l3sinrMap[rnti][cellId] = sinrDb;
                NS_LOG_DEBUG("RNTI: " << rnti << " CellID: " << cellId << " SINR: " << sinrDb
                                      << " dB");
            }
        }
        // else if (enbNode)
        //{
        //     if (enbNode->GetCellId() == cellId)
        //     {
        //         // Using the current RRC, get the UE map
        //         ObjectMapValue ueMap;
        //         m_rrc->GetAttribute("UeMap", ueMap);
        //         auto ue = DynamicCast<UeManager>(ueMap.Get(cellId));
        //         auto ueImsi = ue->GetImsi();
        //         if (ueImsi == imsi)
        //         {
        //             // Use in dB
        //             m_l3sinrMap[imsi][cellId] = sinrDb;
        //             NS_LOG_DEBUG("IMSI: " << imsi << " CellID: " << cellId << " SINR: " << sinrDb
        //             << " dB");
        //         }
        //     }
        // }
        else
        {
            NS_FATAL_ERROR("NetDevice is not a gNB or eNB");
        }
    }
}

void
E2Interface::BuildAndSendReportMessage(E2Termination::RicSubscriptionRequest_rval_s params)
{
    NS_LOG_FUNCTION(this);
    NS_LOG_DEBUG("Building and sending report message for nodeB: " << m_netDev);
    auto e2Term = m_netDev->GetObject<E2Termination>();
    // eNB/gNB needs to have an E2 termination
    NS_ASSERT(e2Term != nullptr);

    // nodeB PLMN ID
    std::string plmId = "111";

    // Check if the nodeB is a gNB or eNB
    auto gnbNode = DynamicCast<NrGnbNetDevice>(m_netDev);
    auto enbNode = DynamicCast<LteEnbNetDevice>(m_netDev);

    // node cell ID
    if (gnbNode)
    {
        m_cellId = gnbNode->GetCellId();
    }
    else
    {
        m_cellId = enbNode->GetCellIds()[0];
    }

    NS_ASSERT(plmId == "111" && m_cellId != 0);
    std::string gnbId = std::to_string(m_cellId);
    NS_LOG_DEBUG("PLMN ID: " << plmId << " gNB cell ID: " << gnbId);
    bool cuUp = true;
    
    if (cuUp)
    {
        // Create CU-UP
        auto header = BuildRicIndicationHeader(plmId, gnbId, m_cellId);
        auto cuUpMsg = BuildRicIndicationMessageCuUp(plmId);

        // Send CU-UP only if offline logging is disabled
        if (header != nullptr && cuUpMsg != nullptr)
        {
            NS_LOG_DEBUG("Send NR CU-UP");
            auto pdu_cuup_ue = new E2AP_PDU;
            encoding::generate_e2apv1_indication_request_parameterized(
                pdu_cuup_ue,
                params.requestorId,
                params.instanceId,
                params.ranFuncionId,
                params.actionId,
                1,                           // TODO sequence number
                (uint8_t*)header->m_buffer,  // buffer containing the encoded header
                header->m_size,              // size of the encoded header
                (uint8_t*)cuUpMsg->m_buffer, // buffer containing the encoded message
                cuUpMsg->m_size);            // size of the encoded message
            e2Term->SendE2Message(pdu_cuup_ue);
            delete pdu_cuup_ue;
        }
    }

    bool m_sendCuCp = true;
    if (m_sendCuCp)
    {
        // Create and send CU-CP
        Ptr<KpmIndicationHeader> header = BuildRicIndicationHeader(plmId, gnbId, m_cellId);
        Ptr<KpmIndicationMessage> cuCpMsg = BuildRicIndicationMessageCuCp(plmId);

        // Send CU-CP only if offline logging is disabled
        if (header != nullptr && cuCpMsg != nullptr)
        {
            NS_LOG_DEBUG("Send NR CU-CP");
            auto pdu_cucp_ue = new E2AP_PDU;
            encoding::generate_e2apv1_indication_request_parameterized(
                pdu_cucp_ue,
                params.requestorId,
                params.instanceId,
                params.ranFuncionId,
                params.actionId,
                1,                           // TODO sequence number
                (uint8_t*)header->m_buffer,  // buffer containing the encoded header
                header->m_size,              // size of the encoded header
                (uint8_t*)cuCpMsg->m_buffer, // buffer containing the encoded message
                cuCpMsg->m_size);            // size of the encoded message
            m_e2term->SendE2Message(pdu_cucp_ue);
            delete pdu_cucp_ue;
        }
    }

    bool m_sendDu = true;
    if (m_sendDu)
    {
        // Create DU
        Ptr<KpmIndicationHeader> header = BuildRicIndicationHeader(plmId, gnbId, m_cellId);
        Ptr<KpmIndicationMessage> duMsg = BuildRicIndicationMessageDu(plmId, m_cellId);

        // Send DU only if offline logging is disabled
        if (header != nullptr && duMsg != nullptr)
        {
            NS_LOG_DEBUG("Send NR DU");
            auto pdu_du_ue = new E2AP_PDU;
            encoding::generate_e2apv1_indication_request_parameterized(
                pdu_du_ue,
                params.requestorId,
                params.instanceId,
                params.ranFuncionId,
                params.actionId,
                1,                          // TODO sequence number
                (uint8_t*)header->m_buffer, // buffer containing the encoded header
                header->m_size,             // size of the encoded header
                (uint8_t*)duMsg->m_buffer,  // buffer containing the encoded message
                duMsg->m_size);             // size of the encoded message
            m_e2term->SendE2Message(pdu_du_ue);
            delete pdu_du_ue;
        }
    }
    // Use without context for thread safety; Need to study why using context it makes safe
    Simulator::ScheduleWithContext(1,
                                   Seconds(m_e2Periodicity),
                                   &E2Interface::BuildAndSendReportMessage,
                                   this,
                                   params);
}

void
E2Interface::KpmSubscriptionCallback(E2AP_PDU_t* sub_req_pdu)
{
    NS_LOG_FUNCTION(this);
    NS_LOG_DEBUG("KPM Subscription Request callback");
    E2Termination::RicSubscriptionRequest_rval_s params =
        m_e2term->ProcessRicSubscriptionRequest(sub_req_pdu);
    NS_LOG_DEBUG("requestorId " << +params.requestorId << ", instanceId " << +params.instanceId
                                << ", ranFuncionId " << +params.ranFuncionId << ", actionId "
                                << +params.actionId);

    static bool isFirsReportMessage = true;
    NS_LOG_DEBUG("=====> isFirsReportMessage: " << isFirsReportMessage);
    if (isFirsReportMessage)
    {
        BuildAndSendReportMessage(params);
        isFirsReportMessage = false;
    }
}

void
E2Interface::SetE2PdcpStatsCalculator(Ptr<NrBearerStatsCalculator> e2PdcpStatsCalculator)
{
    NS_LOG_FUNCTION(this);
    m_e2PdcpStatsCalculator = e2PdcpStatsCalculator;
}

void
E2Interface::SetE2RlcStatsCalculator(Ptr<NrBearerStatsCalculator> e2RlcStatsCalculator)
{
    NS_LOG_FUNCTION(this);
    m_e2RlcStatsCalculator = e2RlcStatsCalculator;
}

Ptr<KpmIndicationMessage>
E2Interface::BuildRicIndicationMessageCuUp(std::string plmId)
{
    /**
     * Force logging and reduced pmvalues not avaliable
     */
    Ptr<MmWaveIndicationMessageHelper> indicationMessageHelper =
        Create<MmWaveIndicationMessageHelper>(IndicationMessageHelper::IndicationMessageType::CuUp,
                                              false,
                                              false);

    // get <rnti, UeManager> map of connected UEs
    ObjectMapValue ueManager;
    m_rrc->GetAttribute("UeMap", ueManager);

    // gNB-wide PDCP volume in downlink
    double cellDlTxVolume = 0;
    // rx bytes in downlink
    double cellDlRxVolume = 0;

    // sum of the per-user average latency
    double perUserAverageLatencySum = 0;

    std::unordered_map<uint64_t, std::string> uePmString{};

    for (auto ueObject = ueManager.Begin(); ueObject != ueManager.End(); ueObject++)
    {
        auto ue = DynamicCast<UeManager>(ueObject->second);
        uint64_t imsi = ue->GetImsi();

        std::string ueImsiComplete = GetImsiString(imsi);

        /**
         * NOTE: save current values in a temporary variable which will be used
         * to update the frame stats. Ex:
         * flow [1]: 1000 bytes -> in this frame window using GetDlTxData()
         * totalFlow of the entire simulation += 1000 bytes
         * flow [2]: 2000 bytes -> in this frame window, where:
         * flow [2] = actual frame  - (flow [1])
         * totalFlow = 2000 bytes
         *
         * So, we can generalize this to:
         * flow [n] = actual frame - (totalFlow)
         */
        // m_e2PdcpStatsCalculator->ResetResults();

        // double rxDlPackets = m_e2PdcpStatsCalculator->GetDlRxPackets(imsi, 3); // LCID 3 is used
        // for data
        // Get the tx packets in DL flow
        long txDlPackets = m_e2PdcpStatsCalculator->GetDlTxPackets(imsi, 3) -
                           m_cellTxDlPackets; // LCID 3 is used for data
        m_cellTxDlPackets += txDlPackets;
        // Get the tx kbits
        double actualTotalTxBytes = m_e2PdcpStatsCalculator->GetDlTxData(imsi, 3) * (8 / 1e3);
        double txBytes = (actualTotalTxBytes - m_cellTxBytes); // in kbit, not byte

        NS_LOG_DEBUG("Actual value of TX bytes: " << (actualTotalTxBytes) << " - " << m_cellTxBytes
                                                  << ", Result = " << txBytes);
        // Save the current value to validate the tx bits in this frame window
        m_cellTxBytes += txBytes;

        // Get the rx kbit
        double actualTotalRxBytes = m_e2PdcpStatsCalculator->GetDlRxData(imsi, 3) * (8 / 1e3);
        double rxBytes = (actualTotalRxBytes - m_cellRxBytes); // in kbit, not byte
        NS_LOG_DEBUG("Actual value of RX bytes: " << (actualTotalRxBytes) << " - " << m_cellRxBytes
                                                  << ", Result = " << rxBytes);
        // Save the current value to validate the rx bits in this frame window
        m_cellRxBytes += rxBytes;

        // Cell volume metrics
        cellDlTxVolume += txBytes;
        cellDlRxVolume += rxBytes;

        long txPdcpPduNrRlc = 0;
        double txPdcpPduBytesNrRlc = 0;

        // Get std::map<uint8_t, ns3::Ptr<ns3::LteDataRadioBearerInfo>> ns3::UeManager::m_drbMap
        ObjectMapValue drbMap;
        ue->GetAttribute("DataRadioBearerMap", drbMap);
        auto rnti = ue->GetRnti();
        // All the drbs report in the same callback function, all the PDU information is being
        // summed in the ReportTxPDU.
        // Tx PDUs in the reporting period, only get in this time window
        // and then reset it
        txPdcpPduNrRlc += m_txPDU[rnti];
        txPdcpPduBytesNrRlc += m_txPDUBytes[rnti];
        // Reset counting in the frame time
        m_txPDU[rnti] = 0;
        m_txPDUBytes[rnti] = 0;

        NS_LOG_DEBUG("Number of Tx PDCP PDU in NR RLC: " << txPdcpPduNrRlc
                                                         << ", in bytes: " << txPdcpPduBytesNrRlc);
        // Use kbit instead of byte
        txPdcpPduBytesNrRlc *= 8 / 1e3;

        // compute mean latency based on PDCP statistics
        /** TODO: Actually, it returns the average latency and i don't know how to reset it */
        [[maybe_unused]] auto stats = m_e2PdcpStatsCalculator->GetDlDelayStats(imsi, 3);
        double pdcpLatency = m_e2PdcpStatsCalculator->GetDlDelay(imsi, 3) / 1e5; // unit: x 0.1 ms
        perUserAverageLatencySum += pdcpLatency;

        double pdcpThroughput = txBytes / m_e2Periodicity;                    // unit kbps
        [[maybe_unused]] double pdcpThroughputRx = rxBytes / m_e2Periodicity; // unit kbps

        if (m_drbThrDlPdcpBasedComputationUeid.find(imsi) !=
            m_drbThrDlPdcpBasedComputationUeid.end())
        {
            m_drbThrDlPdcpBasedComputationUeid.at(imsi) += pdcpThroughputRx;
        }
        else
        {
            m_drbThrDlPdcpBasedComputationUeid[imsi] = pdcpThroughputRx;
        }

        // compute bitrate based on RLC statistics, decoupled from pdcp throughput
        double rlcLatency = m_e2RlcStatsCalculator->GetDlDelay(imsi, 3) / 1e9; // unit: s
        double pduStats =
            m_e2RlcStatsCalculator->GetDlPduSizeStats(imsi, 3)[0] * 8.0 / 1e3; // unit kbit

        double rlcBitrate = (rlcLatency == 0) ? 0 : pduStats / rlcLatency; // unit kbit/s

        m_drbThrDlUeid[imsi] = rlcBitrate;

        NS_LOG_DEBUG("[" << Simulator::Now().GetSeconds() << "s]"
                         << "Cell id: " << m_cellId << " connected UE with IMSI " << imsi
                         << " ueImsiString " << ueImsiComplete << " txDlPackets " << txDlPackets
                         << " txDlPacketsNr " << txPdcpPduNrRlc << " txBytes " << txBytes
                         << " rxBytes " << rxBytes << " txDlBytesNr " << txPdcpPduBytesNrRlc
                         << " pdcpLatency " << pdcpLatency << " pdcpThroughput " << pdcpThroughput
                         << " rlcBitrate " << rlcBitrate);

        if (!indicationMessageHelper->IsOffline())
        {
            indicationMessageHelper->AddCuUpUePmItem(ueImsiComplete,
                                                     txPdcpPduBytesNrRlc,
                                                     txPdcpPduNrRlc);
        }

        uePmString.insert(std::make_pair(imsi,
                                         ",,,," + std::to_string(txPdcpPduBytesNrRlc) + "," +
                                             std::to_string(txPdcpPduNrRlc)));
    }

    if (!indicationMessageHelper->IsOffline())
    {
        indicationMessageHelper->FillCuUpValues(plmId);
    }

    NS_LOG_DEBUG("[" << Simulator::Now().GetSeconds() << "s]"
                     << " in cell ID: " << m_cellId
                     << " with this DL TX cell volume: " << cellDlTxVolume);
    /**
     *
    if (m_forceE2FileLogging)
    {
        std::ofstream csv{};
        csv.open(m_cuUpFileName.c_str(), std::ios_base::app);
        if (!csv.is_open())
        {
            NS_FATAL_ERROR("Can't open file " << m_cuUpFileName.c_str());
        }

        uint64_t timestamp = m_startTime + (uint64_t)Simulator::Now().GetMilliSeconds();

        // the string is timestamp, ueImsiComplete, DRB.PdcpSduDelayDl (cellAverageLatency),
        // m_pDCPBytesUL (0), m_pDCPBytesDL (cellDlTxVolume), DRB.PdcpSduVolumeDl_Filter.UEID
        // (txBytes), Tot.PdcpSduNbrDl.UEID (txDlPackets), DRB.PdcpSduBitRateDl.UEID
        // (pdcpThroughput), DRB.PdcpSduDelayDl.UEID (pdcpLatency),
        // QosFlow.PdcpPduVolumeDL_Filter.UEID (txPdcpPduBytesNrRlc), DRB.PdcpPduNbrDl.Qos.UEID
        // (txPdcpPduNrRlc)

        for (auto ue : ueMap)
        {
            uint64_t imsi = ue.second->GetImsi();
            std::string ueImsiComplete = GetImsiString(imsi);

            auto uePms = uePmString.find(imsi)->second;

            std::string to_print = std::to_string(timestamp) + "," + ueImsiComplete + "," + "," +
                                   "," + "," + uePms + "\n";

            csv << to_print;
        }
        csv.close();
        return nullptr;
    }
    else
    {
    }
    */
    return indicationMessageHelper->CreateIndicationMessage();
}

std::string
E2Interface::GetImsiString(uint64_t imsi)
{
    std::string ueImsi = std::to_string(imsi);
    std::string ueImsiComplete{};
    if (ueImsi.length() == 1)
    {
        ueImsiComplete = "0000" + ueImsi;
    }
    else if (ueImsi.length() == 2)
    {
        ueImsiComplete = "000" + ueImsi;
    }
    else
    {
        ueImsiComplete = "00" + ueImsi;
    }
    return ueImsiComplete;
}

void
E2Interface::ReportTxPDU(uint16_t rnti, uint8_t lcid, uint32_t packetSize)
{
    NS_LOG_DEBUG("Report Tx PDUs for RNTI: " << rnti << " lcid: " << lcid
                                             << " packetSize: " << packetSize << " bytes");

    if (m_txPDU.find(rnti) == m_txPDU.end())
    {
        NS_LOG_DEBUG("First PDU for RNTI: " << rnti);
        m_txPDU.insert(std::make_pair(rnti, 1));
    }
    else
    {
        NS_LOG_DEBUG("Increment PDU for RNTI: " << rnti);
        m_txPDU[rnti] += 1;
    }

    if (m_txPDUBytes.find(rnti) == m_txPDUBytes.end())
    {
        NS_LOG_DEBUG("First PDU bytes for RNTI: " << rnti << " packetSize: " << packetSize);
        m_txPDUBytes.insert(std::make_pair(rnti, packetSize));
    }
    else
    {
        NS_LOG_DEBUG("Increment PDU bytes for RNTI: " << rnti << " packetSize: " << packetSize);
        m_txPDUBytes[rnti] += packetSize;
    }
}

Ptr<KpmIndicationMessage>
E2Interface::BuildRicIndicationMessageCuCp(std::string plmId)
{
    Ptr<MmWaveIndicationMessageHelper> indicationMessageHelper =
        Create<MmWaveIndicationMessageHelper>(IndicationMessageHelper::IndicationMessageType::CuCp,
                                              false,
                                              false);
    ObjectMapValue ueManager;
    m_rrc->GetAttribute("UeMap", ueManager);

    std::unordered_map<uint64_t, std::string> uePmString{};

    for (auto ueObject = ueManager.Begin(); ueObject != ueManager.End(); ueObject++)
    {
        NS_LOG_DEBUG("CU-CP message in UE:" << ueObject->first);
        auto ue = DynamicCast<UeManager>(ueObject->second);
        uint64_t imsi = ue->GetImsi();
        std::string ueImsiComplete = GetImsiString(imsi);

        Ptr<MeasurementItemList> ueVal = Create<MeasurementItemList>(ueImsiComplete);

        ObjectMapValue drbMap;
        ue->GetAttribute("DataRadioBearerMap", drbMap);
        long numDrb = drbMap.GetN();

        /**
         * NOTE: no reduced PM values in the current version
        if (!m_reducedPmValues)
        {
            ueVal->AddItem<long>("DRB.EstabSucc.5QI.UEID", numDrb);
            ueVal->AddItem<long>("DRB.RelActNbr.5QI.UEID", 0); // not modeled in the simulator
        }
        */

        // create L3 RRC reports

        // for the same cell
        auto rnti = ue->GetRnti();
        // Already in dB
        double sinrThisCell = m_l3sinrMap[rnti][m_cellId];
        NS_LOG_DEBUG("This cell SINR: " << sinrThisCell << "DRB num: " << numDrb);

        double convertedSinr = L3RrcMeasurements::ThreeGppMapSinr(sinrThisCell);

        Ptr<L3RrcMeasurements> l3RrcMeasurementServing;
        if (!indicationMessageHelper->IsOffline())
        {
            l3RrcMeasurementServing =
                L3RrcMeasurements::CreateL3RrcUeSpecificSinrServing(m_cellId,
                                                                    m_cellId,
                                                                    convertedSinr);
        }
        NS_LOG_INFO("[" << Simulator::Now().GetSeconds() << "]"
                        << " gNB cell ID: " << m_cellId << " UE " << imsi << " L3 serving SINR "
                        << sinrThisCell << " L3 serving SINR 3gpp " << convertedSinr << ", numDrb: "
                        << numDrb << ", L3 RRC serving: " << l3RrcMeasurementServing);

        std::string servingStr = std::to_string(numDrb) + "," + std::to_string(0) + "," +
                                 std::to_string(m_cellId) + "," + std::to_string(imsi) + "," +
                                 std::to_string(sinrThisCell) + "," + std::to_string(convertedSinr);

        Ptr<L3RrcMeasurements> l3RrcMeasurementNeigh;
        if (!indicationMessageHelper->IsOffline())
        {
            l3RrcMeasurementNeigh = L3RrcMeasurements::CreateL3RrcUeSpecificSinrNeigh();
        }
        double sinr;
        std::string neighStr;

        // invert key and value in sortFlipMap, then sort by value
        std::multimap<long double, uint16_t> sortFlipMap = FlipMap(m_l3sinrMap[rnti]);
        // new sortFlipMap structure sortFlipMap < sinr, cellId >
        // The assumption is that the first cell in the scenario is always LTE and the rest NR
        uint16_t nNeighbours = E2SM_REPORT_MAX_NEIGH;
        if (m_l3sinrMap[rnti].size() < nNeighbours)
        {
            nNeighbours = m_l3sinrMap[rnti].size() - 1;
        }
        int itIndex = 0;
        // Save only the first E2SM_REPORT_MAX_NEIGH SINR for each UE which represent the best
        // values among all the SINRs detected by all the cells
        for (auto it = --sortFlipMap.end(); it != --sortFlipMap.begin() && itIndex < nNeighbours;
             it--)
        {
            uint16_t cellId = it->second;
            NS_LOG_DEBUG("Sort flipMap cellId: " << cellId << " m_cellId: " << m_cellId);
            if (cellId != m_cellId)
            {
                sinr = it->first; // now SINR is a key due to the sort of the map
                convertedSinr = L3RrcMeasurements::ThreeGppMapSinr(sinr);
                if (!indicationMessageHelper->IsOffline())
                {
                    l3RrcMeasurementNeigh->AddNeighbourCellMeasurement(cellId, convertedSinr);
                }
                NS_LOG_INFO(Simulator::Now().GetSeconds()
                            << " enbdev " << m_cellId << " UE " << imsi << " L3 neigh " << cellId
                            << " SINR " << sinr << " sinr encoded " << convertedSinr
                            << " first insert");
                neighStr += "," + std::to_string(cellId) + "," + std::to_string(sinr) + "," +
                            std::to_string(convertedSinr);
                itIndex++;
            }
        }
        for (int i = nNeighbours; i < E2SM_REPORT_MAX_NEIGH; i++)
        {
            neighStr += ",,,";
        }

        uePmString.insert(std::make_pair(imsi, servingStr + neighStr));

        if (!indicationMessageHelper->IsOffline())
        {
            indicationMessageHelper->AddCuCpUePmItem(ueImsiComplete,
                                                     numDrb,
                                                     0,
                                                     l3RrcMeasurementServing,
                                                     l3RrcMeasurementNeigh);
        }
    }

    if (!indicationMessageHelper->IsOffline())
    {
        // Fill CuCp specific fields
        indicationMessageHelper->FillCuCpValues(ueManager.GetN()); // Number of Active UEs
    }

    /**
     *
    if (m_forceE2FileLogging)
    {
        std::ofstream csv{};
        csv.open(m_cuCpFileName.c_str(), std::ios_base::app);
        if (!csv.is_open())
        {
            NS_FATAL_ERROR("Can't open file " << m_cuCpFileName.c_str());
        }

        NS_LOG_DEBUG("m_cuCpFileName open " << m_cuCpFileName);

        // the string is timestamp, ueImsiComplete, numActiveUes, DRB.EstabSucc.5QI.UEID (numDrb),
        // DRB.RelActNbr.5QI.UEID (0), L3 serving Id (m_cellId), UE (imsi), L3 serving SINR, L3
        // serving SINR 3gpp, L3 neigh Id (cellId), L3 neigh Sinr, L3 neigh SINR 3gpp
        // (convertedSinr) The values for L3 neighbour cells are repeated for each neighbour (7
        // times in this implementation)

        uint64_t timestamp = m_startTime + (uint64_t)Simulator::Now().GetMilliSeconds();

        for (auto ue : ueMap)
        {
            uint64_t imsi = ue.second->GetImsi();
            std::string ueImsiComplete = GetImsiString(imsi);

            auto uePms = uePmString.find(imsi)->second;

            std::string to_print = std::to_string(timestamp) + "," + ueImsiComplete + "," +
                                   std::to_string(ueMap.size()) + "," + uePms + "\n";

            NS_LOG_DEBUG(to_print);

            csv << to_print;
        }
        csv.close();
        return nullptr;
    }
    else
    {
     */
    return indicationMessageHelper->CreateIndicationMessage();
    //}
}

Ptr<KpmIndicationMessage>
E2Interface::BuildRicIndicationMessageDu(std::string plmId, uint16_t nrCellId)
{
    Ptr<MmWaveIndicationMessageHelper> indicationMessageHelper =
        Create<MmWaveIndicationMessageHelper>(IndicationMessageHelper::IndicationMessageType::Du,
                                              false,
                                              false);

    ObjectMapValue ueManager;
    m_rrc->GetAttribute("UeMap", ueManager);

    uint32_t macPduCellSpecific = 0;
    uint32_t macPduInitialCellSpecific = 0;
    uint32_t macVolumeCellSpecific = 0;
    uint32_t macQpskCellSpecific = 0;
    uint32_t mac16QamCellSpecific = 0;
    uint32_t mac64QamCellSpecific = 0;
    uint32_t macRetxCellSpecific = 0;
    uint32_t macMac04CellSpecific = 0;
    uint32_t macMac59CellSpecific = 0;
    uint32_t macMac1014CellSpecific = 0;
    uint32_t macMac1519CellSpecific = 0;
    uint32_t macMac2024CellSpecific = 0;
    uint32_t macMac2529CellSpecific = 0;

    uint32_t macSinrBin1CellSpecific = 0;
    uint32_t macSinrBin2CellSpecific = 0;
    uint32_t macSinrBin3CellSpecific = 0;
    uint32_t macSinrBin4CellSpecific = 0;
    uint32_t macSinrBin5CellSpecific = 0;
    uint32_t macSinrBin6CellSpecific = 0;
    uint32_t macSinrBin7CellSpecific = 0;

    uint32_t rlcBufferOccupCellSpecific = 0;

    uint32_t macPrbsCellSpecific = 0;

    m_cellId = nrCellId;

    std::unordered_map<uint64_t, std::string> uePmStringDu{};

    for (auto ueMap = ueManager.Begin(); ueMap != ueManager.End(); ueMap++)
    {
        auto ue = DynamicCast<UeManager>(ueMap->second);
        uint64_t imsi = ue->GetImsi();
        std::string ueImsiComplete = GetImsiString(imsi);
        uint16_t rnti = ue->GetRnti();

        uint32_t macPduUe = m_e2DuCalculator->GetMacPduUeSpecific(rnti, m_cellId);
        macPduCellSpecific += macPduUe;

        uint32_t macPduInitialUe =
            m_e2DuCalculator->GetMacPduInitialTransmissionUeSpecific(rnti, m_cellId);
        macPduInitialCellSpecific += macPduInitialUe;

        uint32_t macVolume = m_e2DuCalculator->GetMacVolumeUeSpecific(rnti, m_cellId);
        macVolumeCellSpecific += macVolume;

        uint32_t macQpsk = m_e2DuCalculator->GetMacPduQpskUeSpecific(rnti, m_cellId);
        macQpskCellSpecific += macQpsk;

        uint32_t mac16Qam = m_e2DuCalculator->GetMacPdu16QamUeSpecific(rnti, m_cellId);
        mac16QamCellSpecific += mac16Qam;

        uint32_t mac64Qam = m_e2DuCalculator->GetMacPdu64QamUeSpecific(rnti, m_cellId);
        mac64QamCellSpecific += mac64Qam;

        uint32_t macRetx = m_e2DuCalculator->GetMacPduRetransmissionUeSpecific(rnti, m_cellId);
        macRetxCellSpecific += macRetx;

        // Numerator = (Sum of number of symbols across all rows (TTIs) group by cell ID and UE ID
        // within a given time window)
        double macNumberOfSymbols =
            m_e2DuCalculator->GetMacNumberOfSymbolsUeSpecific(rnti, m_cellId);

        auto slotPeriod = DynamicCast<NrGnbNetDevice>(m_netDev)->GetPhy(0)->GetSlotPeriod();

        ObjectMapValue ccMapObject;
        DynamicCast<NrGnbNetDevice>(m_netDev)->GetAttribute("BandwidthPartMap", ccMapObject);

        // Denominator = (Periodicity of the report time window in ms*number of TTIs per ms*14)
        Time reportingWindow =
            Simulator::Now() - m_e2DuCalculator->GetLastResetTime(rnti, m_cellId);
        double denominatorPrb =
            std::ceil(reportingWindow.GetNanoSeconds() / slotPeriod.GetNanoSeconds()) * 14;

        NS_LOG_DEBUG("macNumberOfSymbols " << macNumberOfSymbols << " denominatorPrb "
                                           << denominatorPrb);

        // Average Number of PRBs allocated for the UE = (NR/DR)*139 (where 139 is the total number
        // of PRBs available per NR cell, given numerology 2 with 60 kHz SCS)
        double macPrb = 0;
        if (denominatorPrb != 0)
        {
            macPrb = macNumberOfSymbols / denominatorPrb *
                     139; // TODO fix this for different numerologies
        }
        macPrbsCellSpecific += macPrb;

        uint32_t macMac04 = m_e2DuCalculator->GetMacMcs04UeSpecific(rnti, m_cellId);
        macMac04CellSpecific += macMac04;

        uint32_t macMac59 = m_e2DuCalculator->GetMacMcs59UeSpecific(rnti, m_cellId);
        macMac59CellSpecific += macMac59;

        uint32_t macMac1014 = m_e2DuCalculator->GetMacMcs1014UeSpecific(rnti, m_cellId);
        macMac1014CellSpecific += macMac1014;

        uint32_t macMac1519 = m_e2DuCalculator->GetMacMcs1519UeSpecific(rnti, m_cellId);
        macMac1519CellSpecific += macMac1519;

        uint32_t macMac2024 = m_e2DuCalculator->GetMacMcs2024UeSpecific(rnti, m_cellId);
        macMac2024CellSpecific += macMac2024;

        uint32_t macMac2529 = m_e2DuCalculator->GetMacMcs2529UeSpecific(rnti, m_cellId);
        macMac2529CellSpecific += macMac2529;

        uint32_t macSinrBin1 = m_e2DuCalculator->GetMacSinrBin1UeSpecific(rnti, m_cellId);
        macSinrBin1CellSpecific += macSinrBin1;

        uint32_t macSinrBin2 = m_e2DuCalculator->GetMacSinrBin2UeSpecific(rnti, m_cellId);
        macSinrBin2CellSpecific += macSinrBin2;

        uint32_t macSinrBin3 = m_e2DuCalculator->GetMacSinrBin3UeSpecific(rnti, m_cellId);
        macSinrBin3CellSpecific += macSinrBin3;

        uint32_t macSinrBin4 = m_e2DuCalculator->GetMacSinrBin4UeSpecific(rnti, m_cellId);
        macSinrBin4CellSpecific += macSinrBin4;

        uint32_t macSinrBin5 = m_e2DuCalculator->GetMacSinrBin5UeSpecific(rnti, m_cellId);
        macSinrBin5CellSpecific += macSinrBin5;

        uint32_t macSinrBin6 = m_e2DuCalculator->GetMacSinrBin6UeSpecific(rnti, m_cellId);
        macSinrBin6CellSpecific += macSinrBin6;

        uint32_t macSinrBin7 = m_e2DuCalculator->GetMacSinrBin7UeSpecific(rnti, m_cellId);
        macSinrBin7CellSpecific += macSinrBin7;
        /**
         * TODO: Implement the RLC buffer occupancy (GetTxbuffersize())
         *
         */
        // get buffer occupancy info
        uint32_t rlcBufferOccup = 0;
        ObjectMapValue drbMap;
        ue->GetAttribute("DataRadioBearerMap", drbMap);
        for (auto dr = drbMap.Begin(); dr != drbMap.End(); dr++)
        {
            PointerValue ltePtr;
            auto dataRadio = dr->second;
            dataRadio->GetAttribute("LtePdcp", ltePtr);
            [[maybe_unused]] auto lte = ltePtr.Get<LteRlc>();
            Ptr<LteRlcAm> rlcAm = DynamicCast<LteRlcAm>(lte);
            if (rlcAm)
            {   
                /**
                rlcAm->TraceConnectWithoutContext("TxBufferState",
                    MakeCallback([](uint32_t size) {
                        NS_LOG_UNCOND("Buffer size (bytes): " << size);
                    }));
                */
            }         
        }

        /**
         *
        auto rlcMap = ue.second->GetRlcMap(); // secondary-connected RLCs
        for (auto drb : rlcMap)
        {
            auto rlc = drb.second->m_rlc;
            rlcBufferOccup += GetRlcBufferOccupancy(rlc);
        }
         */
        rlcBufferOccupCellSpecific += rlcBufferOccup;

        NS_LOG_DEBUG(Simulator::Now().GetSeconds()
                     << " " << m_cellId << " cell, connected UE with IMSI " << imsi << " rnti "
                     << rnti << " macPduUe " << macPduUe << " macPduInitialUe " << macPduInitialUe
                     << " macVolume " << macVolume << " macQpsk " << macQpsk << " mac16Qam "
                     << mac16Qam << " mac64Qam " << mac64Qam << " macRetx " << macRetx << " macPrb "
                     << macPrb << " macMac04 " << macMac04 << " macMac59 " << macMac59
                     << " macMac1014 " << macMac1014 << " macMac1519 " << macMac1519
                     << " macMac2024 " << macMac2024 << " macMac2529 " << macMac2529
                     << " macSinrBin1 " << macSinrBin1 << " macSinrBin2 " << macSinrBin2
                     << " macSinrBin3 " << macSinrBin3 << " macSinrBin4 " << macSinrBin4
                     << " macSinrBin5 " << macSinrBin5 << " macSinrBin6 " << macSinrBin6
                     << " macSinrBin7 " << macSinrBin7 << " rlcBufferOccup " << rlcBufferOccup);

        // UE-specific Downlink IP combined EN-DC throughput from LTE eNB. Unit is kbps. Pdcp based
        // computation This value is not requested anymore, so it has been removed from the
        // delivery, but it will be still logged;
        double drbThrDlPdcpBasedUeid = m_drbThrDlPdcpBasedComputationUeid.find(imsi) !=
                                               m_drbThrDlPdcpBasedComputationUeid.end()
                                           ? m_drbThrDlPdcpBasedComputationUeid.at(imsi)
                                           : 0;

        // UE-specific Downlink IP combined EN-DC throughput from LTE eNB. Unit is kbps. Rlc based
        // computation
        double drbThrDlUeid =
            m_drbThrDlUeid.find(imsi) != m_drbThrDlUeid.end() ? m_drbThrDlUeid.at(imsi) : 0;

        indicationMessageHelper->AddDuUePmItem(ueImsiComplete,
                                               macPduUe,
                                               macPduInitialUe,
                                               macQpsk,
                                               mac16Qam,
                                               mac64Qam,
                                               macRetx,
                                               macVolume,
                                               macPrb,
                                               macMac04,
                                               macMac59,
                                               macMac1014,
                                               macMac1519,
                                               macMac2024,
                                               macMac2529,
                                               macSinrBin1,
                                               macSinrBin2,
                                               macSinrBin3,
                                               macSinrBin4,
                                               macSinrBin5,
                                               macSinrBin6,
                                               macSinrBin7,
                                               rlcBufferOccup,
                                               drbThrDlUeid);

        uePmStringDu.insert(std::make_pair(
            imsi,
            std::to_string(macPduUe) + "," + std::to_string(macPduInitialUe) + "," +
                std::to_string(macQpsk) + "," + std::to_string(mac16Qam) + "," +
                std::to_string(mac64Qam) + "," + std::to_string(macRetx) + "," +
                std::to_string(macVolume) + "," + std::to_string(macPrb) + "," +
                std::to_string(macMac04) + "," + std::to_string(macMac59) + "," +
                std::to_string(macMac1014) + "," + std::to_string(macMac1519) + "," +
                std::to_string(macMac2024) + "," + std::to_string(macMac2529) + "," +
                std::to_string(macSinrBin1) + "," + std::to_string(macSinrBin2) + "," +
                std::to_string(macSinrBin3) + "," + std::to_string(macSinrBin4) + "," +
                std::to_string(macSinrBin5) + "," + std::to_string(macSinrBin6) + "," +
                std::to_string(macSinrBin7) + "," + std::to_string(rlcBufferOccup) + ',' +
                std::to_string(drbThrDlUeid) + ',' + std::to_string(drbThrDlPdcpBasedUeid)));

        // reset UE
        m_e2DuCalculator->ResetPhyTracesForRntiCellId(rnti, m_cellId);
    }
    m_drbThrDlPdcpBasedComputationUeid.clear();
    m_drbThrDlUeid.clear();

    // Denominator = (Total number of rows (TTIs) within a given time window* 14)
    // Numerator = (Sum of number of symbols across all rows (TTIs) group by cell ID within a given
    // time window) * 139 Average Number of PRBs allocated for the UE = (NR/DR) (where 139 is the
    // total number of PRBs available per NR cell, given numerology 2 with 60 kHz SCS)
    double prbUtilizationDl = macPrbsCellSpecific;

    NS_LOG_INFO(
        Simulator::Now().GetSeconds()
        << " " << m_cellId << " cell, connected UEs number " << ueManager.GetN()
        << " macPduCellSpecific " << macPduCellSpecific << " macPduInitialCellSpecific "
        << macPduInitialCellSpecific << " macVolumeCellSpecific " << macVolumeCellSpecific
        << " macQpskCellSpecific " << macQpskCellSpecific << " mac16QamCellSpecific "
        << mac16QamCellSpecific << " mac64QamCellSpecific " << mac64QamCellSpecific
        << " macRetxCellSpecific " << macRetxCellSpecific << " macPrbsCellSpecific "
        << macPrbsCellSpecific //<< " " << macNumberOfSymbolsCellSpecific << " " << denominatorPrb
        << " macMac04CellSpecific " << macMac04CellSpecific << " macMac59CellSpecific "
        << macMac59CellSpecific << " macMac1014CellSpecific " << macMac1014CellSpecific
        << " macMac1519CellSpecific " << macMac1519CellSpecific << " macMac2024CellSpecific "
        << macMac2024CellSpecific << " macMac2529CellSpecific " << macMac2529CellSpecific
        << " macSinrBin1CellSpecific " << macSinrBin1CellSpecific << " macSinrBin2CellSpecific "
        << macSinrBin2CellSpecific << " macSinrBin3CellSpecific " << macSinrBin3CellSpecific
        << " macSinrBin4CellSpecific " << macSinrBin4CellSpecific << " macSinrBin5CellSpecific "
        << macSinrBin5CellSpecific << " macSinrBin6CellSpecific " << macSinrBin6CellSpecific
        << " macSinrBin7CellSpecific " << macSinrBin7CellSpecific);

    long dlAvailablePrbs = 139; // TODO this is for the current configuration, make it configurable
    long ulAvailablePrbs = 139; // TODO this is for the current configuration, make it configurable
    long qci = 1;
    long dlPrbUsage = std::min((long)(prbUtilizationDl / dlAvailablePrbs * 100),
                               (long)100); // percentage of used PRBs
    long ulPrbUsage = 0;                   // TODO for future implementation

    if (!indicationMessageHelper->IsOffline())
    {
        indicationMessageHelper->AddDuCellPmItem(macPduCellSpecific,
                                                 macPduInitialCellSpecific,
                                                 macQpskCellSpecific,
                                                 mac16QamCellSpecific,
                                                 mac64QamCellSpecific,
                                                 prbUtilizationDl,
                                                 macRetxCellSpecific,
                                                 macVolumeCellSpecific,
                                                 macMac04CellSpecific,
                                                 macMac59CellSpecific,
                                                 macMac1014CellSpecific,
                                                 macMac1519CellSpecific,
                                                 macMac2024CellSpecific,
                                                 macMac2529CellSpecific,
                                                 macSinrBin1CellSpecific,
                                                 macSinrBin2CellSpecific,
                                                 macSinrBin3CellSpecific,
                                                 macSinrBin4CellSpecific,
                                                 macSinrBin5CellSpecific,
                                                 macSinrBin6CellSpecific,
                                                 macSinrBin7CellSpecific,
                                                 rlcBufferOccupCellSpecific,
                                                 ueManager.GetN());

        Ptr<CellResourceReport> cellResRep = Create<CellResourceReport>();
        cellResRep->m_plmId = plmId;
        cellResRep->m_nrCellId = nrCellId;
        cellResRep->dlAvailablePrbs = dlAvailablePrbs;
        cellResRep->ulAvailablePrbs = ulAvailablePrbs;

        Ptr<ServedPlmnPerCell> servedPlmnPerCell = Create<ServedPlmnPerCell>();
        servedPlmnPerCell->m_plmId = plmId;
        servedPlmnPerCell->m_nrCellId = nrCellId;

        Ptr<EpcDuPmContainer> epcDuVal = Create<EpcDuPmContainer>();
        epcDuVal->m_qci = qci;
        epcDuVal->m_dlPrbUsage = dlPrbUsage;
        epcDuVal->m_ulPrbUsage = ulPrbUsage;

        servedPlmnPerCell->m_perQciReportItems.insert(epcDuVal);
        cellResRep->m_servedPlmnPerCellItems.insert(servedPlmnPerCell);

        indicationMessageHelper->AddDuCellResRepPmItem(cellResRep);
        indicationMessageHelper->FillDuValues(plmId + std::to_string(nrCellId));
    }

    bool generateData = true;
    m_duFileName = "metrics_du.csv";

    if (generateData)
    {
        std::ofstream csv{};
        csv.open(m_duFileName.c_str(), std::ios_base::app);
        if (!csv.is_open())
        {
            NS_FATAL_ERROR("Can't open file " << m_duFileName.c_str());
        }

        // Check if the file is empty to write the header
        csv.seekp(0, std::ios::end);
        if (csv.tellp() == 0)
        {
            csv << "timestamp,plmId,nrCellId,dlAvailablePrbs,ulAvailablePrbs,qci,dlPrbUsage,ulPrbUsage,"
                   "macPduCellSpecific,macPduInitialCellSpecific,macQpskCellSpecific,mac16QamCellSpecific,"
                   "mac64QamCellSpecific,prbUtilizationDl,macRetxCellSpecific,macVolumeCellSpecific,"
                   "macMac04CellSpecific,macMac59CellSpecific,macMac1014CellSpecific,macMac1519CellSpecific,"
                   "macMac2024CellSpecific,macMac2529CellSpecific,macSinrBin1CellSpecific,macSinrBin2CellSpecific,"
                   "macSinrBin3CellSpecific,macSinrBin4CellSpecific,macSinrBin5CellSpecific,macSinrBin6CellSpecific,"
                   "macSinrBin7CellSpecific,rlcBufferOccupCellSpecific,numActiveUes,ueImsiComplete,"
                   "macPduUe,macPduInitialUe,macQpsk,mac16Qam,mac64Qam,macRetx,macVolume,macPrb,macMac04,"
                   "macMac59,macMac1014,macMac1519,macMac2024,macMac2529,macSinrBin1,macSinrBin2,macSinrBin3,"
                   "macSinrBin4,macSinrBin5,macSinrBin6,macSinrBin7,rlcBufferOccup,drbThrDlUeid,drbThrDlPdcpBasedUeid\n";
        }

        uint64_t timestamp = m_startTime + (uint64_t)Simulator::Now().GetMilliSeconds();

        std::string to_print_cell =
            std::to_string(timestamp) + "," + plmId + "," + std::to_string(nrCellId) + "," +
            std::to_string(dlAvailablePrbs) + "," + std::to_string(ulAvailablePrbs) + "," +
            std::to_string(qci) + "," + std::to_string(dlPrbUsage) + "," +
            std::to_string(ulPrbUsage) + "," + std::to_string(macPduCellSpecific) + "," +
            std::to_string(macPduInitialCellSpecific) + "," + std::to_string(macQpskCellSpecific) +
            "," + std::to_string(mac16QamCellSpecific) + "," + std::to_string(mac64QamCellSpecific) +
            "," + std::to_string((long)std::ceil(prbUtilizationDl)) + "," +
            std::to_string(macRetxCellSpecific) + "," + std::to_string(macVolumeCellSpecific) + "," +
            std::to_string(macMac04CellSpecific) + "," + std::to_string(macMac59CellSpecific) + "," +
            std::to_string(macMac1014CellSpecific) + "," + std::to_string(macMac1519CellSpecific) +
            "," + std::to_string(macMac2024CellSpecific) + "," + std::to_string(macMac2529CellSpecific) +
            "," + std::to_string(macSinrBin1CellSpecific) + "," + std::to_string(macSinrBin2CellSpecific) +
            "," + std::to_string(macSinrBin3CellSpecific) + "," + std::to_string(macSinrBin4CellSpecific) +
            "," + std::to_string(macSinrBin5CellSpecific) + "," + std::to_string(macSinrBin6CellSpecific) +
            "," + std::to_string(macSinrBin7CellSpecific) + "," + std::to_string(rlcBufferOccupCellSpecific) +
            "," + std::to_string(ueManager.GetN());

        m_rrc->GetAttribute("UeMap", ueManager);

        for (auto ueObject = ueManager.Begin(); ueObject != ueManager.End(); ueObject++)
        {
            auto ue = DynamicCast<UeManager>(ueObject->second);
            uint64_t imsi = ue->GetImsi();
            std::string ueImsiComplete = GetImsiString(imsi);

            auto uePms = uePmStringDu.find(imsi)->second;

            std::string to_print = to_print_cell + "," + ueImsiComplete + "," + uePms + "\n";

            csv << to_print;
        }
        csv.close();
    }
    return indicationMessageHelper->CreateIndicationMessage();
}

std::multimap<long double, uint16_t>
E2Interface::FlipMap(const std::map<uint16_t, long double>& src)
{
    std::multimap<long double, uint16_t> dst;
    std::transform(src.begin(),
                   src.end(),
                   std::inserter(dst, dst.begin()),
                   [](const std::pair<uint16_t, long double>& p) {
                       return std::make_pair(p.second, p.first);
                   });
    return dst;
}

Ptr<KpmIndicationHeader>
E2Interface ::BuildRicIndicationHeader(std::string plmId,
                                       std::string gnbId,
                                       uint16_t nrCellId) const
{
    // if (!m_forceE2FileLogging)
    //{
    KpmIndicationHeader::KpmRicIndicationHeaderValues headerValues;
    headerValues.m_plmId = plmId;
    headerValues.m_gnbId = gnbId;
    headerValues.m_nrCellId = nrCellId;
    auto time = Simulator::Now();
    uint64_t timestamp = m_startTime + (uint64_t)time.GetMilliSeconds();
    NS_LOG_DEBUG("NR plmid " << plmId << " gnbId " << gnbId << " nrCellId " << nrCellId);
    NS_LOG_DEBUG("Timestamp " << timestamp);
    headerValues.m_timestamp = timestamp;

    Ptr<KpmIndicationHeader> header =
        Create<KpmIndicationHeader>(KpmIndicationHeader::GlobalE2nodeType::gNB, headerValues);
    return header;
    /**
    }
    else
    {
        return nullptr;
    }
     */
}

Ptr<NoriE2Report> E2Interface::GetE2DuCalculator()
{
    return m_e2DuCalculator;
}

} // namespace ns3
