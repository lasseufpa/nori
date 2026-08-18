/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2026 LASSE/UFPA - Universidade Federal do Pará
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "E2-interface.h"

#include "ns3/node.h"

#include "E2-report.h"
#include "kpm-indication.h"
#include "oran-interface.h"

#include "ns3/nori-slicing-helper.h"

#include "ns3/attribute.h"
#include "ns3/bandwidth-part-gnb.h"
#include "ns3/config.h"
#include "ns3/double.h"
#include "ns3/log.h"
#include "ns3/nori-indication-message-helper.h"
#include "ns3/nr-gnb-mac.h"
#include "ns3/nr-gnb-net-device.h"
#include "ns3/nr-gnb-rrc.h"
#include "ns3/nr-mac-sched-sap.h"
#include "ns3/nr-rl-mac-scheduler-ofdma.h"
#include "ns3/nr-rlc-am.h"
#include "ns3/nr-rlc.h"
#include "ns3/nstime.h"
#include "ns3/object-map.h"
#include "ns3/object.h"
#include "ns3/pointer.h"
#include "ns3/string.h"
#include "ns3/type-id.h"
#include "ns3/uinteger.h"

#include <encode_e2apv1.hpp>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("E2Interface");
NS_OBJECT_ENSURE_REGISTERED(E2Interface);

E2Interface::E2Interface()
{
    NS_FATAL_ERROR("E2Interface must be created with a net device");
}

E2Interface::E2Interface(Ptr<NetDevice> netDev)
{
    NS_LOG_FUNCTION(this);
    m_netDev = netDev;
    m_rrc = m_netDev->GetObject<NrGnbNetDevice>()->GetRrc();
    m_e2DuCalculator = CreateObject<NoriE2Report>();
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
                                          DoubleValue(0.01),
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
                gnbNode->GetAttribute("NrGnbRrc", rrc);
                auto rrcPtr = rrc.Get<NrGnbRrc>();
                NS_ASSERT(rrcPtr);
                // Using the current RRC, get the UE map
                ObjectMapValue ueMap;
                rrcPtr->GetAttribute("UeMap", ueMap);
                // Get the ue based on the c-rnti
                NS_LOG_DEBUG("ue C-RNTI:" << rnti);
                auto ueMapObjct = ueMap.Get(rnti);
                auto ue = DynamicCast<NrUeManager>(ueMapObjct);
                auto ueRnti = ue->GetRnti();
                NS_ASSERT(ueRnti == rnti);
                // Use in dB
                m_l3sinrMap[rnti][cellId] = sinrDb;
                NS_LOG_DEBUG("RNTI: " << rnti << " CellID: " << cellId << " SINR: " << sinrDb
                                      << " dB");
            }
        }
        else
        {
            NS_FATAL_ERROR("NetDevice is not a gNB");
        }
    }
}

void
E2Interface::BuildAndSendReportMessage(E2Termination::RicSubscriptionRequest_rval_s params)
{
    NS_LOG_FUNCTION(this);
    NS_LOG_DEBUG("Building and sending report message for nodeB: " << m_netDev);
    auto e2Term = m_netDev->GetObject<E2Termination>();
    NS_ASSERT(e2Term != nullptr);

    // nodeB PLMN ID
    // std::string plmId = "111";
    const std::string& plmId = e2Term->GetPlmnId();

    // Check if the nodeB is a gNB or eNB
    auto gnbNode = DynamicCast<NrGnbNetDevice>(m_netDev);
    NS_ASSERT(gnbNode);
    m_cellId = gnbNode->GetCellId();
    std::string gnbId = std::to_string(m_cellId);
    NS_LOG_DEBUG("PLMN ID: " << plmId << " gNB cell ID: " << gnbId);
    
    // ---- Node-Level Message (Report Style 1, Format 1) ----
    if (params.ricStyleType == 0 || params.ricStyleType == 1)
    {
        auto header = BuildRicIndicationHeader();
        auto nodeMsg = BuildNodeLevelIndicationMessage(plmId, m_cellId);

        if (header != nullptr && nodeMsg != nullptr)
        {
            NS_LOG_DEBUG("Send KPM v3 Node-Level (Style 1, Format 1)");
            auto pdu = new E2AP_PDU;
            encoding::generate_e2apv1_indication_request_parameterized(
                pdu,
                params.requestorId,
                params.instanceId,
                params.ranFuncionId,
                params.actionId,
                1,
                (uint8_t*)header->m_buffer,
                header->m_size,
                (uint8_t*)nodeMsg->m_buffer,
                nodeMsg->m_size);
            e2Term->SendE2Message(pdu);
            // delete pdu;
        }
    }

    // ---- UE-Level Message (Report Style 4, Format 3) ----
    if (params.ricStyleType == 0 || params.ricStyleType == 4)
    {
        auto header = BuildRicIndicationHeader();
        auto ueMsg = BuildUeLevelIndicationMessage(plmId, m_cellId);

        if (header != nullptr && ueMsg != nullptr)
        {
            NS_LOG_DEBUG("Send KPM v3 UE-Level (Style 4, Format 3)");
            auto pdu = new E2AP_PDU;
            encoding::generate_e2apv1_indication_request_parameterized(
                pdu,
                params.requestorId,
                params.instanceId,
                params.ranFuncionId,
                params.actionId,
                2,
                (uint8_t*)header->m_buffer,
                header->m_size,
                (uint8_t*)ueMsg->m_buffer,
                ueMsg->m_size);
            m_e2term->SendE2Message(pdu);
            // delete pdu;
        }
    }

    uint32_t nodeId = m_netDev->GetNode()->GetId();
    Simulator::ScheduleWithContext(nodeId,
                                   Seconds(m_e2Periodicity),
                                   &E2Interface::BuildAndSendReportMessage,
                                   this,
                                   params);
}

void
E2Interface::FunctionServiceSubscriptionCallback(E2AP_PDU_t* sub_req_pdu)
{
    NS_LOG_FUNCTION(this);
    NS_LOG_DEBUG("KPM Subscription Request callback");
    auto e2Term = m_netDev->GetObject<E2Termination>();
    NS_ASSERT(e2Term != nullptr);
    E2Termination::RicSubscriptionRequest_rval_s params =
        e2Term->ProcessRicSubscriptionRequest(sub_req_pdu);
    NS_LOG_INFO("Subscription accepted — requestorId " << +params.requestorId
                << ", instanceId " << +params.instanceId
                << ", ranFuncionId " << +params.ranFuncionId
                << ", actionId " << +params.actionId
                << ", ricStyleType " << params.ricStyleType);

    BuildAndSendReportMessage(params);
}

void
E2Interface::ControlMessageReceivedCallback([[maybe_unused]] E2AP_PDU_t* sub_req_pdu)
{
    NS_LOG_WARN("Received RIC Control Message, but E2SM-RC is disabled in current build.");
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
E2Interface ::BuildRicIndicationHeader() const
{
    KpmIndicationHeader::KpmRicIndicationHeaderValues headerValues;
    auto time = Simulator::Now();
    uint64_t timestamp = m_startTime + (uint64_t)time.GetMilliSeconds();
    NS_LOG_DEBUG("Timestamp " << timestamp);
    headerValues.m_timestamp = timestamp;

    Ptr<KpmIndicationHeader> header =
        Create<KpmIndicationHeader>(headerValues);
    return header;

}

Ptr<KpmIndicationMessage> E2Interface::BuildNodeLevelIndicationMessage(std::string plmId, uint16_t nrCellId){
    
    auto helper = Create<NoriIndicationMessageHelper>(IndicationMessageHelper::IndicationMessageType::NodeLevel, false, false);

    helper->SetGranularityPeriod((unsigned long)(m_e2Periodicity * 1000));
    ObjectMapValue ueManager;
    m_rrc->GetAttribute("UeMap", ueManager);

    // --- Aggregate cell-level DU metrics
    uint32_t macPduCellSpecific = 0;
    uint32_t macPduInitialCellSpecific = 0;
    uint32_t macQpskCellSpecific = 0;
    uint32_t mac16QamCellSpecific = 0;
    uint32_t mac64QamCellSpecific = 0;
    uint32_t macRetxCellSpecific = 0;
    uint32_t macVolumeCellSpecific = 0;
    uint32_t macPrbsCellSpecific = 0;

    uint32_t macSinrBin1Cell = 0, macSinrBin2Cell = 0, macSinrBin3Cell = 0,
             macSinrBin4Cell = 0, macSinrBin5Cell = 0, macSinrBin6Cell = 0,
             macSinrBin7Cell = 0;

    uint32_t macMac04Cell = 0, macMac59Cell = 0, macMac1014Cell = 0,
             macMac1519Cell = 0, macMac2024Cell = 0, macMac2529Cell = 0;

    uint32_t rlcBufferOccupCell = 0;
    double cellDlTxVolume = 0;

    m_cellId = nrCellId;

    for (auto ueMap = ueManager.Begin(); ueMap != ueManager.End(); ueMap++)
    {
        auto ue = DynamicCast<NrUeManager>(ueMap->second);
        uint64_t imsi = ue->GetImsi();
        uint16_t rnti = ue->GetRnti();

        // --- CU-UP: aggregate PDCP volume ---
        double actualTotalTxBytes = m_e2PdcpStatsCalculator->GetDlTxData(imsi, 4) * (8 / 1e3);
        if (m_cellTxBytes.find(imsi) == m_cellTxBytes.end())
            m_cellTxBytes.insert(std::make_pair(imsi, 0));
        double txBytes = actualTotalTxBytes - m_cellTxBytes[imsi];
        m_cellTxBytes[imsi] += txBytes;
        cellDlTxVolume += txBytes;

        // --- TX PDU counting reset for this frame ---
        m_txPDU[rnti] = 0;
        m_txPDUBytes[rnti] = 0;

        // --- DU: aggregate per-UE MAC stats ---
        macPduCellSpecific += m_e2DuCalculator->GetMacPduUeSpecific(rnti, m_cellId);
        macPduInitialCellSpecific += m_e2DuCalculator->GetMacPduInitialTransmissionUeSpecific(rnti, m_cellId);
        macVolumeCellSpecific += m_e2DuCalculator->GetMacVolumeUeSpecific(rnti, m_cellId);
        macQpskCellSpecific += m_e2DuCalculator->GetMacPduQpskUeSpecific(rnti, m_cellId);
        mac16QamCellSpecific += m_e2DuCalculator->GetMacPdu16QamUeSpecific(rnti, m_cellId);
        mac64QamCellSpecific += m_e2DuCalculator->GetMacPdu64QamUeSpecific(rnti, m_cellId);
        macRetxCellSpecific += m_e2DuCalculator->GetMacPduRetransmissionUeSpecific(rnti, m_cellId);

        double macSymbols = m_e2DuCalculator->GetMacNumberOfSymbolsUeSpecific(rnti, m_cellId);
        auto slotPeriod = DynamicCast<NrGnbNetDevice>(m_netDev)->GetPhy(0)->GetSlotPeriod();
        Time reportingWindow = Simulator::Now() - m_e2DuCalculator->GetLastResetTime(rnti, m_cellId);
        double denomPrb = std::ceil(reportingWindow.GetNanoSeconds() / slotPeriod.GetNanoSeconds()) * 14;
        double macPrb = (denomPrb != 0) ? macSymbols / denomPrb * 139 : 0;
        macPrbsCellSpecific += macPrb;

        macMac04Cell   += m_e2DuCalculator->GetMacMcs04UeSpecific(rnti, m_cellId);
        macMac59Cell   += m_e2DuCalculator->GetMacMcs59UeSpecific(rnti, m_cellId);
        macMac1014Cell += m_e2DuCalculator->GetMacMcs1014UeSpecific(rnti, m_cellId);
        macMac1519Cell += m_e2DuCalculator->GetMacMcs1519UeSpecific(rnti, m_cellId);
        macMac2024Cell += m_e2DuCalculator->GetMacMcs2024UeSpecific(rnti, m_cellId);
        macMac2529Cell += m_e2DuCalculator->GetMacMcs2529UeSpecific(rnti, m_cellId);

        macSinrBin1Cell += m_e2DuCalculator->GetMacSinrBin1UeSpecific(rnti, m_cellId);
        macSinrBin2Cell += m_e2DuCalculator->GetMacSinrBin2UeSpecific(rnti, m_cellId);
        macSinrBin3Cell += m_e2DuCalculator->GetMacSinrBin3UeSpecific(rnti, m_cellId);
        macSinrBin4Cell += m_e2DuCalculator->GetMacSinrBin4UeSpecific(rnti, m_cellId);
        macSinrBin5Cell += m_e2DuCalculator->GetMacSinrBin5UeSpecific(rnti, m_cellId);
        macSinrBin6Cell += m_e2DuCalculator->GetMacSinrBin6UeSpecific(rnti, m_cellId);
        macSinrBin7Cell += m_e2DuCalculator->GetMacSinrBin7UeSpecific(rnti, m_cellId);
    }

    // TODO: make configurable
    long dlAvailablePrbs = 139;
    long ulAvailablePrbs = 139;

    // ===== Add all node-level metrics =====
    helper->AddNodeMeasurementInteger("DRB.PdcpSduVolumeDL", (unsigned long)cellDlTxVolume);
    helper->AddNodeMeasurementInteger("RRU.PrbUsedDl", (unsigned long)std::ceil(macPrbsCellSpecific));
    helper->AddNodeMeasurementInteger("RRU.PrbAvailDl", (unsigned long)dlAvailablePrbs);
    helper->AddNodeMeasurementInteger("RRU.PrbAvailUl", (unsigned long)ulAvailablePrbs);
    helper->AddNodeMeasurementInteger("TB.TotNbrDlInitial", macPduInitialCellSpecific);
    helper->AddNodeMeasurementInteger("TB.TotNbrDlInitial.Qpsk", macQpskCellSpecific);
    helper->AddNodeMeasurementInteger("TB.TotNbrDlInitial.16Qam", mac16QamCellSpecific);
    helper->AddNodeMeasurementInteger("TB.TotNbrDlInitial.64Qam", mac64QamCellSpecific);
    helper->AddNodeMeasurementInteger("TB.ErrTotalNbrDl.1", macRetxCellSpecific);
    helper->AddNodeMeasurementInteger("QosFlow.PdcpPduVolumeDL_Filter", macVolumeCellSpecific);
    helper->AddNodeMeasurementInteger("DRB.MeanActiveUeDl", ueManager.GetN());
    helper->AddNodeMeasurementInteger("RRC.ConnMean", ueManager.GetN());

    // MCS distribution
    helper->AddNodeMeasurementInteger("CARR.PDSCHMCSDist.Bin1", macMac04Cell);
    helper->AddNodeMeasurementInteger("CARR.PDSCHMCSDist.Bin2", macMac59Cell);
    helper->AddNodeMeasurementInteger("CARR.PDSCHMCSDist.Bin3", macMac1014Cell);
    helper->AddNodeMeasurementInteger("CARR.PDSCHMCSDist.Bin4", macMac1519Cell);
    helper->AddNodeMeasurementInteger("CARR.PDSCHMCSDist.Bin5", macMac2024Cell);
    helper->AddNodeMeasurementInteger("CARR.PDSCHMCSDist.Bin6", macMac2529Cell);

    // SINR distribution
    helper->AddNodeMeasurementInteger("L1M.RS-SINR.Bin34", macSinrBin1Cell);
    helper->AddNodeMeasurementInteger("L1M.RS-SINR.Bin46", macSinrBin2Cell);
    helper->AddNodeMeasurementInteger("L1M.RS-SINR.Bin58", macSinrBin3Cell);
    helper->AddNodeMeasurementInteger("L1M.RS-SINR.Bin70", macSinrBin4Cell);
    helper->AddNodeMeasurementInteger("L1M.RS-SINR.Bin82", macSinrBin5Cell);
    helper->AddNodeMeasurementInteger("L1M.RS-SINR.Bin94", macSinrBin6Cell);
    helper->AddNodeMeasurementInteger("L1M.RS-SINR.Bin127", macSinrBin7Cell);

    helper->AddNodeMeasurementInteger("DRB.BufferSize.Qos", rlcBufferOccupCell);

    NS_LOG_INFO(Simulator::Now().GetSeconds() << " " << m_cellId
                << " cell, node-level message with " << ueManager.GetN() << " UEs");

    return helper->CreateIndicationMessage();
}

Ptr<KpmIndicationMessage>E2Interface::BuildUeLevelIndicationMessage(std::string plmId, uint16_t nrCellId){

    auto helper = Create<NoriIndicationMessageHelper>(IndicationMessageHelper::IndicationMessageType::UeLevel, false, false);

    ObjectMapValue ueManager;
    m_rrc->GetAttribute("UeMap", ueManager);

    m_cellId = nrCellId;

    for (auto ueObject = ueManager.Begin(); ueObject != ueManager.End(); ueObject++)
    {
        auto ue = DynamicCast<NrUeManager>(ueObject->second);
        uint64_t imsi = ue->GetImsi();
        uint16_t rnti = ue->GetRnti();
        uint8_t sst = NoriSlicingHelper::GetSstForRnti(rnti);

        // Begin a new UE report (IMSI as AMF-UE-NGAP-ID for simulation)
        helper->BeginUeReport(imsi, plmId, 0, 0, 0);

        // --- CU-UP per-UE: PDCP throughput ---
        double actualTotalTxBytes = m_e2PdcpStatsCalculator->GetDlTxData(imsi, 4) * (8 / 1e3);
        if (m_cellTxBytes.find(imsi) == m_cellTxBytes.end())
            m_cellTxBytes.insert(std::make_pair(imsi, 0));
        double txBytes = actualTotalTxBytes - m_cellTxBytes[imsi];
        // NOTE: don't update m_cellTxBytes here (already done in NodeLevel)
        double pdcpThroughput = txBytes / m_e2Periodicity; // kbps

        helper->AddUeMeasurementReal("DRB.UEThpDl", pdcpThroughput);
        helper->AddUeMeasurementInteger("QosFlow.PdcpPduVolumeDL_Filter", (unsigned long)txBytes);

        // --- DU per-UE: MAC stats ---
        uint32_t macPduUe = m_e2DuCalculator->GetMacPduUeSpecific(rnti, m_cellId);
        uint32_t macPduInitialUe = m_e2DuCalculator->GetMacPduInitialTransmissionUeSpecific(rnti, m_cellId);
        uint32_t macQpsk = m_e2DuCalculator->GetMacPduQpskUeSpecific(rnti, m_cellId);
        uint32_t mac16Qam = m_e2DuCalculator->GetMacPdu16QamUeSpecific(rnti, m_cellId);
        uint32_t mac64Qam = m_e2DuCalculator->GetMacPdu64QamUeSpecific(rnti, m_cellId);
        uint32_t macRetx = m_e2DuCalculator->GetMacPduRetransmissionUeSpecific(rnti, m_cellId);

        double macSymbols = m_e2DuCalculator->GetMacNumberOfSymbolsUeSpecific(rnti, m_cellId);
        auto slotPeriod = DynamicCast<NrGnbNetDevice>(m_netDev)->GetPhy(0)->GetSlotPeriod();
        Time reportingWindow = Simulator::Now() - m_e2DuCalculator->GetLastResetTime(rnti, m_cellId);
        double denomPrb = std::ceil(reportingWindow.GetNanoSeconds() / slotPeriod.GetNanoSeconds()) * 14;
        double macPrb = (denomPrb != 0) ? macSymbols / denomPrb * 139 : 0;

        helper->AddUeMeasurementInteger("TB.TotNbrDl.1", macPduUe);
        helper->AddUeMeasurementInteger("TB.TotNbrDlInitial", macPduInitialUe);
        helper->AddUeMeasurementInteger("TB.TotNbrDlInitial.Qpsk", macQpsk);
        helper->AddUeMeasurementInteger("TB.TotNbrDlInitial.16Qam", mac16Qam);
        helper->AddUeMeasurementInteger("TB.TotNbrDlInitial.64Qam", mac64Qam);
        helper->AddUeMeasurementInteger("TB.ErrTotalNbrDl.1", macRetx);
        helper->AddUeMeasurementInteger("RRU.PrbUsedDl", (unsigned long)std::ceil(macPrb));

        // MCS distribution per UE
        helper->AddUeMeasurementInteger("CARR.PDSCHMCSDist.Bin1",
                                        m_e2DuCalculator->GetMacMcs04UeSpecific(rnti, m_cellId));
        helper->AddUeMeasurementInteger("CARR.PDSCHMCSDist.Bin2",
                                        m_e2DuCalculator->GetMacMcs59UeSpecific(rnti, m_cellId));
        helper->AddUeMeasurementInteger("CARR.PDSCHMCSDist.Bin3",
                                        m_e2DuCalculator->GetMacMcs1014UeSpecific(rnti, m_cellId));
        helper->AddUeMeasurementInteger("CARR.PDSCHMCSDist.Bin4",
                                        m_e2DuCalculator->GetMacMcs1519UeSpecific(rnti, m_cellId));
        helper->AddUeMeasurementInteger("CARR.PDSCHMCSDist.Bin5",
                                        m_e2DuCalculator->GetMacMcs2024UeSpecific(rnti, m_cellId));
        helper->AddUeMeasurementInteger("CARR.PDSCHMCSDist.Bin6",
                                        m_e2DuCalculator->GetMacMcs2529UeSpecific(rnti, m_cellId));

        // SINR bins per UE
        helper->AddUeMeasurementInteger("L1M.RS-SINR.Bin34",
                                        m_e2DuCalculator->GetMacSinrBin1UeSpecific(rnti, m_cellId));
        helper->AddUeMeasurementInteger("L1M.RS-SINR.Bin46",
                                        m_e2DuCalculator->GetMacSinrBin2UeSpecific(rnti, m_cellId));
        helper->AddUeMeasurementInteger("L1M.RS-SINR.Bin58",
                                        m_e2DuCalculator->GetMacSinrBin3UeSpecific(rnti, m_cellId));
        helper->AddUeMeasurementInteger("L1M.RS-SINR.Bin70",
                                        m_e2DuCalculator->GetMacSinrBin4UeSpecific(rnti, m_cellId));
        helper->AddUeMeasurementInteger("L1M.RS-SINR.Bin82",
                                        m_e2DuCalculator->GetMacSinrBin5UeSpecific(rnti, m_cellId));
        helper->AddUeMeasurementInteger("L1M.RS-SINR.Bin94",
                                        m_e2DuCalculator->GetMacSinrBin6UeSpecific(rnti, m_cellId));
        helper->AddUeMeasurementInteger("L1M.RS-SINR.Bin127",
                                        m_e2DuCalculator->GetMacSinrBin7UeSpecific(rnti, m_cellId));

        // Buffer + slice
        helper->AddUeMeasurementInteger("DRB.BufferSize.Qos", 0); // TODO: implement RLC buffer
        helper->AddUeMeasurementInteger("DRB.NetworkSlicing.SST", (unsigned long)sst);

        // --- CU-CP per-UE: L3 SINR serving ---
        double sinrThisCell = m_l3sinrMap[rnti][m_cellId];
        double convertedSinr = L3RrcMeasurements::ThreeGppMapSinr(sinrThisCell);
        helper->AddUeMeasurementReal("HO.SrcCellQual.RS-SINR", convertedSinr);

        // --- CU-CP per-UE: num DRBs ---
        ObjectMapValue drbMap;
        ue->GetAttribute("DataRadioBearerMap", drbMap);
        helper->AddUeMeasurementInteger("DRB.EstabSucc.5QI", drbMap.GetN());

        // ML Slice Interface
        MLSliceInterface(macPrb, imsi);

        // Reset PHY traces (only here, last function called)
        m_e2DuCalculator->ResetPhyTracesForRntiCellId(rnti, m_cellId);
    }

    m_drbThrDlPdcpBasedComputationUeid.clear();
    m_drbThrDlUeid.clear();

    return helper->CreateIndicationMessage();
}

void
E2Interface::MLSliceInterface(double macPrb, uint64_t imsi)
{
    NS_LOG_FUNCTION(this);

    std::ofstream csv;
    std::string fileName = "ml_slice_interface.csv";
    csv.open(fileName, std::ios_base::app);
    if (!csv.is_open())
    {
        NS_FATAL_ERROR("Can't open file " << fileName);
    }

    // Check if the file is empty to write the header
    csv.seekp(0, std::ios::end);
    if (csv.tellp() == 0)
    {
        csv << "timestamp,imsi,dlThroughput,ulThroughput,spectralEfficiency\n";
    }

    double currentTime = Simulator::Now().GetMilliSeconds();
    double deltatime = currentTime - m_previousTime[imsi];

    double currentDlTxData = m_e2PdcpStatsCalculator->GetDlTxData(imsi, 4);
    double dlThroughput = (currentDlTxData - m_previousDlTxData[imsi]) * 8 / deltatime;
    m_previousDlTxData[imsi] = currentDlTxData;
    double currentUlTxData = m_e2PdcpStatsCalculator->GetUlTxData(imsi, 4);
    double ulThroughput = (currentUlTxData - m_previousUlTxData[imsi]) * 8 / deltatime;
    m_previousUlTxData[imsi] = currentUlTxData;
    m_previousTime[imsi] = currentTime;
    double spectralEfficiency = 0.0;
    if (m_e2DuCalculator)
    {
        if (macPrb > 0)
        {
            spectralEfficiency = dlThroughput / (macPrb * 720000.0);
        }
    }
    uint64_t timestamp = m_startTime + (uint64_t)Simulator::Now().GetMilliSeconds();
    csv << timestamp << "," << imsi << "," << dlThroughput << "," << ulThroughput << ","
        << spectralEfficiency << "\n";

    csv.close();
}

Ptr<NoriE2Report>
E2Interface::GetE2DuCalculator()
{
    return m_e2DuCalculator;
}

} // namespace ns3
