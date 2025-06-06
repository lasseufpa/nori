/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2022 Northeastern University
 * Copyright (c) 2022 Sapienza, University of Rome
 * Copyright (c) 2022 University of Padova
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 *
 *
 * Author: Andrea Lacava <thecave003@gmail.com>
 *         Tommaso Zugno <tommasozugno@gmail.com>
 *         Michele Polese <michele.polese@gmail.com>
 */

#include "ric-control-message.h"

#include "asn1c-types.h"

#include "ns3/log.h"

#include <bitset>

extern "C"
{
#include <E2SM-RC-ControlHeader.h>
#include <E2SM-RC-ControlMessage-Format1.h>
#include <RRMPolicyRatioList.h>
#include <RRMPolicyRatioGroup.h>
#include <RRMPolicyMember.h>
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

void
RicControlMessage::DecodeRicControlMessage(E2AP_PDU_t* pdu)
{
    InitiatingMessage_t* mess = pdu->choice.initiatingMessage;
    auto* request = (RICcontrolRequest_t*)&mess->value.choice.RICcontrolRequest;
    NS_LOG_INFO(xer_fprint(stderr, &asn_DEF_RICcontrolRequest, request));

    size_t count = request->protocolIEs.list.count;
    if (count <= 0)
    {
        NS_LOG_ERROR("[E2SM] received empty list");
        return;
    }

    for (size_t i = 0; i < count; i++)
    {
        RICcontrolRequest_IEs_t* ie = request->protocolIEs.list.array[i];
        switch (ie->value.present)
        {
        case RICcontrolRequest_IEs__value_PR_RICrequestID: {
            NS_LOG_DEBUG("[E2SM] RICcontrolRequest_IEs__value_PR_RICrequestID");
            m_ricRequestId = ie->value.choice.RICrequestID;
            switch (m_ricRequestId.ricRequestorID)
            {
            case 1001: {
                NS_LOG_DEBUG("TS xApp message");
                m_requestType = ControlMessageRequestIdType::TS;
                break;
            }
            case 1002: {
                NS_LOG_DEBUG("QoS xApp message");
                m_requestType = ControlMessageRequestIdType::QoS;
                break;
            }
            case 1003: {
                NS_LOG_DEBUG("RAN Slicing control message");
                m_requestType = ControlMessageRequestIdType::RAN_SLICING;
                break;
            }
            }
            break;
        }
        case RICcontrolRequest_IEs__value_PR_RANfunctionID: {
            m_ranFunctionId = ie->value.choice.RANfunctionID;

            NS_LOG_DEBUG("[E2SM] RICcontrolRequest_IEs__value_PR_RANfunctionID");
            break;
        }
        case RICcontrolRequest_IEs__value_PR_RICcallProcessID: {
            m_ricCallProcessId = ie->value.choice.RICcallProcessID;
            NS_LOG_DEBUG("[E2SM] RICcontrolRequest_IEs__value_PR_RICcallProcessID");
            break;
        }
        case RICcontrolRequest_IEs__value_PR_RICcontrolHeader: {
            NS_LOG_DEBUG("[E2SM] RICcontrolRequest_IEs__value_PR_RICcontrolHeader");
            // xer_fprint(stderr, &asn_DEF_RICcontrolHeader, &ie->value.choice.RICcontrolHeader);

            auto* e2smControlHeader =
                (E2SM_RC_ControlHeader_t*)calloc(1, sizeof(E2SM_RC_ControlHeader_t));
            ASN_STRUCT_RESET(asn_DEF_E2SM_RC_ControlHeader, e2smControlHeader);
            asn_decode(nullptr,
                       ATS_ALIGNED_BASIC_PER,
                       &asn_DEF_E2SM_RC_ControlHeader,
                       (void**)&e2smControlHeader,
                       ie->value.choice.RICcontrolHeader.buf,
                       ie->value.choice.RICcontrolHeader.size);

            NS_LOG_INFO(xer_fprint(stderr, &asn_DEF_E2SM_RC_ControlHeader, e2smControlHeader));
            if (e2smControlHeader->present == E2SM_RC_ControlHeader_PR_controlHeader_Format1)
            {
                m_e2SmRcControlHeaderFormat1 = e2smControlHeader->choice.controlHeader_Format1;

                auto *prList = m_e2SmRcControlHeaderFormat1->rrmPolicyList;

                for (size_t i = 0; i < prList->list.count; ++i)
                {
                    auto *grp = prList->list.array[i];

                    uint32_t sliceId = 0;
                    if (grp->rrmPolicy.rrmPolicyMemberList.list.count > 0){
                        auto *member = grp->rrmPolicy.rrmPolicyMemberList.list.array[0];
                        if (grp->rrmPolicy.rrmPolicyMemberList.list.count > 0) {
                            // Get the slice ID from the first member
                            RRMPolicyMember_t *member =
                              grp->rrmPolicy.rrmPolicyMemberList.list.array[0];
                            if (member->sNSSAI) {
                                auto *raw = member->sNSSAI;

                                SNSSAI_t *snssai = reinterpret_cast<SNSSAI_t*>(raw);
                                //S_NSSAI *snssai = member->sNSSAI;
                                
                                if (snssai->sST.buf && snssai->sST.size > 0) {
                                    sliceId = (uint32_t) snssai->sST.buf[0];
                              }
                            }
                          }
                        //if (member->sNSSAI && member->sNSSAI->sST.buf && member->sNSSAI->sST.size  > 0){
                        //    sliceId = member->sNSSAI->sST.buf[0];
                        //}
                    }
                    long maxPRBRatio = grp->maxPRBPolicyRatio ? *grp->maxPRBPolicyRatio : 100;
                    long minPRBRatio = grp->minPRBPolicyRatio ? *grp->minPRBPolicyRatio : 100;
                    long dedicatePRBRatio = grp->dedicatedPRBPolicyRatio ? *grp->dedicatedPRBPolicyRatio : 100;

                    m_prbQuotas.push_back({sliceId, maxPRBRatio, minPRBRatio, dedicatePRBRatio});
                }

                //uint8_t sliceId = prb->sliceID.sST.buf[0];


                // Get the UE ID
                // uint8_t ueId = m_e2SmRcControlHeaderFormat1->ueId.buf[0];
                //m_slicePRBQuota = {sliceId,
                //                   prb->maxPRBRatio,
                //                   prb->minPRBRatio,
                //                   prb->dedicatePRBRatio};
                // m_e2SmRcControlHeaderFormat1->ric_ControlAction_ID;
                // m_e2SmRcControlHeaderFormat1->ric_ControlStyle_Type;
            }
            else
            {
                NS_LOG_DEBUG("[E2SM] Error in checking format of E2SM Control Header");
            }
            break;
        }
        case RICcontrolRequest_IEs__value_PR_RICcontrolMessage: {
            NS_LOG_DEBUG("[E2SM] RICcontrolRequest_IEs__value_PR_RICcontrolMessage");
            // xer_fprint(stderr, &asn_DEF_RICcontrolMessage, &ie->value.choice.RICcontrolMessage);

            auto* e2SmControlMessage =
                (E2SM_RC_ControlMessage_t*)calloc(1, sizeof(E2SM_RC_ControlMessage_t));
            ASN_STRUCT_RESET(asn_DEF_E2SM_RC_ControlMessage, e2SmControlMessage);
            asn_decode(nullptr,
                       ATS_ALIGNED_BASIC_PER,
                       &asn_DEF_E2SM_RC_ControlMessage,
                       (void**)&e2SmControlMessage,
                       ie->value.choice.RICcontrolMessage.buf,
                       ie->value.choice.RICcontrolMessage.size);

            NS_LOG_INFO(xer_fprint(stderr, &asn_DEF_E2SM_RC_ControlMessage, e2SmControlMessage));

            if (e2SmControlMessage->present == E2SM_RC_ControlMessage_PR_controlMessage_Format1)
            {
                NS_LOG_DEBUG("[E2SM] E2SM_RC_ControlMessage_PR_controlMessage_Format1");
                E2SM_RC_ControlMessage_Format1_t* e2SmRcControlMessageFormat1 =
                    e2SmControlMessage->choice.controlMessage_Format1;
                m_valuesExtracted =
                    ExtractRANParametersFromControlMessage(e2SmRcControlMessageFormat1);

                if (m_requestType == ControlMessageRequestIdType::TS)
                {
                    // Get and parse the secondaty cell id according to 3GPP TS 38.473,
                    // Section 9.2.2.1
                    for (RANParameterItem item : m_valuesExtracted)
                    {
                        if (item.m_valueType == RANParameterItem::ValueType::OctectString)
                        {
                            // First 3 digits are the PLMNID (always 111), last digit is CellId
                            std::string cgi = item.m_valueStr->DecodeContent();
                            NS_LOG_INFO("Decoded CGI value is: " << cgi);
                            m_secondaryCellId = cgi.back();
                        }
                    }
                }
            }
            else
            {
                NS_LOG_DEBUG("[E2SM] Error in checking format of E2SM Control Message");
            }
            break;
        }
        case RICcontrolRequest_IEs__value_PR_RICcontrolAckRequest: {
            NS_LOG_DEBUG("[E2SM] RICcontrolRequest_IEs__value_PR_RICcontrolAckRequest");

            switch (ie->value.choice.RICcontrolAckRequest)
            {
            case RICcontrolAckRequest_noAck: {
                NS_LOG_DEBUG("[E2SM] RIC Control ack value: NO ACK");
                break;
            }
            case RICcontrolAckRequest_ack: {
                NS_LOG_DEBUG("[E2SM] RIC Control ack value: ACK");
                break;
            }
            // case RICcontrolAckRequest_nAck: {
            //     NS_LOG_DEBUG("[E2SM] RIC Control ack value: NACK");
            //     break;
            // }
            default: {
                NS_LOG_DEBUG("[E2SM] RIC Control ack value unknown");
                break;
            }
            }
            break;
        }
        case RICcontrolRequest_IEs__value_PR_NOTHING: {
            NS_LOG_DEBUG("[E2SM] RICcontrolRequest_IEs__value_PR_NOTHING");
            NS_LOG_DEBUG("[E2SM] Nothing");
            break;
        }
        default: {
            NS_LOG_DEBUG("[E2SM] RIC Control value unknown");
            break;
        }
        }
    }
    NS_LOG_INFO("End of DecodeRicControlMessage");
}

std::string
RicControlMessage::GetSecondaryCellIdHO()
{
    return m_secondaryCellId;
}

std::vector<RANParameterItem>
RicControlMessage::ExtractRANParametersFromControlMessage(
    E2SM_RC_ControlMessage_Format1_t* e2SmRcControlMessageFormat1)
{
    // Returning an empty vector for now
    std::vector<RANParameterItem> ranParameterList;
    /**
     *
     int count = e2SmRcControlMessageFormat1->ranParameters_List->list.count;
     std::cout << "flag" << std::endl;
     for (int i = 0; i < count; i++)
     {
        RANParameter_Item_t* ranParameterItem =
        e2SmRcControlMessageFormat1->ranParameters_List->list.array[i];
        for (RANParameterItem extractedParameter :
        RANParameterItem::ExtractRANParametersFromRANParameter(ranParameterItem))
        {
            ranParameterList.push_back(extractedParameter);
        }
    }
    */
    return ranParameterList;
}

} // namespace ns3
