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
#include "ric-control-message.h"

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
#include "ns3/nr-radio-bearer-info.h"
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
#include "ccc-data-models.h"
#include "ccc-control-message.h"
#include "ccc-indication-message.h"
#include "ns3/node-list.h"
#include "ns3/nr-ue-net-device.h"
#include "ns3/nr-ue-phy.h"
#include "ns3/nr-pm-search.h"

#include <encode_e2apv1.hpp>

extern "C"
{
#include "Cause.h"
#include "CauseRICrequest.h"
#include "E2SM-KPM-ActionDefinition-Format1.h"
#include "E2SM-KPM-ActionDefinition-Format2.h"
#include "E2SM-KPM-ActionDefinition-Format3.h"
#include "E2SM-KPM-ActionDefinition-Format4.h"
#include "E2SM-KPM-ActionDefinition-Format5.h"
#include "E2SM-KPM-ActionDefinition.h"
#include "E2SM-KPM-EventTriggerDefinition-Format1.h"
#include "E2SM-KPM-EventTriggerDefinition.h"
#include "InitiatingMessage.h"
#include "ProtocolIE-Field.h"
#include "RICcontrolFailure.h"
#include "RICsubscriptionRequest.h"
#include "UnsuccessfulOutcome.h"
}
namespace ns3
{

NS_LOG_COMPONENT_DEFINE("E2Interface");
NS_OBJECT_ENSURE_REGISTERED(E2Interface);

namespace
{
/**
 * Return the increment of a cumulative counter since the previous report and
 * advance the stored reference. A current value smaller than the reference
 * means the statistics calculator was reset (or a bearer was released); the
 * current value is then the whole increment.
 */
double
ConsumeCounterDelta(double current, double& previous)
{
    const double delta = current >= previous ? current - previous : current;
    previous = current;
    return delta;
}

/// Clamp an ASN.1 INTEGER (1..4294967295) to uint32_t; 0 means "not present"
uint32_t
ToPeriodMs(unsigned long value)
{
    return value > UINT32_MAX ? UINT32_MAX : static_cast<uint32_t>(value);
}

/**
 * Decode the E2SM-KPM Event Trigger Definition (Format 1) of a subscription and
 * return its reportingPeriod in ms, or 0 if it cannot be decoded.
 */
uint32_t
DecodeKpmReportingPeriodMs(const RICeventTriggerDefinition_t& trigger)
{
    if (trigger.buf == nullptr || trigger.size == 0)
    {
        NS_LOG_WARN("KPM subscription without Event Trigger Definition");
        return 0;
    }

    E2SM_KPM_EventTriggerDefinition_t* eventTrigger = nullptr;
    asn_dec_rval_t rval = asn_decode(nullptr,
                                     ATS_ALIGNED_BASIC_PER,
                                     &asn_DEF_E2SM_KPM_EventTriggerDefinition,
                                     (void**)&eventTrigger,
                                     trigger.buf,
                                     trigger.size);
    uint32_t periodMs = 0;
    if (rval.code == RC_OK && eventTrigger != nullptr &&
        eventTrigger->eventDefinition_formats.present ==
            E2SM_KPM_EventTriggerDefinition__eventDefinition_formats_PR_eventDefinition_Format1 &&
        eventTrigger->eventDefinition_formats.choice.eventDefinition_Format1 != nullptr)
    {
        periodMs = ToPeriodMs(
            eventTrigger->eventDefinition_formats.choice.eventDefinition_Format1->reportingPeriod);
    }
    else
    {
        NS_LOG_WARN("Failed to decode E2SM-KPM Event Trigger Definition Format 1");
    }

    if (eventTrigger != nullptr)
    {
        ASN_STRUCT_FREE(asn_DEF_E2SM_KPM_EventTriggerDefinition, eventTrigger);
    }
    return periodMs;
}

/**
 * Decode the E2SM-KPM Action Definition (Formats 1 to 5) of a subscription action
 * and return its granulPeriod in ms, or 0 if it cannot be decoded.
 */
uint32_t
DecodeKpmGranularityPeriodMs(const RICactionDefinition_t& definition)
{
    if (definition.buf == nullptr || definition.size == 0)
    {
        NS_LOG_WARN("KPM action without Action Definition");
        return 0;
    }

    E2SM_KPM_ActionDefinition_t* action = nullptr;
    asn_dec_rval_t rval = asn_decode(nullptr,
                                     ATS_ALIGNED_BASIC_PER,
                                     &asn_DEF_E2SM_KPM_ActionDefinition,
                                     (void**)&action,
                                     definition.buf,
                                     definition.size);
    uint32_t periodMs = 0;
    if (rval.code == RC_OK && action != nullptr)
    {
        const auto& formats = action->actionDefinition_formats;
        switch (formats.present)
        {
        case E2SM_KPM_ActionDefinition__actionDefinition_formats_PR_actionDefinition_Format1:
            periodMs = ToPeriodMs(formats.choice.actionDefinition_Format1->granulPeriod);
            break;
        case E2SM_KPM_ActionDefinition__actionDefinition_formats_PR_actionDefinition_Format2:
            periodMs = ToPeriodMs(
                formats.choice.actionDefinition_Format2->subscriptInfo.granulPeriod);
            break;
        case E2SM_KPM_ActionDefinition__actionDefinition_formats_PR_actionDefinition_Format3:
            periodMs = ToPeriodMs(formats.choice.actionDefinition_Format3->granulPeriod);
            break;
        case E2SM_KPM_ActionDefinition__actionDefinition_formats_PR_actionDefinition_Format4:
            periodMs = ToPeriodMs(
                formats.choice.actionDefinition_Format4->subscriptionInfo.granulPeriod);
            break;
        case E2SM_KPM_ActionDefinition__actionDefinition_formats_PR_actionDefinition_Format5:
            periodMs = ToPeriodMs(
                formats.choice.actionDefinition_Format5->subscriptionInfo.granulPeriod);
            break;
        default:
            NS_LOG_WARN("Unknown E2SM-KPM Action Definition format " << formats.present);
            break;
        }
    }
    else
    {
        NS_LOG_WARN("Failed to decode E2SM-KPM Action Definition");
    }

    if (action != nullptr)
    {
        ASN_STRUCT_FREE(asn_DEF_E2SM_KPM_ActionDefinition, action);
    }
    return periodMs;
}

/**
 * Fill the reporting and granularity periods requested by a KPM subscription.
 * The granularity is read from the action accepted by ProcessRicSubscriptionRequest().
 */
void
DecodeKpmSubscriptionPeriods(const E2AP_PDU_t* pdu,
                             E2Termination::RicSubscriptionRequest_rval_s& params)
{
    if (pdu == nullptr || pdu->present != E2AP_PDU_PR_initiatingMessage ||
        pdu->choice.initiatingMessage == nullptr ||
        pdu->choice.initiatingMessage->value.present !=
            InitiatingMessage__value_PR_RICsubscriptionRequest)
    {
        NS_LOG_WARN("KPM subscription PDU is not a RIC Subscription Request");
        return;
    }

    const RICsubscriptionRequest_t& request =
        pdu->choice.initiatingMessage->value.choice.RICsubscriptionRequest;
    const RICsubscriptionDetails_t* details = nullptr;
    for (int i = 0; i < request.protocolIEs.list.count; i++)
    {
        auto* ie = (RICsubscriptionRequest_IEs_t*)request.protocolIEs.list.array[i];
        if (ie != nullptr &&
            ie->value.present == RICsubscriptionRequest_IEs__value_PR_RICsubscriptionDetails)
        {
            details = &ie->value.choice.RICsubscriptionDetails;
            break;
        }
    }
    if (details == nullptr)
    {
        NS_LOG_WARN("KPM subscription without RIC Subscription Details");
        return;
    }

    params.reportingPeriodMs = DecodeKpmReportingPeriodMs(details->ricEventTriggerDefinition);

    const auto& actions = details->ricAction_ToBeSetup_List;
    for (int i = 0; i < actions.list.count; i++)
    {
        auto* ie = (RICaction_ToBeSetup_ItemIEs_t*)actions.list.array[i];
        if (ie == nullptr ||
            ie->value.present != RICaction_ToBeSetup_ItemIEs__value_PR_RICaction_ToBeSetup_Item)
        {
            continue;
        }
        const auto& item = ie->value.choice.RICaction_ToBeSetup_Item;
        if (item.ricActionID != params.actionId)
        {
            continue;
        }
        if (item.ricActionDefinition != nullptr)
        {
            params.granularityPeriodMs = DecodeKpmGranularityPeriodMs(*item.ricActionDefinition);
        }
        break;
    }
}
} // namespace

E2Interface::PrbGeometry
E2Interface::GetPrimaryBwpPrbGeometry() const
{
    PrbGeometry geometry;
    auto gnbDevice = DynamicCast<NrGnbNetDevice>(m_netDev);
    NS_ABORT_MSG_IF(!gnbDevice, "PRB metrics require an NrGnbNetDevice");
    auto phy = gnbDevice->GetPhy(0); // primary BWP
    NS_ABORT_MSG_IF(!phy, "PRB metrics require a PHY on the primary BWP");
    geometry.rbNum = phy->GetRbNum();
    geometry.symbolsPerSlot = phy->GetSymbolsPerSlot();
    geometry.slotPeriod = phy->GetSlotPeriod();
    geometry.subcarrierSpacingHz = phy->GetSubcarrierSpacing();
    NS_ABORT_MSG_IF(geometry.rbNum == 0 || geometry.symbolsPerSlot == 0 ||
                        geometry.slotPeriod.IsZero(),
                    "Invalid primary BWP geometry");
    return geometry;
}

double
E2Interface::GetUePrbUsedDl(uint16_t rnti, const PrbGeometry& geometry) const
{
    // Average number of PRBs used by the UE in the reporting window:
    //   sum(assigned RBs x symbols of each received DL TB) / (slots in window x symbols per slot)
    // The previous formula (UE symbols / window symbols x 139) assumed that a UE uses the whole
    // band in its symbols, which only holds for TDMA, and used a fixed 139 PRBs (numerology 2).
    const double rbSymbols = m_e2DuCalculator->GetMacRbSymbolsUeSpecific(rnti, m_cellId);
    const Time reportingWindow =
        Simulator::Now() - m_e2DuCalculator->GetLastResetTime(rnti, m_cellId);
    const double slotsInWindow =
        std::ceil(static_cast<double>(reportingWindow.GetNanoSeconds()) /
                  static_cast<double>(geometry.slotPeriod.GetNanoSeconds()));
    const double windowRbSymbols = slotsInWindow * geometry.symbolsPerSlot;
    return windowRbSymbols > 0 ? rbSymbols / windowRbSymbols : 0.0;
}

uint64_t
E2Interface::SumUeDrbPdcpCounter(
    Ptr<NrUeManager> ue,
    const std::function<uint64_t(uint64_t imsi, uint8_t lcid)>& counter) const
{
    if (!m_e2PdcpStatsCalculator || !ue)
    {
        return 0;
    }

    // Sum every data radio bearer of the UE. Reading a fixed LCID (previously 4)
    // returned zero for UEs that only have the default bearer.
    const uint64_t imsi = ue->GetImsi();
    ObjectMapValue drbMap;
    ue->GetAttribute("DataRadioBearerMap", drbMap);

    uint64_t total = 0;
    for (auto drb = drbMap.Begin(); drb != drbMap.End(); ++drb)
    {
        Ptr<NrDataRadioBearerInfo> bearerInfo = drb->second->GetObject<NrDataRadioBearerInfo>();
        if (!bearerInfo)
        {
            continue;
        }
        total += counter(imsi, bearerInfo->m_logicalChannelIdentity);
    }
    return total;
}

uint64_t
E2Interface::GetUePdcpTxBytes(Ptr<NrUeManager> ue, bool downlink) const
{
    return SumUeDrbPdcpCounter(ue, [this, downlink](uint64_t imsi, uint8_t lcid) {
        return downlink ? m_e2PdcpStatsCalculator->GetDlTxData(imsi, lcid)
                        : m_e2PdcpStatsCalculator->GetUlTxData(imsi, lcid);
    });
}

uint64_t
E2Interface::GetUePdcpDlRxBytes(Ptr<NrUeManager> ue) const
{
    return SumUeDrbPdcpCounter(ue, [this](uint64_t imsi, uint8_t lcid) {
        return m_e2PdcpStatsCalculator->GetDlRxData(imsi, lcid);
    });
}

uint64_t
E2Interface::GetUePdcpUlRxBytes(Ptr<NrUeManager> ue) const
{
    return SumUeDrbPdcpCounter(ue, [this](uint64_t imsi, uint8_t lcid) {
        return m_e2PdcpStatsCalculator->GetUlRxData(imsi, lcid);
    });
}

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
                                          "Period (s) of the CCC indications and of the KPM "
                                          "reports whose subscription has no E2SM-KPM "
                                          "reportingPeriod",
                                          DoubleValue(0.01),
                                          MakeDoubleAccessor(&E2Interface::m_e2Periodicity),
                                          MakeDoubleChecker<double>())
                            .AddAttribute("StartTime",
                                          "The start time of the E2 report messages",
                                          DoubleValue(0),
                                          MakeDoubleAccessor(&E2Interface::m_startTime),
                                          MakeDoubleChecker<double>())
                            .AddAttribute("RcControlDrainPeriod",
                                          "Period of the event that handles KPM/CCC subscriptions and "
                                          "RC/CCC controls "
                                          "queued by the SCTP thread",
                                          TimeValue(MilliSeconds(1)),
                                          MakeTimeAccessor(&E2Interface::m_rcControlDrainPeriod),
                                          MakeTimeChecker(MicroSeconds(1)));
    return tid;
}

void
E2Interface::RegisterNewSinrReadingCallback([[maybe_unused]] std::string path,
                                            uint16_t cellId,
                                            uint16_t rnti,
                                            double avgSinr,
                                            [[maybe_unused]] uint16_t bwpId)
{
    NS_LOG_FUNCTION(this);
    if (avgSinr <= 0)
    {
        return;
    }
    double sinrDb = 10 * log10(avgSinr);
    NS_LOG_DEBUG("Registering new SINR reading for cellId: " << cellId << " RNTI: " << rnti
                                                             << " avgSinr: " << sinrDb);
    auto gnbNode = DynamicCast<NrGnbNetDevice>(m_netDev);
    if (!gnbNode)
    {
        return;
    }
    // NR v4.2+: a gNB has a single CellId (GetCellIds() was removed)
    for (auto id : {gnbNode->GetCellId()})
    {
        NS_LOG_DEBUG("CellId: " << cellId << " gNB cellId: " << id);
        if (id == cellId)
        {
            m_cellId = id;
            // Get the current gNB RRC instance
            PointerValue rrc;
            gnbNode->GetAttribute("NrGnbRrc", rrc);
            auto rrcPtr = rrc.Get<NrGnbRrc>();
            if (!rrcPtr)
            {
                return;
            }
            // Using the current RRC, get the UE map
            ObjectMapValue ueMap;
            rrcPtr->GetAttribute("UeMap", ueMap);
            // Get the ue based on the c-rnti
            NS_LOG_DEBUG("ue C-RNTI:" << rnti);
            auto ueMapObjct = ueMap.Get(rnti);
            if (!ueMapObjct)
            {
                return;
            }
            auto ue = DynamicCast<NrUeManager>(ueMapObjct);
            if (!ue)
            {
                return;
            }
            auto ueRnti = ue->GetRnti();
            if (ueRnti == rnti)
            {
                // Use in dB
                m_l3sinrMap[rnti][cellId] = sinrDb;
                NS_LOG_DEBUG("RNTI: " << rnti << " CellID: " << cellId << " SINR: " << sinrDb
                                      << " dB");
            }
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

    // Period requested by the subscription (E2SM-KPM Event Trigger), or E2Periodicity
    const Time reportingPeriod = params.reportingPeriodMs > 0
                                     ? MilliSeconds(params.reportingPeriodMs)
                                     : Seconds(m_e2Periodicity);
    // One record per indication: the measurements cover the whole reporting period
    const auto granularityPeriodMs =
        static_cast<unsigned long>(reportingPeriod.GetMilliSeconds());

    // ---- Node-Level Message (Report Style 1, Format 1) ----
    {
        auto header = BuildRicIndicationHeader();
        auto nodeMsg = BuildNodeLevelIndicationMessage(plmId, m_cellId, granularityPeriodMs);

        if (header != nullptr && nodeMsg != nullptr)
        {
            NS_LOG_DEBUG("Send KPM v3 Node-Level");
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
    {
        auto header = BuildRicIndicationHeader();
        auto ueMsg = BuildUeLevelIndicationMessage(plmId, m_cellId, granularityPeriodMs);

        if (header != nullptr && ueMsg != nullptr)
        {
            NS_LOG_DEBUG("Send KPM v3 UE-Level");
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
            e2Term->SendE2Message(pdu);
            // delete pdu;
        }
    }

    uint32_t nodeId = m_netDev->GetNode()->GetId();
    Simulator::ScheduleWithContext(nodeId,
                                   reportingPeriod,
                                   &E2Interface::BuildAndSendReportMessage,
                                   this,
                                   params);
}
void E2Interface::BuildBwpReport(nlohmann::json& bwpList)
{
    auto gnb = DynamicCast<NrGnbNetDevice>(m_netDev);
    for (uint8_t bwpIdx = 0; bwpIdx < gnb->GetCcMapSize(); bwpIdx++)
    {
        auto phy = gnb->GetPhy(bwpIdx);
        nlohmann::json bwp;
        bwp["bwpContext"] = "DL"; 
        bwp["subCarrierSpacing"] = std::to_string(15 * (1 << phy->GetNumerology()));
        bwp["numberOfRBs"] = phy->GetRbNum();
        bwpList.push_back(bwp);
    }
}

void E2Interface::BuildCccCellDuIndication(nlohmann::json& payload)
{
    auto gnb = DynamicCast<NrGnbNetDevice>(m_netDev);
    payload["cellLocalId"] = gnb->GetCellId();
    payload["operationalState"] = "ENABLED";
    payload["cellState"] = "ACTIVE";
    payload["nrPci"] = gnb->GetCellId();

    nlohmann::json plmnInfo = nlohmann::json::object();
    auto e2Term = m_netDev->GetObject<E2Termination>();
    plmnInfo["plmnId"] = e2Term ? e2Term->GetPlmnId() : "111"; 
    
    nlohmann::json snssaiList = nlohmann::json::array();
    
    nlohmann::json embb = {{"sst", 1}, {"sd", "000001"}};
    nlohmann::json urllc = {{"sst", 2}, {"sd", "000002"}};
    
    snssaiList.push_back(embb);
    snssaiList.push_back(urllc);
    plmnInfo["snssaiList"] = snssaiList;

    payload["plmnInfoList"] = nlohmann::json::array({plmnInfo});

    nlohmann::json bwpList = nlohmann::json::array();
    BuildBwpReport(bwpList);
    payload["bwpList"] = bwpList;
}

void E2Interface::ApplyCellDuControl(const nlohmann::json& cellDuJson)
{
    NS_LOG_FUNCTION(this);
    
    CccCellDuControlMessage controlMsg = cellDuJson.get<CccCellDuControlMessage>();
    std::vector<SlicePRBQuota> macQuotas;
    for (const auto& bwp : controlMsg.bwpList) {
        for (const auto& part : bwp.partitionList) {
            SlicePRBQuota macQ;
            macQ.sliceId = part.snssai.sst;
            macQ.dedicatePRBRatio = part.prbQuota;
            macQ.minPRBRatio = part.minPRBRatio;
            macQ.maxPRBRatio = part.maxPRBRatio;
            
            macQuotas.push_back(macQ);
        }
    }    if (!macQuotas.empty()) {
        auto gnbNode = DynamicCast<NrGnbNetDevice>(m_netDev);
        if (gnbNode) {
            Ptr<NrMacScheduler> scheduler = gnbNode->GetScheduler(0);
            Ptr<NrRLMacSchedulerOfdma> rlScheduler = DynamicCast<NrRLMacSchedulerOfdma>(scheduler);
            if (rlScheduler) {
                rlScheduler->SetSlicingParameters(macQuotas);
            }
        }
    }
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
    NS_LOG_DEBUG("requestorId " << +params.requestorId << ", instanceId " << +params.instanceId
                                << ", ranFuncionId " << +params.ranFuncionId << ", actionId "
                                << +params.actionId);

    DecodeKpmSubscriptionPeriods(sub_req_pdu, params);
    if (params.reportingPeriodMs == 0)
    {
        NS_LOG_WARN("KPM subscription without a valid reportingPeriod; using E2Periodicity ("
                    << m_e2Periodicity * 1000 << " ms)");
    }
    const double reportingPeriodMs =
        params.reportingPeriodMs > 0 ? params.reportingPeriodMs : m_e2Periodicity * 1000;
    if (params.granularityPeriodMs != 0 && params.granularityPeriodMs != reportingPeriodMs)
    {
        // Each indication carries one measurement record collected over the whole
        // reporting period, so the granularity period reported is the reporting period.
        NS_LOG_WARN("KPM granulPeriod " << params.granularityPeriodMs
                                        << " ms differs from the reporting period "
                                        << reportingPeriodMs
                                        << " ms; NORI reports one record per period");
    }
    NS_LOG_INFO("KPM subscription requestorId " << params.requestorId << ": reportingPeriod "
                                                << params.reportingPeriodMs << " ms, granulPeriod "
                                                << params.granularityPeriodMs << " ms");

    // This callback runs on the SCTP thread: only enqueue. DrainSctpQueues() sends the first
    // report at the current simulation time (see StartSctpQueueDrain()).
    {
        std::lock_guard<std::mutex> lock(m_rcControlMutex);
        m_pendingKpmSubscriptions.push_back(params);
    }
    if (!m_rcControlDrainStarted)
    {
        NS_LOG_WARN("KPM subscription queued but the drain event is not started; call "
                    "E2Interface::StartSctpQueueDrain() from the simulation thread");
    }
}

void E2Interface::CccSubscriptionCallback(E2AP_PDU_t* subReqPdu)
{
    NS_LOG_FUNCTION(this);
    if (!subReqPdu || !m_netDev) return;

    if (subReqPdu->present != E2AP_PDU_PR_initiatingMessage) {
        NS_LOG_WARN("CccSubscriptionCallback: PDU unexpected (not initiatingMessage).");
        return;
    }

    long procedureCode =
        subReqPdu->choice.initiatingMessage->procedureCode;

    if (procedureCode == ProcedureCode_id_RICsubscription) {
        auto e2Term = m_netDev->GetObject<E2Termination>();
        if (!e2Term) return;
        auto params = e2Term->ProcessRicSubscriptionRequest(subReqPdu);
        NS_LOG_INFO("CCC Subscription accepted, queued for periodic indications.");

        // This callback runs on the SCTP thread: only enqueue. DrainSctpQueues() sends the
        // first indication at the current simulation time (see StartSctpQueueDrain()).
        {
            std::lock_guard<std::mutex> lock(m_rcControlMutex);
            m_pendingCccSubscriptions.push_back(params);
        }
        if (!m_rcControlDrainStarted)
        {
            NS_LOG_WARN("CCC subscription queued but the drain event is not started; call "
                        "E2Interface::StartSctpQueueDrain() from the simulation thread");
        }

        } else if (procedureCode == ProcedureCode_id_RICcontrol) {
        NS_LOG_INFO("CCC RIC Control received.");
        CccControlMessage ctrlMsg(subReqPdu);
        if (!ctrlMsg.IsValid()) {
            NS_LOG_WARN("CccSubscriptionCallback: RICcontrolRequest invalid.");
            return;
        }

        PendingCccControl pending;
        pending.quotas = ctrlMsg.GetPrbQuotas();
        pending.ricRequestorId = 0;
        pending.ricInstanceId = 0;
        pending.ranFunctionId = 4;
        auto& ies = subReqPdu->choice.initiatingMessage->value.choice.RICcontrolRequest.protocolIEs.list;
        for (int i = 0; i < ies.count; ++i) {
            auto* ie = reinterpret_cast<RICcontrolRequest_IEs_t*>(ies.array[i]);
            if (ie->value.present == RICcontrolRequest_IEs__value_PR_RICrequestID) {
                pending.ricRequestorId = ie->value.choice.RICrequestID.ricRequestorID;
                pending.ricInstanceId = ie->value.choice.RICrequestID.ricInstanceID;
            } else if (ie->value.present == RICcontrolRequest_IEs__value_PR_RANfunctionID) {
                pending.ranFunctionId = ie->value.choice.RANfunctionID;
            }
        }

        // This callback runs on the SCTP thread, concurrent with the simulation: only enqueue.
        // DrainSctpQueues() applies the quotas and acknowledges on the simulation thread.
        size_t queued = 0;
        {
            std::lock_guard<std::mutex> lock(m_rcControlMutex);
            m_pendingCccControls.push_back(std::move(pending));
            queued = m_pendingCccControls.size();
        }
        NS_LOG_INFO("CCC Control queued (" << queued << " pending).");
        if (!m_rcControlDrainStarted)
        {
            NS_LOG_WARN("CCC control queued but the drain event is not started; call "
                        "E2Interface::StartSctpQueueDrain() from the simulation thread");
        }
    }
}

void
E2Interface::SendCccControlAck(long ricRequestorId, long ricInstanceId, long ranFunctionId)
{
        const long reqId = ricRequestorId;
        const long instId = ricInstanceId;
        const long ranFuncId = ranFunctionId;
        auto e2Term = m_netDev->GetObject<E2Termination>();
        if (e2Term) {
            auto* ackPdu = (E2AP_PDU_t*)calloc(1, sizeof(E2AP_PDU_t));
            ackPdu->present = E2AP_PDU_PR_successfulOutcome;
            ackPdu->choice.successfulOutcome = (SuccessfulOutcome_t*)calloc(1, sizeof(SuccessfulOutcome_t));
            ackPdu->choice.successfulOutcome->procedureCode = ProcedureCode_id_RICcontrol;
            ackPdu->choice.successfulOutcome->criticality = Criticality_reject;
            ackPdu->choice.successfulOutcome->value.present = SuccessfulOutcome__value_PR_RICcontrolAcknowledge;

            auto* ack = &ackPdu->choice.successfulOutcome->value.choice.RICcontrolAcknowledge;

            // IE 1: RICrequestID
            auto* ieReqId = (RICcontrolAcknowledge_IEs_t*)calloc(1, sizeof(RICcontrolAcknowledge_IEs_t));
            ieReqId->id = ProtocolIE_ID_id_RICrequestID;
            ieReqId->criticality = Criticality_reject;
            ieReqId->value.present = RICcontrolAcknowledge_IEs__value_PR_RICrequestID;
            ieReqId->value.choice.RICrequestID.ricRequestorID = reqId;
            ieReqId->value.choice.RICrequestID.ricInstanceID = instId;
            ASN_SEQUENCE_ADD(&ack->protocolIEs.list, ieReqId);

            // IE 2: RANfunctionID
            auto* ieFuncId = (RICcontrolAcknowledge_IEs_t*)calloc(1, sizeof(RICcontrolAcknowledge_IEs_t));
            ieFuncId->id = ProtocolIE_ID_id_RANfunctionID;
            ieFuncId->criticality = Criticality_reject;
            ieFuncId->value.present = RICcontrolAcknowledge_IEs__value_PR_RANfunctionID;
            ieFuncId->value.choice.RANfunctionID = ranFuncId;
            ASN_SEQUENCE_ADD(&ack->protocolIEs.list, ieFuncId);

            e2Term->SendE2Message(ackPdu);
            NS_LOG_INFO("RIC control Acknowledgement successfully sent to reqId=" << reqId);
        }
}

void E2Interface::BuildAndSendCccIndication(E2Termination::RicSubscriptionRequest_rval_s params)
{
    auto e2Term = m_netDev->GetObject<E2Termination>();
    if (!e2Term) return;

    Ptr<NrRLMacSchedulerOfdma> rlScheduler;
    auto gnbNode = DynamicCast<NrGnbNetDevice>(m_netDev);
    if (gnbNode) {
        Ptr<NrMacScheduler> scheduler = gnbNode->GetScheduler(0);
        rlScheduler = DynamicCast<NrRLMacSchedulerOfdma>(scheduler);
    }

    std::string payload = CccIndicationMessage::BuildPayload(m_netDev, e2Term->GetPlmnId(), rlScheduler);
    std::string header = "CCC";

    auto pdu = new E2AP_PDU;
    encoding::generate_e2apv1_indication_request_parameterized(
        pdu,
        params.requestorId,
        params.instanceId,
        params.ranFuncionId,
        params.actionId,
        1,
        reinterpret_cast<uint8_t*>(header.data()), header.size(),
        reinterpret_cast<uint8_t*>(payload.data()), payload.size()
    );

    e2Term->SendE2Message(pdu);

    uint32_t nodeId = m_netDev->GetNode()->GetId();
    Simulator::ScheduleWithContext(nodeId, Seconds(m_e2Periodicity),
                                   &E2Interface::BuildAndSendCccIndication, this, params);
}


void
E2Interface::ControlMessageReceivedCallback(E2AP_PDU_t* sub_req_pdu)
{
    NS_LOG_FUNCTION(this);
    NS_LOG_INFO("RIC Control Message received in E2Interface callback");

    if (m_netDev == nullptr || m_netDev->GetNode() == nullptr)
    {
        NS_LOG_ERROR("NetDevice or Node is null, cannot process RIC Control Message");
        return;
    }

    if (sub_req_pdu == nullptr)
    {
        NS_LOG_ERROR("sub_req_pdu is null");
        return;
    }

    RicControlMessage ctrlMsg(sub_req_pdu);

    switch (ctrlMsg.GetRequestType())
    {
    case RicControlMessage::RAN_SLICING:
    {
        auto quotas = ctrlMsg.GetPrbQuotas();
        if (quotas.empty())
        {
            NS_LOG_WARN("[E2SM-RC] Slice-level PRB quota without valid RRM Policy Ratio Group");
            SendRcControlOutcome(ctrlMsg.GetRicRequestId(),
                                 ctrlMsg.GetRanFunctionId(),
                                 false,
                                 CauseRICrequest_control_message_invalid);
            break;
        }
        // This callback runs on the SCTP thread: only enqueue. DrainSctpQueues() applies
        // the quota and answers the RIC on the simulation thread (see StartSctpQueueDrain()).
        size_t pending = 0;
        {
            std::lock_guard<std::mutex> lock(m_rcControlMutex);
            m_pendingRcControls.push_back(
                {quotas, ctrlMsg.GetRicRequestId(), static_cast<long>(ctrlMsg.GetRanFunctionId())});
            pending = m_pendingRcControls.size();
        }
        NS_LOG_INFO("RAN Slicing control directive from RIC queued (" << quotas.size()
                                                                     << " slice quotas, "
                                                                     << pending << " pending)");
        if (!m_rcControlDrainStarted)
        {
            NS_LOG_WARN("[E2SM-RC] RC control drain event not started; call "
                        "E2Interface::StartSctpQueueDrain() from the simulation thread");
        }
        break;
    }
    case RicControlMessage::UNSUPPORTED:
        SendRcControlOutcome(ctrlMsg.GetRicRequestId(),
                             ctrlMsg.GetRanFunctionId(),
                             false,
                             CauseRICrequest_action_not_supported);
        break;
    case RicControlMessage::TS:
        NS_LOG_INFO("Traffic Steering control directive received: SecondaryCell="
                    << ctrlMsg.GetSecondaryCellIdHO());
        break;
    case RicControlMessage::QoS:
        NS_LOG_INFO("QoS control directive received");
        break;
    default:
        NS_LOG_WARN("Unknown control request type: " << ctrlMsg.GetRequestType());
        break;
    }
}

void
E2Interface::ProcessControlMessage([[maybe_unused]] E2AP_PDU_t* pdu)
{
}

void
E2Interface::StartSctpQueueDrain()
{
    NS_LOG_FUNCTION(this);
    if (m_rcControlDrainStarted)
    {
        return;
    }
    m_rcControlDrainStarted = true;
    m_rcControlDrainEvent =
        Simulator::Schedule(m_rcControlDrainPeriod, &E2Interface::DrainSctpQueues, this);
}

void
E2Interface::DrainSctpQueues()
{
    std::deque<E2Termination::RicSubscriptionRequest_rval_s> subscriptions;
    std::deque<E2Termination::RicSubscriptionRequest_rval_s> cccSubscriptions;
    std::deque<PendingRcControl> batch;
    std::deque<PendingCccControl> cccBatch;
    {
        std::lock_guard<std::mutex> lock(m_rcControlMutex);
        subscriptions.swap(m_pendingKpmSubscriptions);
        cccSubscriptions.swap(m_pendingCccSubscriptions);
        batch.swap(m_pendingRcControls);
        cccBatch.swap(m_pendingCccControls);
    }

    // Called on the simulation thread: zero delay means the current simulation time.
    const uint32_t nodeId = m_netDev->GetNode()->GetId();
    for (const auto& params : subscriptions)
    {
        NS_LOG_INFO("Starting KPM reports for requestorId " << params.requestorId);
        Simulator::ScheduleWithContext(nodeId,
                                       Seconds(0),
                                       &E2Interface::BuildAndSendReportMessage,
                                       this,
                                       params);
    }
    for (const auto& params : cccSubscriptions)
    {
        NS_LOG_INFO("Starting CCC indications for requestorId " << params.requestorId);
        Simulator::ScheduleWithContext(nodeId,
                                       Seconds(0),
                                       &E2Interface::BuildAndSendCccIndication,
                                       this,
                                       params);
    }

    for (const auto& ctrl : cccBatch)
    {
        NS_LOG_INFO("Applying CCC control from RIC (requestorId=" << ctrl.ricRequestorId << ", "
                                                                  << ctrl.quotas.size()
                                                                  << " quotas)");
        ApplyCccSlicingControl(ctrl.quotas);
        SendCccControlAck(ctrl.ricRequestorId, ctrl.ricInstanceId, ctrl.ranFunctionId);
    }

    for (const auto& ctrl : batch)
    {
        NS_LOG_INFO("Applying RAN Slicing control directive from RIC (requestorId="
                    << ctrl.ricRequestId.ricRequestorID << ", " << ctrl.quotas.size()
                    << " slice quotas)");
        bool applied = ApplySlicingControl(ctrl.quotas);
        SendRcControlOutcome(ctrl.ricRequestId,
                             ctrl.ranFunctionId,
                             applied,
                             CauseRICrequest_control_failed_to_execute);
    }

    // Called on the simulation thread: the next drain is scheduled in simulation time.
    m_rcControlDrainEvent =
        Simulator::Schedule(m_rcControlDrainPeriod, &E2Interface::DrainSctpQueues, this);
}

void
E2Interface::SendRcControlOutcome(const RICrequestID_t& ricRequestId,
                                  long ranFunctionId,
                                  bool success,
                                  long ricRequestCause)
{
    auto e2Term = m_netDev->GetObject<E2Termination>();
    if (!e2Term)
    {
        NS_LOG_ERROR("[E2SM-RC] E2Termination not found, cannot send control outcome");
        return;
    }

    const auto& reqId = ricRequestId;
    long ranFuncId = ranFunctionId;

    auto* pdu = (E2AP_PDU_t*)calloc(1, sizeof(E2AP_PDU_t));
    if (success)
    {
        pdu->present = E2AP_PDU_PR_successfulOutcome;
        pdu->choice.successfulOutcome = (SuccessfulOutcome_t*)calloc(1, sizeof(SuccessfulOutcome_t));
        pdu->choice.successfulOutcome->procedureCode = ProcedureCode_id_RICcontrol;
        pdu->choice.successfulOutcome->criticality = Criticality_reject;
        pdu->choice.successfulOutcome->value.present =
            SuccessfulOutcome__value_PR_RICcontrolAcknowledge;
        auto* ack = &pdu->choice.successfulOutcome->value.choice.RICcontrolAcknowledge;

        auto* ieReqId = (RICcontrolAcknowledge_IEs_t*)calloc(1, sizeof(RICcontrolAcknowledge_IEs_t));
        ieReqId->id = ProtocolIE_ID_id_RICrequestID;
        ieReqId->criticality = Criticality_reject;
        ieReqId->value.present = RICcontrolAcknowledge_IEs__value_PR_RICrequestID;
        ieReqId->value.choice.RICrequestID.ricRequestorID = reqId.ricRequestorID;
        ieReqId->value.choice.RICrequestID.ricInstanceID = reqId.ricInstanceID;
        ASN_SEQUENCE_ADD(&ack->protocolIEs.list, ieReqId);

        auto* ieFuncId = (RICcontrolAcknowledge_IEs_t*)calloc(1, sizeof(RICcontrolAcknowledge_IEs_t));
        ieFuncId->id = ProtocolIE_ID_id_RANfunctionID;
        ieFuncId->criticality = Criticality_reject;
        ieFuncId->value.present = RICcontrolAcknowledge_IEs__value_PR_RANfunctionID;
        ieFuncId->value.choice.RANfunctionID = ranFuncId;
        ASN_SEQUENCE_ADD(&ack->protocolIEs.list, ieFuncId);
    }
    else
    {
        pdu->present = E2AP_PDU_PR_unsuccessfulOutcome;
        pdu->choice.unsuccessfulOutcome =
            (UnsuccessfulOutcome_t*)calloc(1, sizeof(UnsuccessfulOutcome_t));
        pdu->choice.unsuccessfulOutcome->procedureCode = ProcedureCode_id_RICcontrol;
        pdu->choice.unsuccessfulOutcome->criticality = Criticality_reject;
        pdu->choice.unsuccessfulOutcome->value.present =
            UnsuccessfulOutcome__value_PR_RICcontrolFailure;
        auto* fail = &pdu->choice.unsuccessfulOutcome->value.choice.RICcontrolFailure;

        auto* ieReqId = (RICcontrolFailure_IEs_t*)calloc(1, sizeof(RICcontrolFailure_IEs_t));
        ieReqId->id = ProtocolIE_ID_id_RICrequestID;
        ieReqId->criticality = Criticality_reject;
        ieReqId->value.present = RICcontrolFailure_IEs__value_PR_RICrequestID;
        ieReqId->value.choice.RICrequestID.ricRequestorID = reqId.ricRequestorID;
        ieReqId->value.choice.RICrequestID.ricInstanceID = reqId.ricInstanceID;
        ASN_SEQUENCE_ADD(&fail->protocolIEs.list, ieReqId);

        auto* ieFuncId = (RICcontrolFailure_IEs_t*)calloc(1, sizeof(RICcontrolFailure_IEs_t));
        ieFuncId->id = ProtocolIE_ID_id_RANfunctionID;
        ieFuncId->criticality = Criticality_reject;
        ieFuncId->value.present = RICcontrolFailure_IEs__value_PR_RANfunctionID;
        ieFuncId->value.choice.RANfunctionID = ranFuncId;
        ASN_SEQUENCE_ADD(&fail->protocolIEs.list, ieFuncId);

        auto* ieCause = (RICcontrolFailure_IEs_t*)calloc(1, sizeof(RICcontrolFailure_IEs_t));
        ieCause->id = ProtocolIE_ID_id_Cause;
        ieCause->criticality = Criticality_ignore;
        ieCause->value.present = RICcontrolFailure_IEs__value_PR_Cause;
        ieCause->value.choice.Cause.present = Cause_PR_ricRequest;
        ieCause->value.choice.Cause.choice.ricRequest = ricRequestCause;
        ASN_SEQUENCE_ADD(&fail->protocolIEs.list, ieCause);
    }

    e2Term->SendE2Message(pdu);
    NS_LOG_INFO("[E2SM-RC] RIC Control " << (success ? "Acknowledge" : "Failure")
                                         << " sent: requestorId=" << reqId.ricRequestorID
                                         << " instanceId=" << reqId.ricInstanceID
                                         << " ranFunctionId=" << ranFuncId
                                         << (success ? "" : " cause=")
                                         << (success ? "" : std::to_string(ricRequestCause)));
}

bool
E2Interface::ApplySlicingControl(const std::vector<RicControlMessage::SlicePRBQuota>& quotas)
{
    NS_LOG_FUNCTION(this);

    auto gnbNode = DynamicCast<NrGnbNetDevice>(m_netDev);
    if (!gnbNode)
    {
        NS_LOG_ERROR("NetDevice is not a NrGnbNetDevice");
        return false;
    }

    Ptr<NrMacScheduler> scheduler = gnbNode->GetScheduler(0);
    Ptr<NrRLMacSchedulerOfdma> rlScheduler = DynamicCast<NrRLMacSchedulerOfdma>(scheduler);

    if (!rlScheduler)
    {
        NS_LOG_WARN("Scheduler is not NrRLMacSchedulerOfdma, cannot apply slicing quotas");
        return false;
    }

    std::vector<SlicePRBQuota> macQuotas;
    for (const auto& q : quotas)
    {
        SlicePRBQuota macQ;
        macQ.sliceId = q.sst;
        macQ.maxPRBRatio = q.maxPRBRatio;
        macQ.minPRBRatio = q.minPRBRatio;
        macQ.dedicatePRBRatio = q.dedicatePRBRatio;
        macQuotas.push_back(macQ);

        NS_LOG_INFO("Applied PRB quota: SST=" << static_cast<uint32_t>(q.sst)
                    << ", dedicated=" << q.dedicatePRBRatio
                    << "%, min=" << q.minPRBRatio
                    << "%, max=" << q.maxPRBRatio << "%");
    }

    rlScheduler->SetSlicingParameters(macQuotas);
    return true;
}

void
E2Interface::ApplyCccSlicingControl(const std::vector<SlicePRBQuota>& quotas)
{
    NS_LOG_FUNCTION(this);
    for (const auto& q : quotas)
    {
        if (q.maxMimoLayers > 0)
        {
            for (uint32_t i = 0; i < NodeList::GetNNodes(); ++i)
            {
                Ptr<Node> node = NodeList::GetNode(i);
                for (uint32_t j = 0; j < node->GetNDevices(); ++j)
                {
                    Ptr<NrUeNetDevice> ueDev = DynamicCast<NrUeNetDevice>(node->GetDevice(j));
                    if (ueDev && ueDev->GetPhy(0) && ueDev->GetPhy(0)->GetPmSearch())
                    {
                        ueDev->GetPhy(0)->GetPmSearch()->SetAttribute("RankLimit", UintegerValue(q.maxMimoLayers));
                        std::cout << "\n>>> [MIMO CONTROL VIA xAPP] RankLimit atualizado para: " 
                                  << q.maxMimoLayers << " layer(s) no UE (Node " << node->GetId() << ") <<<\n" << std::endl;
                    }
                }
            }
            break; 
        }
    }
    auto gnbNode = DynamicCast<NrGnbNetDevice>(m_netDev);
    if (!gnbNode)
    {
        return;
    }

    Ptr<NrMacScheduler> scheduler = gnbNode->GetScheduler(0);
    Ptr<NrRLMacSchedulerOfdma> rlScheduler = DynamicCast<NrRLMacSchedulerOfdma>(scheduler);

    if (rlScheduler)
    {
        rlScheduler->SetSlicingParameters(quotas);
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

Ptr<KpmIndicationMessage> E2Interface::BuildNodeLevelIndicationMessage(std::string plmId,
                                                                      uint16_t nrCellId,
                                                                      unsigned long granularityPeriodMs){

    auto helper = Create<NoriIndicationMessageHelper>(IndicationMessageHelper::IndicationMessageType::NodeLevel, false, false);

    helper->SetGranularityPeriod(granularityPeriodMs);
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
    const PrbGeometry prbGeometry = GetPrimaryBwpPrbGeometry();

    for (auto ueMap = ueManager.Begin(); ueMap != ueManager.End(); ueMap++)
    {
        auto ue = DynamicCast<NrUeManager>(ueMap->second);
        if (!ue)
        {
            continue;
        }
        uint64_t imsi = ue->GetImsi();
        uint16_t rnti = ue->GetRnti();

        // --- CU-UP: aggregate PDCP volume ---
        double actualTotalTxBytes = GetUePdcpTxBytes(ue, true) * (8 / 1e3); // kbit, all DRBs
        double txBytes = ConsumeCounterDelta(actualTotalTxBytes, m_cellTxBytes[imsi]);
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

        double macPrb = GetUePrbUsedDl(rnti, prbGeometry);
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

    // Primary BWP of the gNB (paired carrier: same geometry in DL and UL)
    long dlAvailablePrbs = static_cast<long>(prbGeometry.rbNum);
    long ulAvailablePrbs = static_cast<long>(prbGeometry.rbNum);

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

Ptr<KpmIndicationMessage>E2Interface::BuildUeLevelIndicationMessage(std::string plmId,
                                                                    uint16_t nrCellId,
                                                                    unsigned long granularityPeriodMs){

    auto helper = Create<NoriIndicationMessageHelper>(IndicationMessageHelper::IndicationMessageType::UeLevel, false, false);
    helper->SetGranularityPeriod(granularityPeriodMs);

    ObjectMapValue ueManager;
    m_rrc->GetAttribute("UeMap", ueManager);

    // If no UEs are attached to this gNB, skip UE-level report
    // (ASN.1 Format 3 requires at least 1 UE in UEMeasurementReportList)
    if (ueManager.GetN() == 0)
    {
        return nullptr;
    }

    m_cellId = nrCellId;
    const PrbGeometry prbGeometry = GetPrimaryBwpPrbGeometry();

    for (auto ueObject = ueManager.Begin(); ueObject != ueManager.End(); ueObject++)
    {
        auto ue = DynamicCast<NrUeManager>(ueObject->second);
        if (!ue)
        {
            continue;
        }
        uint64_t imsi = ue->GetImsi();
        uint16_t rnti = ue->GetRnti();
        uint8_t sst = NoriSlicingHelper::GetSstForRnti(rnti);

        // Begin a new UE report (IMSI as AMF-UE-NGAP-ID for simulation)
        helper->BeginUeReport(imsi, plmId, 0, 0, 0);

        // --- CU-UP per-UE: PDCP volume (TX) and UE throughput (delivered) ---
        double actualTotalTxBytes = GetUePdcpTxBytes(ue, true) * (8 / 1e3); // kbit, all DRBs
        // Node-level and UE-level reports are built at the same instant, so each
        // one keeps its own reference value (operator[] starts it at zero).
        double txBytes = ConsumeCounterDelta(actualTotalTxBytes, m_ueTxBytes[imsi]);

        // DRB.UEThpDl (TS 28.552, 5.1.1.3.1) is based on the volume acknowledged by
        // the UE, not on the volume offered to the gNB. Use the PDCP volume received
        // by the UE since the previous UE report (all DRBs), divided by the elapsed
        // simulation time. On the first report the reference time is the simulation
        // start, so the value is the average since then instead of a spike. The TS
        // divides by the active transmission time of each data burst; here the divisor
        // is the whole interval, so bursty traffic is underestimated.
        double actualTotalRxKbits = GetUePdcpDlRxBytes(ue) * (8 / 1e3); // kbit, all DRBs
        double rxKbits = ConsumeCounterDelta(actualTotalRxKbits, m_ueRxBytes[imsi]);
        const Time now = Simulator::Now();
        const double elapsedS = (now - m_ueRxLastReportTime[imsi]).GetSeconds();
        m_ueRxLastReportTime[imsi] = now;
        double ueThroughputDl = elapsedS > 0 ? rxKbits / elapsedS : 0.0; // kbps

        helper->AddUeMeasurementReal("DRB.UEThpDl", ueThroughputDl);
        helper->AddUeMeasurementInteger("QosFlow.PdcpPduVolumeDL_Filter", (unsigned long)txBytes);

        // --- DU per-UE: MAC stats ---
        uint32_t macPduUe = m_e2DuCalculator->GetMacPduUeSpecific(rnti, m_cellId);
        uint32_t macPduInitialUe = m_e2DuCalculator->GetMacPduInitialTransmissionUeSpecific(rnti, m_cellId);
        uint32_t macQpsk = m_e2DuCalculator->GetMacPduQpskUeSpecific(rnti, m_cellId);
        uint32_t mac16Qam = m_e2DuCalculator->GetMacPdu16QamUeSpecific(rnti, m_cellId);
        uint32_t mac64Qam = m_e2DuCalculator->GetMacPdu64QamUeSpecific(rnti, m_cellId);
        uint32_t macRetx = m_e2DuCalculator->GetMacPduRetransmissionUeSpecific(rnti, m_cellId);

        double macPrb = GetUePrbUsedDl(rnti, prbGeometry);

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
        csv << "timestamp,imsi,dlThroughput,ulThroughput,spectralEfficiency,dlOffered,ulOffered\n";
    }

    double currentTime = Simulator::Now().GetMilliSeconds();
    double deltatime = currentTime - m_previousTime[imsi];

    Ptr<NrUeManager> ueManager;
    ObjectMapValue ueMap;
    m_rrc->GetAttribute("UeMap", ueMap);
    for (auto it = ueMap.Begin(); it != ueMap.End(); ++it)
    {
        auto candidate = DynamicCast<NrUeManager>(it->second);
        if (candidate && candidate->GetImsi() == imsi)
        {
            ueManager = candidate;
            break;
        }
    }

    // All DRBs of the UE. Throughput columns use the delivered volume (PDCP RX: at the
    // UE for DL, at the gNB for UL), as DRB.UEThpDl does; the offered load (PDCP TX)
    // goes to separate columns. Units: kbit/s (bytes * 8 / ms).
    const bool validInterval = currentTime > m_previousTime[imsi];
    auto rate = [validInterval, deltatime](double deltaBytes) {
        return validInterval ? deltaBytes * 8 / deltatime : 0.0;
    };
    double dlThroughput = rate(ConsumeCounterDelta(
        static_cast<double>(GetUePdcpDlRxBytes(ueManager)), m_previousDlRxData[imsi]));
    double ulThroughput = rate(ConsumeCounterDelta(
        static_cast<double>(GetUePdcpUlRxBytes(ueManager)), m_previousUlRxData[imsi]));
    double dlOffered = rate(ConsumeCounterDelta(
        static_cast<double>(GetUePdcpTxBytes(ueManager, true)), m_previousDlTxData[imsi]));
    double ulOffered = rate(ConsumeCounterDelta(
        static_cast<double>(GetUePdcpTxBytes(ueManager, false)), m_previousUlTxData[imsi]));
    m_previousTime[imsi] = currentTime;
    double spectralEfficiency = 0.0;
    if (m_e2DuCalculator)
    {
        if (macPrb > 0)
        {
            // PRB bandwidth = 12 subcarriers x SCS of the primary BWP (was 720 kHz,
            // i.e. 60 kHz SCS, fixed)
            const PrbGeometry prbGeometry = GetPrimaryBwpPrbGeometry();
            const double prbBandwidthHz = 12.0 * prbGeometry.subcarrierSpacingHz;
            spectralEfficiency = dlThroughput / (macPrb * prbBandwidthHz);
        }
    }
    uint64_t timestamp = m_startTime + (uint64_t)Simulator::Now().GetMilliSeconds();
    csv << timestamp << "," << imsi << "," << dlThroughput << "," << ulThroughput << ","
        << spectralEfficiency << "," << dlOffered << "," << ulOffered << "\n";

    csv.close();
}

Ptr<NoriE2Report>
E2Interface::GetE2DuCalculator()
{
    return m_e2DuCalculator;
}

} // namespace ns3
