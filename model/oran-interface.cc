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
#include <iomanip>
#include "oran-interface.h"

#include "asn1c-types.h"
#include "encode_e2apv1.hpp"
// #include "ric-control-message.h"

#include "ns3/log.h"

#include <thread>

extern "C"
{
#include "InitiatingMessage.h"
#include "ProtocolIE-Field.h"
#include "RICactionType.h"
#include "RICsubscriptionRequest.h"
#include "e2sim_sctp.h"
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
    m_e2sim = new E2SimMod(m_gnbId, m_plmnId);
}
    const std::string& E2Termination::GetGnbId() const
    {
        return m_gnbId;
    }

    const std::string& E2Termination::GetPlmnId() const
    {
        return m_plmnId;
    }   

void
E2Termination::RegisterFunctionDescToE2Sm(long ranFunctionId,
                                          Ptr<FunctionDescription> ranFunctionDescription)
{
    // create an octet string and copy the e2smbuffer
    auto* rfdBuf = (OCTET_STRING_t*)calloc(1, sizeof(OCTET_STRING_t));
    rfdBuf->buf = (uint8_t*)calloc(1, ranFunctionDescription->m_size);
    rfdBuf->size = ranFunctionDescription->m_size;
    memcpy(rfdBuf->buf, ranFunctionDescription->m_buffer, ranFunctionDescription->m_size);

    m_e2sim->register_e2sm(ranFunctionId, rfdBuf);
}

void
E2Termination::RegisterKpmCallbackToE2Sm(long ranFunctionId,
                                         Ptr<FunctionDescription> ranFunctionDescription,
                                         SubscriptionCallback sbCb)
{
    RegisterFunctionDescToE2Sm(ranFunctionId, ranFunctionDescription);
    m_e2sim->register_subscription_callback(ranFunctionId, sbCb);
}

void
E2Termination::RegisterSmCallbackToE2Sm(long ranFunctionId,
                                        Ptr<FunctionDescription> ranFunctionDescription,
                                        SubscriptionCallback smCb)
{
    RegisterFunctionDescToE2Sm(ranFunctionId, ranFunctionDescription);
    // No e2sim atualizado, register_sm_callback não existe.
    // Usamos register_subscription_callback que aceita o mesmo tipo de callback.
    m_e2sim->register_subscription_callback(ranFunctionId, smCb);
}

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

    NS_LOG_INFO("In ns3::E2Term: GNB" << m_gnbId << ", clientPort " << m_clientPort
                << ", ricPort " << m_ricPort << ", PlmnID " << ", PlmnID "
            << std::hex
            << std::setw(2) << std::setfill('0') << (int)(uint8_t)m_plmnId[0] << " "
            << std::setw(2) << (int)(uint8_t)m_plmnId[1] << " "
            << std::setw(2) << (int)(uint8_t)m_plmnId[2]
            << std::dec);

    std::vector<char*> args;
    args.push_back(strdup("e2sim"));
    args.push_back(strdup(m_ricAddress.c_str()));
    args.push_back(strdup(std::to_string(m_ricPort).c_str()));

    m_e2sim->run_loop(args.size(), args.data());

    // Limpar memória alocada com strdup
    for (auto* arg : args)
    {
        free(arg);
    }
}

E2Termination::~E2Termination()
{
    NS_LOG_FUNCTION(this);
    delete m_e2sim;
}

E2Termination::RicSubscriptionRequest_rval_s
E2Termination::ProcessRicSubscriptionRequest(E2AP_PDU_t* sub_req_pdu)
{
    // Intro params
    RicSubscriptionRequest_rval_s reqParams;

    // Record RIC Request ID
    // Go through RIC action to be Setup List
    // Find first entry with REPORT action Type
    // Record ricActionID
    // Encode subscription response

    RICsubscriptionRequest_t orig_req =
        sub_req_pdu->choice.initiatingMessage->value.choice.RICsubscriptionRequest;

    // RICsubscriptionResponse_IEs_t *ricreqid = (RICsubscriptionResponse_IEs_t*)calloc(1,
    // sizeof(RICsubscriptionResponse_IEs_t));

    int count = orig_req.protocolIEs.list.count;
    int size = orig_req.protocolIEs.list.size;

    auto** ies = (RICsubscriptionRequest_IEs_t**)orig_req.protocolIEs.list.array;

    NS_LOG_DEBUG("Number of IEs " << count);
    NS_LOG_DEBUG("Size of IEs " << size);

    RICsubscriptionRequest_IEs__value_PR pres;

    uint16_t reqRequestorId{};
    uint16_t reqInstanceId{};
    uint16_t ranFuncionId{};
    uint8_t reqActionId{};

    std::vector<long> actionIdsAccept;
    std::vector<long> actionIdsReject;

    // iterate over the IEs
    for (int i = 0; i < count; i++)
    {
        RICsubscriptionRequest_IEs_t* next_ie = ies[i];
        pres = next_ie->value.present; // value of the current IE

        switch (pres)
        {
        // IE containing the RIC Request ID
        case RICsubscriptionRequest_IEs__value_PR_RICrequestID: {
            NS_LOG_DEBUG("Processing RIC Request ID field");
            RICrequestID_t reqId = next_ie->value.choice.RICrequestID;
            reqRequestorId = reqId.ricRequestorID;
            reqInstanceId = reqId.ricInstanceID;
            NS_LOG_DEBUG("RIC Requestor ID " << reqRequestorId);
            NS_LOG_DEBUG("RIC Instance ID " << reqInstanceId);
            break;
        }
        // IE containing the RAN Function ID
        case RICsubscriptionRequest_IEs__value_PR_RANfunctionID: {
            NS_LOG_DEBUG("Processing RAN Function ID field");
            ranFuncionId = next_ie->value.choice.RANfunctionID;
            NS_LOG_DEBUG("RAN Function ID " << ranFuncionId);
            break;
        }
        case RICsubscriptionRequest_IEs__value_PR_RICsubscriptionDetails: {
            NS_LOG_DEBUG("Processing RIC Subscription Details field");
            RICsubscriptionDetails_t subDetails = next_ie->value.choice.RICsubscriptionDetails;

            // RIC Event Trigger Definition
            RICeventTriggerDefinition_t triggerDef = subDetails.ricEventTriggerDefinition;

            // TODO How to decode this field?
            uint8_t size = 20;
            auto* buf = (uint8_t*)calloc(1, size);
            memcpy(buf, &triggerDef, size);
            NS_LOG_DEBUG("RIC Event Trigger Definition " << std::to_string(*buf));

            // Sequence of actions
            RICactions_ToBeSetup_List_t actionList = subDetails.ricAction_ToBeSetup_List;
            // TODO We are ignoring the trigger definition

            int actionCount = actionList.list.count;
            NS_LOG_DEBUG("Number of actions " << actionCount);

            auto** item_array = actionList.list.array;
            bool foundAction = false;

            for (int i = 0; i < actionCount; i++)
            {
                auto* next_item = item_array[i];
                RICactionID_t actionId = ((RICaction_ToBeSetup_ItemIEs*)next_item)
                                             ->value.choice.RICaction_ToBeSetup_Item.ricActionID;
                RICactionType_t actionType =
                    ((RICaction_ToBeSetup_ItemIEs*)next_item)
                        ->value.choice.RICaction_ToBeSetup_Item.ricActionType;

                // We identify the first action whose type is REPORT or INSERT
                // That is the only one accepted; all others are rejected
                if (!foundAction &&
                    (actionType == RICactionType_report || actionType == RICactionType_insert))
                {
                    reqActionId = actionId;
                    actionIdsAccept.push_back(reqActionId);
                    NS_LOG_DEBUG("Action ID " << actionId << " accepted");
                    foundAction = true;
                }
                else
                {
                    // reqActionId = actionId;
                    // NS_LOG_DEBUG("Policy Action ID " << actionId << " processed");
                    // actionIdsReject.push_back(reqActionId);
                }
            }
            break;
        }
        default: {
            NS_LOG_DEBUG("in case default");
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

    m_e2sim->generate_e2apv1_subscription_response_success(e2ap_pdu,
                                                            accept_array,
                                                            reject_array,
                                                            accept_size,
                                                            reject_size,
                                                            reqRequestorId,
                                                            reqInstanceId,
                                                            ranFuncionId);


    NS_LOG_DEBUG("Send RIC Subscription Response");
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

} // namespace ns3