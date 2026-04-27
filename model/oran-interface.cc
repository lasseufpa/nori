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

#include "oran-interface.h"

#include "asn1c-types.h"
#include "encode_e2apv1.hpp"
//#include "ric-control-message.h"

#include "ns3/log.h"

#include <thread>

extern "C"
{
#include "InitiatingMessage.h"
#include "ProtocolIE-Field.h"
#include "RICactionType.h"
#include "ProtocolIE-Container.h"
#include "RICsubscriptionRequest.h"
#include "RICactions-ToBeSetup-List.h"
#include "E2SM-KPM-ActionDefinition.h"
}

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("E2Termination");

NS_OBJECT_ENSURE_REGISTERED(E2Termination);

TypeId
E2Termination::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::E2Termination").SetParent<Object>().AddConstructor<E2Termination>();
    return tid;
}

E2Termination::E2Termination()
{
    NS_FATAL_ERROR("Do not use the default constructor");
}

E2Termination::E2Termination(const std::string ricAddress,
                             const uint16_t ricPort,
                             const uint16_t clientPort,
                             const std::string gnbId,
                             const std::string plmnId)
    : m_ricAddress(ricAddress),
      m_ricPort(ricPort),
      m_clientPort(clientPort),
      m_gnbId(gnbId),
      m_plmnId(plmnId)
{
    NS_LOG_FUNCTION(this);
    m_e2sim = new E2Sim;
}

void
E2Termination::RegisterFunctionDescToE2Sm(long ranFunctionId,
                                          Ptr<KpmFunctionDescription> ranFunctionDescription)
{
    // create an octet string and copy the e2smbuffer
    auto* rfdBuf = (OCTET_STRING_t*)calloc(1, sizeof(OCTET_STRING_t));
    rfdBuf->buf = (uint8_t*)calloc(1, ranFunctionDescription->m_size+1);
    rfdBuf->size = ranFunctionDescription->m_size;
    memcpy(rfdBuf->buf, ranFunctionDescription->m_buffer, ranFunctionDescription->m_size);

    m_e2sim->register_e2sm(ranFunctionId, rfdBuf);
}

void
E2Termination::RegisterKpmCallbackToE2Sm(long ranFunctionId,
                                         Ptr<KpmFunctionDescription> ranFunctionDescription,
                                         SubscriptionCallback sbCb) // Use o typedef aqui
{
    RegisterFunctionDescToE2Sm(ranFunctionId, ranFunctionDescription);
    
    
    auto ptr = sbCb.target<void(*)(E2AP_PDU_t*)>();
    if (ptr) {
        m_e2sim->register_subscription_callback(ranFunctionId, *ptr);
    } else {
    
        m_e2sim->register_subscription_callback(ranFunctionId, sbCb.target<void(E2AP_PDU_t*)>());
    }
}

// void
// E2Termination::RegisterSmCallbackToE2Sm(long ranFunctionId,
//                                         Ptr<FunctionDescription> ranFunctionDescription,
//                                         SmCallback smCb)
// {
//     RegisterFunctionDescToE2Sm(ranFunctionId, ranFunctionDescription);
//     m_e2sim->register_sm_callback(ranFunctionId, smCb);
// }

void
E2Termination::Start()
{
    NS_LOG_FUNCTION(this);

    NS_ABORT_MSG_IF(m_ricAddress.empty(), "Set the RIC information first");

    // create a thread to host e2sim execution
    std::thread e2simThread(&E2Termination::DoStart, this);
    e2simThread.detach();
}

void
E2Termination::DoStart()
{
    NS_LOG_FUNCTION(this);


    NS_LOG_INFO("In ns3::E2Term:  GNB" << m_gnbId << ", clientPort " << m_clientPort << ", ricPort "
                                       << m_ricPort << ", PlmnID " << m_plmnId);

    // char* argv [] = {nullptr, &second [0], &third [0], &fourth[0], &fifth[0],&sixth[0]};
    std::string portStr = std::to_string(m_ricPort);

    std::vector<char*> args;
    
    args.push_back(strdup("e2sim"));
    //args.push_back(strdup("-address"));
    args.push_back(strdup(m_ricAddress.c_str()));
    //args.push_back(strdup("-port"));
    args.push_back(strdup(std::to_string(m_ricPort).c_str()));

    m_e2sim->run_loop(args.size(), args.data());

    
    for (char* a : args) free(a);
}

E2Termination::~E2Termination()
{
    NS_LOG_FUNCTION(this);
    delete m_e2sim;
}

E2Termination::RicSubscriptionRequest_rval_s
E2Termination::ProcessRicSubscriptionRequest(E2AP_PDU_t* sub_req_pdu)
{
    RicSubscriptionRequest_rval_s reqParams;

    RICsubscriptionRequest_t *orig_req =
        sub_req_pdu->choice.initiatingMessage->value.choice.RICsubscriptionRequest;

    ProtocolIE_Container_85P0_t *ies_conteiner = (ProtocolIE_Container_85P0_t *)orig_req->protocolIEs;

    int count = ies_conteiner->list.count;
    auto** ies_array = (RICsubscriptionRequest_IEs_t**)ies_conteiner->list.array;

    uint16_t reqRequestorId{};
    uint16_t reqInstanceId{};
    uint16_t ranFuncionId{};
    uint8_t reqActionId{};

    std::vector<long> actionIdsAccept;
    std::vector<long> actionIdsReject;

    // Iterar sobre os IEs
    for (int i = 0; i < count; i++)
    {
        RICsubscriptionRequest_IEs_t* next_ie = ies_array[i];
        RICsubscriptionRequest_IEs__value_PR pres = next_ie->value.present; 

        switch (pres)
        {
        case RICsubscriptionRequest_IEs__value_PR_RICrequestID: {
            RICrequestID_t *reqId = next_ie->value.choice.RICrequestID;
            reqRequestorId = reqId->ricRequestorID;
            reqInstanceId = reqId->ricInstanceID;
            break;
        }
        case RICsubscriptionRequest_IEs__value_PR_RANfunctionID: {
            ranFuncionId = (uint16_t)(*(next_ie->value.choice.RANfunctionID));
            break;
        }
        case RICsubscriptionRequest_IEs__value_PR_RICsubscriptionDetails: {
            RICsubscriptionDetails_t *subDetails = next_ie->value.choice.RICsubscriptionDetails;

            RICeventTriggerDefinition_t triggerDef = subDetails->ricEventTriggerDefinition;
            if (triggerDef.buf != nullptr) {
                NS_LOG_DEBUG("RIC Event Trigger Definition recebido com " << triggerDef.size << " bytes.");
            }

            RICactions_ToBeSetup_List_t *actionList = subDetails->ricAction_ToBeSetup_List;
            int actionCount = actionList->list.count;
            auto** item_array = actionList->list.array;
            bool foundAction = false;

            for (int j = 0; j < actionCount; j++)
            {
                auto* next_item = item_array[j];
                auto* actionItem = ((RICaction_ToBeSetup_ItemIEs*)next_item)->value.choice.RICaction_ToBeSetup_Item;
                
                RICactionID_t actionId = actionItem->ricActionID;
                RICactionType_t actionType = actionItem->ricActionType;

                if (!foundAction && (actionType == RICactionType_report || actionType == RICactionType_insert))
                {
                    reqActionId = actionId;
                    actionIdsAccept.push_back(reqActionId);
                    foundAction = true;
                    
                    RICactionDefinition_t* actionDefIE = actionItem->ricActionDefinition;
                    if (actionDefIE != nullptr && actionDefIE->buf != nullptr) {
                        E2SM_KPM_ActionDefinition_t *kpmActionDef = nullptr;
                        asn_dec_rval_t decode_result = asn_decode(nullptr, ATS_ALIGNED_BASIC_PER, 
                                                                  &asn_DEF_E2SM_KPM_ActionDefinition, 
                                                                  (void **)&kpmActionDef, 
                                                                  actionDefIE->buf, actionDefIE->size);

                        if (decode_result.code == RC_OK && kpmActionDef != nullptr) {
                            NS_LOG_DEBUG("E2SM-KPM Action Definition decoded successfully!");

                            
                           
                            ASN_STRUCT_FREE(asn_DEF_E2SM_KPM_ActionDefinition, kpmActionDef);
                        } else {
                            NS_LOG_ERROR("Fail to decode o E2SM-KPM Action Definition!");
                        }
                    }
                }
                else
                {
                    
                    actionIdsReject.push_back(actionId);
                }
            }
            break;
        }
        default: {
            break;
        }
        }
    }

    NS_LOG_DEBUG("Create RIC Subscription Response");
    auto* e2ap_pdu = (E2AP_PDU*)calloc(1, sizeof(E2AP_PDU));
    
    long* accept_array = actionIdsAccept.empty() ? nullptr : actionIdsAccept.data();
    long* reject_array = actionIdsReject.empty() ? nullptr : actionIdsReject.data();
    int accept_size = actionIdsAccept.size();
    int reject_size = actionIdsReject.size();

    encoding::generate_e2apv1_subscription_response_success(e2ap_pdu,
                                                            accept_array,
                                                            reject_array,
                                                            accept_size,
                                                            reject_size,
                                                            reqRequestorId,
                                                            reqInstanceId);

    NS_LOG_DEBUG("send Subscription Response");
    m_e2sim->encode_and_send_sctp_data(e2ap_pdu);

    reqParams.requestorId = reqRequestorId;
    reqParams.instanceId = reqInstanceId;
    reqParams.ranFuncionId = ranFuncionId;
    reqParams.actionId = reqActionId;
    return reqParams;
}

void
E2Termination::SendE2Message(E2AP_PDU* pdu)
{
    m_e2sim->encode_and_send_sctp_data(pdu);
}

}
 // namespace ns3