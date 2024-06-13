// Copyright (c) 2023 Núcleo de P&D em Telecomunicações, Automação e Eletrônica (LASSE)
//

#include "nori-gnb-net-device.h"

#include "encode_e2apv1.hpp"
#include "nori-E2-report.h"
#include "nori-lte-rlc.h"
#include "nori-lte-rlc-um.h"

#include <ns3/core-module.h>
#include <ns3/lte-radio-bearer-info.h>
#include <ns3/lte-rlc-am.h>
//#include <ns3/lte-rlc-um.h>
//#include <ns3/lte-rlc.h>
#include <ns3/nr-bearer-stats-calculator.h>
#include <ns3/nr-gnb-net-device.h>
#include <ns3/nr-phy.h>
#include <ns3/oran-interface-module.h>
#include <ns3/radio-bearer-stats-calculator.h>

NS_LOG_COMPONENT_DEFINE("NoriGnbNetDevice");

namespace ns3
{
NS_OBJECT_ENSURE_REGISTERED(NoriGnbNetDevice);

NoriGnbNetDevice::NoriGnbNetDevice()
{
    NS_LOG_FUNCTION(this);
}

NoriGnbNetDevice::~NoriGnbNetDevice()
{
    NS_LOG_FUNCTION(this);
}

/**
 * \brief Get the TypeId of the NoriGnbNetDevice class.
 *
 * This function returns the TypeId of the NoriGnbNetDevice class, which is used for object
 * identification and dynamic casting in the ns-3 simulation environment.
 *
 * \return The TypeId of the NoriGnbNetDevice class.
 */
TypeId
NoriGnbNetDevice::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::NoriGnbNetDevice")
            .SetParent<NrGnbNetDevice>()
            .AddConstructor<NoriGnbNetDevice>()
            .AddAttribute("E2Periodicity",
                          "Periodicity of E2 reports",
                          DoubleValue(1.0),
                          MakeDoubleAccessor(&NoriGnbNetDevice::m_e2Periodicity),
                          MakeDoubleChecker<double>())
            .AddAttribute("E2Termination",
                          "The E2 termination object associated to this node",
                          PointerValue(),
                          MakePointerAccessor(&NoriGnbNetDevice::SetE2Termination,
                                              &NoriGnbNetDevice::GetE2Termination),
                          MakePointerChecker<E2Termination>())
            .AddAttribute("BasicCellId",
                          "Basic cell ID. This is needed to properly loop over neighbors.",
                          UintegerValue(1),
                          MakeUintegerAccessor(&NoriGnbNetDevice::m_basicCellId),
                          MakeUintegerChecker<uint16_t>())
            .AddAttribute("E2PdcpCalculator",
                          "The PDCP calculator object for E2 reporting",
                          PointerValue(),
                          MakePointerAccessor(&NoriGnbNetDevice::m_e2PdcpStatsCalculator),
                          MakePointerChecker<NrBearerStatsCalculator>())
            .AddAttribute("E2Periodicity",
                          "Periodicity of E2 reporting (value in seconds)",
                          DoubleValue(0.1),
                          MakeDoubleAccessor(&NoriGnbNetDevice::m_e2Periodicity),
                          MakeDoubleChecker<double>())
            .AddAttribute("E2RlcCalculator",
                          "The RLC calculator object for E2 reporting",
                          PointerValue(),
                          MakePointerAccessor(&NoriGnbNetDevice::m_e2RlcStatsCalculator),
                          MakePointerChecker<NrBearerStatsCalculator>())
            .AddAttribute("E2DuCalculator",
                          "The DU calculator object for E2 reporting",
                          PointerValue(),
                          MakePointerAccessor(&NoriGnbNetDevice::m_e2DuCalculator),
                          MakePointerChecker<NoriE2Report>())
            .AddAttribute("EnableCuUpReport",
                          "If true, send CuUpReport",
                          BooleanValue(false),
                          MakeBooleanAccessor(&NoriGnbNetDevice::m_sendCuUp),
                          MakeBooleanChecker())
            .AddAttribute("EnableCuCpReport",
                          "If true, send CuCpReport",
                          BooleanValue(false),
                          MakeBooleanAccessor(&NoriGnbNetDevice::m_sendCuCp),
                          MakeBooleanChecker())
            .AddAttribute("EnableDuReport",
                          "If true, send DuReport",
                          BooleanValue(true),
                          MakeBooleanAccessor(&NoriGnbNetDevice::m_sendDu),
                          MakeBooleanChecker())
            .AddAttribute("ReducedPmValues",
                          "If true, send only a subset of pmValues",
                          BooleanValue(false),
                          MakeBooleanAccessor(&NoriGnbNetDevice::m_reducedPmValues),
                          MakeBooleanChecker())
            .AddAttribute("EnableE2FileLogging",
                          "If true, force E2 indication generation and write E2 fields in csv file",
                          BooleanValue(false),
                          MakeBooleanAccessor(&NoriGnbNetDevice::m_forceE2FileLogging),
                          MakeBooleanChecker());
    return tid;
}

/**
 * \brief Initializes the NoriGnbNetDevice.
 *
 * This function is responsible for initializing the NoriGnbNetDevice. It connects
 * the NotifyMmWaveSinr callback to the LteEnbRrc module if m_sendCuCp is true.
 * The NotifyMmWaveSinr callback is used to register a new SINR reading.
 */
void
NoriGnbNetDevice::DoInialize()
{
    if (m_sendCuCp)
    {
        Config::ConnectFailSafe(
            "/NodeList/*/DeviceList/*/LteEnbRrc/NotifyMmWaveSinr",
            MakeCallback(&NoriGnbNetDevice::RegisterNewSinrReadingCallback, this));
    }
}

/**
 * @brief Callback function for handling RIC subscription requests.
 *
 * This function is called when a RIC subscription request is received.
 * It processes the request and performs the necessary actions based on the request parameters.
 *
 * @param sub_req_pdu Pointer to the E2AP_PDU_t structure containing the subscription request PDU.
 */
void
NoriGnbNetDevice::KpmSubscriptionCallback(E2AP_PDU_t* sub_req_pdu)
{
    NS_LOG_DEBUG("\nReceived RIC Subscription Request, cellId= " << GetCellId() << "\n");

    E2Termination::RicSubscriptionRequest_rval_s params =
        m_e2term->ProcessRicSubscriptionRequest(sub_req_pdu);
    NS_LOG_DEBUG("requestorId " << +params.requestorId << ", instanceId " << +params.instanceId
                                << ", ranFuncionId " << +params.ranFuncionId << ", actionId "
                                << +params.actionId);
    if (!m_isReportingEnabled)
    {
        BuildAndSendReportMessage(params);
        m_isReportingEnabled = true;
    }
}

/**
 * \brief Registers a new SINR reading callback for the NoriGnbNetDevice.
 *
 * This function registers a new SINR reading callback for the NoriGnbNetDevice.
 * The callback is triggered whenever a new SINR reading is available for a specific IMSI and cell ID.
 *
 * \param netDev   A pointer to the NrGnbNetDevice object.
 * \param context  The context of the SINR reading.
 * \param imsi     The IMSI (International Mobile Subscriber Identity) of the device.
 * \param cellId   The ID of the cell.
 * \param sinr     The SINR (Signal-to-Interference-plus-Noise Ratio) value.
 */
void
NoriGnbNetDevice::RegisterNewSinrReadingCallback(Ptr<NrGnbNetDevice> netDev,
                                                 std::string context,
                                                 uint64_t imsi,
                                                 uint16_t cellId,
                                                 long double sinr)
{
    // netDev->RegisterNewSinrReading(imsi, cellId, sinr);
}

/**
 * @brief Updates the configuration of the NoriGnbNetDevice.
 *
 * This function is responsible for updating the configuration of the NoriGnbNetDevice.
 * It checks if the E2Termination object is not null and then performs the necessary
 * configuration updates. If the forceE2FileLogging flag is not set, it schedules the
 * Start function of the E2Termination object. Otherwise, it creates and writes data to
 * CSV files for logging purposes and schedules the BuildAndSendReportMessage function
 * to send a report message.
 */
void
NoriGnbNetDevice::UpdateConfig()
{
    if (m_e2term != nullptr)
    {
        uint16_t cellId = GetCellId();
        NS_LOG_DEBUG("E2sim start in cell " << cellId << " force CSV logging "
                                            << m_forceE2FileLogging);

        if (!m_forceE2FileLogging)
        {
            Simulator::Schedule(MicroSeconds(0), &E2Termination::Start, m_e2term);
        }
        else
        {
            m_cuUpFileName = "cu-up-cell-" + std::to_string(cellId) + ".txt";
            std::ofstream csv{};
            csv.open(m_cuUpFileName.c_str());
            csv << "timestamp,ueImsiComplete,DRB.PdcpSduDelayDl (cellAverageLatency),"
                   "m_pDCPBytesUL (0),"
                   "m_pDCPBytesDL (cellDlTxVolume),DRB.PdcpSduVolumeDl_Filter.UEID (txBytes),"
                   "Tot.PdcpSduNbrDl.UEID (txDlPackets),DRB.PdcpSduBitRateDl.UEID"
                   "(pdcpThroughput),"
                   "DRB.PdcpSduDelayDl.UEID (pdcpLatency),QosFlow.PdcpPduVolumeDL_Filter.UEID"
                   "(txPdcpPduBytesNrRlc),DRB.PdcpPduNbrDl.Qos.UEID (txPdcpPduNrRlc)\n";
            csv.close();

            m_cuCpFileName = "cu-cp-cell-" + std::to_string(cellId) + ".txt";
            csv.open(m_cuCpFileName.c_str());
            csv << "timestamp,ueImsiComplete,numActiveUes,DRB.EstabSucc.5QI.UEID (numDrb),"
                   "DRB.RelActNbr.5QI.UEID (0),L3 serving Id(m_cellId),UE (imsi),L3 serving SINR,"
                   "L3 serving SINR 3gpp,"
                   "L3 neigh Id 1 (cellId),L3 neigh SINR 1,L3 neigh SINR 3gpp 1 (convertedSinr),"
                   "L3 neigh Id 2 (cellId),L3 neigh SINR 2,L3 neigh SINR 3gpp 2 (convertedSinr),"
                   "L3 neigh Id 3 (cellId),L3 neigh SINR 3,L3 neigh SINR 3gpp 3 (convertedSinr),"
                   "L3 neigh Id 4 (cellId),L3 neigh SINR 4,L3 neigh SINR 3gpp 4 (convertedSinr),"
                   "L3 neigh Id 5 (cellId),L3 neigh SINR 5,L3 neigh SINR 3gpp 5 (convertedSinr),"
                   "L3 neigh Id 6 (cellId),L3 neigh SINR 6,L3 neigh SINR 3gpp 6 (convertedSinr),"
                   "L3 neigh Id 7 (cellId),L3 neigh SINR 7,L3 neigh SINR 3gpp 7 (convertedSinr),"
                   "L3 neigh Id 8 (cellId),L3 neigh SINR 8,L3 neigh SINR 3gpp 8 (convertedSinr)"
                   "\n";
            csv.close();

            m_duFileName = "du-cell-" + std::to_string(cellId) + ".txt";
            csv.open(m_duFileName.c_str());

            std::string header_csv = "timestamp,ueImsiComplete,plmId,nrCellId,dlAvailablePrbs,"
                                     "ulAvailablePrbs,qci,dlPrbUsage,ulPrbUsage";

            std::string cell_header =
                "TB.TotNbrDl.1,TB.TotNbrDlInitial,TB.TotNbrDlInitial.Qpsk,"
                "TB.TotNbrDlInitial.16Qam,"
                "TB.TotNbrDlInitial.64Qam,RRU.PrbUsedDl,TB.ErrTotalNbrDl.1,"
                "QosFlow.PdcpPduVolumeDL_Filter,CARR.PDSCHMCSDist.Bin1,"
                "CARR.PDSCHMCSDist.Bin2,"
                "CARR.PDSCHMCSDist.Bin3,CARR.PDSCHMCSDist.Bin4,CARR.PDSCHMCSDist.Bin5,"
                "CARR.PDSCHMCSDist.Bin6,L1M.RS-SINR.Bin34,L1M.RS-SINR.Bin46, "
                "L1M.RS-SINR.Bin58,"
                "L1M.RS-SINR.Bin70,L1M.RS-SINR.Bin82,L1M.RS-SINR.Bin94,L1M.RS-SINR.Bin127,"
                "DRB.BufferSize.Qos,DRB.MeanActiveUeDl";

            std::string ue_header =
                "TB.TotNbrDl.1.UEID,TB.TotNbrDlInitial.UEID,TB.TotNbrDlInitial.Qpsk.UEID,"
                "TB.TotNbrDlInitial.16Qam.UEID,TB.TotNbrDlInitial.64Qam.UEID,"
                "TB.ErrTotalNbrDl.1.UEID,"
                "QosFlow.PdcpPduVolumeDL_Filter.UEID,RRU.PrbUsedDl.UEID,"
                "CARR.PDSCHMCSDist.Bin1.UEID,"
                "CARR.PDSCHMCSDist.Bin2.UEID,CARR.PDSCHMCSDist.Bin3.UEID,"
                "CARR.PDSCHMCSDist.Bin4.UEID,"
                "CARR.PDSCHMCSDist.Bin5.UEID,"
                "CARR.PDSCHMCSDist.Bin6.UEID,L1M.RS-SINR.Bin34.UEID, L1M.RS-SINR.Bin46.UEID,"
                "L1M.RS-SINR.Bin58.UEID,L1M.RS-SINR.Bin70.UEID,L1M.RS-SINR.Bin82.UEID,"
                "L1M.RS-SINR.Bin94.UEID,L1M.RS-SINR.Bin127.UEID,DRB.BufferSize.Qos.UEID,"
                "DRB.UEThpDl.UEID, DRB.UEThpDlPdcpBased.UEID";

            csv << header_csv + "," + cell_header + "," + ue_header + "\n";
            csv.close();
            Simulator::Schedule(MicroSeconds(500),
                                &NoriGnbNetDevice::BuildAndSendReportMessage,
                                this,
                                E2Termination::RicSubscriptionRequest_rval_s{});
        }
    }
}

/**
 * \brief Get the E2 termination associated with this NoriGnbNetDevice.
 *
 * This function returns a pointer to the E2Termination object associated with this NoriGnbNetDevice.
 * The E2Termination object represents the E2 interface termination point of the gNB (gNodeB).
 *
 * \return A pointer to the E2Termination object.
 */
Ptr<E2Termination>
NoriGnbNetDevice::GetE2Termination() const
{
    return m_e2term;
}

/**
 * \brief Sets the E2 termination for the NoriGnbNetDevice.
 *
 * This method sets the E2 termination for the NoriGnbNetDevice. The E2 termination is responsible for handling E2 service
 * requests and providing E2 service responses.
 *
 * \param e2term A pointer to the E2Termination object.
 */
void
NoriGnbNetDevice::SetE2Termination(Ptr<E2Termination> e2term)
{
    m_e2term = e2term;

    NS_LOG_DEBUG("Register E2SM");

    if (!m_forceE2FileLogging)
    {
        Ptr<KpmFunctionDescription> kpmFd = Create<KpmFunctionDescription>();
        e2term->RegisterKpmCallbackToE2Sm(
            200,
            kpmFd,
            std::bind(&NoriGnbNetDevice::KpmSubscriptionCallback, this, std::placeholders::_1));
    }
}

/**
 * @brief Converts a given IMSI (International Mobile Subscriber Identity) to a string representation.
 *
 * This function takes a 64-bit IMSI value and converts it to a string representation.
 * The resulting string is padded with leading zeros to ensure a consistent length.
 *
 * @param imsi The IMSI value to convert.
 * @return The string representation of the IMSI.
 */
std::string
NoriGnbNetDevice::GetImsiString(uint64_t imsi)
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

/**
 * \brief Builds the RIC (RAN Intelligent Controller) indication header.
 *
 * This function constructs and returns a pointer to a KpmIndicationHeader object
 * that represents the RIC indication header. The header contains information such
 * as the PLM ID, gNB ID, NR cell ID, and timestamp.
 *
 * \param plmId The PLM (Private Label Manufacturer) ID.
 * \param gnbId The gNB (gNodeB) ID.
 * \param nrCellId The NR (New Radio) cell ID.
 *
 * \returns A pointer to the constructed KpmIndicationHeader object if file logging is not forced,
 *          otherwise returns nullptr.
 */
Ptr<KpmIndicationHeader>
NoriGnbNetDevice::BuildRicIndicationHeader(std::string plmId,
                                           std::string gnbId,
                                           uint16_t nrCellId) const
{
    if (!m_forceE2FileLogging)
    {
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
    }
    else
    {
        return nullptr;
    }
}

/**
 * \brief Builds and returns a RIC (RAN Intelligent Controller) indication message for the CU-UP (Centralized Unit - User Plane) interface.
 *
 * This function constructs a RIC indication message for the CU-UP interface based on the current state of the gNB (base station) and the connected UEs (User Equipments).
 * The indication message contains various performance metrics and statistics related to the gNB and the UEs, such as PDCP (Packet Data Convergence Protocol) volume, latency, throughput, etc.
 *
 * \param plmId The PLM (Private Land Mobile) ID associated with the indication message.
 * \return A pointer to the constructed RIC indication message.
 */
Ptr<KpmIndicationMessage>
NoriGnbNetDevice::BuildRicIndicationMessageCuUp(std::string plmId)
{
    Ptr<MmWaveIndicationMessageHelper> indicationMessageHelper =
        Create<MmWaveIndicationMessageHelper>(IndicationMessageHelper::IndicationMessageType::CuUp,
                                              m_forceE2FileLogging,
                                              m_reducedPmValues);

    // get <rnti, UeManager> map of connected UEs
    uint16_t cellId = GetCellId();
    auto rrc = GetRrc();
    ObjectMapValue ueMap;
    rrc->GetAttribute("UeManagerMap", ueMap);

    // gNB-wide PDCP volume in downlink
    double cellDlTxVolume = 0;
    // rx bytes in downlink
    double cellDlRxVolume = 0;

    // sum of the per-user average latency
    double perUserAverageLatencySum = 0;

    std::unordered_map<uint64_t, std::string> uePmString{};

    for (auto ue = ueMap.Begin(); ue != ueMap.End(); ++ue)
    {
        uint64_t imsi = DynamicCast<UeManager>(ue->second)->GetImsi();
        std::string ueImsiComplete = GetImsiString(imsi);

        long txDlPackets = m_e2PdcpStatsCalculator->GetDlTxPackets(imsi, 3); // LCID 3 is used for data
        double txBytes = m_e2PdcpStatsCalculator->GetDlTxData(imsi, 3) * 8 / 1e3; // in kbit, not byte
        double rxBytes = m_e2PdcpStatsCalculator->GetDlRxData(imsi, 3) * 8 / 1e3; // in kbit, not byte
        cellDlTxVolume += txBytes;
        cellDlRxVolume += rxBytes;

        long txPdcpPduNrRlc = 0;
        double txPdcpPduBytesNrRlc = 0;
        ObjectMapValue drbMap;
        DynamicCast<UeManager>(ue->second)->GetAttribute("DrbMap", drbMap);

        for (auto drb = drbMap.Begin(); drb != drbMap.End(); ++drb)
        {
            ///**
            // * \todo GetTx...() is defined in mmwave. How can we implement this?
            // */
            //txPdcpPduNrRlc += DynamicCast<LteDataRadioBearerInfo>(drb->second)->m_rlc->GetTxPacketsInReportingPeriod();
            //txPdcpPduBytesNrRlc += DynamicCast<LteDataRadioBearerInfo>(drb->second)->m_rlc->GetTxBytesInReportingPeriod();
            //DynamicCast<LteDataRadioBearerInfo>(drb->second)->m_rlc->ResetRlcCounters();

            ns3::Ptr<ns3::LteRlc> baseRlcPtr = DynamicCast<ns3::LteDataRadioBearerInfo>(drb->second)->m_rlc;

            // Convertendo `baseRlcPtr` para o tipo personalizado `NoriLteRlc`
            ns3::Ptr<NoriLteRlc> customRlcPtr = DynamicCast<NoriLteRlc>(baseRlcPtr);

            if (customRlcPtr != nullptr) {
                txPdcpPduNrRlc += customRlcPtr->GetTxPacketsInReportingPeriod();
                txPdcpPduBytesNrRlc += customRlcPtr->GetTxBytesInReportingPeriod();
                customRlcPtr->ResetRlcCounters();
            } else {
                NS_LOG_ERROR("Falha ao converter m_rlc para NoriLteRlc");
            }

        }

        /**
         * \todo RLC map is defined in mmwave. How can we implement this?
         */
        auto rlcMap = DynamicCast<UeManager>(ue->second)->GetRlcMap(); // secondary-connected RLCs
        for (auto drb : rlcMap)
        {
            txPdcpPduNrRlc += drb.second->m_rlc->GetTxPacketsInReportingPeriod();
            txPdcpPduBytesNrRlc += drb.second->m_rlc->GetTxBytesInReportingPeriod();
            drb.second->m_rlc->ResetRlcCounters();
        }
        txPdcpPduBytesNrRlc *= 8 / 1e3;

        double pdcpLatency = m_e2PdcpStatsCalculator->GetDlDelay(imsi, 3) / 1e5; // unit: x 0.1 ms
        perUserAverageLatencySum += pdcpLatency;

        double pdcpThroughput = txBytes / m_e2Periodicity;   // unit kbps
        double pdcpThroughputRx = rxBytes / m_e2Periodicity; // unit kbps

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
        double rlcBitrate = (rlcLatency == 0) ? 0 : pduStats / rlcLatency;     // unit kbit/s

        m_drbThrDlUeid[imsi] = rlcBitrate;

        NS_LOG_DEBUG(Simulator::Now().GetSeconds()
                     << " " << cellId << " cell, connected UE with IMSI " << imsi
                     << " ueImsiString " << ueImsiComplete << " txDlPackets " << txDlPackets
                     << " txDlPacketsNr " << txPdcpPduNrRlc << " txBytes " << txBytes << " rxBytes "
                     << rxBytes << " txDlBytesNr " << txPdcpPduBytesNrRlc << " pdcpLatency "
                     << pdcpLatency << " pdcpThroughput " << pdcpThroughput << " rlcBitrate "
                     << rlcBitrate);
        /**
         * \todo ResetResultsForImsiLcid() is defined only in mmwave. How can we implement this?
        */
        m_e2PdcpStatsCalculator->ResetResultsForImsiLcid(imsi, 3);

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

    NS_LOG_DEBUG(Simulator::Now().GetSeconds()
                 << " " << cellId << " cell volume " << cellDlTxVolume);

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

        for (auto ue = ueMap.Begin(); ue != ueMap.End(); ++ue)
        {
            uint64_t imsi = DynamicCast<UeManager>(ue->second)->GetImsi();
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
        return indicationMessageHelper->CreateIndicationMessage();
    }
}

template <typename A, typename B>
std::pair<B, A>
flip_pair(const std::pair<A, B>& p)
{
    return std::pair<B, A>(p.second, p.first);
}

template <typename A, typename B>
std::multimap<B, A>
flip_map(const std::map<A, B>& src)
{
    std::multimap<B, A> dst;
    std::transform(src.begin(), src.end(), std::inserter(dst, dst.begin()), flip_pair<A, B>);
    return dst;
}

/**
 * @brief Builds the RIC (RAN Intelligent Controller) indication message for the CU-CP (Centralized Unit - Control Plane) type.
 *
 * This function constructs the RIC indication message for the CU-CP type, which is used to provide information about the network device to the RAN Intelligent Controller.
 * The indication message includes various measurements and parameters related to the UE (User Equipment) and its serving and neighboring cells.
 *
 * @param plmId The PLM (Private Land Mobile) ID associated with the indication message.
 * @return A pointer to the built RIC indication message, or nullptr if file logging is enabled.
 */
Ptr<KpmIndicationMessage>
NoriGnbNetDevice::BuildRicIndicationMessageCuCp(std::string plmId)
{
    Ptr<MmWaveIndicationMessageHelper> indicationMessageHelper =
        Create<MmWaveIndicationMessageHelper>(IndicationMessageHelper::IndicationMessageType::CuCp,
                                              m_forceE2FileLogging,
                                              m_reducedPmValues);

    auto rrc = GetRrc();

    ObjectMapValue ueMap;
    rrc->GetAttribute("UeManagerMap", ueMap);
    std::unordered_map<uint64_t, std::string> uePmString{};

    for (auto ue = ueMap.Begin(); ue != ueMap.End(); ++ue)
    {
        uint64_t imsi = DynamicCast<UeManager>(ue->second)->GetImsi();
        std::string ueImsiComplete = GetImsiString(imsi);

        Ptr<MeasurementItemList> ueVal = Create<MeasurementItemList>(ueImsiComplete);

        ObjectMapValue drbMap;
        DynamicCast<UeManager>(ue->second)->GetAttribute("DrbMap", drbMap);
        long numDrb{0};
        for (auto drb = drbMap.Begin(); drb != drbMap.End(); ++drb)
        {
            numDrb++;
        }

        if (!m_reducedPmValues)
        {
            ueVal->AddItem<long>("DRB.EstabSucc.5QI.UEID", numDrb);
            ueVal->AddItem<long>("DRB.RelActNbr.5QI.UEID", 0); // not modeled in the simulator
        }

        // create L3 RRC reports
        uint16_t cellId = GetCellId();
        // for the same cell
        double sinrThisCell = 10 * std::log10(m_l3sinrMap[imsi][cellId]);
        double convertedSinr = L3RrcMeasurements::ThreeGppMapSinr(sinrThisCell);

        Ptr<L3RrcMeasurements> l3RrcMeasurementServing;
        if (!indicationMessageHelper->IsOffline())
        {
            l3RrcMeasurementServing =
                L3RrcMeasurements::CreateL3RrcUeSpecificSinrServing(cellId, cellId, convertedSinr);
        }
        NS_LOG_DEBUG(Simulator::Now().GetSeconds()
                     << " enbdev " << cellId << " UE " << imsi << " L3 serving SINR "
                     << sinrThisCell << " L3 serving SINR 3gpp " << convertedSinr);

        std::string servingStr = std::to_string(numDrb) + "," + std::to_string(0) + "," +
                                 std::to_string(cellId) + "," + std::to_string(imsi) + "," +
                                 std::to_string(sinrThisCell) + "," + std::to_string(convertedSinr);

        // For the neighbors

        Ptr<L3RrcMeasurements> l3RrcMeasurementNeigh;
        if (!indicationMessageHelper->IsOffline())
        {
            l3RrcMeasurementNeigh = L3RrcMeasurements::CreateL3RrcUeSpecificSinrNeigh();
        }
        double sinr;
        std::string neighStr;

        // invert key and value in sortFlipMap, then sort by value
        std::multimap<long double, uint16_t> sortFlipMap = flip_map(m_l3sinrMap[imsi]);
        // new sortFlipMap structure sortFlipMap < sinr, cellId >
        // The assumption is that the first cell in the scenario is always LTE and the rest NR
        uint16_t nNeighbours = E2SM_REPORT_MAX_NEIGH;
        if (m_l3sinrMap[imsi].size() < nNeighbours)
        {
            nNeighbours = m_l3sinrMap[imsi].size() - 1;
        }
        int itIndex = 0;
        // Save only the first E2SM_REPORT_MAX_NEIGH SINR for each UE which represent the best
        // values among all the SINRs detected by all the cells
        for (auto it = --sortFlipMap.end(); it != --sortFlipMap.begin() && itIndex < nNeighbours;
             it--)
        {
            uint16_t currentCellId = it->second;
            if (currentCellId != cellId)
            {
                sinr = 10 * std::log10(it->first); // now SINR is a key due to the sort of the map
                convertedSinr = L3RrcMeasurements::ThreeGppMapSinr(sinr);
                if (!indicationMessageHelper->IsOffline())
                {
                    l3RrcMeasurementNeigh->AddNeighbourCellMeasurement(cellId, convertedSinr);
                }
                NS_LOG_DEBUG(Simulator::Now().GetSeconds()
                             << " enbdev " << cellId << " UE " << imsi << " L3 neigh " << cellId
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

    auto it = ueMap.Begin();
    uint32_t size = 0;
    while (it != ueMap.End())
    {
        size++;
        it++;
    }

    if (!indicationMessageHelper->IsOffline())
    {
        // Fill CuCp specific fields
        indicationMessageHelper->FillCuCpValues(size); // Number of Active UEs
    }

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

        for (auto ue = ueMap.Begin(); ue != ueMap.End(); ++ue)

        {
            uint64_t imsi = DynamicCast<UeManager>(ue->second)->GetImsi();
            std::string ueImsiComplete = GetImsiString(imsi);

            auto uePms = uePmString.find(imsi)->second;

            std::string to_print = std::to_string(timestamp) + "," + ueImsiComplete + "," +
                                   std::to_string(size) + "," + uePms + "\n";

            NS_LOG_DEBUG(to_print);

            csv << to_print;
        }
        csv.close();
        return nullptr;
    }
    else
    {
        return indicationMessageHelper->CreateIndicationMessage();
    }
}

uint32_t
NoriGnbNetDevice::GetRlcBufferOccupancy(Ptr<LteRlc> rlc) const
{
    if (DynamicCast<LteRlcAm>(rlc) != nullptr)
    {
        /**
         * \todo Create GetTxBufferSize method in LteRlcAm class
         */
        return DynamicCast<LteRlcAm>(rlc)->GetTxBufferSize();
    }
    else if (DynamicCast<NoriLteRlcUm>(rlc) != nullptr)
    {
        /**
         * \todo Create GetTxBufferSize method in LteRlcUm class
         */
        return DynamicCast<NoriLteRlcUm>(rlc)->GetTxBufferSize();
    }
    /**
     * \note Disabled because the class LteRlcUmLowLat is not available in the current version of
    ns-3
     *
    else if(DynamicCast<LteRlcUmLowLat>(rlc) != nullptr)
    {
      return DynamicCast<LteRlcUmLowLat>(rlc)->GetTxBufferSize();
    }
    */
    else
    {
        return 0;
    }
}

/**
 * \todo fix this function and the others below
 */
/**
 * \brief Builds and returns a RIC (Radio Intelligent Controller) indication message for the DU (Distributed Unit).
 *
 * This function constructs a RIC indication message for the DU based on various metrics and parameters
 * related to the connected UEs (User Equipments). The metrics include MAC PDU (Protocol Data Unit),
 * MAC volume, MAC QPSK (Quadrature Phase Shift Keying), MAC 16QAM (16 Quadrature Amplitude Modulation),
 * MAC 64QAM (64 Quadrature Amplitude Modulation), MAC retransmission, MAC PRB (Physical Resource Block),
 * MAC MCS (Modulation and Coding Scheme), MAC SINR (Signal-to-Interference plus Noise Ratio), and RLC
 * (Radio Link Control) buffer occupancy.
 *
 * \param plmId The PLM (Private Land Mobile) ID associated with the DU.
 * \param nrCellId The NR (New Radio) cell ID associated with the DU.
 *
 * \return A pointer to the constructed RIC indication message.
 */
Ptr<KpmIndicationMessage>
NoriGnbNetDevice::BuildRicIndicationMessageDu(std::string plmId, uint16_t nrCellId)
{
    Ptr<MmWaveIndicationMessageHelper> indicationMessageHelper =
        Create<MmWaveIndicationMessageHelper>(IndicationMessageHelper::IndicationMessageType::Du,
                                              m_forceE2FileLogging,
                                              m_reducedPmValues);
    auto rrc = GetRrc();
    ObjectMapValue ueMap;
    rrc->GetAttribute("UeManagerMap", ueMap);

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

    std::unordered_map<uint64_t, std::string> uePmStringDu{};

    for (auto ue = ueMap.Begin(); ue != ueMap.End(); ++ue)
    {
        uint64_t imsi = DynamicCast<UeManager>(ue->second)->GetImsi();
        std::string ueImsiComplete = GetImsiString(imsi);
        uint16_t rnti = DynamicCast<UeManager>(ue->second)->GetRnti();
        uint16_t cellId = GetCellId();
        uint32_t macPduUe = m_e2DuCalculator->GetMacPduUeSpecific(rnti, cellId);

        macPduCellSpecific += macPduUe;

        uint32_t macPduInitialUe =
            m_e2DuCalculator->GetMacPduInitialTransmissionUeSpecific(rnti, cellId);
        macPduInitialCellSpecific += macPduInitialUe;

        uint32_t macVolume = m_e2DuCalculator->GetMacVolumeUeSpecific(rnti, cellId);
        macVolumeCellSpecific += macVolume;

        uint32_t macQpsk = m_e2DuCalculator->GetMacPduQpskUeSpecific(rnti, cellId);
        macQpskCellSpecific += macQpsk;

        uint32_t mac16Qam = m_e2DuCalculator->GetMacPdu16QamUeSpecific(rnti, cellId);
        mac16QamCellSpecific += mac16Qam;

        uint32_t mac64Qam = m_e2DuCalculator->GetMacPdu64QamUeSpecific(rnti, cellId);
        mac64QamCellSpecific += mac64Qam;

        uint32_t macRetx = m_e2DuCalculator->GetMacPduRetransmissionUeSpecific(rnti, cellId);
        macRetxCellSpecific += macRetx;

        // Numerator = (Sum of number of symbols across all rows (TTIs) group by cell ID and UE ID
        // within a given time window)
        double macNumberOfSymbols = m_e2DuCalculator->GetMacNumberOfSymbolsUeSpecific(rnti, cellId);

        /**
         * \todo: Here it depends of the number of component carriers (cc) per band. Maybe need to
         * make an iteration over the MAC of each cc.
         */
        auto phyMac = GetPhy(0);

        // Denominator = (Periodicity of the report time window in ms*number of TTIs per ms*14)
        Time reportingWindow = Simulator::Now() - m_e2DuCalculator->GetLastResetTime(rnti, cellId);
        double denominatorPrb =
            std::ceil(reportingWindow.GetNanoSeconds() / phyMac->GetSlotPeriod().GetNanoSeconds()) *
            14;

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

        uint32_t macMac04 = m_e2DuCalculator->GetMacMcs04UeSpecific(rnti, cellId);
        macMac04CellSpecific += macMac04;

        uint32_t macMac59 = m_e2DuCalculator->GetMacMcs59UeSpecific(rnti, cellId);
        macMac59CellSpecific += macMac59;

        uint32_t macMac1014 = m_e2DuCalculator->GetMacMcs1014UeSpecific(rnti, cellId);
        macMac1014CellSpecific += macMac1014;

        uint32_t macMac1519 = m_e2DuCalculator->GetMacMcs1519UeSpecific(rnti, cellId);
        macMac1519CellSpecific += macMac1519;

        uint32_t macMac2024 = m_e2DuCalculator->GetMacMcs2024UeSpecific(rnti, cellId);
        macMac2024CellSpecific += macMac2024;

        uint32_t macMac2529 = m_e2DuCalculator->GetMacMcs2529UeSpecific(rnti, cellId);
        macMac2529CellSpecific += macMac2529;

        uint32_t macSinrBin1 = m_e2DuCalculator->GetMacSinrBin1UeSpecific(rnti, cellId);
        macSinrBin1CellSpecific += macSinrBin1;

        uint32_t macSinrBin2 = m_e2DuCalculator->GetMacSinrBin2UeSpecific(rnti, cellId);
        macSinrBin2CellSpecific += macSinrBin2;

        uint32_t macSinrBin3 = m_e2DuCalculator->GetMacSinrBin3UeSpecific(rnti, cellId);
        macSinrBin3CellSpecific += macSinrBin3;

        uint32_t macSinrBin4 = m_e2DuCalculator->GetMacSinrBin4UeSpecific(rnti, cellId);
        macSinrBin4CellSpecific += macSinrBin4;

        uint32_t macSinrBin5 = m_e2DuCalculator->GetMacSinrBin5UeSpecific(rnti, cellId);
        macSinrBin5CellSpecific += macSinrBin5;

        uint32_t macSinrBin6 = m_e2DuCalculator->GetMacSinrBin6UeSpecific(rnti, cellId);
        macSinrBin6CellSpecific += macSinrBin6;

        uint32_t macSinrBin7 = m_e2DuCalculator->GetMacSinrBin7UeSpecific(rnti, cellId);
        macSinrBin7CellSpecific += macSinrBin7;

        // get buffer occupancy info
        uint32_t rlcBufferOccup = 0;
        ObjectMapValue drbMap;
        DynamicCast<UeManager>(ue->second)->GetAttribute("DrbMap", drbMap);

        for (auto drb = drbMap.Begin(); drb != drbMap.End(); ++drb)
        {
            auto rlc = DynamicCast<LteDataRadioBearerInfo>(drb->second)->m_rlc;
            rlcBufferOccup += GetRlcBufferOccupancy(rlc);
        }
        /**
         * \todo RLC map is defined in mmwave. How can we implement this?
        */
        auto rlcMap = DynamicCast<UeManager>(ue->second)->GetRlcMap(); // secondary-connected RLCs
        for (auto drb : rlcMap)
        {
            auto rlc = drb.second->m_rlc;
            rlcBufferOccup += GetRlcBufferOccupancy(rlc);
        }
        rlcBufferOccupCellSpecific += rlcBufferOccup;

        NS_LOG_DEBUG(Simulator::Now().GetSeconds()
                     << " " << cellId << " cell, connected UE with IMSI " << imsi << " rnti "
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
        m_e2DuCalculator->ResetPhyTracesForRntiCellId(rnti, cellId);
    }
    m_drbThrDlPdcpBasedComputationUeid.clear();
    m_drbThrDlUeid.clear();

    // Denominator = (Total number of rows (TTIs) within a given time window* 14)
    // Numerator = (Sum of number of symbols across all rows (TTIs) group by cell ID within a given
    // time window) * 139 Average Number of PRBs allocated for the UE = (NR/DR) (where 139 is the
    // total number of PRBs available per NR cell, given numerology 2 with 60 kHz SCS)
    double prbUtilizationDl = macPrbsCellSpecific;
    uint16_t cellId = GetCellId();
    uint16_t n_ueManagers{0};
    for (auto nUeMap = ueMap.Begin(); nUeMap != ueMap.End(); nUeMap++)
    {
        n_ueManagers++;
    }

    NS_LOG_DEBUG(
        Simulator::Now().GetSeconds()
        << " " << cellId << " cell, connected UEs number " << n_ueManagers << " macPduCellSpecific "
        << macPduCellSpecific << " macPduInitialCellSpecific " << macPduInitialCellSpecific
        << " macVolumeCellSpecific " << macVolumeCellSpecific << " macQpskCellSpecific "
        << macQpskCellSpecific << " mac16QamCellSpecific " << mac16QamCellSpecific
        << " mac64QamCellSpecific " << mac64QamCellSpecific << " macRetxCellSpecific "
        << macRetxCellSpecific << " macPrbsCellSpecific "
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
                                                 n_ueManagers);

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

    if (m_forceE2FileLogging)
    {
        std::ofstream csv{};
        csv.open(m_duFileName.c_str(), std::ios_base::app);
        if (!csv.is_open())
        {
            NS_FATAL_ERROR("Can't open file " << m_duFileName.c_str());
        }

        uint64_t timestamp = m_startTime + (uint64_t)Simulator::Now().GetMilliSeconds();

        // the string is timestamp, ueImsiComplete, plmId, nrCellId, dlAvailablePrbs,
        // ulAvailablePrbs, qci , dlPrbUsage, ulPrbUsage, /*CellSpecificValues*/, /*
        // UESpecificValues */

        /*
          CellSpecificValues:
            TB.TotNbrDl.1, TB.TotNbrDlInitial, TB.TotNbrDlInitial.Qpsk, TB.TotNbrDlInitial.16Qam,
          TB.TotNbrDlInitial.64Qam, RRU.PrbUsedDl, TB.ErrTotalNbrDl.1,
          QosFlow.PdcpPduVolumeDL_Filter, CARR.PDSCHMCSDist.Bin1, CARR.PDSCHMCSDist.Bin2,
          CARR.PDSCHMCSDist.Bin3, CARR.PDSCHMCSDist.Bin4, CARR.PDSCHMCSDist.Bin5,
          CARR.PDSCHMCSDist.Bin6, L1M.RS-SINR.Bin34, L1M.RS-SINR.Bin46, L1M.RS-SINR.Bin58,
            L1M.RS-SINR.Bin70, L1M.RS-SINR.Bin82, L1M.RS-SINR.Bin94, L1M.RS-SINR.Bin127,
          DRB.BufferSize.Qos, DRB.MeanActiveUeDl
        */

        std::string to_print_cell =
            plmId + "," + std::to_string(nrCellId) + "," + std::to_string(dlAvailablePrbs) + "," +
            std::to_string(ulAvailablePrbs) + "," + std::to_string(qci) + "," +
            std::to_string(dlPrbUsage) + "," + std::to_string(ulPrbUsage) + "," +
            std::to_string(macPduCellSpecific) + "," + std::to_string(macPduInitialCellSpecific) +
            "," + std::to_string(macQpskCellSpecific) + "," + std::to_string(mac16QamCellSpecific) +
            "," + std::to_string(mac64QamCellSpecific) + "," +
            std::to_string((long)std::ceil(prbUtilizationDl)) + "," +
            std::to_string(macRetxCellSpecific) + "," + std::to_string(macVolumeCellSpecific) +
            "," + std::to_string(macMac04CellSpecific) + "," +
            std::to_string(macMac59CellSpecific) + "," + std::to_string(macMac1014CellSpecific) +
            "," + std::to_string(macMac1519CellSpecific) + "," +
            std::to_string(macMac2024CellSpecific) + "," + std::to_string(macMac2529CellSpecific) +
            "," + std::to_string(macSinrBin1CellSpecific) + "," +
            std::to_string(macSinrBin2CellSpecific) + "," +
            std::to_string(macSinrBin3CellSpecific) + "," +
            std::to_string(macSinrBin4CellSpecific) + "," +
            std::to_string(macSinrBin5CellSpecific) + "," +
            std::to_string(macSinrBin6CellSpecific) + "," +
            std::to_string(macSinrBin7CellSpecific) + "," +
            std::to_string(rlcBufferOccupCellSpecific) + "," + std::to_string(n_ueManagers);

        /*
          UESpecificValues:

              TB.TotNbrDl.1.UEID, TB.TotNbrDlInitial.UEID, TB.TotNbrDlInitial.Qpsk.UEID,
          TB.TotNbrDlInitial.16Qam.UEID,TB.TotNbrDlInitial.64Qam.UEID, TB.ErrTotalNbrDl.1.UEID,
          QosFlow.PdcpPduVolumeDL_Filter.UEID, RRU.PrbUsedDl.UEID, CARR.PDSCHMCSDist.Bin1.UEID,
          CARR.PDSCHMCSDist.Bin2.UEID, CARR.PDSCHMCSDist.Bin3.UEID, CARR.PDSCHMCSDist.Bin5.UEID,
          CARR.PDSCHMCSDist.Bin6.UEID, L1M.RS-SINR.Bin34.UEID, L1M.RS-SINR.Bin46.UEID,
          L1M.RS-SINR.Bin58.UEID, L1M.RS-SINR.Bin70.UEID, L1M.RS-SINR.Bin82.UEID,
          L1M.RS-SINR.Bin94.UEID, L1M.RS-SINR.Bin127.UEID, DRB.BufferSize.Qos.UEID,
          DRB.UEThpDl.UEID, DRB.UEThpDlPdcpBased.UEID
        */

        for (auto ue = ueMap.Begin(); ue != ueMap.End(); ++ue)
        {
            uint64_t imsi = DynamicCast<UeManager>(ue->second)->GetImsi();
            std::string ueImsiComplete = GetImsiString(imsi);

            auto uePms = uePmStringDu.find(imsi)->second;

            std::string to_print = std::to_string(timestamp) + "," + ueImsiComplete + "," +
                                   to_print_cell + "," + uePms + "\n";

            csv << to_print;
        }
        csv.close();

        return nullptr;
    }
    else
    {
        return indicationMessageHelper->CreateIndicationMessage();
    }
}

void
NoriGnbNetDevice::BuildAndSendReportMessage(E2Termination::RicSubscriptionRequest_rval_s params)
{
    uint16_t cellId = GetCellId();
    std::string plmId = "111";
    std::string gnbId = std::to_string(cellId);

    // TODO here we can get something from RRC and onward
    NS_LOG_DEBUG("MmWaveEnbNetDevice " << cellId << " BuildAndSendMessage at time "
                                       << Simulator::Now().GetSeconds());

    if (m_sendCuUp)
    {
        // Create CU-UP
        Ptr<KpmIndicationHeader> header = BuildRicIndicationHeader(plmId, gnbId, cellId);
        Ptr<KpmIndicationMessage> cuUpMsg = BuildRicIndicationMessageCuUp(plmId);

        // Send CU-UP only if offline logging is disabled
        if (!m_forceE2FileLogging && header != nullptr && cuUpMsg != nullptr)
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
            m_e2term->SendE2Message(pdu_cuup_ue);
            delete pdu_cuup_ue;
        }
    }

    if (m_sendCuCp)
    {
        // Create and send CU-CP
        Ptr<KpmIndicationHeader> header = BuildRicIndicationHeader(plmId, gnbId, cellId);
        Ptr<KpmIndicationMessage> cuCpMsg = BuildRicIndicationMessageCuCp(plmId);

        // Send CU-CP only if offline logging is disabled
        if (!m_forceE2FileLogging && header != nullptr && cuCpMsg != nullptr)
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

    if (m_sendDu)
    {
        // Create DU
        Ptr<KpmIndicationHeader> header = BuildRicIndicationHeader(plmId, gnbId, cellId);
        Ptr<KpmIndicationMessage> duMsg = BuildRicIndicationMessageDu(plmId, cellId);

        // Send DU only if offline logging is disabled
        if (!m_forceE2FileLogging && header != nullptr && duMsg != nullptr)
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

    if (!m_forceE2FileLogging)
    {
        Simulator::ScheduleWithContext(1,
                                       Seconds(m_e2Periodicity),
                                       &NoriGnbNetDevice::BuildAndSendReportMessage,
                                       this,
                                       params);
    }
    else
    {
        Simulator::Schedule(Seconds(m_e2Periodicity),
                            &NoriGnbNetDevice::BuildAndSendReportMessage,
                            this,
                            params);
    }
}

/**
 * \brief Sets the start time for the NoriGnbNetDevice.
 *
 * This function sets the start time for the NoriGnbNetDevice. The start time
 * is used for internal calculations and synchronization purposes.
 *
 * \param st The start time to be set, specified as a uint64_t value.
 */
void
NoriGnbNetDevice::SetStartTime(uint64_t st)
{
    m_startTime = st;
}

} // namespace ns3
