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

static RicControlMessage::SlicePRBQuota
ParseSliceGroupStructure(const RANParameter_STRUCTURE_t* groupStruct)
{
    RicControlMessage::SlicePRBQuota quota;
    quota.sst = 0;
    quota.sd = 0;
    quota.maxPRBRatio = 100;
    quota.minPRBRatio = 0;
    quota.dedicatePRBRatio = 0;

    if (groupStruct == nullptr || groupStruct->sequence_of_ranParameters == nullptr)
    {
        return quota;
    }

    int count = groupStruct->sequence_of_ranParameters->list.count;
    for (int i = 0; i < count; i++)
    {
        auto* item = groupStruct->sequence_of_ranParameters->list.array[i];
        if (item == nullptr || item->ranParameter_valueType == nullptr)
        {
            continue;
        }

        switch (item->ranParameter_ID)
        {
        case 3: // RRM Policy SST
            ExtractSstValue(item->ranParameter_valueType, quota.sst);
            break;
        case 4: // Max PRB Policy Ratio
            ExtractIntValue(item->ranParameter_valueType, quota.maxPRBRatio);
            break;
        case 5: // Min PRB Policy Ratio
            ExtractIntValue(item->ranParameter_valueType, quota.minPRBRatio);
            break;
        case 6: // Dedicated PRB Policy Ratio
            ExtractIntValue(item->ranParameter_valueType, quota.dedicatePRBRatio);
            break;
        default:
            break;
        }
    }
    return quota;
}

void
RicControlMessage::ParseRanParameterStructureForSlicing(const RANParameter_STRUCTURE_t* structure)
{
    if (structure == nullptr || structure->sequence_of_ranParameters == nullptr)
    {
        return;
    }

    int count = structure->sequence_of_ranParameters->list.count;
    for (int i = 0; i < count; i++)
    {
        auto* item = structure->sequence_of_ranParameters->list.array[i];
        if (item == nullptr || item->ranParameter_valueType == nullptr)
        {
            continue;
        }

        if (item->ranParameter_ID == 2) // RRM Policy Ratio Group
        {
            if (item->ranParameter_valueType->present ==
                    RANParameter_ValueType_PR_ranP_Choice_Structure &&
                item->ranParameter_valueType->choice.ranP_Choice_Structure != nullptr &&
                item->ranParameter_valueType->choice.ranP_Choice_Structure
                        ->ranParameter_Structure != nullptr)
            {
                SlicePRBQuota q = ParseSliceGroupStructure(
                    item->ranParameter_valueType->choice.ranP_Choice_Structure
                        ->ranParameter_Structure);
                m_prbQuotas.push_back(q);
            }
        }
        else if (item->ranParameter_ID == 1) // RRM Policy Ratio List
        {
            ParseRanParameterValueTypeForSlicing(item->ranParameter_ID,
                                                 item->ranParameter_valueType);
        }
        else
        {
            ParseRanParameterValueTypeForSlicing(item->ranParameter_ID,
                                                 item->ranParameter_valueType);
        }
    }
}

void
RicControlMessage::ParseRanParameterListForSlicing(const RANParameter_LIST_t* list)
{
    if (list == nullptr)
    {
        return;
    }

    int count = list->list_of_ranParameter.list.count;
    for (int i = 0; i < count; i++)
    {
        auto* structItem = list->list_of_ranParameter.list.array[i];
        if (structItem != nullptr)
        {
            SlicePRBQuota q = ParseSliceGroupStructure(structItem);
            m_prbQuotas.push_back(q);
        }
    }
}

void
RicControlMessage::ParseRanParameterValueTypeForSlicing(long paramId,
                                                        const RANParameter_ValueType_t* valueType)
{
    if (valueType == nullptr)
    {
        return;
    }

    switch (valueType->present)
    {
    case RANParameter_ValueType_PR_ranP_Choice_Structure:
        if (valueType->choice.ranP_Choice_Structure != nullptr &&
            valueType->choice.ranP_Choice_Structure->ranParameter_Structure != nullptr)
        {
            if (paramId == 2) // Single Group
            {
                SlicePRBQuota q = ParseSliceGroupStructure(
                    valueType->choice.ranP_Choice_Structure->ranParameter_Structure);
                m_prbQuotas.push_back(q);
            }
            else
            {
                ParseRanParameterStructureForSlicing(
                    valueType->choice.ranP_Choice_Structure->ranParameter_Structure);
            }
        }
        break;

    case RANParameter_ValueType_PR_ranP_Choice_List:
        if (valueType->choice.ranP_Choice_List != nullptr &&
            valueType->choice.ranP_Choice_List->ranParameter_List != nullptr)
        {
            ParseRanParameterListForSlicing(
                valueType->choice.ranP_Choice_List->ranParameter_List);
        }
        break;

    default:
        break;
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

            if (m_ricControlActionId == 6)
            {
                m_requestType = ControlMessageRequestIdType::RAN_SLICING;
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
                    ParseRanParameterValueTypeForSlicing(item->ranParameter_ID,
                                                         &item->ranParameter_valueType);

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
