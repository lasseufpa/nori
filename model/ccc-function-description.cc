#include "ccc-function-description.h"

#include "ns3/log.h"

#include <nlohmann/json.hpp>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("CccFunctionDescription");

using json = nlohmann::json;

CccFunctionDescription::CccFunctionDescription()
{
    BuildFunctionDescription();
}

CccFunctionDescription::~CccFunctionDescription()
{
    free(m_buffer);
    m_buffer = nullptr;
    m_size = 0;
}

void
CccFunctionDescription::BuildFunctionDescription()
{

    json functionDescription;

    functionDescription["ranFunctionName"] = {
        {"ranFunctionShortName", "ORAN-E2SM-CCC"},
        // {"ranFunctionServiceModelOID", "1.3.6.1.4.1.53148.1.1.2.4"},
        {"ranFunctionServiceModelOID", "1.3.6.1.4.1.53148.1.6.2.4"},
        {"ranFunctionDescription", "Cell Configuration and Control"}
    };
    functionDescription["supportedSlices"] = {
        {{"sst", 1}, {"sd", "000001"}},  // eMBB
        {{"sst", 2}, {"sd", "000002"}}   // URLLC
    };

    std::string encoded = functionDescription.dump();

    m_size = encoded.size();

    m_buffer = static_cast<uint8_t*>(calloc(1, m_size));

    memcpy(
        m_buffer,
        encoded.data(),
        m_size
    );

    NS_LOG_INFO(
        "CCC RAN Function Definition: "
        << encoded
    );
}

} // namespace ns3
