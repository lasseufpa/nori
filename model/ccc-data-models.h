#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace ns3 {

struct CccSnssai {
    uint32_t sst{0};
    std::string sd{""};
};

struct CccPartition {
    CccSnssai snssai;
    uint32_t prbQuota{0};
    uint32_t minPRBRatio{0};
    uint32_t maxPRBRatio{100};
};

struct CccBwp {
    std::string bwpContext{"DL"};
    std::string subCarrierSpacing{"15"};
    uint32_t numberOfRBs{0};
    std::vector<CccPartition> partitionList;
};

struct CccPlmnInfo {
    std::string plmnId{""};
    std::vector<CccSnssai> snssaiList;
};

struct CccCellDuControlMessage {
    std::vector<CccPlmnInfo> plmnInfoList;
    std::vector<CccBwp> bwpList;
};

struct CccCellDuIndicationMessage {
    uint16_t cellLocalId{0};
    std::string operationalState{"ENABLED"};
    std::string cellState{"ACTIVE"};
    uint16_t nrPci{0};
    std::vector<CccPlmnInfo> plmnInfoList;
    std::vector<CccBwp> bwpList;
};

inline void to_json(nlohmann::json& j, const CccSnssai& p) {
    j = nlohmann::json{{"sst", p.sst}, {"sd", p.sd}};
}
inline void from_json(const nlohmann::json& j, CccSnssai& p) {
    p.sst = j.value("sst", 0U);
    p.sd = j.value("sd", "");
}

inline void to_json(nlohmann::json& j, const CccPartition& p) {
    j = nlohmann::json{
        {"snssai", p.snssai},
        {"prbQuota", p.prbQuota},
        {"minPRBRatio", p.minPRBRatio},
        {"maxPRBRatio", p.maxPRBRatio}
    };
}
inline void from_json(const nlohmann::json& j, CccPartition& p) {
    if (j.contains("snssai")) {
        p.snssai = j.at("snssai").get<CccSnssai>();
    }
    p.prbQuota = j.value("prbQuota", 0U);
    p.minPRBRatio = j.value("minPRBRatio", 0U);
    p.maxPRBRatio = j.value("maxPRBRatio", 100U);
}

inline void to_json(nlohmann::json& j, const CccBwp& p) {
    j = nlohmann::json{
        {"bwpContext", p.bwpContext},
        {"subCarrierSpacing", p.subCarrierSpacing},
        {"numberOfRBs", p.numberOfRBs},
        {"partitionList", p.partitionList}
    };
}
inline void from_json(const nlohmann::json& j, CccBwp& p) {
    p.bwpContext = j.value("bwpContext", "DL");
    p.subCarrierSpacing = j.value("subCarrierSpacing", "15");
    p.numberOfRBs = j.value("numberOfRBs", 0U);
    if (j.contains("partitionList")) {
        p.partitionList = j.at("partitionList").get<std::vector<CccPartition>>();
    }
}

inline void to_json(nlohmann::json& j, const CccPlmnInfo& p) {
    j = nlohmann::json{{"plmnId", p.plmnId}, {"snssaiList", p.snssaiList}};
}
inline void from_json(const nlohmann::json& j, CccPlmnInfo& p) {
    p.plmnId = j.value("plmnId", "");
    if (j.contains("snssaiList")) {
        p.snssaiList = j.at("snssaiList").get<std::vector<CccSnssai>>();
    }
}

inline void to_json(nlohmann::json& j, const CccCellDuControlMessage& p) {
    j = nlohmann::json{{"plmnInfoList", p.plmnInfoList}, {"bwpList", p.bwpList}};
}
inline void from_json(const nlohmann::json& j, CccCellDuControlMessage& p) {
    if (j.contains("plmnInfoList")) {
        p.plmnInfoList = j.at("plmnInfoList").get<std::vector<CccPlmnInfo>>();
    }
    if (j.contains("bwpList")) {
        p.bwpList = j.at("bwpList").get<std::vector<CccBwp>>();
    }
}

inline void to_json(nlohmann::json& j, const CccCellDuIndicationMessage& p) {
    j = nlohmann::json{
        {"cellLocalId", p.cellLocalId},
        {"operationalState", p.operationalState},
        {"cellState", p.cellState},
        {"nrPci", p.nrPci},
        {"plmnInfoList", p.plmnInfoList},
        {"bwpList", p.bwpList}
    };
}
inline void from_json(const nlohmann::json& j, CccCellDuIndicationMessage& p) {
    p.cellLocalId = j.value("cellLocalId", static_cast<uint16_t>(0));
    p.operationalState = j.value("operationalState", "ENABLED");
    p.cellState = j.value("cellState", "ACTIVE");
    p.nrPci = j.value("nrPci", static_cast<uint16_t>(0));
    if (j.contains("plmnInfoList")) {
        p.plmnInfoList = j.at("plmnInfoList").get<std::vector<CccPlmnInfo>>();
    }
    if (j.contains("bwpList")) {
        p.bwpList = j.at("bwpList").get<std::vector<CccBwp>>();
    }
}

struct CccPlmnIdentity
{
    std::string mcc{""};
    std::string mnc{""};
};

struct CccRrmPolicyMember
{
    CccPlmnIdentity plmnId;
    CccSnssai snssai;
};

struct CccOrrmpolicyRatio
{
    std::string resourceType{"PRB"}; ///< "PRB", "PRB_DL" ou "PRB_UL"
    std::vector<CccRrmPolicyMember> rRMPolicyMemberList;
    uint32_t rRMPolicyMinRatio{0};
    uint32_t rRMPolicyMaxRatio{100};
    uint32_t rRMPolicyDedicatedRatio{0};
    uint32_t maxMimoLayers{0}; 

};

struct CccConfigStructure
{
    std::string ranConfigurationStructureName{""};
    CccOrrmpolicyRatio ranConfigurationStructure;
};

struct CccCellControlled
{
    std::string nRCellIdentity{""}; ///< hex string, ex "00066C000"
    CccPlmnIdentity plmnIdentity;
    std::vector<CccConfigStructure> listOfConfigurationStructures;
};

struct CccControlMessageStandard
{
    std::vector<CccCellControlled> listOfCellsControlled;
};


inline void
from_json(const nlohmann::json& j, CccPlmnIdentity& p)
{
    p.mcc = j.value("mcc", "");
    p.mnc = j.value("mnc", "");
}

inline void
from_json(const nlohmann::json& j, CccRrmPolicyMember& p)
{
    if (j.contains("plmnId"))
        p.plmnId = j.at("plmnId").get<CccPlmnIdentity>();
    if (j.contains("snssai"))
        p.snssai = j.at("snssai").get<CccSnssai>();
}

inline void
from_json(const nlohmann::json& j, CccOrrmpolicyRatio& p)
{
    p.resourceType = j.value("resourceType", "PRB");
    if (j.contains("rRMPolicyMemberList"))
        p.rRMPolicyMemberList =
            j.at("rRMPolicyMemberList").get<std::vector<CccRrmPolicyMember>>();
    p.rRMPolicyMinRatio       = j.value("rRMPolicyMinRatio", 0U);
    p.rRMPolicyMaxRatio       = j.value("rRMPolicyMaxRatio", 100U);
    p.rRMPolicyDedicatedRatio = j.value("rRMPolicyDedicatedRatio", 0U);
    p.maxMimoLayers = j.value("maxMimoLayers", 0U);
}

inline void
from_json(const nlohmann::json& j, CccConfigStructure& p)
{
    p.ranConfigurationStructureName = j.value("ranConfigurationStructureName", "");
    if (j.contains("newValuesOfAttributes"))
    {
        const auto& nva = j.at("newValuesOfAttributes");
        if (nva.contains("ranConfigurationStructure"))
            p.ranConfigurationStructure =
                nva.at("ranConfigurationStructure").get<CccOrrmpolicyRatio>();
    }
}

inline void
from_json(const nlohmann::json& j, CccCellControlled& p)
{
    if (j.contains("cellGlobalId"))
    {
        const auto& cgi = j.at("cellGlobalId");
        p.nRCellIdentity = cgi.value("nRCellIdentity", "");
        if (cgi.contains("plmnIdentity"))
            p.plmnIdentity = cgi.at("plmnIdentity").get<CccPlmnIdentity>();
    }
    if (j.contains("listOfConfigurationStructures"))
        p.listOfConfigurationStructures =
            j.at("listOfConfigurationStructures").get<std::vector<CccConfigStructure>>();
}

inline void
from_json(const nlohmann::json& j, CccControlMessageStandard& p)
{
    if (j.contains("controlMessageFormat"))
    {
        const auto& fmt = j.at("controlMessageFormat");
        if (fmt.contains("listOfCellsControlled"))
            p.listOfCellsControlled =
                fmt.at("listOfCellsControlled").get<std::vector<CccCellControlled>>();
    }
}

} // namespace ns3