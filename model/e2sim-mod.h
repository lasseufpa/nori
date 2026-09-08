#ifndef E2SIM_MOD_H
#define E2SIM_MOD_H

#pragma once

#include <iostream>
#include <vector>
#include <fstream>
#include <cstring>
#include <unistd.h>
#include <functional>
#include <algorithm>
#include <cerrno>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <arpa/inet.h>

#include <e2sim/e2sim.hpp>
#include <e2sim/e2sim_defs.h>
#include <e2sim/encode_e2apv1.hpp> 
#include <e2sim/e2ap_message_handler.hpp>
#include <e2sim/E2AP-PDU.h>
#include <e2sim/InitiatingMessage.h>
#include <e2sim/E2setupRequest.h>
#include <e2sim/ProtocolIE-Field.h>

#include <e2sim/GlobalE2node-gNB-ID.h>
#include <e2sim/GlobalgNB-ID.h>
#include <e2sim/GNB-ID-Choice.h>
#include <e2sim/e2sim_sctp.h>
#include <e2sim/E2nodeComponentInterfaceNG.h>
#include <e2sim/ProcedureCode.h>
#include <e2sim/RICcontrolRequest.h>

class E2SimMod : public E2Sim {
public:
    std::string mod_gnb_id;
    std::string mod_plmn_id;
    uint16_t m_local_port{0};
    int m_client_fd{-1};
    bool m_running{true};

    std::unordered_map<long, OCTET_STRING_t*> m_ranFunctions;
    std::unordered_map<long, std::function<void(E2AP_PDU_t*)>> m_subCallbacks;

    E2SimMod(std::string gnb, std::string plmn, uint16_t localPort = 0)
        : mod_gnb_id(gnb), mod_plmn_id(plmn), m_local_port(localPort) {}

    ~E2SimMod()
    {
        m_running = false;
        if (m_client_fd != -1)
        {
            close(m_client_fd);
            m_client_fd = -1;
        }
    }

    void register_e2sm(long func_id, OCTET_STRING_t *ostr)
    {
        E2Sim::register_e2sm(func_id, ostr);   
        m_ranFunctions[func_id] = ostr;        
    }

    void register_subscription_callback_fn(long func_id, std::function<void(E2AP_PDU_t*)> cb)
    {
        m_subCallbacks[func_id] = cb;
    }

    void encode_and_send_sctp_data(E2AP_PDU_t* pdu)
    {
        if (m_client_fd == -1)
        {
            LOG_E("[E2SimMod-gNB%s] Cannot send E2 message: socket not connected", mod_gnb_id.c_str());
            return;
        }
        uint8_t *buf = nullptr;
        sctp_buffer_t data;

        data.len = e2ap_asn1c_encode_pdu(pdu, &buf);
        if (data.len > 0 && buf != nullptr)
        {
            memcpy(data.buffer, buf, std::min(static_cast<size_t>(data.len), static_cast<size_t>(MAX_SCTP_BUFFER)));
            sctp_send_data(m_client_fd, data);
        }
        if (buf)
        {
            free(buf);
        }
    }

    void handle_sctp_data(int& socket_fd, sctp_buffer_t& data, bool xmlenc)
    {
        E2AP_PDU_t* pdu = (E2AP_PDU_t*)calloc(1, sizeof(E2AP_PDU));
        ASN_STRUCT_RESET(asn_DEF_E2AP_PDU, pdu);

        asn_transfer_syntax syntax = ATS_ALIGNED_BASIC_PER;
        auto rval = asn_decode(nullptr, syntax, &asn_DEF_E2AP_PDU, (void**)&pdu, data.buffer, data.len);

        if (rval.code != RC_OK)
        {
            LOG_E("[E2SimMod-gNB%s] Failed to decode E2AP data from SCTP connection (code=%d)", mod_gnb_id.c_str(), rval.code);
            ASN_STRUCT_FREE(asn_DEF_E2AP_PDU, pdu);
            return;
        }

        int procedureCode = -1;
        if (pdu->present == E2AP_PDU_PR_initiatingMessage && pdu->choice.initiatingMessage)
        {
            procedureCode = pdu->choice.initiatingMessage->procedureCode;
        }
        else if (pdu->present == E2AP_PDU_PR_successfulOutcome && pdu->choice.successfulOutcome)
        {
            procedureCode = pdu->choice.successfulOutcome->procedureCode;
        }
        else if (pdu->present == E2AP_PDU_PR_unsuccessfulOutcome && pdu->choice.unsuccessfulOutcome)
        {
            procedureCode = pdu->choice.unsuccessfulOutcome->procedureCode;
        }

        LOG_I("[E2SimMod-gNB%s] Unpacked E2AP-PDU: index = %d, procedureCode = %d", mod_gnb_id.c_str(), (int)pdu->present, procedureCode);

        if (procedureCode == ProcedureCode_id_RICcontrol && pdu->present == E2AP_PDU_PR_initiatingMessage)
        {
            long func_id = -1;
            auto& ies_list = pdu->choice.initiatingMessage->value.choice.RICcontrolRequest.protocolIEs.list;
            for (int i = 0; i < ies_list.count; i++)
            {
                RICcontrolRequest_IEs_t* next_ie = (RICcontrolRequest_IEs_t*)ies_list.array[i];
                if (next_ie->value.present == RICcontrolRequest_IEs__value_PR_RANfunctionID)
                {
                    func_id = next_ie->value.choice.RANfunctionID;
                    break;
                }
            }
            LOG_I("[E2SimMod-gNB%s] Received RICcontrolRequest for RANfunctionID=%ld", mod_gnb_id.c_str(), func_id);

            auto it = m_subCallbacks.find(func_id);
            if (it != m_subCallbacks.end() && it->second)
            {
                LOG_I("[E2SimMod-gNB%s] Invoking instance callback for control request (func_id=%ld)", mod_gnb_id.c_str(), func_id);
                it->second(pdu);
            }
            else
            {
                try
                {
                    SubscriptionCallback cb = get_subscription_callback(func_id);
                    LOG_I("[E2SimMod-gNB%s] Invoking base callback for control request (func_id=%ld)", mod_gnb_id.c_str(), func_id);
                    cb(pdu);
                }
                catch (const std::out_of_range& e)
                {
                    LOG_E("[E2SimMod-gNB%s] No RAN Function callback registered for ID %ld", mod_gnb_id.c_str(), func_id);
                }
            }
            ASN_STRUCT_FREE(asn_DEF_E2AP_PDU, pdu);
        }
        else if (procedureCode == ProcedureCode_id_RICsubscription && pdu->present == E2AP_PDU_PR_initiatingMessage)
        {
            long func_id = -1;
            auto& ies_list = pdu->choice.initiatingMessage->value.choice.RICsubscriptionRequest.protocolIEs.list;
            for (int i = 0; i < ies_list.count; i++)
            {
                RICsubscriptionRequest_IEs_t* next_ie = (RICsubscriptionRequest_IEs_t*)ies_list.array[i];
                if (next_ie->value.present == RICsubscriptionRequest_IEs__value_PR_RANfunctionID)
                {
                    func_id = next_ie->value.choice.RANfunctionID;
                    break;
                }
            }
            LOG_I("[E2SimMod-gNB%s] Received RICsubscriptionRequest for RANfunctionID=%ld", mod_gnb_id.c_str(), func_id);

            auto it = m_subCallbacks.find(func_id);
            if (it != m_subCallbacks.end() && it->second)
            {
                LOG_I("[E2SimMod-gNB%s] Invoking instance callback for subscription request (func_id=%ld)", mod_gnb_id.c_str(), func_id);
                it->second(pdu);
                ASN_STRUCT_FREE(asn_DEF_E2AP_PDU, pdu);
            }
            else
            {
                ASN_STRUCT_FREE(asn_DEF_E2AP_PDU, pdu);
                e2ap_handle_sctp_data(socket_fd, data, xmlenc, this);
            }
        }
        else
        {
            ASN_STRUCT_FREE(asn_DEF_E2AP_PDU, pdu);
            e2ap_handle_sctp_data(socket_fd, data, xmlenc, this);
        }
    }

    int run_loop(int argc, char* argv[])
    {
        LOG_I("[E2SimMod-gNB%s] Start E2 Agent (E2 Simulator)", mod_gnb_id.c_str());

        bool xmlenc = false;
        options_t ops = read_input_options(argc, argv);

        LOG_I("[E2SimMod-gNB%s] After reading input options: RIC IP=%s, Port=%d, LocalPort=%u",
              mod_gnb_id.c_str(), ops.server_ip, ops.server_port, m_local_port);

        m_client_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_SCTP);
        if (m_client_fd == -1)
        {
            LOG_E("[E2SimMod-gNB%s] Error creating SCTP socket: %s", mod_gnb_id.c_str(), strerror(errno));
            return -1;
        }

        int optval = 1;
        setsockopt(m_client_fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
        setsockopt(m_client_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

        struct sockaddr_in client_addr;
        memset(&client_addr, 0, sizeof(client_addr));
        client_addr.sin_family = AF_INET;
        client_addr.sin_port = htons(m_local_port);
        client_addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(m_client_fd, (struct sockaddr*)&client_addr, sizeof(client_addr)) == -1)
        {
            LOG_E("[E2SimMod-gNB%s] bind() on local port %u returned: %s",
                  mod_gnb_id.c_str(), m_local_port, strerror(errno));
        }
        else
        {
            LOG_I("[E2SimMod-gNB%s] Bound SCTP socket to local port %u", mod_gnb_id.c_str(), m_local_port);
        }

        std::string cleanIp = (ops.server_ip != nullptr) ? ops.server_ip : "";
        cleanIp.erase(std::remove(cleanIp.begin(), cleanIp.end(), '\"'), cleanIp.end());
        cleanIp.erase(std::remove(cleanIp.begin(), cleanIp.end(), '\''), cleanIp.end());
        cleanIp.erase(std::remove(cleanIp.begin(), cleanIp.end(), ' '), cleanIp.end());

        struct sockaddr_in peer_addr;
        memset(&peer_addr, 0, sizeof(peer_addr));
        peer_addr.sin_family = AF_INET;
        peer_addr.sin_port = htons(ops.server_port);
        if (inet_pton(AF_INET, cleanIp.c_str(), &peer_addr.sin_addr) != 1)
        {
            LOG_E("[E2SimMod-gNB%s] Invalid RIC server IP address: '%s' (raw: '%s')",
                  mod_gnb_id.c_str(), cleanIp.c_str(), ops.server_ip);
            close(m_client_fd);
            m_client_fd = -1;
            return -1;
        }

        LOG_I("[E2SimMod-gNB%s] Connecting to RIC at %s:%d ...", mod_gnb_id.c_str(), cleanIp.c_str(), ops.server_port);
        if (connect(m_client_fd, (struct sockaddr*)&peer_addr, sizeof(peer_addr)) == -1)
        {
            LOG_E("[E2SimMod-gNB%s] connect() failed to RIC at %s:%d: %s",
                  mod_gnb_id.c_str(), cleanIp.c_str(), ops.server_port, strerror(errno));
            close(m_client_fd);
            m_client_fd = -1;
            return -1;
        }

        LOG_I("[E2SimMod-gNB%s] SCTP Connection established (fd=%d)", mod_gnb_id.c_str(), m_client_fd);

        E2AP_PDU_t* pdu_setup = (E2AP_PDU_t*)calloc(1, sizeof(E2AP_PDU));
        std::vector<encoding::ran_func_info> all_funcs;

        LOG_I("[E2SimMod-gNB%s] Constructing list of RAN functions", mod_gnb_id.c_str());

        for (std::pair<long, OCTET_STRING_t*> elem : m_ranFunctions)
        {
            encoding::ran_func_info current_func;
            current_func.ranFunctionId = elem.first;
            current_func.ranFunctionDesc = elem.second;
            current_func.ranFunctionRev = (long)3;
            current_func.ranFunctionOId = nullptr;
            all_funcs.push_back(current_func);
        }

        LOG_I("[E2SimMod-gNB%s] Encoding E2-SETUP-REQUEST", mod_gnb_id.c_str());
        generate_e2apv1_setup_request_parameterized(pdu_setup, all_funcs);

        size_t buffer_size = MAX_SCTP_BUFFER;
        uint8_t buffer[MAX_SCTP_BUFFER];
        sctp_buffer_t data;

        asn_enc_rval_t er = asn_encode_to_buffer(nullptr, ATS_ALIGNED_BASIC_PER, &asn_DEF_E2AP_PDU, pdu_setup, buffer, buffer_size);
        data.len = er.encoded;

        if (er.encoded > 0)
        {
            memcpy(data.buffer, buffer, er.encoded);
            if (sctp_send_data(m_client_fd, data) > 0)
            {
                LOG_I("[E2SimMod-gNB%s] Sent E2-SETUP-REQUEST as E2AP message", mod_gnb_id.c_str());
            }
            else
            {
                LOG_E("[E2SimMod-gNB%s] Failed to send E2-SETUP-REQUEST", mod_gnb_id.c_str());
            }
        }
        else
        {
            LOG_E("[E2SimMod-gNB%s] Failed to encode E2-SETUP-REQUEST", mod_gnb_id.c_str());
        }

        ASN_STRUCT_FREE(asn_DEF_E2AP_PDU, pdu_setup);

        sctp_buffer_t recv_buf;
        LOG_I("[E2SimMod-gNB%s] Waiting for SCTP data from RIC", mod_gnb_id.c_str());

        while (m_running && m_client_fd != -1)
        {
            if (sctp_receive_data(m_client_fd, recv_buf) <= 0)
            {
                LOG_I("[E2SimMod-gNB%s] SCTP connection closed by peer", mod_gnb_id.c_str());
                break;
            }

            LOG_I("[E2SimMod-gNB%s] Received new data of size %d", mod_gnb_id.c_str(), recv_buf.len);
            handle_sctp_data(m_client_fd, recv_buf, xmlenc);
            if (xmlenc) xmlenc = false;
        }

        if (m_client_fd != -1)
        {
            close(m_client_fd);
            m_client_fd = -1;
        }

        return 0;
    }

    void generate_e2apv1_subscription_response_success(E2AP_PDU *e2ap_pdu, long reqActionIdsAccepted[],
						   long reqActionIdsRejected[], int accept_size, int reject_size,
						   long reqRequestorId, long reqInstanceId, long ranFuncionId)
    {
        RICsubscriptionResponse_IEs_t *respricreqid =
            (RICsubscriptionResponse_IEs_t*)calloc(1, sizeof(RICsubscriptionResponse_IEs_t));
        
        respricreqid->id = ProtocolIE_ID_id_RICrequestID;
        respricreqid->criticality = 0;
        respricreqid->value.present = RICsubscriptionResponse_IEs__value_PR_RICrequestID;
        respricreqid->value.choice.RICrequestID.ricRequestorID = reqRequestorId;
        respricreqid->value.choice.RICrequestID.ricInstanceID = reqInstanceId;

        RICsubscriptionResponse_IEs_t *respfuncid =
            (RICsubscriptionResponse_IEs_t*)calloc(1, sizeof(RICsubscriptionResponse_IEs_t));
        respfuncid->id = ProtocolIE_ID_id_RANfunctionID;
        respfuncid->criticality = 0;
        respfuncid->value.present = RICsubscriptionResponse_IEs__value_PR_RANfunctionID;
        respfuncid->value.choice.RANfunctionID = ranFuncionId;

        RICsubscriptionResponse_IEs_t *ricactionadmitted =
            (RICsubscriptionResponse_IEs_t*)calloc(1, sizeof(RICsubscriptionResponse_IEs_t));
        ricactionadmitted->id = ProtocolIE_ID_id_RICactions_Admitted;
        ricactionadmitted->criticality = 0;
        ricactionadmitted->value.present = RICsubscriptionResponse_IEs__value_PR_RICaction_Admitted_List;

        RICaction_Admitted_List_t* admlist = 
            (RICaction_Admitted_List_t*)calloc(1,sizeof(RICaction_Admitted_List_t));
        ricactionadmitted->value.choice.RICaction_Admitted_List = *admlist;
        if (admlist) free(admlist);

        int numAccept = accept_size;
        int numReject = reject_size;

        for (int i=0; i < numAccept ; i++)
        {
            long aid = reqActionIdsAccepted[i];
            RICaction_Admitted_ItemIEs_t *admitie = (RICaction_Admitted_ItemIEs_t*)calloc(1,sizeof(RICaction_Admitted_ItemIEs_t));
            admitie->id = ProtocolIE_ID_id_RICaction_Admitted_Item;
            admitie->criticality = 0;
            admitie->value.present = RICaction_Admitted_ItemIEs__value_PR_RICaction_Admitted_Item;
            admitie->value.choice.RICaction_Admitted_Item.ricActionID = aid;
            ASN_SEQUENCE_ADD(&ricactionadmitted->value.choice.RICaction_Admitted_List.list, admitie);
        }

        RICsubscriptionResponse_t *ricsubresp = (RICsubscriptionResponse_t*)calloc(1,sizeof(RICsubscriptionResponse_t));
        ASN_SEQUENCE_ADD(&ricsubresp->protocolIEs.list, respricreqid);
        ASN_SEQUENCE_ADD(&ricsubresp->protocolIEs.list, respfuncid);
        ASN_SEQUENCE_ADD(&ricsubresp->protocolIEs.list, ricactionadmitted);

        if (numReject > 0)
        {
            RICsubscriptionResponse_IEs_t *ricactionrejected =
                (RICsubscriptionResponse_IEs_t*)calloc(1, sizeof(RICsubscriptionResponse_IEs_t));
            ricactionrejected->id = ProtocolIE_ID_id_RICactions_NotAdmitted;
            ricactionrejected->criticality = 0;
            ricactionrejected->value.present = RICsubscriptionResponse_IEs__value_PR_RICaction_NotAdmitted_List;
            
            RICaction_NotAdmitted_List_t* rejectlist = 
                (RICaction_NotAdmitted_List_t*)calloc(1,sizeof(RICaction_NotAdmitted_List_t));
            ricactionrejected->value.choice.RICaction_NotAdmitted_List = *rejectlist;
            if (rejectlist) free(rejectlist);
            
            for (int i=0; i < numReject; i++)
            {
                long aid = reqActionIdsRejected[i];
                RICaction_NotAdmitted_ItemIEs_t *noadmitie = (RICaction_NotAdmitted_ItemIEs_t*)calloc(1,sizeof(RICaction_NotAdmitted_ItemIEs_t));
                noadmitie->id = ProtocolIE_ID_id_RICaction_NotAdmitted_Item;
                noadmitie->criticality = 0;
                noadmitie->value.present = RICaction_NotAdmitted_ItemIEs__value_PR_RICaction_NotAdmitted_Item;
                noadmitie->value.choice.RICaction_NotAdmitted_Item.ricActionID = aid;
                
                ASN_SEQUENCE_ADD(&ricactionrejected->value.choice.RICaction_NotAdmitted_List.list, noadmitie);
            }
            ASN_SEQUENCE_ADD(&ricsubresp->protocolIEs.list, ricactionrejected);      
        }

        SuccessfulOutcome__value_PR pres2;
        pres2 = SuccessfulOutcome__value_PR_RICsubscriptionResponse;
        SuccessfulOutcome_t *successoutcome = (SuccessfulOutcome_t*)calloc(1, sizeof(SuccessfulOutcome_t));
        successoutcome->procedureCode = ProcedureCode_id_RICsubscription;
        successoutcome->criticality = 0;
        successoutcome->value.present = pres2;
        successoutcome->value.choice.RICsubscriptionResponse = *ricsubresp;
        if (ricsubresp) free(ricsubresp);

        E2AP_PDU_PR pres5 = E2AP_PDU_PR_successfulOutcome;
        e2ap_pdu->present = pres5;
        e2ap_pdu->choice.successfulOutcome = successoutcome;

        char error_buf[300] = {0, };
        size_t errlen = 0;
        asn_check_constraints(&asn_DEF_E2AP_PDU, e2ap_pdu, error_buf, &errlen);

        LOG_I("[E2SimMod-gNB%s] Subscription response created", mod_gnb_id.c_str());
        xer_fprint(stderr, &asn_DEF_E2AP_PDU, e2ap_pdu); 
    }

private:
    void generate_e2apv1_setup_request_parameterized(E2AP_PDU_t *e2ap_pdu, std::vector<encoding::ran_func_info> all_funcs)
    {
        uint32_t gnb_val = std::stoul(mod_gnb_id); 
        uint32_t encoded_gnb = gnb_val << 3;
        
        BIT_STRING_t *gnb_bstring = (BIT_STRING_t*)calloc(1, sizeof(BIT_STRING_t));
        gnb_bstring->buf = (uint8_t*)calloc(1, 4); 
        gnb_bstring->size = 4; 
        gnb_bstring->bits_unused = 3;

        gnb_bstring->buf[0] = (encoded_gnb >> 24) & 0xFF;
        gnb_bstring->buf[1] = (encoded_gnb >> 16) & 0xFF;
        gnb_bstring->buf[2] = (encoded_gnb >> 8)  & 0xFF;
        gnb_bstring->buf[3] = (encoded_gnb)       & 0xFF;

        OCTET_STRING_t *plmn = (OCTET_STRING_t*)calloc(1, sizeof(OCTET_STRING_t));
        plmn->buf = (uint8_t*)calloc(3, sizeof(uint8_t));
        plmn->size = 3; 
        std::memcpy(plmn->buf, mod_plmn_id.data(), 3);

        GNB_ID_Choice_t *gnbchoice = (GNB_ID_Choice_t*)calloc(1,sizeof(GNB_ID_Choice_t));
        GNB_ID_Choice_PR pres2 = GNB_ID_Choice_PR_gnb_ID;
        gnbchoice->present = pres2;
        gnbchoice->choice.gnb_ID = *gnb_bstring;
        if (gnb_bstring) free(gnb_bstring);

        GlobalgNB_ID_t *gnb = (GlobalgNB_ID_t*)calloc(1, sizeof(GlobalgNB_ID_t));
        gnb->plmn_id = *plmn;
        gnb->gnb_id = *gnbchoice;
        if (plmn) free(plmn);
        if (gnbchoice) free(gnbchoice);

        GlobalE2node_gNB_ID_t *e2gnb = (GlobalE2node_gNB_ID_t*)calloc(1, sizeof(GlobalE2node_gNB_ID_t));
        e2gnb->global_gNB_ID = *gnb;
        if (gnb) free(gnb);

        GlobalE2node_ID_t *globale2nodeid = (GlobalE2node_ID_t*)calloc(1, sizeof(GlobalE2node_ID_t));
        GlobalE2node_ID_PR pres = GlobalE2node_ID_PR_gNB;
        globale2nodeid->present = pres;
        globale2nodeid->choice.gNB = e2gnb;
        
        E2setupRequestIEs_t *e2setuprid = (E2setupRequestIEs_t*)calloc(1, sizeof(E2setupRequestIEs_t));
        E2setupRequestIEs__value_PR pres3 = E2setupRequestIEs__value_PR_GlobalE2node_ID;
        e2setuprid->id = 3;
        e2setuprid->criticality = 0;
        e2setuprid->value.choice.GlobalE2node_ID = *globale2nodeid;
        e2setuprid->value.present = pres3;
        if(globale2nodeid) free(globale2nodeid);

        auto *e2txid = (E2setupRequestIEs_t *)calloc(1, sizeof(E2setupRequestIEs_t));
        e2txid->id = ProtocolIE_ID_id_TransactionID;
        e2txid->criticality = 0;
        e2txid->value.present = E2setupRequestIEs__value_PR_TransactionID;
        e2txid->value.choice.TransactionID = 1;

        auto *ranFlistIEs = (E2setupRequestIEs_t *)calloc(1, sizeof(E2setupRequestIEs_t));
        ASN_STRUCT_RESET(asn_DEF_E2setupRequestIEs, ranFlistIEs);
        ranFlistIEs->criticality = 0;
        ranFlistIEs->id = ProtocolIE_ID_id_RANfunctionsAdded;
        ranFlistIEs->value.present = E2setupRequestIEs__value_PR_RANfunctions_List;

        for (size_t i = 0; i < all_funcs.size(); i++)
        {
            encoding::ran_func_info nextRanFunc = all_funcs.at(i);
            long nextRanFuncId = nextRanFunc.ranFunctionId;
            OCTET_STRING_t *nextRanFuncDesc = nextRanFunc.ranFunctionDesc;
            long nextRanFuncRev = nextRanFunc.ranFunctionRev;

            auto *itemIes = (RANfunction_ItemIEs_t *)calloc(1, sizeof(RANfunction_ItemIEs_t));
            itemIes->id = ProtocolIE_ID_id_RANfunction_Item;
            itemIes->criticality = Criticality_reject;
            itemIes->value.present = RANfunction_ItemIEs__value_PR_RANfunction_Item;
            itemIes->value.choice.RANfunction_Item.ranFunctionID = nextRanFuncId;

            // Deep-copy OID buffer so ASN_STRUCT_FREE does not double-free
            const char *oid_str = "1.3.6.1.4.1.53148.1.2.2.2"; 
            size_t oid_len = strlen(oid_str);
            itemIes->value.choice.RANfunction_Item.ranFunctionOID.buf = (uint8_t*)calloc(1, oid_len);
            std::memcpy(itemIes->value.choice.RANfunction_Item.ranFunctionOID.buf, oid_str, oid_len);
            itemIes->value.choice.RANfunction_Item.ranFunctionOID.size = oid_len;

            // Deep-copy Function Definition buffer so ASN_STRUCT_FREE does not corrupt m_ranFunctions
            if (nextRanFuncDesc && nextRanFuncDesc->buf && nextRanFuncDesc->size > 0)
            {
                itemIes->value.choice.RANfunction_Item.ranFunctionDefinition.buf = (uint8_t*)calloc(1, nextRanFuncDesc->size);
                std::memcpy(itemIes->value.choice.RANfunction_Item.ranFunctionDefinition.buf, nextRanFuncDesc->buf, nextRanFuncDesc->size);
                itemIes->value.choice.RANfunction_Item.ranFunctionDefinition.size = nextRanFuncDesc->size;
            }

            itemIes->value.choice.RANfunction_Item.ranFunctionRevision = nextRanFuncRev;
            
            ASN_SEQUENCE_ADD(&ranFlistIEs->value.choice.RANfunctions_List.list, itemIes);
        } 

        auto *e2configIE = (E2setupRequestIEs_t *)calloc(1, sizeof(E2setupRequestIEs_t));
        e2configIE->id = ProtocolIE_ID_id_E2nodeComponentConfigAddition;
        e2configIE->criticality = Criticality_reject;
        e2configIE->value.present = E2setupRequestIEs__value_PR_E2nodeComponentConfigAddition_List;

        auto *e2configAdditionItem = (E2nodeComponentConfigAddition_ItemIEs_t *)calloc(1, sizeof(E2nodeComponentConfigAddition_ItemIEs_t));
        e2configAdditionItem->id = ProtocolIE_ID_id_E2nodeComponentConfigAddition_Item;
        e2configAdditionItem->criticality = Criticality_reject;
        e2configAdditionItem->value.present = E2nodeComponentConfigAddition_ItemIEs__value_PR_E2nodeComponentConfigAddition_Item;

        e2configAdditionItem->value.choice.E2nodeComponentConfigAddition_Item.e2nodeComponentInterfaceType = E2nodeComponentInterfaceType_ng;
        e2configAdditionItem->value.choice.E2nodeComponentConfigAddition_Item.e2nodeComponentID.present = E2nodeComponentID_PR_e2nodeComponentInterfaceTypeNG;

        auto *intfNG = (E2nodeComponentInterfaceNG *) calloc(1, sizeof(E2nodeComponentInterfaceNG));
        
        OCTET_STRING_t nginterf;
        nginterf.buf = (uint8_t*)calloc(1, 8);
        memcpy(nginterf.buf, (uint8_t *)"nginterf", 8);
        nginterf.size = 8;
        intfNG->amf_name = nginterf;

        e2configAdditionItem->value.choice.E2nodeComponentConfigAddition_Item.e2nodeComponentID.choice.e2nodeComponentInterfaceTypeNG = intfNG;

        OCTET_STRING_t reqPart;
        reqPart.buf = (uint8_t*)calloc(1, 7);
        memcpy(reqPart.buf, (uint8_t *)"reqpart", 7);
        reqPart.size = 7;
        e2configAdditionItem->value.choice.E2nodeComponentConfigAddition_Item.e2nodeComponentConfiguration.e2nodeComponentRequestPart = reqPart;

        OCTET_STRING_t resPart;
        resPart.buf = (uint8_t*)calloc(1, 7);
        memcpy(resPart.buf, (uint8_t *)"respart", 7);
        resPart.size = 7;
        e2configAdditionItem->value.choice.E2nodeComponentConfigAddition_Item.e2nodeComponentConfiguration.e2nodeComponentResponsePart = resPart;

        ASN_SEQUENCE_ADD(&e2configIE->value.choice.E2nodeComponentConfigAddition_List.list, e2configAdditionItem);

        E2setupRequest_t *e2setupreq = (E2setupRequest_t*)calloc(1, sizeof(E2setupRequest_t));
        ASN_SEQUENCE_ADD(&e2setupreq->protocolIEs.list, e2txid);
        ASN_SEQUENCE_ADD(&e2setupreq->protocolIEs.list, e2setuprid);
        if (!all_funcs.empty())
        {
            ASN_SEQUENCE_ADD(&e2setupreq->protocolIEs.list, ranFlistIEs);
        }
        ASN_SEQUENCE_ADD(&e2setupreq->protocolIEs.list, e2configIE);

        InitiatingMessage__value_PR pres4 = InitiatingMessage__value_PR_E2setupRequest;
        InitiatingMessage_t *initmsg = (InitiatingMessage_t*)calloc(1, sizeof(InitiatingMessage_t));

        initmsg->procedureCode = ProcedureCode_id_E2setup;
        initmsg->criticality = Criticality_reject;
        initmsg->value.present = pres4;
        initmsg->value.choice.E2setupRequest = *e2setupreq;
        if (e2setupreq) free(e2setupreq);

        E2AP_PDU_PR pres5 = E2AP_PDU_PR_initiatingMessage;
        e2ap_pdu->present = pres5;
        e2ap_pdu->choice.initiatingMessage = initmsg;  
    }
};

#endif // E2SIM_MOD_H