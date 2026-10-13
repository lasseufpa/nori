#include "ccc-control-message.h"
#include "ns3/log.h"

extern "C" {
#include "InitiatingMessage.h"
#include "RICcontrolRequest.h"
#include "ProtocolIE-Field.h"
}

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("CccControlMessage");

CccControlMessage::CccControlMessage(E2AP_PDU_t* pdu)
{
    if (!pdu || pdu->present != E2AP_PDU_PR_initiatingMessage) {
        NS_LOG_WARN("CccControlMessage: PDU null or unexpected type");
        return;
    }

    auto& ies =
        pdu->choice.initiatingMessage->value.choice.RICcontrolRequest.protocolIEs.list;

    for (int i = 0; i < ies.count; ++i) {
        auto* ie = reinterpret_cast<RICcontrolRequest_IEs_t*>(ies.array[i]);
        if (ie->value.present == RICcontrolRequest_IEs__value_PR_RICcontrolHeader) {
            std::string hdr(
                reinterpret_cast<char*>(ie->value.choice.RICcontrolHeader.buf),
                ie->value.choice.RICcontrolHeader.size);
            try {
                auto jh = nlohmann::json::parse(hdr);
                if (jh.contains("controlHeaderFormat")) {
                    m_ricStyleType =
                        jh.at("controlHeaderFormat").value("ricStyleType", -1);
                }
                NS_LOG_INFO("CCC RICcontrolHeader: ricStyleType=" << m_ricStyleType);
            } catch (const std::exception& e) {
                NS_LOG_WARN("CCC RICcontrolHeader: parse failed: " << e.what());
            }
            break;
        }
    }
    if (m_ricStyleType != -1 && m_ricStyleType != 2) {
        NS_LOG_WARN("CCC Control: ricStyleType=" << m_ricStyleType
                    << "Not supported (expected: 2). Message discarded.");
        return;
    }

    for (int i = 0; i < ies.count; ++i) {
        auto* ie = reinterpret_cast<RICcontrolRequest_IEs_t*>(ies.array[i]);
        if (ie->value.present != RICcontrolRequest_IEs__value_PR_RICcontrolMessage) {
            continue;
        }

        std::string payload(
            reinterpret_cast<char*>(ie->value.choice.RICcontrolMessage.buf),
            ie->value.choice.RICcontrolMessage.size);

        try {
            auto j = nlohmann::json::parse(payload);

            if (j.contains("controlMessageFormat")) {
                auto stdMsg = j.get<CccControlMessageStandard>();

                for (const auto& cell : stdMsg.listOfCellsControlled) {
                    for (const auto& cfgStruct : cell.listOfConfigurationStructures) {
                        const auto& rrm = cfgStruct.ranConfigurationStructure;
                        for (const auto& member : rrm.rRMPolicyMemberList) {
                            if (member.snssai.sst == 0) {
                                continue; // SST=0 is invalid
                            }
                            SlicePRBQuota q;
                            q.sliceId          = member.snssai.sst;
                            q.dedicatePRBRatio = static_cast<long>(rrm.rRMPolicyDedicatedRatio);
                            q.minPRBRatio      = static_cast<long>(rrm.rRMPolicyMinRatio);
                            q.maxPRBRatio      = static_cast<long>(rrm.rRMPolicyMaxRatio);
                            q.resourceType     = rrm.resourceType;
                            q.maxMimoLayers = rrm.maxMimoLayers;
                            m_prbQuotas.push_back(q);
                            NS_LOG_INFO("CCC Control (O-RRMPolicyRatio): SST="
                                        << static_cast<uint32_t>(q.sliceId)
                                        << " ded=" << q.dedicatePRBRatio
                                        << "% min=" << q.minPRBRatio
                                        << "% max=" << q.maxPRBRatio << "%"
                                        << " res=" << q.resourceType
                                        << " maxMimoLayers=" << q.maxMimoLayers);
                        }
                    }
                }
                m_valid = !m_prbQuotas.empty();
                if (!m_valid) {
                    NS_LOG_WARN("CCC Control (O-RAN): no quota extracted from the payload.");
                }

            } else if (j.contains("bwpList")) {s
                m_controlData = j.get<CccCellDuControlMessage>();
                m_valid = true;
                NS_LOG_INFO("CCC Control (internal/legacy format) decoded.");

            } else {
                NS_LOG_WARN("CCC Control: unknown JSON format (no 'controlMessageFormat' or 'bwpList').");
            }
        } catch (const std::exception& e) {
            NS_LOG_ERROR("Error decoding JSON CCC Control: " << e.what());
        }
        break; 
    }
}

std::vector<SlicePRBQuota>
CccControlMessage::GetPrbQuotas() const
{
    if (!m_prbQuotas.empty()) {
        return m_prbQuotas;
    }

    std::vector<SlicePRBQuota> quotas;
    for (const auto& bwp : m_controlData.bwpList) {
        for (const auto& part : bwp.partitionList) {
            if (part.snssai.sst == 0) {
                continue;
            }
            SlicePRBQuota q;
            q.sliceId          = part.snssai.sst;
            q.dedicatePRBRatio = static_cast<long>(part.prbQuota);
            q.minPRBRatio      = static_cast<long>(part.minPRBRatio);
            q.maxPRBRatio      = static_cast<long>(part.maxPRBRatio);
            quotas.push_back(q);
        }
    }
    return quotas;
}

} // namespace ns3