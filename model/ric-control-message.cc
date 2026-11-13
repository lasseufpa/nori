/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2022 Northeastern University
 * Copyright (c) 2022 Sapienza, University of Rome
 * Copyright (c) 2022 University of Padova
 * Copyright (c) 2026 LASSE/UFPA - Universidade Federal do Pará
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Andrea Lacava <thecave003@gmail.com>
 *         Tommaso Zugno <tommasozugno@gmail.com>
 *         Michele Polese <michele.polese@gmail.com>
 *         Andrey Adailso <andreyadailsom@gmail.com>
 */

#include "ric-control-message.h"

#include "asn1c-types.h"

#include "ns3/log.h"

extern "C"
{
#include "E2SM-RC-ControlHeader-Format1.h"
#include "E2SM-RC-ControlHeader.h"
#include "E2SM-RC-ControlMessage-Format1-Item.h"
#include "E2SM-RC-ControlMessage-Format1.h"
#include "E2SM-RC-ControlMessage.h"
#include "RANParameter-LIST.h"
#include "RANParameter-STRUCTURE-Item.h"
#include "RANParameter-STRUCTURE.h"
#include "RANParameter-Value.h"
#include "RANParameter-ValueType-Choice-ElementFalse.h"
#include "RANParameter-ValueType-Choice-ElementTrue.h"
#include "RANParameter-ValueType-Choice-List.h"
#include "RANParameter-ValueType-Choice-Structure.h"
#include "RANParameter-ValueType.h"
#include "UEID-GNB.h"
}

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("RicControlMessage");

RicControlMessage::RicControlMessage(E2AP_PDU_t* pdu)
{
    DecodeRicControlMessage(pdu);
    NS_LOG_INFO("End of RicControlMessage::RicControlMessage()");
}

RicControlMessage::~RicControlMessage()
{
}

static bool
ExtractIntValue(const RANParameter_ValueType_t* vt, long& outVal)
{
    if (vt == nullptr)
    {
        return false;
    }
    if (vt->present == RANParameter_ValueType_PR_ranP_Choice_ElementTrue &&
        vt->choice.ranP_Choice_ElementTrue != nullptr)
    {
        const auto& val = vt->choice.ranP_Choice_ElementTrue->ranParameter_value;
        if (val.present == RANParameter_Value_PR_valueInt)
        {
            outVal = val.choice.valueInt;
            return true;
        }
    }
    else if (vt->present == RANParameter_ValueType_PR_ranP_Choice_ElementFalse &&
             vt->choice.ranP_Choice_ElementFalse != nullptr)
    {
        if (vt->choice.ranP_Choice_ElementFalse->ranParameter_value != nullptr)
        {
            const auto& val = *vt->choice.ranP_Choice_ElementFalse->ranParameter_value;
            if (val.present == RANParameter_Value_PR_valueInt)
            {
                outVal = val.choice.valueInt;
                return true;
            }
        }
    }
    return false;
}

static bool
ExtractOctetStringValue(const RANParameter_ValueType_t* vt, std::string& outStr)
{
    if (vt == nullptr)
    {
        return false;
    }
    if (vt->present == RANParameter_ValueType_PR_ranP_Choice_ElementTrue &&
        vt->choice.ranP_Choice_ElementTrue != nullptr)
    {
        const auto& val = vt->choice.ranP_Choice_ElementTrue->ranParameter_value;
        if (val.present == RANParameter_Value_PR_valueOctS && val.choice.valueOctS.buf != nullptr)
        {
            outStr.assign(reinterpret_cast<char*>(val.choice.valueOctS.buf),
                          val.choice.valueOctS.size);
            return true;
        }
        else if (val.present == RANParameter_Value_PR_valuePrintableString &&
                 val.choice.valuePrintableString.buf != nullptr)
        {
            outStr.assign(reinterpret_cast<char*>(val.choice.valuePrintableString.buf),
                          val.choice.valuePrintableString.size);
            return true;
        }
    }
    else if (vt->present == RANParameter_ValueType_PR_ranP_Choice_ElementFalse &&
             vt->choice.ranP_Choice_ElementFalse != nullptr)
    {
        if (vt->choice.ranP_Choice_ElementFalse->ranParameter_value != nullptr)
        {
            const auto& val = *vt->choice.ranP_Choice_ElementFalse->ranParameter_value;
            if (val.present == RANParameter_Value_PR_valueOctS &&
                val.choice.valueOctS.buf != nullptr)
            {
                outStr.assign(reinterpret_cast<char*>(val.choice.valueOctS.buf),
                              val.choice.valueOctS.size);
                return true;
            }
            else if (val.present == RANParameter_Value_PR_valuePrintableString &&
                     val.choice.valuePrintableString.buf != nullptr)
            {
                outStr.assign(reinterpret_cast<char*>(val.choice.valuePrintableString.buf),
                              val.choice.valuePrintableString.size);
                return true;
            }
        }
    }
    return false;
}

static bool
ExtractSstValue(const RANParameter_ValueType_t* vt, uint8_t& outSst)
{
    long intVal = 0;
    if (ExtractIntValue(vt, intVal))
    {
        outSst = static_cast<uint8_t>(intVal);
        return true;
    }
    std::string strVal;
    if (ExtractOctetStringValue(vt, strVal) && !strVal.empty())
    {
        outSst = static_cast<uint8_t>(strVal[0]);
        return true;
    }
    return false;
}

static bool
ExtractSdValue(const RANParameter_ValueType_t* vt, uint32_t& outSd)
{
    long intVal = 0;
    if (ExtractIntValue(vt, intVal))
    {
        outSd = static_cast<uint32_t>(intVal);
        return true;
    }
    std::string strVal;
    if (ExtractOctetStringValue(vt, strVal) && !strVal.empty())
    {
        // SD is OCTET STRING (SIZE(3)) in TS 38.473
        uint32_t sd = 0;
        for (unsigned char c : strVal)
        {
            sd = (sd << 8) | c;
        }
        outSd = sd;
        return true;
    }
    return false;
}

static const RANParameter_STRUCTURE_t*
GetStructure(const RANParameter_ValueType_t* vt)
{
    if (vt != nullptr && vt->present == RANParameter_ValueType_PR_ranP_Choice_Structure &&
        vt->choice.ranP_Choice_Structure != nullptr)
    {
        return vt->choice.ranP_Choice_Structure->ranParameter_Structure;
    }
    return nullptr;
}

static const RANParameter_LIST_t*
GetList(const RANParameter_ValueType_t* vt)
{
    if (vt != nullptr && vt->present == RANParameter_ValueType_PR_ranP_Choice_List &&
        vt->choice.ranP_Choice_List != nullptr)
    {
        return vt->choice.ranP_Choice_List->ranParameter_List;
    }
    return nullptr;
}

/**
 * Parses an "RRM Policy Member" structure (RAN Parameter ID 6):
 *   7 PLMN Identity, 8 S-NSSAI { 9 SST, 10 SD }
 */
static void
ParsePolicyMember(const RANParameter_STRUCTURE_t* member, RicControlMessage::SlicePRBQuota& quota)
{
    if (member == nullptr || member->sequence_of_ranParameters == nullptr)
    {
        return;
    }
    for (int i = 0; i < member->sequence_of_ranParameters->list.count; i++)
    {
        auto* item = member->sequence_of_ranParameters->list.array[i];
        if (item == nullptr || item->ranParameter_valueType == nullptr)
        {
            continue;
        }
        switch (item->ranParameter_ID)
        {
        case RicControlMessage::RC_PARAM_PLMN_IDENTITY:
            ExtractOctetStringValue(item->ranParameter_valueType, quota.plmnId);
            break;
        case RicControlMessage::RC_PARAM_S_NSSAI: {
            const auto* snssai = GetStructure(item->ranParameter_valueType);
            if (snssai == nullptr || snssai->sequence_of_ranParameters == nullptr)
            {
                break;
            }
            for (int j = 0; j < snssai->sequence_of_ranParameters->list.count; j++)
            {
                auto* sItem = snssai->sequence_of_ranParameters->list.array[j];
                if (sItem == nullptr)
                {
                    continue;
                }
                if (sItem->ranParameter_ID == RicControlMessage::RC_PARAM_SST)
                {
                    ExtractSstValue(sItem->ranParameter_valueType, quota.sst);
                }
                else if (sItem->ranParameter_ID == RicControlMessage::RC_PARAM_SD)
                {
                    ExtractSdValue(sItem->ranParameter_valueType, quota.sd);
                }
            }
            break;
        }
        default:
            NS_LOG_WARN("[E2SM-RC] Unexpected RAN Parameter ID " << item->ranParameter_ID
                                                                 << " inside RRM Policy Member");
            break;
        }
    }
}

/** Parses an "RRM Policy Member List" (ID 5); each element is an RRM Policy Member (ID 6). */
static void
ParseMemberList(const RANParameter_LIST_t* list,
                std::vector<RicControlMessage::SlicePRBQuota>& members)
{
    if (list == nullptr)
    {
        return;
    }
    for (int j = 0; j < list->list_of_ranParameter.list.count; j++)
    {
        RicControlMessage::SlicePRBQuota member;
        ParsePolicyMember(list->list_of_ranParameter.list.array[j], member);
        members.push_back(member);
    }
}

/**
 * Collects the RRM Policy Members found under an "RRM Policy" structure (ID 3).
 * The member list (ID 5) is accepted either inside ID 3 or as a sibling of it,
 * because the nesting level of ID 5 is ambiguous in the E2SM-RC v03.00 table.
 */
static void
CollectPolicyMembers(const RANParameter_STRUCTURE_t* structure,
                     std::vector<RicControlMessage::SlicePRBQuota>& members)
{
    if (structure == nullptr || structure->sequence_of_ranParameters == nullptr)
    {
        return;
    }
    for (int i = 0; i < structure->sequence_of_ranParameters->list.count; i++)
    {
        auto* item = structure->sequence_of_ranParameters->list.array[i];
        if (item == nullptr || item->ranParameter_valueType == nullptr)
        {
            continue;
        }
        if (item->ranParameter_ID == RicControlMessage::RC_PARAM_RRM_POLICY)
        {
            CollectPolicyMembers(GetStructure(item->ranParameter_valueType), members);
        }
        else if (item->ranParameter_ID == RicControlMessage::RC_PARAM_RRM_POLICY_MEMBER_LIST)
        {
            ParseMemberList(GetList(item->ranParameter_valueType), members);
        }
    }
}

/**
 * Parses one "RRM Policy Ratio Group" structure (RAN Parameter ID 2), as defined in
 * O-RAN.WG3.E2SM-RC v03.00, section 8.4.3.6 (Slice-level PRB quota):
 *   3 RRM Policy { 5 RRM Policy Member List { 6 RRM Policy Member { 7 PLMN, 8 S-NSSAI {9 SST, 10 SD} } } }
 *   11 Min PRB Policy Ratio, 12 Max PRB Policy Ratio, 13 Dedicated PRB Policy Ratio
 * One SlicePRBQuota is produced per RRM Policy Member, all sharing the group ratios.
 */
static std::vector<RicControlMessage::SlicePRBQuota>
ParseSliceGroupStructure(const RANParameter_STRUCTURE_t* groupStruct)
{
    std::vector<RicControlMessage::SlicePRBQuota> result;
    if (groupStruct == nullptr || groupStruct->sequence_of_ranParameters == nullptr)
    {
        return result;
    }

    long minRatio = 0;
    long maxRatio = 100;
    long dedRatio = 0;
    std::vector<RicControlMessage::SlicePRBQuota> members;

    for (int i = 0; i < groupStruct->sequence_of_ranParameters->list.count; i++)
    {
        auto* item = groupStruct->sequence_of_ranParameters->list.array[i];
        if (item == nullptr || item->ranParameter_valueType == nullptr)
        {
            continue;
        }
        switch (item->ranParameter_ID)
        {
        case RicControlMessage::RC_PARAM_RRM_POLICY:
            CollectPolicyMembers(GetStructure(item->ranParameter_valueType), members);
            break;
        case RicControlMessage::RC_PARAM_RRM_POLICY_MEMBER_LIST:
            ParseMemberList(GetList(item->ranParameter_valueType), members);
            break;
        case RicControlMessage::RC_PARAM_MIN_PRB_RATIO:
            ExtractIntValue(item->ranParameter_valueType, minRatio);
            break;
        case RicControlMessage::RC_PARAM_MAX_PRB_RATIO:
            ExtractIntValue(item->ranParameter_valueType, maxRatio);
            break;
        case RicControlMessage::RC_PARAM_DEDICATED_PRB_RATIO:
            ExtractIntValue(item->ranParameter_valueType, dedRatio);
            break;
        default:
            NS_LOG_WARN("[E2SM-RC] Unexpected RAN Parameter ID " << item->ranParameter_ID
                                                                 << " inside RRM Policy Ratio Group");
            break;
        }
    }

    if (members.empty())
    {
        NS_LOG_WARN("[E2SM-RC] RRM Policy Ratio Group without RRM Policy Member (S-NSSAI); ignored");
        return result;
    }

    for (auto& m : members)
    {
        m.minPRBRatio = minRatio;
        m.maxPRBRatio = maxRatio;
        m.dedicatePRBRatio = dedRatio;
        NS_LOG_INFO("[E2SM-RC] Slice quota: SST=" << +m.sst << " SD=" << m.sd
                                                  << " min=" << minRatio << " max=" << maxRatio
                                                  << " dedicated=" << dedRatio);
        result.push_back(m);
    }
    return result;
}

void
RicControlMessage::ParseRrmPolicyRatioList(const RANParameter_ValueType_t* valueType)
{
    const auto* list = GetList(valueType);
    if (list == nullptr)
    {
        NS_LOG_ERROR("[E2SM-RC] RAN Parameter ID 1 (RRM Policy Ratio List) is not a LIST");
        return;
    }
    // Each list element is an RRM Policy Ratio Group (ID 2) structure
    for (int i = 0; i < list->list_of_ranParameter.list.count; i++)
    {
        auto quotas = ParseSliceGroupStructure(list->list_of_ranParameter.list.array[i]);
        m_prbQuotas.insert(m_prbQuotas.end(), quotas.begin(), quotas.end());
    }
}

void
RicControlMessage::DecodeControlHeader(const RICcontrolHeader_t& headerIe)
{
    E2SM_RC_ControlHeader_t* controlHeader = nullptr;
    asn_dec_rval_t decRval = asn_decode(nullptr,
                                        ATS_ALIGNED_BASIC_PER,
                                        &asn_DEF_E2SM_RC_ControlHeader,
                                        (void**)&controlHeader,
                                        headerIe.buf,
                                        headerIe.size);

    if (decRval.code != RC_OK || controlHeader == nullptr)
    {
        NS_LOG_ERROR("Failed to decode E2SM-RC-ControlHeader");
        if (controlHeader != nullptr)
        {
            ASN_STRUCT_FREE(asn_DEF_E2SM_RC_ControlHeader, controlHeader);
        }
        return;
    }

    NS_LOG_INFO(xer_fprint(stderr, &asn_DEF_E2SM_RC_ControlHeader, controlHeader));

    if (controlHeader->ric_controlHeader_formats.present ==
        E2SM_RC_ControlHeader__ric_controlHeader_formats_PR_controlHeader_Format1)
    {
        auto* format1 = controlHeader->ric_controlHeader_formats.choice.controlHeader_Format1;
        if (format1 != nullptr)
        {
            m_ricStyleType = format1->ric_Style_Type;
            m_ricControlActionId = format1->ric_ControlAction_ID;

            if (m_ricStyleType == RC_CONTROL_STYLE_RADIO_RESOURCE_ALLOCATION &&
                m_ricControlActionId == RC_CONTROL_ACTION_SLICE_PRB_QUOTA)
            {
                m_requestType = ControlMessageRequestIdType::RAN_SLICING;
            }
            else if (m_ricControlActionId == RC_CONTROL_ACTION_SLICE_PRB_QUOTA)
            {
                // E2SM-RC v03.00 section 7.6.3: Slice-level PRB quota is Control Action 6 of
                // Control Style 2 (Radio Resource Allocation Control). Action 6 of Style 1 is
                // "DRB split ratio control", which NORI does not implement.
                NS_LOG_ERROR("[E2SM-RC] Control Style " << m_ricStyleType << " / Action "
                             << m_ricControlActionId
                             << " not supported (Slice-level PRB quota is Style 2, Action 6)");
                m_requestType = ControlMessageRequestIdType::UNSUPPORTED;
            }

            if (format1->ric_ControlDecision != nullptr)
            {
                m_ricControlDecision = *format1->ric_ControlDecision;
            }

            // Extract UEID
            if (format1->ueID.present == UEID_PR_gNB_UEID &&
                format1->ueID.choice.gNB_UEID != nullptr)
            {
                long ueVal = 0;
                if (asn_INTEGER2long(&format1->ueID.choice.gNB_UEID->amf_UE_NGAP_ID, &ueVal) == 0)
                {
                    m_ueId = static_cast<uint64_t>(ueVal);
                }
            }

            NS_LOG_INFO("Decoded RC Control Header Format 1: StyleType="
                        << m_ricStyleType << ", ActionID=" << m_ricControlActionId
                        << ", Decision=" << m_ricControlDecision << ", UEID=" << m_ueId);
        }
    }
    else
    {
        NS_LOG_WARN("E2SM-RC-ControlHeader format not supported: "
                    << controlHeader->ric_controlHeader_formats.present);
    }

    ASN_STRUCT_FREE(asn_DEF_E2SM_RC_ControlHeader, controlHeader);
}

void
RicControlMessage::DecodeControlMessage(const RICcontrolMessage_t& messageIe)
{
    E2SM_RC_ControlMessage_t* controlMsg = nullptr;
    asn_dec_rval_t decRval = asn_decode(nullptr,
                                        ATS_ALIGNED_BASIC_PER,
                                        &asn_DEF_E2SM_RC_ControlMessage,
                                        (void**)&controlMsg,
                                        messageIe.buf,
                                        messageIe.size);

    if (decRval.code != RC_OK || controlMsg == nullptr)
    {
        NS_LOG_ERROR("Failed to decode E2SM-RC-ControlMessage");
        if (controlMsg != nullptr)
        {
            ASN_STRUCT_FREE(asn_DEF_E2SM_RC_ControlMessage, controlMsg);
        }
        return;
    }

    NS_LOG_INFO(xer_fprint(stderr, &asn_DEF_E2SM_RC_ControlMessage, controlMsg));

    if (controlMsg->ric_controlMessage_formats.present ==
        E2SM_RC_ControlMessage__ric_controlMessage_formats_PR_controlMessage_Format1)
    {
        auto* format1 = controlMsg->ric_controlMessage_formats.choice.controlMessage_Format1;
        if (format1 != nullptr)
        {
            int count = format1->ranP_List.list.count;
            for (int i = 0; i < count; i++)
            {
                auto* item = format1->ranP_List.list.array[i];
                if (item != nullptr)
                {
                    if (m_requestType == ControlMessageRequestIdType::RAN_SLICING)
                    {
                        if (item->ranParameter_ID == RC_PARAM_RRM_POLICY_RATIO_LIST)
                        {
                            ParseRrmPolicyRatioList(&item->ranParameter_valueType);
                        }
                        else
                        {
                            NS_LOG_WARN("[E2SM-RC] Unexpected top-level RAN Parameter ID "
                                        << item->ranParameter_ID
                                        << " for Slice-level PRB quota (expected 1)");
                        }
                    }

                    // Extract secondary cell ID for Traffic Steering if present
                    if (m_requestType == ControlMessageRequestIdType::TS)
                    {
                        std::string octVal;
                        if (ExtractOctetStringValue(&item->ranParameter_valueType, octVal) &&
                            !octVal.empty())
                        {
                            m_secondaryCellId = octVal.back();
                            NS_LOG_INFO("Decoded CGI secondaryCellId: " << m_secondaryCellId);
                        }
                    }
                }
            }
        }
    }
    else
    {
        NS_LOG_WARN("E2SM-RC-ControlMessage format not supported: "
                    << controlMsg->ric_controlMessage_formats.present);
    }

    ASN_STRUCT_FREE(asn_DEF_E2SM_RC_ControlMessage, controlMsg);
}

void
RicControlMessage::DecodeRicControlMessage(E2AP_PDU_t* pdu)
{
    if (pdu == nullptr || pdu->choice.initiatingMessage == nullptr)
    {
        NS_LOG_ERROR("Invalid E2AP PDU for RIC Control Message");
        return;
    }

    InitiatingMessage_t* mess = pdu->choice.initiatingMessage;
    auto* request = reinterpret_cast<RICcontrolRequest_t*>(&mess->value.choice.RICcontrolRequest);
    NS_LOG_INFO(xer_fprint(stderr, &asn_DEF_RICcontrolRequest, request));

    size_t count = request->protocolIEs.list.count;
    if (count <= 0)
    {
        NS_LOG_ERROR("[E2SM-RC] received empty protocolIEs list");
        return;
    }

    for (size_t i = 0; i < count; i++)
    {
        RICcontrolRequest_IEs_t* ie = request->protocolIEs.list.array[i];
        if (ie == nullptr)
        {
            continue;
        }

        switch (ie->value.present)
        {
        case RICcontrolRequest_IEs__value_PR_RICrequestID: {
            NS_LOG_DEBUG("[E2SM-RC] RICcontrolRequest_IEs__value_PR_RICrequestID");
            m_ricRequestId = ie->value.choice.RICrequestID;
            switch (m_ricRequestId.ricRequestorID)
            {
            case 1001:
                m_requestType = ControlMessageRequestIdType::TS;
                break;
            case 1002:
                m_requestType = ControlMessageRequestIdType::QoS;
                break;
            case 1003:
                m_requestType = ControlMessageRequestIdType::RAN_SLICING;
                break;
            default:
                break;
            }
            break;
        }
        case RICcontrolRequest_IEs__value_PR_RANfunctionID: {
            m_ranFunctionId = ie->value.choice.RANfunctionID;
            NS_LOG_DEBUG("[E2SM-RC] RICcontrolRequest_IEs__value_PR_RANfunctionID: "
                         << m_ranFunctionId);
            break;
        }
        case RICcontrolRequest_IEs__value_PR_RICcallProcessID: {
            m_ricCallProcessId = ie->value.choice.RICcallProcessID;
            NS_LOG_DEBUG("[E2SM-RC] RICcontrolRequest_IEs__value_PR_RICcallProcessID");
            break;
        }
        case RICcontrolRequest_IEs__value_PR_RICcontrolHeader: {
            NS_LOG_DEBUG("[E2SM-RC] Decoding RICcontrolHeader");
            DecodeControlHeader(ie->value.choice.RICcontrolHeader);
            break;
        }
        case RICcontrolRequest_IEs__value_PR_RICcontrolMessage: {
            NS_LOG_DEBUG("[E2SM-RC] Decoding RICcontrolMessage");
            DecodeControlMessage(ie->value.choice.RICcontrolMessage);
            break;
        }
        case RICcontrolRequest_IEs__value_PR_RICcontrolAckRequest: {
            NS_LOG_DEBUG("[E2SM-RC] RICcontrolAckRequest: "
                         << ie->value.choice.RICcontrolAckRequest);
            break;
        }
        default:
            break;
        }
    }
    NS_LOG_INFO("End of DecodeRicControlMessage, slices extracted: " << m_prbQuotas.size());
}

std::string
RicControlMessage::GetSecondaryCellIdHO() const
{
    return m_secondaryCellId;
}

} // namespace ns3
