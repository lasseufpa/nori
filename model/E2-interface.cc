/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2022 Northeastern University
 * Copyright (c) 2022 Sapienza, University of Rome
 * Copyright (c) 2022 University of Padova
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "E2-interface.h"

#include "E2-report.h"
#include "kpm-indication.h"
#include "oran-interface.h"

#include "ns3/attribute.h"
#include "ns3/bandwidth-part-gnb.h"
#include "ns3/config.h"
#include "ns3/double.h"
#include "ns3/log.h"
#include "ns3/mmwave-indication-message-helper.h"
#include "ns3/nr-gnb-mac.h"
#include "ns3/nr-gnb-net-device.h"
#include "ns3/nr-gnb-rrc.h"
#include "ns3/nr-mac-sched-sap.h"
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
        if (gnbNode)
        {
            if (id == cellId)
            {
                m_cellId = id;
                PointerValue rrc;
                gnbNode->GetAttribute("NrGnbRrc", rrc);
                auto rrcPtr = rrc.Get<NrGnbRrc>();
                NS_ASSERT(rrcPtr);
                ObjectMapValue ueMap;
                rrcPtr->GetAttribute("UeMap", ueMap);
                auto ueMapObjct = ueMap.Get(rnti);
                auto ue = DynamicCast<NrUeManager>(ueMapObjct);
                auto ueRnti = ue->GetRnti();
                NS_ASSERT(ueRnti == rnti);
                m_l3sinrMap[rnti][cellId] = sinrDb;
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

    std::string plmId = "111";
    auto gnbNode = DynamicCast<NrGnbNetDevice>(m_netDev);
    NS_ASSERT(gnbNode);
    m_cellId = gnbNode->GetCellId();
    std::string gnbId = std::to_string(m_cellId);

    //
    MmWaveIndicationMessageHelper helper(false, false);

    ObjectMapValue ueManager;
    m_rrc->GetAttribute("UeMap", ueManager);

    //
    uint32_t macPduCellSpecific = 0, macPduInitialCellSpecific = 0, macVolumeCellSpecific = 0;
    uint32_t macQpskCellSpecific = 0, mac16QamCellSpecific = 0, mac64QamCellSpecific = 0, macRetxCellSpecific = 0;
    uint32_t macMac04CellSpecific=0, macMac59CellSpecific=0, macMac1014CellSpecific=0, macMac1519CellSpecific=0, macMac2024CellSpecific=0, macMac2529CellSpecific=0;
    uint32_t macSinrBin1CellSpecific=0, macSinrBin2CellSpecific=0, macSinrBin3CellSpecific=0, macSinrBin4CellSpecific=0, macSinrBin5CellSpecific=0, macSinrBin6CellSpecific=0, macSinrBin7CellSpecific=0;
    double macPrbsCellSpecific = 0;

    //
    for (auto ueMap = ueManager.Begin(); ueMap != ueManager.End(); ueMap++)
    {
        auto ue = DynamicCast<NrUeManager>(ueMap->second);
        uint64_t imsi = ue->GetImsi();
        std::string ueImsiComplete = GetImsiString(imsi);
        uint16_t rnti = ue->GetRnti();

        // 
        ObjectMapValue drbMap;
        ue->GetAttribute("DataRadioBearerMap", drbMap);
        long numDrb = drbMap.GetN();
        helper.AddCuCpUePmItem(ueImsiComplete, numDrb, 0);

        //OCU-up
        long txDlPackets = m_e2PdcpStatsCalculator->GetDlTxPackets(imsi, 4) - m_cellTxDlPackets;
        m_cellTxDlPackets += txDlPackets;
        double actualTotalTxBytes = m_e2PdcpStatsCalculator->GetDlTxData(imsi, 4) * (8 / 1e3);
        if (m_cellTxBytes.find(imsi) == m_cellTxBytes.end()) m_cellTxBytes[imsi] = 0;
        double txBytes = (actualTotalTxBytes - m_cellTxBytes[imsi]);
        m_cellTxBytes[imsi] += txBytes;
        
        //OCU-cp
        long txPdcpPduNrRlc = m_txPDU[rnti];
        double txPdcpPduBytesNrRlc = m_txPDUBytes[rnti] * 8 / 1e3;
        m_txPDU[rnti] = 0; m_txPDUBytes[rnti] = 0;
        double pdcpThroughput = txBytes / m_e2Periodicity;
        
        helper.AddCuUpUePmItem(ueImsiComplete, plmId, txPdcpPduBytesNrRlc, txPdcpPduNrRlc, pdcpThroughput);

        // DU
        uint32_t macPduUe = m_e2DuCalculator->GetMacPduUeSpecific(rnti, m_cellId); macPduCellSpecific += macPduUe;
        uint32_t macPduInitialUe = m_e2DuCalculator->GetMacPduInitialTransmissionUeSpecific(rnti, m_cellId); macPduInitialCellSpecific += macPduInitialUe;
        uint32_t macVolume = m_e2DuCalculator->GetMacVolumeUeSpecific(rnti, m_cellId); macVolumeCellSpecific += macVolume;
        uint32_t macQpsk = m_e2DuCalculator->GetMacPduQpskUeSpecific(rnti, m_cellId); macQpskCellSpecific += macQpsk;
        uint32_t mac16Qam = m_e2DuCalculator->GetMacPdu16QamUeSpecific(rnti, m_cellId); mac16QamCellSpecific += mac16Qam;
        uint32_t mac64Qam = m_e2DuCalculator->GetMacPdu64QamUeSpecific(rnti, m_cellId); mac64QamCellSpecific += mac64Qam;
        uint32_t macRetx = m_e2DuCalculator->GetMacPduRetransmissionUeSpecific(rnti, m_cellId); macRetxCellSpecific += macRetx;

        double macNumberOfSymbols = m_e2DuCalculator->GetMacNumberOfSymbolsUeSpecific(rnti, m_cellId);
        auto slotPeriod = DynamicCast<NrGnbNetDevice>(m_netDev)->GetPhy(0)->GetSlotPeriod();
        Time reportingWindow = Simulator::Now() - m_e2DuCalculator->GetLastResetTime(rnti, m_cellId);
        double denominatorPrb = std::ceil(reportingWindow.GetNanoSeconds() / slotPeriod.GetNanoSeconds()) * 14;
        double macPrb = (denominatorPrb != 0) ? (macNumberOfSymbols / denominatorPrb * 139) : 0;
        macPrbsCellSpecific += macPrb;

        uint32_t macMac04 = m_e2DuCalculator->GetMacMcs04UeSpecific(rnti, m_cellId); macMac04CellSpecific += macMac04;
        uint32_t macMac59 = m_e2DuCalculator->GetMacMcs59UeSpecific(rnti, m_cellId); macMac59CellSpecific += macMac59;
        uint32_t macMac1014 = m_e2DuCalculator->GetMacMcs1014UeSpecific(rnti, m_cellId); macMac1014CellSpecific += macMac1014;
        uint32_t macMac1519 = m_e2DuCalculator->GetMacMcs1519UeSpecific(rnti, m_cellId); macMac1519CellSpecific += macMac1519;
        uint32_t macMac2024 = m_e2DuCalculator->GetMacMcs2024UeSpecific(rnti, m_cellId); macMac2024CellSpecific += macMac2024;
        uint32_t macMac2529 = m_e2DuCalculator->GetMacMcs2529UeSpecific(rnti, m_cellId); macMac2529CellSpecific += macMac2529;

        uint32_t macSinrBin1 = m_e2DuCalculator->GetMacSinrBin1UeSpecific(rnti, m_cellId); macSinrBin1CellSpecific += macSinrBin1;
        uint32_t macSinrBin2 = m_e2DuCalculator->GetMacSinrBin2UeSpecific(rnti, m_cellId); macSinrBin2CellSpecific += macSinrBin2;
        uint32_t macSinrBin3 = m_e2DuCalculator->GetMacSinrBin3UeSpecific(rnti, m_cellId); macSinrBin3CellSpecific += macSinrBin3;
        uint32_t macSinrBin4 = m_e2DuCalculator->GetMacSinrBin4UeSpecific(rnti, m_cellId); macSinrBin4CellSpecific += macSinrBin4;
        uint32_t macSinrBin5 = m_e2DuCalculator->GetMacSinrBin5UeSpecific(rnti, m_cellId); macSinrBin5CellSpecific += macSinrBin5;
        uint32_t macSinrBin6 = m_e2DuCalculator->GetMacSinrBin6UeSpecific(rnti, m_cellId); macSinrBin6CellSpecific += macSinrBin6;
        uint32_t macSinrBin7 = m_e2DuCalculator->GetMacSinrBin7UeSpecific(rnti, m_cellId); macSinrBin7CellSpecific += macSinrBin7;

        double rlcLatency = m_e2RlcStatsCalculator->GetDlDelay(imsi, 4) / 1e9;
        double pduStats = m_e2RlcStatsCalculator->GetDlPduSizeStats(imsi, 4)[0] * 8.0 / 1e3;
        double rlcBitrate = (rlcLatency == 0) ? 0 : pduStats / rlcLatency;

        helper.AddDuUePmItem(ueImsiComplete, m_cellId, macPduUe, macPduInitialUe, macQpsk, mac16Qam, mac64Qam, macRetx, 
                             macPrb, macMac04, macMac59, macMac1014, macMac1519, macMac2024, macMac2529,
                             macSinrBin1, macSinrBin2, macSinrBin3, macSinrBin4, macSinrBin5, macSinrBin6, macSinrBin7,
                             0, rlcBitrate); // 0 = rlcBufferOccupancy (stubbed)

        MLSliceInterface(macPrb, imsi);
        m_e2DuCalculator->ResetPhyTracesForRntiCellId(rnti, m_cellId);
    }

    // 3. Adiciona Métricas de Nível de Célula
    helper.AddDuCellPmItem(m_cellId, macPrbsCellSpecific, ueManager.GetN());

    // 4. Cria e Envia a Mensagem (Um único relatório para tudo!)
    Ptr<KpmIndicationMessage> indMsg = helper.CreateIndicationMessage();
    Ptr<KpmIndicationHeader> indHeader = BuildRicIndicationHeader(plmId, gnbId, m_cellId);

    if (indHeader != nullptr && indMsg != nullptr)
    {
        NS_LOG_DEBUG("Sending Unificated NR E2SM-KPM Indication Message");
        auto pdu = new E2AP_PDU;
        encoding::generate_e2apv1_indication_request_parameterized(
            pdu, params.requestorId, params.instanceId, params.ranFuncionId, params.actionId,
            1, // sequence number
            (uint8_t*)indHeader->m_buffer, indHeader->m_size,
            (uint8_t*)indMsg->m_buffer, indMsg->m_size);
            
        e2Term->SendE2Message(pdu);
        delete pdu;
    }

    Simulator::ScheduleWithContext(1, Seconds(m_e2Periodicity), &E2Interface::BuildAndSendReportMessage, this, params);
}

void
E2Interface::FunctionServiceSubscriptionCallback(E2AP_PDU_t* sub_req_pdu)
{
    NS_LOG_FUNCTION(this);
    E2Termination::RicSubscriptionRequest_rval_s params = m_e2term->ProcessRicSubscriptionRequest(sub_req_pdu);
    
    static bool isFirsReportMessage = true;
    if (isFirsReportMessage)
    {
        BuildAndSendReportMessage(params);
        isFirsReportMessage = false;
    }
}

void
E2Interface::SetE2PdcpStatsCalculator(Ptr<NrBearerStatsCalculator> e2PdcpStatsCalculator)
{
    m_e2PdcpStatsCalculator = e2PdcpStatsCalculator;
}

void
E2Interface::SetE2RlcStatsCalculator(Ptr<NrBearerStatsCalculator> e2RlcStatsCalculator)
{
    m_e2RlcStatsCalculator = e2RlcStatsCalculator;
}

std::string
E2Interface::GetImsiString(uint64_t imsi)
{
    std::string ueImsi = std::to_string(imsi);
    if (ueImsi.length() == 1) return "0000" + ueImsi;
    if (ueImsi.length() == 2) return "000" + ueImsi;
    return "00" + ueImsi;
}

void
E2Interface::ReportTxPDU(uint16_t rnti, uint8_t lcid, uint32_t packetSize)
{
    if (m_txPDU.find(rnti) == m_txPDU.end()) m_txPDU.insert(std::make_pair(rnti, 1));
    else m_txPDU[rnti] += 1;

    if (m_txPDUBytes.find(rnti) == m_txPDUBytes.end()) m_txPDUBytes.insert(std::make_pair(rnti, packetSize));
    else m_txPDUBytes[rnti] += packetSize;
}

std::multimap<long double, uint16_t>
E2Interface::FlipMap(const std::map<uint16_t, long double>& src)
{
    std::multimap<long double, uint16_t> dst;
    std::transform(src.begin(), src.end(), std::inserter(dst, dst.begin()),
                   [](const std::pair<uint16_t, long double>& p) { return std::make_pair(p.second, p.first); });
    return dst;
}

Ptr<KpmIndicationHeader>
E2Interface::BuildRicIndicationHeader(std::string plmId, std::string gnbId, uint16_t nrCellId) const
{
    // No KPM v3.00, IDs como plmId e nrCellId vão nas Labels da Mensagem. O Header é mais simples.
    KpmIndicationHeader::KpmRicIndicationHeaderValues headerValues;
    headerValues.m_timestamp = m_startTime + (uint64_t)Simulator::Now().GetMilliSeconds();
    headerValues.m_senderName = "O-RAN-gNB";
    headerValues.m_senderType = "gNB";

    return Create<KpmIndicationHeader>(headerValues);
}

void
E2Interface::MLSliceInterface(double macPrb, uint64_t imsi)
{
    NS_LOG_FUNCTION(this);
    std::ofstream csv;
    std::string fileName = "ml_slice_interface.csv";
    csv.open(fileName, std::ios_base::app);
    if (!csv.is_open()) NS_FATAL_ERROR("Can't open file " << fileName);

    csv.seekp(0, std::ios::end);
    if (csv.tellp() == 0) csv << "timestamp,imsi,dlThroughput,ulThroughput,spectralEfficiency\n";

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
    if (m_e2DuCalculator && macPrb > 0) spectralEfficiency = dlThroughput / (macPrb * 720000.0);
    
    uint64_t timestamp = m_startTime + (uint64_t)Simulator::Now().GetMilliSeconds();
    csv << timestamp << "," << imsi << "," << dlThroughput << "," << ulThroughput << "," << spectralEfficiency << "\n";
    csv.close();
}

Ptr<NoriE2Report>
E2Interface::GetE2DuCalculator()
{
    return m_e2DuCalculator;
}

} // namespace ns3