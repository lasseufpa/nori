#include "E2-report.h"

#include <ns3/core-module.h>
#include <ns3/event-id.h>
#include <ns3/nr-bearer-stats-connector.h>
#include <ns3/nr-phy-rx-trace.h>

NS_LOG_COMPONENT_DEFINE("NoriE2Report");

namespace ns3
{
NS_OBJECT_ENSURE_REGISTERED(NoriE2Report);

NoriE2Report::NoriE2Report()
{
}

NoriE2Report::~NoriE2Report()
{
}

TypeId
NoriE2Report::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::NoriE2Report").SetParent<NrPhyRxTrace>().AddConstructor<NoriE2Report>();
    return tid;
}

void
NoriE2Report::UpdateTraces(/*Ptr<NoriE2Report> phyStats, */ std::string path,
                           RxPacketTraceParams params)
{
    RntiCellIdPair_t pair{params.m_rnti, params.m_cellId};

    NS_LOG_LOGIC("Update trace rnti " << params.m_rnti << " cellId " << params.m_cellId);

    m_macPduUeSpecific = IncreaseMapValue(m_macPduUeSpecific, pair, 1);
    NS_LOG_DEBUG("M_rv: " << (unsigned)params.m_rv << ", MCS: " << (unsigned)params.m_mcs
                          << ", SINR: " << 10 * std::log10(params.m_sinr) << ", TB size: "
                          << params.m_tbSize << ", Num sym: " << (unsigned)params.m_numSym);
    if ((unsigned)params.m_rv == 0)
    {
        m_macPduInitialTransmissionUeSpecific =
            IncreaseMapValue(m_macPduInitialTransmissionUeSpecific, pair, 1);
    }
    else
    {
        m_macPduRetransmissionUeSpecific =
            IncreaseMapValue(m_macPduRetransmissionUeSpecific, pair, 1);
    }

    // UE specific MAC volume
    m_macVolumeUeSpecific = IncreaseMapValue(m_macVolumeUeSpecific, pair, params.m_tbSize);

    if ((unsigned)params.m_mcs >= 0 && (unsigned)params.m_mcs <= 9)
    {
        // UE specific MAC PDUs QPSK
        m_macPduQpskUeSpecific = IncreaseMapValue(m_macPduQpskUeSpecific, pair, 1);
    }
    else if ((unsigned)params.m_mcs >= 10 && (unsigned)params.m_mcs <= 16)
    {
        // UE specific MAC PDUs 16QAM
        m_macPdu16QamUeSpecific = IncreaseMapValue(m_macPdu16QamUeSpecific, pair, 1);
    }
    else if ((unsigned)params.m_mcs >= 17 && (unsigned)params.m_mcs <= 28)
    {
        // UE specific MAC PDUs 64QAM
        m_macPdu64QamUeSpecific = IncreaseMapValue(m_macPdu64QamUeSpecific, pair, 1);
    }

    if ((unsigned)params.m_mcs <= 4)
    {
        m_macMcs04UeSpecific = IncreaseMapValue(m_macMcs04UeSpecific, pair, 1);
    }
    else if ((unsigned)params.m_mcs >= 5 && (unsigned)params.m_mcs <= 9)
    {
        m_macMcs59UeSpecific = IncreaseMapValue(m_macMcs59UeSpecific, pair, 1);
    }
    else if ((unsigned)params.m_mcs >= 10 && (unsigned)params.m_mcs <= 14)
    {
        m_macMcs1014UeSpecific = IncreaseMapValue(m_macMcs1014UeSpecific, pair, 1);
    }
    else if ((unsigned)params.m_mcs >= 15 && (unsigned)params.m_mcs <= 19)
    {
        m_macMcs1519UeSpecific = IncreaseMapValue(m_macMcs1519UeSpecific, pair, 1);
    }
    else if ((unsigned)params.m_mcs >= 20 && (unsigned)params.m_mcs <= 24)
    {
        m_macMcs2024UeSpecific = IncreaseMapValue(m_macMcs2024UeSpecific, pair, 1);
    }
    else if ((unsigned)params.m_mcs >= 25 && (unsigned)params.m_mcs <= 29)
    {
        m_macMcs2529UeSpecific = IncreaseMapValue(m_macMcs2529UeSpecific, pair, 1);
    }

    double sinrLog = 10 * std::log10(params.m_sinr);
    if (sinrLog <= -6)
    {
        m_macSinrBin1UeSpecific = IncreaseMapValue(m_macSinrBin1UeSpecific, pair, 1);
    }
    else if (sinrLog <= 0)
    {
        m_macSinrBin2UeSpecific = IncreaseMapValue(m_macSinrBin2UeSpecific, pair, 1);
    }
    else if (sinrLog <= 6)
    {
        m_macSinrBin3UeSpecific = IncreaseMapValue(m_macSinrBin3UeSpecific, pair, 1);
    }
    else if (sinrLog <= 12)
    {
        m_macSinrBin4UeSpecific = IncreaseMapValue(m_macSinrBin4UeSpecific, pair, 1);
    }
    else if (sinrLog <= 18)
    {
        m_macSinrBin5UeSpecific = IncreaseMapValue(m_macSinrBin5UeSpecific, pair, 1);
    }
    else if (sinrLog <= 24)
    {
        m_macSinrBin6UeSpecific = IncreaseMapValue(m_macSinrBin6UeSpecific, pair, 1);
    }
    else
    {
        m_macSinrBin7UeSpecific = IncreaseMapValue(m_macSinrBin7UeSpecific, pair, 1);
    }

    // UE specific number of symbols
    m_macNumberOfSymbols = IncreaseMapValue(m_macNumberOfSymbols, pair, (unsigned)params.m_numSym);
}

std::map<RntiCellIdPair_t, uint32_t>
NoriE2Report::UpdateMapValue(std::map<RntiCellIdPair_t, uint32_t> map,
                             RntiCellIdPair_t key,
                             uint32_t newValue)
{
    auto pair = map.find(key);
    if (pair != map.end())
    {
        pair->second = newValue;
    }
    else
    {
        map[key] = newValue;
    }
    return map;
}

std::map<RntiCellIdPair_t, uint32_t>
NoriE2Report::IncreaseMapValue(std::map<RntiCellIdPair_t, uint32_t> map,
                               RntiCellIdPair_t key,
                               uint32_t value)
{
    auto pair = map.find(key);
    if (pair != map.end())
    {
        pair->second += value;
    }
    else
    {
        map[key] = value;
    }
    return map;
}

uint32_t
NoriE2Report::GetMapValue(std::map<RntiCellIdPair_t, uint32_t> map, RntiCellIdPair_t key)
{
    uint32_t ret = 0;
    auto pair = map.find(key);
    if (pair != map.end())
    {
        ret = pair->second;
    }
    return ret;
}

std::map<RntiCellIdPair_t, uint32_t>
NoriE2Report::ResetMapValue(std::map<RntiCellIdPair_t, uint32_t> map, RntiCellIdPair_t key)
{
    auto pair = map.find(key);
    if (pair != map.end())
    {
        map.erase(pair);
    }
    m_lastReset[key] = Simulator::Now();
    return map;
}

void
NoriE2Report::ResetPhyTracesForRntiCellId(uint16_t rnti, uint16_t cellId)
{
    NS_LOG_LOGIC("Reset rnti " << rnti << " cellId " << cellId);
    RntiCellIdPair_t pair{rnti, cellId};

    m_macPduUeSpecific = ResetMapValue(m_macPduUeSpecific, pair);

    m_macPduInitialTransmissionUeSpecific =
        ResetMapValue(m_macPduInitialTransmissionUeSpecific, pair);

    m_macPduRetransmissionUeSpecific = ResetMapValue(m_macPduRetransmissionUeSpecific, pair);

    m_macVolumeUeSpecific = ResetMapValue(m_macVolumeUeSpecific, pair);

    m_macPduQpskUeSpecific = ResetMapValue(m_macPduQpskUeSpecific, pair);

    m_macPdu16QamUeSpecific = ResetMapValue(m_macPdu16QamUeSpecific, pair);

    m_macPdu64QamUeSpecific = ResetMapValue(m_macPdu64QamUeSpecific, pair);

    m_macMcs04UeSpecific = ResetMapValue(m_macMcs04UeSpecific, pair);

    m_macMcs59UeSpecific = ResetMapValue(m_macMcs59UeSpecific, pair);

    m_macMcs1014UeSpecific = ResetMapValue(m_macMcs1014UeSpecific, pair);

    m_macMcs1519UeSpecific = ResetMapValue(m_macMcs1519UeSpecific, pair);

    m_macMcs2024UeSpecific = ResetMapValue(m_macMcs2024UeSpecific, pair);

    m_macMcs2529UeSpecific = ResetMapValue(m_macMcs2529UeSpecific, pair);

    m_macSinrBin1UeSpecific = ResetMapValue(m_macSinrBin1UeSpecific, pair);

    m_macSinrBin2UeSpecific = ResetMapValue(m_macSinrBin2UeSpecific, pair);

    m_macSinrBin3UeSpecific = ResetMapValue(m_macSinrBin3UeSpecific, pair);

    m_macSinrBin4UeSpecific = ResetMapValue(m_macSinrBin4UeSpecific, pair);

    m_macSinrBin5UeSpecific = ResetMapValue(m_macSinrBin5UeSpecific, pair);

    m_macSinrBin6UeSpecific = ResetMapValue(m_macSinrBin6UeSpecific, pair);

    m_macSinrBin7UeSpecific = ResetMapValue(m_macSinrBin7UeSpecific, pair);

    m_macNumberOfSymbols = ResetMapValue(m_macNumberOfSymbols, pair);
}

Time
NoriE2Report::GetLastResetTime(uint16_t rnti, uint16_t cellId)
{
    Time ret = Seconds(0);
    RntiCellIdPair_t key{rnti, cellId};
    if (m_lastReset.find(key) != m_lastReset.end())
    {
        ret = m_lastReset.at(key);
    }
    return ret;
}

uint32_t
NoriE2Report::GetMacPduUeSpecific(uint16_t rnti, uint16_t cellId)
{
    RntiCellIdPair_t pair{rnti, cellId};
    return GetMapValue(m_macPduUeSpecific, pair);
}

uint32_t
NoriE2Report::GetMacPduInitialTransmissionUeSpecific(uint16_t rnti, uint16_t cellId)
{
    RntiCellIdPair_t pair{rnti, cellId};
    return GetMapValue(m_macPduInitialTransmissionUeSpecific, pair);
}

uint32_t
NoriE2Report::GetMacPduRetransmissionUeSpecific(uint16_t rnti, uint16_t cellId)
{
    RntiCellIdPair_t pair{rnti, cellId};
    return GetMapValue(m_macPduRetransmissionUeSpecific, pair);
}

uint32_t
NoriE2Report::GetMacVolumeUeSpecific(uint16_t rnti, uint16_t cellId)
{
    RntiCellIdPair_t pair{rnti, cellId};
    return GetMapValue(m_macVolumeUeSpecific, pair);
}

uint32_t
NoriE2Report::GetMacPduQpskUeSpecific(uint16_t rnti, uint16_t cellId)
{
    RntiCellIdPair_t pair{rnti, cellId};
    return GetMapValue(m_macPduQpskUeSpecific, pair);
}

uint32_t
NoriE2Report::GetMacPdu16QamUeSpecific(uint16_t rnti, uint16_t cellId)
{
    RntiCellIdPair_t pair{rnti, cellId};
    return GetMapValue(m_macPdu16QamUeSpecific, pair);
}

uint32_t
NoriE2Report::GetMacPdu64QamUeSpecific(uint16_t rnti, uint16_t cellId)
{
    RntiCellIdPair_t pair{rnti, cellId};
    return GetMapValue(m_macPdu64QamUeSpecific, pair);
}

uint32_t
NoriE2Report::GetMacNumberOfSymbolsUeSpecific(uint16_t rnti, uint16_t cellId)
{
    RntiCellIdPair_t pair{rnti, cellId};
    return GetMapValue(m_macNumberOfSymbols, pair);
}

uint32_t
NoriE2Report::GetMacMcs04UeSpecific(uint16_t rnti, uint16_t cellId)
{
    RntiCellIdPair_t pair{rnti, cellId};
    return GetMapValue(m_macMcs04UeSpecific, pair);
}

uint32_t
NoriE2Report::GetMacMcs59UeSpecific(uint16_t rnti, uint16_t cellId)
{
    RntiCellIdPair_t pair{rnti, cellId};
    return GetMapValue(m_macMcs59UeSpecific, pair);
}

uint32_t
NoriE2Report::GetMacMcs1014UeSpecific(uint16_t rnti, uint16_t cellId)
{
    RntiCellIdPair_t pair{rnti, cellId};
    return GetMapValue(m_macMcs1014UeSpecific, pair);
}

uint32_t
NoriE2Report::GetMacMcs1519UeSpecific(uint16_t rnti, uint16_t cellId)
{
    RntiCellIdPair_t pair{rnti, cellId};
    return GetMapValue(m_macMcs1519UeSpecific, pair);
}

uint32_t
NoriE2Report::GetMacMcs2024UeSpecific(uint16_t rnti, uint16_t cellId)
{
    RntiCellIdPair_t pair{rnti, cellId};
    return GetMapValue(m_macMcs2024UeSpecific, pair);
}

uint32_t
NoriE2Report::GetMacMcs2529UeSpecific(uint16_t rnti, uint16_t cellId)
{
    RntiCellIdPair_t pair{rnti, cellId};
    return GetMapValue(m_macMcs2529UeSpecific, pair);
}

uint32_t
NoriE2Report::GetMacSinrBin1UeSpecific(uint16_t rnti, uint16_t cellId)
{
    RntiCellIdPair_t pair{rnti, cellId};
    return GetMapValue(m_macSinrBin1UeSpecific, pair);
}

uint32_t
NoriE2Report::GetMacSinrBin2UeSpecific(uint16_t rnti, uint16_t cellId)
{
    RntiCellIdPair_t pair{rnti, cellId};
    return GetMapValue(m_macSinrBin2UeSpecific, pair);
}

uint32_t
NoriE2Report::GetMacSinrBin3UeSpecific(uint16_t rnti, uint16_t cellId)
{
    RntiCellIdPair_t pair{rnti, cellId};
    return GetMapValue(m_macSinrBin3UeSpecific, pair);
}

uint32_t
NoriE2Report::GetMacSinrBin4UeSpecific(uint16_t rnti, uint16_t cellId)
{
    RntiCellIdPair_t pair{rnti, cellId};
    return GetMapValue(m_macSinrBin4UeSpecific, pair);
}

uint32_t
NoriE2Report::GetMacSinrBin5UeSpecific(uint16_t rnti, uint16_t cellId)
{
    RntiCellIdPair_t pair{rnti, cellId};
    return GetMapValue(m_macSinrBin5UeSpecific, pair);
}

uint32_t
NoriE2Report::GetMacSinrBin6UeSpecific(uint16_t rnti, uint16_t cellId)
{
    RntiCellIdPair_t pair{rnti, cellId};
    return GetMapValue(m_macSinrBin6UeSpecific, pair);
}

uint32_t
NoriE2Report::GetMacSinrBin7UeSpecific(uint16_t rnti, uint16_t cellId)
{
    RntiCellIdPair_t pair{rnti, cellId};
    return GetMapValue(m_macSinrBin7UeSpecific, pair);
}

void
NoriE2Report::EnableE2PdcpStats(Ptr<NrBearerStatsCalculator> e2PdcpStats)
{
    if (m_e2PdcpStatsVector.empty())
    {
        Simulator::Schedule(Seconds(0),
                            &NrBearerStatsConnector::EnsureConnected,
                            m_bearerStatsCalculator);
    }

    m_e2PdcpStatsVector.push_back(e2PdcpStats);
}

void
NoriE2Report::EnableE2RlcStats(Ptr<NrBearerStatsCalculator> e2RlcStats)
{
    if (m_e2RlcStatsVector.empty())
    {
        Simulator::Schedule(Seconds(0),
                            &NrBearerStatsConnector::EnsureConnected,
                            m_bearerStatsCalculator);
    }

    m_e2RlcStatsVector.push_back(e2RlcStats);
}

} // namespace ns3
