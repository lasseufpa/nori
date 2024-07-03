/*
 * Copyright (c) 2011 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
 * Copyright (c) 2018 Fraunhofer ESK : RLF extensions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors:
 *   Nicola Baldo <nbaldo@cttc.es>
 *   Marco Miozzo <mmiozzo@cttc.es>
 *   Manuel Requena <manuel.requena@cttc.es>
 * Modified by:
 *   Danilo Abrignani <danilo.abrignani@unibo.it> (Carrier Aggregation - GSoC 2015),
 *   Biljana Bojovic <biljana.bojovic@cttc.es> (Carrier Aggregation)
 *   Vignesh Babu <ns3-dev@esk.fraunhofer.de> (RLF extensions)
 */

#include "nori-lte-enb-rrc.h"

#include "ns3/component-carrier-enb.h"
#include "ns3/eps-bearer-tag.h"
#include "ns3/lte-pdcp.h"
#include "ns3/lte-radio-bearer-info.h"
#include "ns3/lte-rlc-tm.h"
#include "nori-lte-rlc-um.h"
#include "nori-lte-rlc.h"
#include "nori-lte-rlc-am.h"
#include "nori-lte-enb-rrc.h"

#include <ns3/abort.h>
#include <ns3/fatal-error.h>
#include <ns3/log.h>
#include <ns3/object-factory.h>
#include <ns3/object-map.h>
#include <ns3/packet.h>
#include <ns3/pointer.h>
#include <ns3/simulator.h>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("LteEnbRrc");

///////////////////////////////////////////
// CMAC SAP forwarder
///////////////////////////////////////////

/**
 * \brief Class for forwarding CMAC SAP User functions.
 */
class EnbRrcMemberLteEnbCmacSapUser : public LteEnbCmacSapUser
{
  public:
    /**
     * Constructor
     *
     * \param rrc ENB RRC
     * \param componentCarrierId
     */
    EnbRrcMemberLteEnbCmacSapUser(NoriLteEnbRrc* rrc, uint8_t componentCarrierId);

    uint16_t AllocateTemporaryCellRnti() override;
    void NotifyLcConfigResult(uint16_t rnti, uint8_t lcid, bool success) override;
    void RrcConfigurationUpdateInd(UeConfig params) override;
    bool IsRandomAccessCompleted(uint16_t rnti) override;

  private:
    NoriLteEnbRrc* m_rrc;             ///< the RRC
    uint8_t m_componentCarrierId; ///< Component carrier ID
};

EnbRrcMemberLteEnbCmacSapUser::EnbRrcMemberLteEnbCmacSapUser(NoriLteEnbRrc* rrc,
                                                             uint8_t componentCarrierId)
    : m_rrc(rrc),
      m_componentCarrierId{componentCarrierId}
{
}

uint16_t
EnbRrcMemberLteEnbCmacSapUser::AllocateTemporaryCellRnti()
{
    return m_rrc->DoAllocateTemporaryCellRnti(m_componentCarrierId);
}

void
EnbRrcMemberLteEnbCmacSapUser::NotifyLcConfigResult(uint16_t rnti, uint8_t lcid, bool success)
{
    m_rrc->DoNotifyLcConfigResult(rnti, lcid, success);
}

void
EnbRrcMemberLteEnbCmacSapUser::RrcConfigurationUpdateInd(UeConfig params)
{
    m_rrc->DoRrcConfigurationUpdateInd(params);
}

bool
EnbRrcMemberLteEnbCmacSapUser::IsRandomAccessCompleted(uint16_t rnti)
{
    return m_rrc->IsRandomAccessCompleted(rnti);
}

///////////////////////////////////////////
// UeManager
///////////////////////////////////////////

/// Map each of UE Manager states to its string representation.
static const std::string g_ueManagerStateName[NoriUeManager::NUM_STATES] = {
    "INITIAL_RANDOM_ACCESS",
    "CONNECTION_SETUP",
    "CONNECTION_REJECTED",
    "ATTACH_REQUEST",
    "CONNECTED_NORMALLY",
    "CONNECTION_RECONFIGURATION",
    "CONNECTION_REESTABLISHMENT",
    "HANDOVER_PREPARATION",
    "HANDOVER_JOINING",
    "HANDOVER_PATH_SWITCH",
    "HANDOVER_LEAVING",
};

/**
 * \param s The UE manager state.
 * \return The string representation of the given state.
 */
static const std::string&
ToString(NoriUeManager::State s)
{
    return g_ueManagerStateName[s];
}

NS_OBJECT_ENSURE_REGISTERED(NoriUeManager);

NoriUeManager::NoriUeManager()
{
    NS_FATAL_ERROR("this constructor is not expected to be used");
}

NoriUeManager::NoriUeManager(Ptr<NoriLteEnbRrc> rrc, uint16_t rnti, State s, uint8_t componentCarrierId)
    : m_lastAllocatedDrbid(0),
      m_rnti(rnti),
      m_imsi(0),
      m_componentCarrierId(componentCarrierId),
      m_lastRrcTransactionIdentifier(0),
      m_rrc(rrc),
      m_state(s),
      m_pendingRrcConnectionReconfiguration(false),
      m_sourceX2apId(0),
      m_sourceCellId(0),
      m_needPhyMacConfiguration(false),
      m_caSupportConfigured(false),
      m_pendingStartDataRadioBearers(false)
{
    NS_LOG_FUNCTION(this);
}

void
NoriUeManager::DoInitialize()
{
    NS_LOG_FUNCTION(this);
    m_drbPdcpSapUser = new LtePdcpSpecificLtePdcpSapUser<NoriUeManager>(this);

    m_physicalConfigDedicated.haveAntennaInfoDedicated = true;
    m_physicalConfigDedicated.antennaInfo.transmissionMode = m_rrc->m_defaultTransmissionMode;
    m_physicalConfigDedicated.haveSoundingRsUlConfigDedicated = true;
    m_physicalConfigDedicated.soundingRsUlConfigDedicated.srsConfigIndex =
        m_rrc->GetNewSrsConfigurationIndex();
    m_physicalConfigDedicated.soundingRsUlConfigDedicated.type =
        LteRrcSap::SoundingRsUlConfigDedicated::SETUP;
    m_physicalConfigDedicated.soundingRsUlConfigDedicated.srsBandwidth = 0;
    m_physicalConfigDedicated.havePdschConfigDedicated = true;
    m_physicalConfigDedicated.pdschConfigDedicated.pa = LteRrcSap::PdschConfigDedicated::dB0;

    for (uint16_t i = 0; i < m_rrc->m_numberOfComponentCarriers; i++)
    {
        m_rrc->m_cmacSapProvider.at(i)->AddUe(m_rnti);
        m_rrc->m_cphySapProvider.at(i)->AddUe(m_rnti);
    }

    // setup the eNB side of SRB0
    {
        uint8_t lcid = 0;

        Ptr<LteRlc> rlc = CreateObject<LteRlcTm>()->GetObject<LteRlc>();
        rlc->SetLteMacSapProvider(m_rrc->m_macSapProvider);
        rlc->SetRnti(m_rnti);
        rlc->SetLcId(lcid);

        m_srb0 = CreateObject<LteSignalingRadioBearerInfo>();
        m_srb0->m_rlc = rlc;
        m_srb0->m_srbIdentity = 0;
        // no need to store logicalChannelConfig as SRB0 is pre-configured

        LteEnbCmacSapProvider::LcInfo lcinfo;
        lcinfo.rnti = m_rnti;
        lcinfo.lcId = lcid;
        // Initialise the rest of lcinfo structure even if CCCH (LCID 0) is pre-configured, and only
        // m_rnti and lcid will be used from passed lcinfo structure. See FF LTE MAC Scheduler
        // Iinterface Specification v1.11, 4.3.4 logicalChannelConfigListElement
        lcinfo.lcGroup = 0;
        lcinfo.qci = 0;
        lcinfo.resourceType = 0;
        lcinfo.mbrUl = 0;
        lcinfo.mbrDl = 0;
        lcinfo.gbrUl = 0;
        lcinfo.gbrDl = 0;

        // MacSapUserForRlc in the ComponentCarrierManager MacSapUser
        LteMacSapUser* lteMacSapUser =
            m_rrc->m_ccmRrcSapProvider->ConfigureSignalBearer(lcinfo, rlc->GetLteMacSapUser());
        // Signal Channel are only on Primary Carrier
        m_rrc->m_cmacSapProvider.at(m_componentCarrierId)->AddLc(lcinfo, lteMacSapUser);
        m_rrc->m_ccmRrcSapProvider->AddLc(lcinfo, lteMacSapUser);
    }

    // setup the eNB side of SRB1; the UE side will be set up upon RRC connection establishment
    {
        uint8_t lcid = 1;

        Ptr<LteRlc> rlc = CreateObject<NoriLteRlcAm>()->GetObject<LteRlc>();
        rlc->SetLteMacSapProvider(m_rrc->m_macSapProvider);
        rlc->SetRnti(m_rnti);
        rlc->SetLcId(lcid);

        Ptr<LtePdcp> pdcp = CreateObject<LtePdcp>();
        pdcp->SetRnti(m_rnti);
        pdcp->SetLcId(lcid);
        pdcp->SetLtePdcpSapUser(m_drbPdcpSapUser);
        pdcp->SetLteRlcSapProvider(rlc->GetLteRlcSapProvider());
        rlc->SetLteRlcSapUser(pdcp->GetLteRlcSapUser());

        m_srb1 = CreateObject<LteSignalingRadioBearerInfo>();
        m_srb1->m_rlc = rlc;
        m_srb1->m_pdcp = pdcp;
        m_srb1->m_srbIdentity = 1;
        m_srb1->m_logicalChannelConfig.priority = 1;
        m_srb1->m_logicalChannelConfig.prioritizedBitRateKbps = 100;
        m_srb1->m_logicalChannelConfig.bucketSizeDurationMs = 100;
        m_srb1->m_logicalChannelConfig.logicalChannelGroup = 0;

        LteEnbCmacSapProvider::LcInfo lcinfo;
        lcinfo.rnti = m_rnti;
        lcinfo.lcId = lcid;
        lcinfo.lcGroup = 0; // all SRBs always mapped to LCG 0
        lcinfo.qci =
            EpsBearer::GBR_CONV_VOICE; // not sure why the FF API requires a CQI even for SRBs...
        lcinfo.resourceType = 1;       // GBR resource type
        lcinfo.mbrUl = 1e6;
        lcinfo.mbrDl = 1e6;
        lcinfo.gbrUl = 1e4;
        lcinfo.gbrDl = 1e4;
        // MacSapUserForRlc in the ComponentCarrierManager MacSapUser
        LteMacSapUser* MacSapUserForRlc =
            m_rrc->m_ccmRrcSapProvider->ConfigureSignalBearer(lcinfo, rlc->GetLteMacSapUser());
        // Signal Channel are only on Primary Carrier
        m_rrc->m_cmacSapProvider.at(m_componentCarrierId)->AddLc(lcinfo, MacSapUserForRlc);
        m_rrc->m_ccmRrcSapProvider->AddLc(lcinfo, MacSapUserForRlc);
    }

    LteEnbRrcSapUser::SetupUeParameters ueParams;
    ueParams.srb0SapProvider = m_srb0->m_rlc->GetLteRlcSapProvider();
    ueParams.srb1SapProvider = m_srb1->m_pdcp->GetLtePdcpSapProvider();
    m_rrc->m_rrcSapUser->SetupUe(m_rnti, ueParams);

    // configure MAC (and scheduler)
    LteEnbCmacSapProvider::UeConfig req;
    req.m_rnti = m_rnti;
    req.m_transmissionMode = m_physicalConfigDedicated.antennaInfo.transmissionMode;

    // configure PHY
    for (uint16_t i = 0; i < m_rrc->m_numberOfComponentCarriers; i++)
    {
        m_rrc->m_cmacSapProvider.at(i)->UeUpdateConfigurationReq(req);
        m_rrc->m_cphySapProvider.at(i)->SetTransmissionMode(
            m_rnti,
            m_physicalConfigDedicated.antennaInfo.transmissionMode);
        m_rrc->m_cphySapProvider.at(i)->SetSrsConfigurationIndex(
            m_rnti,
            m_physicalConfigDedicated.soundingRsUlConfigDedicated.srsConfigIndex);
    }
    // schedule this UeManager instance to be deleted if the UE does not give any sign of life
    // within a reasonable time
    Time maxConnectionDelay;
    switch (m_state)
    {
    case INITIAL_RANDOM_ACCESS:
        m_connectionRequestTimeout = Simulator::Schedule(m_rrc->m_connectionRequestTimeoutDuration,
                                                         &NoriLteEnbRrc::ConnectionRequestTimeout,
                                                         m_rrc,
                                                         m_rnti);
        break;

    case HANDOVER_JOINING:
        m_handoverJoiningTimeout = Simulator::Schedule(m_rrc->m_handoverJoiningTimeoutDuration,
                                                       &NoriLteEnbRrc::HandoverJoiningTimeout,
                                                       m_rrc,
                                                       m_rnti);
        break;

    default:
        NS_FATAL_ERROR("unexpected state " << ToString(m_state));
        break;
    }
    m_caSupportConfigured = false;
}

NoriUeManager::~NoriUeManager()
{
}

void
NoriUeManager::DoDispose()
{
    delete m_drbPdcpSapUser;
    // delete eventual X2-U TEIDs
    for (auto it = m_drbMap.begin(); it != m_drbMap.end(); ++it)
    {
        m_rrc->m_x2uTeidInfoMap.erase(it->second->m_gtpTeid);
    }
}

TypeId
NoriUeManager::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::NoriUeManager")
            .SetParent<Object>()
            .AddConstructor<NoriUeManager>()
            .AddAttribute("DataRadioBearerMap",
                          "List of UE DataRadioBearerInfo by DRBID.",
                          ObjectMapValue(),
                          MakeObjectMapAccessor(&NoriUeManager::m_drbMap),
                          MakeObjectMapChecker<LteDataRadioBearerInfo>())
            .AddAttribute("Srb0",
                          "SignalingRadioBearerInfo for SRB0",
                          PointerValue(),
                          MakePointerAccessor(&NoriUeManager::m_srb0),
                          MakePointerChecker<LteSignalingRadioBearerInfo>())
            .AddAttribute("Srb1",
                          "SignalingRadioBearerInfo for SRB1",
                          PointerValue(),
                          MakePointerAccessor(&NoriUeManager::m_srb1),
                          MakePointerChecker<LteSignalingRadioBearerInfo>())
            .AddAttribute("C-RNTI",
                          "Cell Radio Network Temporary Identifier",
                          TypeId::ATTR_GET, // read-only attribute
                          UintegerValue(0), // unused, read-only attribute
                          MakeUintegerAccessor(&NoriUeManager::m_rnti),
                          MakeUintegerChecker<uint16_t>())
            .AddTraceSource("StateTransition",
                            "fired upon every UE state transition seen by the "
                            "UeManager at the eNB RRC",
                            MakeTraceSourceAccessor(&NoriUeManager::m_stateTransitionTrace),
                            "ns3::NoriUeManager::StateTracedCallback")
            .AddTraceSource("DrbCreated",
                            "trace fired after DRB is created",
                            MakeTraceSourceAccessor(&NoriUeManager::m_drbCreatedTrace),
                            "ns3::NoriUeManager::ImsiCidRntiLcIdTracedCallback");
    return tid;
}

void
NoriUeManager::SetSource(uint16_t sourceCellId, uint16_t sourceX2apId)
{
    m_sourceX2apId = sourceX2apId;
    m_sourceCellId = sourceCellId;
}

void
NoriUeManager::SetImsi(uint64_t imsi)
{
    m_imsi = imsi;
}

void
NoriUeManager::InitialContextSetupRequest()
{
    NS_LOG_FUNCTION(this << m_rnti);

    if (m_state == ATTACH_REQUEST)
    {
        SwitchToState(CONNECTED_NORMALLY);
    }
    else
    {
        NS_FATAL_ERROR("method unexpected in state " << ToString(m_state));
    }
}

void
NoriUeManager::SetupDataRadioBearer(EpsBearer bearer,
                                uint8_t bearerId,
                                uint32_t gtpTeid,
                                Ipv4Address transportLayerAddress)
{
    NS_LOG_FUNCTION(this << (uint32_t)m_rnti);

    Ptr<LteDataRadioBearerInfo> drbInfo = CreateObject<LteDataRadioBearerInfo>();
    uint8_t drbid = AddDataRadioBearerInfo(drbInfo);
    uint8_t lcid = Drbid2Lcid(drbid);
    uint8_t bid = Drbid2Bid(drbid);
    NS_ASSERT_MSG(bearerId == 0 || bid == bearerId,
                  "bearer ID mismatch (" << (uint32_t)bid << " != " << (uint32_t)bearerId
                                         << ", the assumption that ID are allocated in the same "
                                            "way by MME and RRC is not valid any more");
    drbInfo->m_epsBearer = bearer;
    drbInfo->m_epsBearerIdentity = bid;
    drbInfo->m_drbIdentity = drbid;
    drbInfo->m_logicalChannelIdentity = lcid;
    drbInfo->m_gtpTeid = gtpTeid;
    drbInfo->m_transportLayerAddress = transportLayerAddress;

    if (m_state == HANDOVER_JOINING)
    {
        // setup TEIDs for receiving data eventually forwarded over X2-U
        NoriLteEnbRrc::X2uTeidInfo x2uTeidInfo;
        x2uTeidInfo.rnti = m_rnti;
        x2uTeidInfo.drbid = drbid;
        auto ret = m_rrc->m_x2uTeidInfoMap.insert(
            std::pair<uint32_t, NoriLteEnbRrc::X2uTeidInfo>(gtpTeid, x2uTeidInfo));
        NS_ASSERT_MSG(ret.second == true, "overwriting a pre-existing entry in m_x2uTeidInfoMap");
    }

    TypeId rlcTypeId = m_rrc->GetRlcType(bearer);

    ObjectFactory rlcObjectFactory;
    rlcObjectFactory.SetTypeId(rlcTypeId);
    Ptr<LteRlc> rlc = rlcObjectFactory.Create()->GetObject<LteRlc>();
    rlc->SetLteMacSapProvider(m_rrc->m_macSapProvider);
    rlc->SetRnti(m_rnti);
    rlc->SetPacketDelayBudgetMs(bearer.GetPacketDelayBudgetMs());

    drbInfo->m_rlc = rlc;

    rlc->SetLcId(lcid);

    // we need PDCP only for real RLC, i.e., RLC/UM or RLC/AM
    // if we are using RLC/SM we don't care of anything above RLC
    if (rlcTypeId != LteRlcSm::GetTypeId())
    {
        Ptr<LtePdcp> pdcp = CreateObject<LtePdcp>();
        pdcp->SetRnti(m_rnti);
        pdcp->SetLcId(lcid);
        pdcp->SetLtePdcpSapUser(m_drbPdcpSapUser);
        pdcp->SetLteRlcSapProvider(rlc->GetLteRlcSapProvider());
        rlc->SetLteRlcSapUser(pdcp->GetLteRlcSapUser());
        drbInfo->m_pdcp = pdcp;
    }

    m_drbCreatedTrace(m_imsi, m_rrc->ComponentCarrierToCellId(m_componentCarrierId), m_rnti, lcid);

    std::vector<LteCcmRrcSapProvider::LcsConfig> lcOnCcMapping =
        m_rrc->m_ccmRrcSapProvider->SetupDataRadioBearer(bearer,
                                                         bearerId,
                                                         m_rnti,
                                                         lcid,
                                                         m_rrc->GetLogicalChannelGroup(bearer),
                                                         rlc->GetLteMacSapUser());
    // LteEnbCmacSapProvider::LcInfo lcinfo;
    // lcinfo.rnti = m_rnti;
    // lcinfo.lcId = lcid;
    // lcinfo.lcGroup = m_rrc->GetLogicalChannelGroup (bearer);
    // lcinfo.qci = bearer.qci;
    // lcinfo.resourceType = bearer.GetResourceType();
    // lcinfo.mbrUl = bearer.gbrQosInfo.mbrUl;
    // lcinfo.mbrDl = bearer.gbrQosInfo.mbrDl;
    // lcinfo.gbrUl = bearer.gbrQosInfo.gbrUl;
    // lcinfo.gbrDl = bearer.gbrQosInfo.gbrDl;
    // use a for cycle to send the AddLc to the appropriate Mac Sap
    // if the sap is not initialized the appropriated method has to be called
    auto itLcOnCcMapping = lcOnCcMapping.begin();
    NS_ASSERT_MSG(itLcOnCcMapping != lcOnCcMapping.end(), "Problem");
    for (itLcOnCcMapping = lcOnCcMapping.begin(); itLcOnCcMapping != lcOnCcMapping.end();
         ++itLcOnCcMapping)
    {
        NS_LOG_DEBUG(this << " RNTI " << itLcOnCcMapping->lc.rnti << "Lcid "
                          << (uint16_t)itLcOnCcMapping->lc.lcId << " lcGroup "
                          << (uint16_t)itLcOnCcMapping->lc.lcGroup << " ComponentCarrierId "
                          << itLcOnCcMapping->componentCarrierId);
        uint8_t index = itLcOnCcMapping->componentCarrierId;
        LteEnbCmacSapProvider::LcInfo lcinfo = itLcOnCcMapping->lc;
        LteMacSapUser* msu = itLcOnCcMapping->msu;
        m_rrc->m_cmacSapProvider.at(index)->AddLc(lcinfo, msu);
        m_rrc->m_ccmRrcSapProvider->AddLc(lcinfo, msu);
    }

    if (rlcTypeId == NoriLteRlcAm::GetTypeId())
    {
        drbInfo->m_rlcConfig.choice = LteRrcSap::RlcConfig::AM;
    }
    else
    {
        drbInfo->m_rlcConfig.choice = LteRrcSap::RlcConfig::UM_BI_DIRECTIONAL;
    }

    drbInfo->m_logicalChannelIdentity = lcid;
    drbInfo->m_logicalChannelConfig.priority = m_rrc->GetLogicalChannelPriority(bearer);
    drbInfo->m_logicalChannelConfig.logicalChannelGroup = m_rrc->GetLogicalChannelGroup(bearer);
    if (bearer.GetResourceType() > 0) // 1, 2 for GBR and DC-GBR
    {
        drbInfo->m_logicalChannelConfig.prioritizedBitRateKbps = bearer.gbrQosInfo.gbrUl;
    }
    else
    {
        drbInfo->m_logicalChannelConfig.prioritizedBitRateKbps = 0;
    }
    drbInfo->m_logicalChannelConfig.bucketSizeDurationMs = 1000;

    ScheduleRrcConnectionReconfiguration();
}

void
NoriUeManager::RecordDataRadioBearersToBeStarted()
{
    NS_LOG_FUNCTION(this << (uint32_t)m_rnti);
    for (auto it = m_drbMap.begin(); it != m_drbMap.end(); ++it)
    {
        m_drbsToBeStarted.push_back(it->first);
    }
}

void
NoriUeManager::StartDataRadioBearers()
{
    NS_LOG_FUNCTION(this << (uint32_t)m_rnti);
    for (auto drbIdIt = m_drbsToBeStarted.begin(); drbIdIt != m_drbsToBeStarted.end(); ++drbIdIt)
    {
        auto drbIt = m_drbMap.find(*drbIdIt);
        NS_ASSERT(drbIt != m_drbMap.end());
        drbIt->second->m_rlc->Initialize();
        if (drbIt->second->m_pdcp)
        {
            drbIt->second->m_pdcp->Initialize();
        }
    }
    m_drbsToBeStarted.clear();
}

void
NoriUeManager::ReleaseDataRadioBearer(uint8_t drbid)
{
    NS_LOG_FUNCTION(this << (uint32_t)m_rnti << (uint32_t)drbid);
    uint8_t lcid = Drbid2Lcid(drbid);
    auto it = m_drbMap.find(drbid);
    NS_ASSERT_MSG(it != m_drbMap.end(),
                  "request to remove radio bearer with unknown drbid " << drbid);

    // first delete eventual X2-U TEIDs
    m_rrc->m_x2uTeidInfoMap.erase(it->second->m_gtpTeid);

    m_drbMap.erase(it);
    std::vector<uint8_t> ccToRelease =
        m_rrc->m_ccmRrcSapProvider->ReleaseDataRadioBearer(m_rnti, lcid);
    auto itCcToRelease = ccToRelease.begin();
    NS_ASSERT_MSG(itCcToRelease != ccToRelease.end(),
                  "request to remove radio bearer with unknown drbid (ComponentCarrierManager)");
    for (itCcToRelease = ccToRelease.begin(); itCcToRelease != ccToRelease.end(); ++itCcToRelease)
    {
        m_rrc->m_cmacSapProvider.at(*itCcToRelease)->ReleaseLc(m_rnti, lcid);
    }
    LteRrcSap::RadioResourceConfigDedicated rrcd;
    rrcd.havePhysicalConfigDedicated = false;
    rrcd.drbToReleaseList.push_back(drbid);
    // populating RadioResourceConfigDedicated information element as per 3GPP TS 36.331
    // version 9.2.0
    rrcd.havePhysicalConfigDedicated = true;
    rrcd.physicalConfigDedicated = m_physicalConfigDedicated;

    // populating RRCConnectionReconfiguration message as per 3GPP TS 36.331 version 9.2.0 Release 9
    LteRrcSap::RrcConnectionReconfiguration msg;
    msg.haveMeasConfig = false;
    msg.haveMobilityControlInfo = false;
    msg.radioResourceConfigDedicated = rrcd;
    msg.haveRadioResourceConfigDedicated = true;
    // ToDo: Resend in any case this configuration
    // needs to be initialized
    msg.haveNonCriticalExtension = false;
    // RRC Connection Reconfiguration towards UE
    m_rrc->m_rrcSapUser->SendRrcConnectionReconfiguration(m_rnti, msg);
}

void
NoriLteEnbRrc::DoSendReleaseDataRadioBearer(uint64_t imsi, uint16_t rnti, uint8_t bearerId)
{
    NS_LOG_FUNCTION(this << imsi << rnti << (uint16_t)bearerId);

    // check if the RNTI to be removed is not stale
    if (HasUeManager(rnti))
    {
        Ptr<NoriUeManager> ueManager = GetUeManager(rnti);
        // Bearer de-activation towards UE
        ueManager->ReleaseDataRadioBearer(bearerId);
        // Bearer de-activation indication towards epc-enb application
        m_s1SapProvider->DoSendReleaseIndication(imsi, rnti, bearerId);
    }
}

void
NoriUeManager::RecvIdealUeContextRemoveRequest(uint16_t rnti)
{
    NS_LOG_FUNCTION(this << m_rnti);

    // release the bearer info for the UE at SGW/PGW
    if (m_rrc->m_s1SapProvider != nullptr) // if EPC is enabled
    {
        for (const auto& it : m_drbMap)
        {
            NS_LOG_DEBUG("Sending release of bearer id : "
                         << (uint16_t)(it.first)
                         << "LCID : " << (uint16_t)(it.second->m_logicalChannelIdentity));
            // Bearer de-activation indication towards epc-enb application
            m_rrc->m_s1SapProvider->DoSendReleaseIndication(GetImsi(), rnti, it.first);
        }
    }
}

void
NoriUeManager::ScheduleRrcConnectionReconfiguration()
{
    NS_LOG_FUNCTION(this);
    switch (m_state)
    {
    case INITIAL_RANDOM_ACCESS:
    case CONNECTION_SETUP:
    case ATTACH_REQUEST:
    case CONNECTION_RECONFIGURATION:
    case CONNECTION_REESTABLISHMENT:
    case HANDOVER_PREPARATION:
    case HANDOVER_JOINING:
    case HANDOVER_LEAVING:
        // a previous reconfiguration still ongoing, we need to wait for it to be finished
        m_pendingRrcConnectionReconfiguration = true;
        break;

    case CONNECTED_NORMALLY: {
        m_pendingRrcConnectionReconfiguration = false;
        LteRrcSap::RrcConnectionReconfiguration msg = BuildRrcConnectionReconfiguration();
        m_rrc->m_rrcSapUser->SendRrcConnectionReconfiguration(m_rnti, msg);
        RecordDataRadioBearersToBeStarted();
        SwitchToState(CONNECTION_RECONFIGURATION);
    }
    break;

    default:
        NS_FATAL_ERROR("method unexpected in state " << ToString(m_state));
        break;
    }
}

void
NoriUeManager::PrepareHandover(uint16_t cellId)
{
    NS_LOG_FUNCTION(this << cellId);
    switch (m_state)
    {
    case CONNECTED_NORMALLY: {
        m_targetCellId = cellId;

        auto sourceComponentCarrier = DynamicCast<ComponentCarrierEnb>(
            m_rrc->m_componentCarrierPhyConf.at(m_componentCarrierId));
        NS_ASSERT(m_targetCellId != sourceComponentCarrier->GetCellId());

        if (m_rrc->HasCellId(cellId))
        {
            // Intra-eNB handover
            NS_LOG_DEBUG("Intra-eNB handover for cellId " << cellId);
            uint8_t componentCarrierId = m_rrc->CellToComponentCarrierId(cellId);
            uint16_t rnti = m_rrc->AddUe(NoriUeManager::HANDOVER_JOINING, componentCarrierId);
            LteEnbCmacSapProvider::AllocateNcRaPreambleReturnValue anrcrv =
                m_rrc->m_cmacSapProvider.at(componentCarrierId)->AllocateNcRaPreamble(rnti);
            if (!anrcrv.valid)
            {
                NS_LOG_INFO(this << " failed to allocate a preamble for non-contention based RA => "
                                    "cannot perform HO");
                NS_FATAL_ERROR("should trigger HO Preparation Failure, but it is not implemented");
                return;
            }

            Ptr<NoriUeManager> ueManager = m_rrc->GetUeManager(rnti);
            ueManager->SetSource(sourceComponentCarrier->GetCellId(), m_rnti);
            ueManager->SetImsi(m_imsi);

            // Setup data radio bearers
            for (auto& it : m_drbMap)
            {
                ueManager->SetupDataRadioBearer(it.second->m_epsBearer,
                                                it.second->m_epsBearerIdentity,
                                                it.second->m_gtpTeid,
                                                it.second->m_transportLayerAddress);
            }

            LteRrcSap::RrcConnectionReconfiguration handoverCommand =
                GetRrcConnectionReconfigurationForHandover(componentCarrierId);

            handoverCommand.mobilityControlInfo.newUeIdentity = rnti;
            handoverCommand.mobilityControlInfo.haveRachConfigDedicated = true;
            handoverCommand.mobilityControlInfo.rachConfigDedicated.raPreambleIndex =
                anrcrv.raPreambleId;
            handoverCommand.mobilityControlInfo.rachConfigDedicated.raPrachMaskIndex =
                anrcrv.raPrachMaskIndex;

            LteEnbCmacSapProvider::RachConfig rc =
                m_rrc->m_cmacSapProvider.at(componentCarrierId)->GetRachConfig();
            handoverCommand.mobilityControlInfo.radioResourceConfigCommon.rachConfigCommon
                .preambleInfo.numberOfRaPreambles = rc.numberOfRaPreambles;
            handoverCommand.mobilityControlInfo.radioResourceConfigCommon.rachConfigCommon
                .raSupervisionInfo.preambleTransMax = rc.preambleTransMax;
            handoverCommand.mobilityControlInfo.radioResourceConfigCommon.rachConfigCommon
                .raSupervisionInfo.raResponseWindowSize = rc.raResponseWindowSize;

            m_rrc->m_rrcSapUser->SendRrcConnectionReconfiguration(m_rnti, handoverCommand);

            // We skip handover preparation
            SwitchToState(HANDOVER_LEAVING);
            m_handoverLeavingTimeout = Simulator::Schedule(m_rrc->m_handoverLeavingTimeoutDuration,
                                                           &NoriLteEnbRrc::HandoverLeavingTimeout,
                                                           m_rrc,
                                                           m_rnti);
            m_rrc->m_handoverStartTrace(m_imsi,
                                        sourceComponentCarrier->GetCellId(),
                                        m_rnti,
                                        handoverCommand.mobilityControlInfo.targetPhysCellId);
        }
        else
        {
            // Inter-eNB aka X2 handover
            NS_LOG_DEBUG("Inter-eNB handover (i.e., X2) for cellId " << cellId);
            EpcX2SapProvider::HandoverRequestParams params;
            params.oldEnbUeX2apId = m_rnti;
            params.cause = EpcX2SapProvider::HandoverDesirableForRadioReason;
            params.sourceCellId = m_rrc->ComponentCarrierToCellId(m_componentCarrierId);
            params.targetCellId = cellId;
            params.mmeUeS1apId = m_imsi;
            params.ueAggregateMaxBitRateDownlink = 200 * 1000;
            params.ueAggregateMaxBitRateUplink = 100 * 1000;
            params.bearers = GetErabList();

            LteRrcSap::HandoverPreparationInfo hpi;
            hpi.asConfig.sourceUeIdentity = m_rnti;
            hpi.asConfig.sourceDlCarrierFreq = sourceComponentCarrier->GetDlEarfcn();
            hpi.asConfig.sourceMeasConfig = m_rrc->m_ueMeasConfig;
            hpi.asConfig.sourceRadioResourceConfig =
                GetRadioResourceConfigForHandoverPreparationInfo();
            hpi.asConfig.sourceMasterInformationBlock.dlBandwidth =
                sourceComponentCarrier->GetDlBandwidth();
            hpi.asConfig.sourceMasterInformationBlock.systemFrameNumber = 0;
            hpi.asConfig.sourceSystemInformationBlockType1.cellAccessRelatedInfo.plmnIdentityInfo
                .plmnIdentity = m_rrc->m_sib1.at(m_componentCarrierId)
                                    .cellAccessRelatedInfo.plmnIdentityInfo.plmnIdentity;
            hpi.asConfig.sourceSystemInformationBlockType1.cellAccessRelatedInfo.cellIdentity =
                m_rrc->ComponentCarrierToCellId(m_componentCarrierId);
            hpi.asConfig.sourceSystemInformationBlockType1.cellAccessRelatedInfo.csgIndication =
                m_rrc->m_sib1.at(m_componentCarrierId).cellAccessRelatedInfo.csgIndication;
            hpi.asConfig.sourceSystemInformationBlockType1.cellAccessRelatedInfo.csgIdentity =
                m_rrc->m_sib1.at(m_componentCarrierId).cellAccessRelatedInfo.csgIdentity;
            LteEnbCmacSapProvider::RachConfig rc =
                m_rrc->m_cmacSapProvider.at(m_componentCarrierId)->GetRachConfig();
            hpi.asConfig.sourceSystemInformationBlockType2.radioResourceConfigCommon
                .rachConfigCommon.preambleInfo.numberOfRaPreambles = rc.numberOfRaPreambles;
            hpi.asConfig.sourceSystemInformationBlockType2.radioResourceConfigCommon
                .rachConfigCommon.raSupervisionInfo.preambleTransMax = rc.preambleTransMax;
            hpi.asConfig.sourceSystemInformationBlockType2.radioResourceConfigCommon
                .rachConfigCommon.raSupervisionInfo.raResponseWindowSize = rc.raResponseWindowSize;
            hpi.asConfig.sourceSystemInformationBlockType2.radioResourceConfigCommon
                .rachConfigCommon.txFailParam.connEstFailCount = rc.connEstFailCount;
            hpi.asConfig.sourceSystemInformationBlockType2.freqInfo.ulCarrierFreq =
                sourceComponentCarrier->GetUlEarfcn();
            hpi.asConfig.sourceSystemInformationBlockType2.freqInfo.ulBandwidth =
                sourceComponentCarrier->GetUlBandwidth();
            params.rrcContext = m_rrc->m_rrcSapUser->EncodeHandoverPreparationInformation(hpi);

            NS_LOG_LOGIC("oldEnbUeX2apId = " << params.oldEnbUeX2apId);
            NS_LOG_LOGIC("sourceCellId = " << params.sourceCellId);
            NS_LOG_LOGIC("targetCellId = " << params.targetCellId);
            NS_LOG_LOGIC("mmeUeS1apId = " << params.mmeUeS1apId);
            NS_LOG_LOGIC("rrcContext   = " << params.rrcContext);

            m_rrc->m_x2SapProvider->SendHandoverRequest(params);
            SwitchToState(HANDOVER_PREPARATION);
        }
    }
    break;

    default:
        NS_FATAL_ERROR("method unexpected in state " << ToString(m_state));
        break;
    }
}

void
NoriUeManager::RecvHandoverRequestAck(EpcX2SapUser::HandoverRequestAckParams params)
{
    NS_LOG_FUNCTION(this);

    NS_ASSERT_MSG(params.notAdmittedBearers.empty(),
                  "not admission of some bearers upon handover is not supported");
    NS_ASSERT_MSG(params.admittedBearers.size() == m_drbMap.size(),
                  "not enough bearers in admittedBearers");

    // note: the Handover command from the target eNB to the source eNB
    // is expected to be sent transparently to the UE; however, here we
    // decode the message and eventually re-encode it. This way we can
    // support both a real RRC protocol implementation and an ideal one
    // without actual RRC protocol encoding.

    Ptr<Packet> encodedHandoverCommand = params.rrcContext;
    LteRrcSap::RrcConnectionReconfiguration handoverCommand =
        m_rrc->m_rrcSapUser->DecodeHandoverCommand(encodedHandoverCommand);
    if (handoverCommand.haveNonCriticalExtension)
    {
        // Total number of component carriers =
        // handoverCommand.nonCriticalExtension.sCellToAddModList.size() + 1 (Primary carrier)
        if (handoverCommand.nonCriticalExtension.sCellToAddModList.size() + 1 !=
            m_rrc->m_numberOfComponentCarriers)
        {
            // Currently handover is only possible if source and target eNBs have equal number of
            // component carriers
            NS_FATAL_ERROR("The source and target eNBs have unequal number of component carriers. "
                           "Target eNB CCs = "
                           << handoverCommand.nonCriticalExtension.sCellToAddModList.size() + 1
                           << " Source eNB CCs = " << m_rrc->m_numberOfComponentCarriers);
        }
    }
    m_rrc->m_rrcSapUser->SendRrcConnectionReconfiguration(m_rnti, handoverCommand);
    SwitchToState(HANDOVER_LEAVING);
    m_handoverLeavingTimeout = Simulator::Schedule(m_rrc->m_handoverLeavingTimeoutDuration,
                                                   &NoriLteEnbRrc::HandoverLeavingTimeout,
                                                   m_rrc,
                                                   m_rnti);
    NS_ASSERT(handoverCommand.haveMobilityControlInfo);
    m_rrc->m_handoverStartTrace(m_imsi,
                                m_rrc->ComponentCarrierToCellId(m_componentCarrierId),
                                m_rnti,
                                handoverCommand.mobilityControlInfo.targetPhysCellId);

    // Set the target cell ID and the RNTI so that handover cancel message can be sent if required
    m_targetX2apId = params.newEnbUeX2apId;
    m_targetCellId = params.targetCellId;

    EpcX2SapProvider::SnStatusTransferParams sst;
    sst.oldEnbUeX2apId = params.oldEnbUeX2apId;
    sst.newEnbUeX2apId = params.newEnbUeX2apId;
    sst.sourceCellId = params.sourceCellId;
    sst.targetCellId = params.targetCellId;
    for (auto drbIt = m_drbMap.begin(); drbIt != m_drbMap.end(); ++drbIt)
    {
        // SN status transfer is only for AM RLC
        if (drbIt->second->m_rlc->GetObject<NoriLteRlcAm>())
        {
            LtePdcp::Status status = drbIt->second->m_pdcp->GetStatus();
            EpcX2Sap::ErabsSubjectToStatusTransferItem i;
            i.dlPdcpSn = status.txSn;
            i.ulPdcpSn = status.rxSn;
            sst.erabsSubjectToStatusTransferList.push_back(i);
        }
    }
    m_rrc->m_x2SapProvider->SendSnStatusTransfer(sst);
}

LteRrcSap::RadioResourceConfigDedicated
NoriUeManager::GetRadioResourceConfigForHandoverPreparationInfo()
{
    NS_LOG_FUNCTION(this);
    return BuildRadioResourceConfigDedicated();
}

LteRrcSap::RrcConnectionReconfiguration
NoriUeManager::GetRrcConnectionReconfigurationForHandover(uint8_t componentCarrierId)
{
    NS_LOG_FUNCTION(this);

    LteRrcSap::RrcConnectionReconfiguration result = BuildRrcConnectionReconfiguration();

    auto targetComponentCarrier =
        DynamicCast<ComponentCarrierEnb>(m_rrc->m_componentCarrierPhyConf.at(componentCarrierId));
    result.haveMobilityControlInfo = true;
    result.mobilityControlInfo.targetPhysCellId = targetComponentCarrier->GetCellId();
    result.mobilityControlInfo.haveCarrierFreq = true;
    result.mobilityControlInfo.carrierFreq.dlCarrierFreq = targetComponentCarrier->GetDlEarfcn();
    result.mobilityControlInfo.carrierFreq.ulCarrierFreq = targetComponentCarrier->GetUlEarfcn();
    result.mobilityControlInfo.haveCarrierBandwidth = true;
    result.mobilityControlInfo.carrierBandwidth.dlBandwidth =
        targetComponentCarrier->GetDlBandwidth();
    result.mobilityControlInfo.carrierBandwidth.ulBandwidth =
        targetComponentCarrier->GetUlBandwidth();

    if (m_caSupportConfigured && m_rrc->m_numberOfComponentCarriers > 1)
    {
        // Release sCells
        result.haveNonCriticalExtension = true;

        for (auto& it : m_rrc->m_componentCarrierPhyConf)
        {
            uint8_t ccId = it.first;

            if (ccId == m_componentCarrierId)
            {
                // Skip primary CC.
                continue;
            }
            else if (ccId < m_componentCarrierId)
            {
                // Shift all IDs below PCC forward so PCC can use CC ID 1.
                result.nonCriticalExtension.sCellToReleaseList.push_back(ccId + 1);
            }
        }
    }
    else
    {
        result.haveNonCriticalExtension = false;
    }

    return result;
}

void
NoriUeManager::SendPacket(uint8_t bid, Ptr<Packet> p)
{
    NS_LOG_FUNCTION(this << p << (uint16_t)bid);
    LtePdcpSapProvider::TransmitPdcpSduParameters params;
    params.pdcpSdu = p;
    params.rnti = m_rnti;
    params.lcid = Bid2Lcid(bid);
    uint8_t drbid = Bid2Drbid(bid);
    // Transmit PDCP sdu only if DRB ID found in drbMap
    auto it = m_drbMap.find(drbid);
    if (it != m_drbMap.end())
    {
        Ptr<LteDataRadioBearerInfo> bearerInfo = GetDataRadioBearerInfo(drbid);
        if (bearerInfo)
        {
            NS_LOG_INFO("Send packet to PDCP layer");
            LtePdcpSapProvider* pdcpSapProvider = bearerInfo->m_pdcp->GetLtePdcpSapProvider();
            pdcpSapProvider->TransmitPdcpSdu(params);
        }
    }
}

void
NoriUeManager::SendData(uint8_t bid, Ptr<Packet> p)
{
    NS_LOG_FUNCTION(this << p << (uint16_t)bid);
    switch (m_state)
    {
    case INITIAL_RANDOM_ACCESS:
    case CONNECTION_SETUP:
        NS_LOG_WARN("not connected, discarding packet");
        return;

    case CONNECTED_NORMALLY:
    case CONNECTION_RECONFIGURATION:
    case CONNECTION_REESTABLISHMENT:
    case HANDOVER_PREPARATION:
    case HANDOVER_PATH_SWITCH: {
        NS_LOG_INFO("queueing data on PDCP for transmission over the air");
        SendPacket(bid, p);
    }
    break;

    case HANDOVER_JOINING: {
        // Buffer data until RRC Connection Reconfiguration Complete message is received
        NS_LOG_INFO("buffering data");
        m_packetBuffer.emplace_back(bid, p);
    }
    break;

    case HANDOVER_LEAVING: {
        NS_LOG_INFO("forwarding data to target eNB over X2-U");
        uint8_t drbid = Bid2Drbid(bid);
        EpcX2Sap::UeDataParams params;
        params.sourceCellId = m_rrc->ComponentCarrierToCellId(m_componentCarrierId);
        params.targetCellId = m_targetCellId;
        params.gtpTeid = GetDataRadioBearerInfo(drbid)->m_gtpTeid;
        params.ueData = p;
        m_rrc->m_x2SapProvider->SendUeData(params);
    }
    break;

    default:
        NS_FATAL_ERROR("method unexpected in state " << ToString(m_state));
        break;
    }
}

std::vector<EpcX2Sap::ErabToBeSetupItem>
NoriUeManager::GetErabList()
{
    NS_LOG_FUNCTION(this);
    std::vector<EpcX2Sap::ErabToBeSetupItem> ret;
    for (auto it = m_drbMap.begin(); it != m_drbMap.end(); ++it)
    {
        EpcX2Sap::ErabToBeSetupItem etbsi;
        etbsi.erabId = it->second->m_epsBearerIdentity;
        etbsi.erabLevelQosParameters = it->second->m_epsBearer;
        etbsi.dlForwarding = false;
        etbsi.transportLayerAddress = it->second->m_transportLayerAddress;
        etbsi.gtpTeid = it->second->m_gtpTeid;
        ret.push_back(etbsi);
    }
    return ret;
}

void
NoriUeManager::SendUeContextRelease()
{
    NS_LOG_FUNCTION(this);
    switch (m_state)
    {
    case HANDOVER_PATH_SWITCH:
        NS_LOG_INFO("Send UE CONTEXT RELEASE from target eNB to source eNB");
        EpcX2SapProvider::UeContextReleaseParams ueCtxReleaseParams;
        ueCtxReleaseParams.oldEnbUeX2apId = m_sourceX2apId;
        ueCtxReleaseParams.newEnbUeX2apId = m_rnti;
        ueCtxReleaseParams.sourceCellId = m_sourceCellId;
        ueCtxReleaseParams.targetCellId = m_targetCellId;
        if (!m_rrc->HasCellId(m_sourceCellId))
        {
            m_rrc->m_x2SapProvider->SendUeContextRelease(ueCtxReleaseParams);
        }
        else
        {
            NS_LOG_INFO("Not sending UE CONTEXT RELEASE because handover is internal");
            m_rrc->DoRecvUeContextRelease(ueCtxReleaseParams);
        }
        SwitchToState(CONNECTED_NORMALLY);
        m_rrc->m_handoverEndOkTrace(m_imsi,
                                    m_rrc->ComponentCarrierToCellId(m_componentCarrierId),
                                    m_rnti);
        break;

    default:
        NS_FATAL_ERROR("method unexpected in state " << ToString(m_state));
        break;
    }
}

void
NoriUeManager::RecvHandoverPreparationFailure(uint16_t cellId)
{
    NS_LOG_FUNCTION(this << cellId);
    switch (m_state)
    {
    case HANDOVER_PREPARATION:
        NS_ASSERT(cellId == m_targetCellId);
        NS_LOG_INFO("target eNB sent HO preparation failure, aborting HO");
        SwitchToState(CONNECTED_NORMALLY);
        break;
    case HANDOVER_LEAVING: // case added to tackle HO leaving timer expiration
        NS_ASSERT(cellId == m_targetCellId);
        NS_LOG_INFO("target eNB sent HO preparation failure, aborting HO");
        m_handoverLeavingTimeout.Cancel();
        SendRrcConnectionRelease();
        break;

    default:
        NS_FATAL_ERROR("method unexpected in state " << ToString(m_state));
        break;
    }
}

void
NoriUeManager::RecvSnStatusTransfer(EpcX2SapUser::SnStatusTransferParams params)
{
    NS_LOG_FUNCTION(this);
    for (auto erabIt = params.erabsSubjectToStatusTransferList.begin();
         erabIt != params.erabsSubjectToStatusTransferList.end();
         ++erabIt)
    {
        // LtePdcp::Status status;
        // status.txSn = erabIt->dlPdcpSn;
        // status.rxSn = erabIt->ulPdcpSn;
        // uint8_t drbId = Bid2Drbid (erabIt->erabId);
        // auto drbIt = m_drbMap.find (drbId);
        // NS_ASSERT_MSG (drbIt != m_drbMap.end (), "could not find DRBID " << (uint32_t) drbId);
        // drbIt->second->m_pdcp->SetStatus (status);
    }
}

void
NoriUeManager::RecvUeContextRelease(EpcX2SapUser::UeContextReleaseParams params)
{
    NS_LOG_FUNCTION(this);
    NS_ASSERT_MSG(m_state == HANDOVER_LEAVING, "method unexpected in state " << ToString(m_state));
    m_handoverLeavingTimeout.Cancel();
}

void
NoriUeManager::RecvHandoverCancel(EpcX2SapUser::HandoverCancelParams params)
{
    NS_LOG_FUNCTION(this);
    NS_ASSERT_MSG(m_state == HANDOVER_JOINING, "method unexpected in state " << ToString(m_state));
    m_handoverJoiningTimeout.Cancel();
}

void
NoriUeManager::SendRrcConnectionRelease()
{
    // TODO implement in the 3gpp way, see Section 5.3.8 of 3GPP TS 36.331.
    NS_LOG_FUNCTION(this << (uint32_t)m_rnti);
    // De-activation towards UE, it will deactivate all bearers
    LteRrcSap::RrcConnectionRelease msg;
    msg.rrcTransactionIdentifier = this->GetNewRrcTransactionIdentifier();
    m_rrc->m_rrcSapUser->SendRrcConnectionRelease(m_rnti, msg);

    /**
     * Bearer de-activation indication towards epc-enb application
     * and removal of UE context at the eNodeB
     *
     */
    m_rrc->DoRecvIdealUeContextRemoveRequest(m_rnti);
}

// methods forwarded from RRC SAP

void
NoriUeManager::CompleteSetupUe(LteEnbRrcSapProvider::CompleteSetupUeParameters params)
{
    NS_LOG_FUNCTION(this);
    m_srb0->m_rlc->SetLteRlcSapUser(params.srb0SapUser);
    m_srb1->m_pdcp->SetLtePdcpSapUser(params.srb1SapUser);
}

void
NoriUeManager::RecvRrcConnectionRequest(LteRrcSap::RrcConnectionRequest msg)
{
    NS_LOG_FUNCTION(this);
    switch (m_state)
    {
    case INITIAL_RANDOM_ACCESS: {
        m_connectionRequestTimeout.Cancel();

        if (m_rrc->m_admitRrcConnectionRequest)
        {
            m_imsi = msg.ueIdentity;

            // send RRC CONNECTION SETUP to UE
            LteRrcSap::RrcConnectionSetup msg2;
            msg2.rrcTransactionIdentifier = GetNewRrcTransactionIdentifier();
            msg2.radioResourceConfigDedicated = BuildRadioResourceConfigDedicated();
            m_rrc->m_rrcSapUser->SendRrcConnectionSetup(m_rnti, msg2);

            RecordDataRadioBearersToBeStarted();
            m_connectionSetupTimeout = Simulator::Schedule(m_rrc->m_connectionSetupTimeoutDuration,
                                                           &NoriLteEnbRrc::ConnectionSetupTimeout,
                                                           m_rrc,
                                                           m_rnti);
            SwitchToState(CONNECTION_SETUP);
        }
        else
        {
            NS_LOG_INFO("rejecting connection request for RNTI " << m_rnti);

            // send RRC CONNECTION REJECT to UE
            LteRrcSap::RrcConnectionReject rejectMsg;
            rejectMsg.waitTime = 3;
            m_rrc->m_rrcSapUser->SendRrcConnectionReject(m_rnti, rejectMsg);

            m_connectionRejectedTimeout =
                Simulator::Schedule(m_rrc->m_connectionRejectedTimeoutDuration,
                                    &NoriLteEnbRrc::ConnectionRejectedTimeout,
                                    m_rrc,
                                    m_rnti);
            SwitchToState(CONNECTION_REJECTED);
        }
    }
    break;

    default:
        NS_FATAL_ERROR("method unexpected in state " << ToString(m_state));
        break;
    }
}

void
NoriUeManager::RecvRrcConnectionSetupCompleted(LteRrcSap::RrcConnectionSetupCompleted msg)
{
    NS_LOG_FUNCTION(this);
    switch (m_state)
    {
    case CONNECTION_SETUP:
        m_connectionSetupTimeout.Cancel();
        if (!m_caSupportConfigured && m_rrc->m_numberOfComponentCarriers > 1)
        {
            m_pendingRrcConnectionReconfiguration = true; // Force Reconfiguration
            m_pendingStartDataRadioBearers = true;
        }

        if (m_rrc->m_s1SapProvider != nullptr)
        {
            m_rrc->m_s1SapProvider->InitialUeMessage(m_imsi, m_rnti);
            SwitchToState(ATTACH_REQUEST);
        }
        else
        {
            SwitchToState(CONNECTED_NORMALLY);
        }
        m_rrc->m_connectionEstablishedTrace(m_imsi,
                                            m_rrc->ComponentCarrierToCellId(m_componentCarrierId),
                                            m_rnti);
        break;

    default:
        NS_FATAL_ERROR("method unexpected in state " << ToString(m_state));
        break;
    }
}

void
NoriUeManager::RecvRrcConnectionReconfigurationCompleted(
    LteRrcSap::RrcConnectionReconfigurationCompleted msg)
{
    NS_LOG_FUNCTION(this);
    switch (m_state)
    {
    case CONNECTION_RECONFIGURATION:
        StartDataRadioBearers();
        if (m_needPhyMacConfiguration)
        {
            // configure MAC (and scheduler)
            LteEnbCmacSapProvider::UeConfig req;
            req.m_rnti = m_rnti;
            req.m_transmissionMode = m_physicalConfigDedicated.antennaInfo.transmissionMode;
            for (uint16_t i = 0; i < m_rrc->m_numberOfComponentCarriers; i++)
            {
                m_rrc->m_cmacSapProvider.at(i)->UeUpdateConfigurationReq(req);

                // configure PHY
                m_rrc->m_cphySapProvider.at(i)->SetTransmissionMode(req.m_rnti,
                                                                    req.m_transmissionMode);
                double paDouble = LteRrcSap::ConvertPdschConfigDedicated2Double(
                    m_physicalConfigDedicated.pdschConfigDedicated);
                m_rrc->m_cphySapProvider.at(i)->SetPa(m_rnti, paDouble);
            }

            m_needPhyMacConfiguration = false;
        }
        SwitchToState(CONNECTED_NORMALLY);
        m_rrc->m_connectionReconfigurationTrace(
            m_imsi,
            m_rrc->ComponentCarrierToCellId(m_componentCarrierId),
            m_rnti);
        break;

    // This case is added to NS-3 in order to handle bearer de-activation scenario for CONNECTED
    // state UE
    case CONNECTED_NORMALLY:
        NS_LOG_INFO("ignoring RecvRrcConnectionReconfigurationCompleted in state "
                    << ToString(m_state));
        break;

    case HANDOVER_LEAVING:
        NS_LOG_INFO("ignoring RecvRrcConnectionReconfigurationCompleted in state "
                    << ToString(m_state));
        break;

    case HANDOVER_JOINING: {
        m_handoverJoiningTimeout.Cancel();

        while (!m_packetBuffer.empty())
        {
            NS_LOG_LOGIC("dequeueing data from buffer");
            std::pair<uint8_t, Ptr<Packet>> bidPacket = m_packetBuffer.front();
            uint8_t bid = bidPacket.first;
            Ptr<Packet> p = bidPacket.second;

            NS_LOG_LOGIC("queueing data on PDCP for transmission over the air");
            SendPacket(bid, p);

            m_packetBuffer.pop_front();
        }

        NS_LOG_INFO("Send PATH SWITCH REQUEST to the MME");
        EpcEnbS1SapProvider::PathSwitchRequestParameters params;
        params.rnti = m_rnti;
        params.cellId = m_rrc->ComponentCarrierToCellId(m_componentCarrierId);
        params.mmeUeS1Id = m_imsi;
        SwitchToState(HANDOVER_PATH_SWITCH);
        for (auto it = m_drbMap.begin(); it != m_drbMap.end(); ++it)
        {
            EpcEnbS1SapProvider::BearerToBeSwitched b;
            b.epsBearerId = it->second->m_epsBearerIdentity;
            b.teid = it->second->m_gtpTeid;
            params.bearersToBeSwitched.push_back(b);
        }
        m_rrc->m_s1SapProvider->PathSwitchRequest(params);
    }
    break;

    default:
        NS_FATAL_ERROR("method unexpected in state " << ToString(m_state));
        break;
    }
}

void
NoriUeManager::RecvRrcConnectionReestablishmentRequest(
    LteRrcSap::RrcConnectionReestablishmentRequest msg)
{
    NS_LOG_FUNCTION(this);
    switch (m_state)
    {
    case CONNECTED_NORMALLY:
        break;

    case HANDOVER_LEAVING:
        m_handoverLeavingTimeout.Cancel();
        break;

    default:
        NS_FATAL_ERROR("method unexpected in state " << ToString(m_state));
        break;
    }

    LteRrcSap::RrcConnectionReestablishment msg2;
    msg2.rrcTransactionIdentifier = GetNewRrcTransactionIdentifier();
    msg2.radioResourceConfigDedicated = BuildRadioResourceConfigDedicated();
    m_rrc->m_rrcSapUser->SendRrcConnectionReestablishment(m_rnti, msg2);
    SwitchToState(CONNECTION_REESTABLISHMENT);
}

void
NoriUeManager::RecvRrcConnectionReestablishmentComplete(
    LteRrcSap::RrcConnectionReestablishmentComplete msg)
{
    NS_LOG_FUNCTION(this);
    SwitchToState(CONNECTED_NORMALLY);
}

void
NoriUeManager::RecvMeasurementReport(LteRrcSap::MeasurementReport msg)
{
    uint8_t measId = msg.measResults.measId;
    NS_LOG_FUNCTION(this << (uint16_t)measId);
    NS_LOG_LOGIC(
        "measId " << (uint16_t)measId << " haveMeasResultNeighCells "
                  << msg.measResults.haveMeasResultNeighCells << " measResultListEutra "
                  << msg.measResults.measResultListEutra.size() << " haveMeasResultServFreqList "
                  << msg.measResults.haveMeasResultServFreqList << " measResultServFreqList "
                  << msg.measResults.measResultServFreqList.size());
    NS_LOG_LOGIC("serving cellId "
                 << m_rrc->ComponentCarrierToCellId(m_componentCarrierId) << " RSRP "
                 << (uint16_t)msg.measResults.measResultPCell.rsrpResult << " RSRQ "
                 << (uint16_t)msg.measResults.measResultPCell.rsrqResult);

    for (auto it = msg.measResults.measResultListEutra.begin();
         it != msg.measResults.measResultListEutra.end();
         ++it)
    {
        NS_LOG_LOGIC("neighbour cellId " << it->physCellId << " RSRP "
                                         << (it->haveRsrpResult ? (uint16_t)it->rsrpResult : 255)
                                         << " RSRQ "
                                         << (it->haveRsrqResult ? (uint16_t)it->rsrqResult : 255));
    }

    if ((m_rrc->m_handoverManagementSapProvider != nullptr) &&
        (m_rrc->m_handoverMeasIds.find(measId) != m_rrc->m_handoverMeasIds.end()))
    {
        // this measurement was requested by the handover algorithm
        m_rrc->m_handoverManagementSapProvider->ReportUeMeas(m_rnti, msg.measResults);
    }

    if ((m_rrc->m_ccmRrcSapProvider != nullptr) &&
        (m_rrc->m_componentCarrierMeasIds.find(measId) != m_rrc->m_componentCarrierMeasIds.end()))
    {
        // this measurement was requested by the handover algorithm
        m_rrc->m_ccmRrcSapProvider->ReportUeMeas(m_rnti, msg.measResults);
    }

    if ((m_rrc->m_anrSapProvider != nullptr) &&
        (m_rrc->m_anrMeasIds.find(measId) != m_rrc->m_anrMeasIds.end()))
    {
        // this measurement was requested by the ANR function
        m_rrc->m_anrSapProvider->ReportUeMeas(msg.measResults);
    }

    if ((!m_rrc->m_ffrRrcSapProvider.empty()) &&
        (m_rrc->m_ffrMeasIds.find(measId) != m_rrc->m_ffrMeasIds.end()))
    {
        // this measurement was requested by the FFR function
        m_rrc->m_ffrRrcSapProvider.at(0)->ReportUeMeas(m_rnti, msg.measResults);
    }
    if (msg.measResults.haveMeasResultServFreqList)
    {
        for (const auto& it : msg.measResults.measResultServFreqList)
        {
            /// ToDo: implement on Ffr algorithm the code to properly parsing the new measResults
            /// message format alternatively it is needed to 'repack' properly the measResults
            /// message before sending to Ffr
            m_rrc->m_ffrRrcSapProvider.at(it.servFreqId)->ReportUeMeas(m_rnti, msg.measResults);
        }
    }

    /// Report any measurements to ComponentCarrierManager, so it can react to any change or
    /// activate the SCC
    m_rrc->m_ccmRrcSapProvider->ReportUeMeas(m_rnti, msg.measResults);
    // fire a trace source
    m_rrc->m_recvMeasurementReportTrace(m_imsi,
                                        m_rrc->ComponentCarrierToCellId(m_componentCarrierId),
                                        m_rnti,
                                        msg);

} // end of NoriUeManager::RecvMeasurementReport

// methods forwarded from CMAC SAP

void
NoriUeManager::CmacUeConfigUpdateInd(LteEnbCmacSapUser::UeConfig cmacParams)
{
    NS_LOG_FUNCTION(this << m_rnti);
    // at this stage used only by the scheduler for updating txMode

    m_physicalConfigDedicated.antennaInfo.transmissionMode = cmacParams.m_transmissionMode;

    m_needPhyMacConfiguration = true;

    // reconfigure the UE RRC
    ScheduleRrcConnectionReconfiguration();
}

// methods forwarded from PDCP SAP

void
NoriUeManager::DoReceivePdcpSdu(LtePdcpSapUser::ReceivePdcpSduParameters params)
{
    NS_LOG_FUNCTION(this);
    if (params.lcid > 2)
    {
        // data radio bearer
        EpsBearerTag tag;
        tag.SetRnti(params.rnti);
        tag.SetBid(Lcid2Bid(params.lcid));
        params.pdcpSdu->AddPacketTag(tag);
        m_rrc->m_forwardUpCallback(params.pdcpSdu);
    }
}

uint16_t
NoriUeManager::GetRnti() const
{
    return m_rnti;
}

uint64_t
NoriUeManager::GetImsi() const
{
    return m_imsi;
}

uint8_t
NoriUeManager::GetComponentCarrierId() const
{
    return m_componentCarrierId;
}

uint16_t
NoriUeManager::GetSrsConfigurationIndex() const
{
    return m_physicalConfigDedicated.soundingRsUlConfigDedicated.srsConfigIndex;
}

void
NoriUeManager::SetSrsConfigurationIndex(uint16_t srsConfIndex)
{
    NS_LOG_FUNCTION(this);
    m_physicalConfigDedicated.soundingRsUlConfigDedicated.srsConfigIndex = srsConfIndex;
    for (uint16_t i = 0; i < m_rrc->m_numberOfComponentCarriers; i++)
    {
        m_rrc->m_cphySapProvider.at(i)->SetSrsConfigurationIndex(m_rnti, srsConfIndex);
    }
    switch (m_state)
    {
    case INITIAL_RANDOM_ACCESS:
        // do nothing, srs conf index will be correctly enforced upon
        // RRC connection establishment
        break;

    default:
        ScheduleRrcConnectionReconfiguration();
        break;
    }
}

NoriUeManager::State
NoriUeManager::GetState() const
{
    return m_state;
}

void
NoriUeManager::SetPdschConfigDedicated(LteRrcSap::PdschConfigDedicated pdschConfigDedicated)
{
    NS_LOG_FUNCTION(this);
    m_physicalConfigDedicated.pdschConfigDedicated = pdschConfigDedicated;

    m_needPhyMacConfiguration = true;

    // reconfigure the UE RRC
    ScheduleRrcConnectionReconfiguration();
}

void
NoriUeManager::CancelPendingEvents()
{
    NS_LOG_FUNCTION(this);
    m_connectionRequestTimeout.Cancel();
    m_connectionRejectedTimeout.Cancel();
    m_connectionSetupTimeout.Cancel();
    m_handoverJoiningTimeout.Cancel();
    m_handoverLeavingTimeout.Cancel();
}

EpcX2Sap::HandoverPreparationFailureParams
NoriUeManager::BuildHoPrepFailMsg()
{
    NS_LOG_FUNCTION(this);
    EpcX2Sap::HandoverPreparationFailureParams res;
    res.oldEnbUeX2apId = m_sourceX2apId;
    res.sourceCellId = m_sourceCellId;
    res.targetCellId = m_rrc->ComponentCarrierToCellId(m_componentCarrierId);
    res.cause = 0;
    res.criticalityDiagnostics = 0;

    return res;
}

EpcX2Sap::HandoverCancelParams
NoriUeManager::BuildHoCancelMsg()
{
    NS_LOG_FUNCTION(this);
    EpcX2Sap::HandoverCancelParams res;
    res.oldEnbUeX2apId = m_rnti; // source cell rnti
    res.newEnbUeX2apId = m_targetX2apId;
    res.sourceCellId = m_rrc->ComponentCarrierToCellId(m_componentCarrierId);
    res.targetCellId = m_targetCellId;
    res.cause = 0;

    return res;
}

uint8_t
NoriUeManager::AddDataRadioBearerInfo(Ptr<LteDataRadioBearerInfo> drbInfo)
{
    NS_LOG_FUNCTION(this);
    const uint8_t MAX_DRB_ID = 32;
    for (int drbid = (m_lastAllocatedDrbid + 1) % MAX_DRB_ID; drbid != m_lastAllocatedDrbid;
         drbid = (drbid + 1) % MAX_DRB_ID)
    {
        if (drbid != 0) // 0 is not allowed
        {
            if (m_drbMap.find(drbid) == m_drbMap.end())
            {
                m_drbMap.insert(std::pair<uint8_t, Ptr<LteDataRadioBearerInfo>>(drbid, drbInfo));
                drbInfo->m_drbIdentity = drbid;
                m_lastAllocatedDrbid = drbid;
                return drbid;
            }
        }
    }
    NS_FATAL_ERROR("no more data radio bearer ids available");
    return 0;
}

Ptr<LteDataRadioBearerInfo>
NoriUeManager::GetDataRadioBearerInfo(uint8_t drbid)
{
    NS_LOG_FUNCTION(this << (uint32_t)drbid);
    NS_ASSERT(0 != drbid);
    auto it = m_drbMap.find(drbid);
    NS_ABORT_IF(it == m_drbMap.end());
    return it->second;
}

void
NoriUeManager::RemoveDataRadioBearerInfo(uint8_t drbid)
{
    NS_LOG_FUNCTION(this << (uint32_t)drbid);
    auto it = m_drbMap.find(drbid);
    NS_ASSERT_MSG(it != m_drbMap.end(),
                  "request to remove radio bearer with unknown drbid " << drbid);
    m_drbMap.erase(it);
}

LteRrcSap::RrcConnectionReconfiguration
NoriUeManager::BuildRrcConnectionReconfiguration()
{
    NS_LOG_FUNCTION(this);
    LteRrcSap::RrcConnectionReconfiguration msg;
    msg.rrcTransactionIdentifier = GetNewRrcTransactionIdentifier();
    msg.haveRadioResourceConfigDedicated = true;
    msg.radioResourceConfigDedicated = BuildRadioResourceConfigDedicated();
    msg.haveMobilityControlInfo = false;
    msg.haveMeasConfig = true;
    msg.measConfig = m_rrc->m_ueMeasConfig;
    if (!m_caSupportConfigured && m_rrc->m_numberOfComponentCarriers > 1)
    {
        m_caSupportConfigured = true;
        NS_LOG_FUNCTION(this << "CA not configured. Configure now!");
        msg.haveNonCriticalExtension = true;
        msg.nonCriticalExtension = BuildNonCriticalExtensionConfigurationCa();
        NS_LOG_FUNCTION(this << " haveNonCriticalExtension " << msg.haveNonCriticalExtension);
    }
    else
    {
        msg.haveNonCriticalExtension = false;
    }

    return msg;
}

LteRrcSap::RadioResourceConfigDedicated
NoriUeManager::BuildRadioResourceConfigDedicated()
{
    NS_LOG_FUNCTION(this);
    LteRrcSap::RadioResourceConfigDedicated rrcd;

    if (m_srb1)
    {
        LteRrcSap::SrbToAddMod stam;
        stam.srbIdentity = m_srb1->m_srbIdentity;
        stam.logicalChannelConfig = m_srb1->m_logicalChannelConfig;
        rrcd.srbToAddModList.push_back(stam);
    }

    for (auto it = m_drbMap.begin(); it != m_drbMap.end(); ++it)
    {
        LteRrcSap::DrbToAddMod dtam;
        dtam.epsBearerIdentity = it->second->m_epsBearerIdentity;
        dtam.drbIdentity = it->second->m_drbIdentity;
        dtam.rlcConfig = it->second->m_rlcConfig;
        dtam.logicalChannelIdentity = it->second->m_logicalChannelIdentity;
        dtam.logicalChannelConfig = it->second->m_logicalChannelConfig;
        rrcd.drbToAddModList.push_back(dtam);
    }

    rrcd.havePhysicalConfigDedicated = true;
    rrcd.physicalConfigDedicated = m_physicalConfigDedicated;
    return rrcd;
}

uint8_t
NoriUeManager::GetNewRrcTransactionIdentifier()
{
    NS_LOG_FUNCTION(this);
    ++m_lastRrcTransactionIdentifier;
    m_lastRrcTransactionIdentifier %= 4;
    return m_lastRrcTransactionIdentifier;
}

uint8_t
NoriUeManager::Lcid2Drbid(uint8_t lcid)
{
    NS_ASSERT(lcid > 2);
    return lcid - 2;
}

uint8_t
NoriUeManager::Drbid2Lcid(uint8_t drbid)
{
    return drbid + 2;
}

uint8_t
NoriUeManager::Lcid2Bid(uint8_t lcid)
{
    NS_ASSERT(lcid > 2);
    return lcid - 2;
}

uint8_t
NoriUeManager::Bid2Lcid(uint8_t bid)
{
    return bid + 2;
}

uint8_t
NoriUeManager::Drbid2Bid(uint8_t drbid)
{
    return drbid;
}

uint8_t
NoriUeManager::Bid2Drbid(uint8_t bid)
{
    return bid;
}

void
NoriUeManager::SwitchToState(State newState)
{
    NS_LOG_FUNCTION(this << ToString(newState));
    State oldState = m_state;
    m_state = newState;
    NS_LOG_INFO(this << " IMSI " << m_imsi << " RNTI " << m_rnti << " UeManager "
                     << ToString(oldState) << " --> " << ToString(newState));
    m_stateTransitionTrace(m_imsi,
                           m_rrc->ComponentCarrierToCellId(m_componentCarrierId),
                           m_rnti,
                           oldState,
                           newState);

    switch (newState)
    {
    case INITIAL_RANDOM_ACCESS:
    case HANDOVER_JOINING:
        NS_FATAL_ERROR("cannot switch to an initial state");
        break;

    case CONNECTION_SETUP:
        break;

    case ATTACH_REQUEST:
        break;

    case CONNECTED_NORMALLY: {
        if (m_pendingRrcConnectionReconfiguration)
        {
            ScheduleRrcConnectionReconfiguration();
        }
        if (m_pendingStartDataRadioBearers && m_caSupportConfigured)
        {
            StartDataRadioBearers();
        }
    }
    break;

    case CONNECTION_RECONFIGURATION:
        break;

    case CONNECTION_REESTABLISHMENT:
        break;

    case HANDOVER_LEAVING:
        break;

    default:
        break;
    }
}

LteRrcSap::NonCriticalExtensionConfiguration
NoriUeManager::BuildNonCriticalExtensionConfigurationCa()
{
    NS_LOG_FUNCTION(this);
    LteRrcSap::NonCriticalExtensionConfiguration ncec;

    for (auto& it : m_rrc->m_componentCarrierPhyConf)
    {
        uint8_t ccId = it.first;

        if (ccId == m_componentCarrierId)
        {
            // Skip primary CC.
            continue;
        }
        else if (ccId < m_componentCarrierId)
        {
            // Shift all IDs below PCC forward so PCC can use CC ID 1.
            ccId++;
        }

        Ptr<ComponentCarrierBaseStation> eNbCcm = it.second;
        LteRrcSap::SCellToAddMod component;
        component.sCellIndex = ccId;
        component.cellIdentification.physCellId = eNbCcm->GetCellId();
        component.cellIdentification.dlCarrierFreq = eNbCcm->GetDlEarfcn();
        component.radioResourceConfigCommonSCell.haveNonUlConfiguration = true;
        component.radioResourceConfigCommonSCell.nonUlConfiguration.dlBandwidth =
            eNbCcm->GetDlBandwidth();
        component.radioResourceConfigCommonSCell.nonUlConfiguration.antennaInfoCommon
            .antennaPortsCount = 0;
        component.radioResourceConfigCommonSCell.nonUlConfiguration.pdschConfigCommon
            .referenceSignalPower = m_rrc->m_cphySapProvider.at(0)->GetReferenceSignalPower();
        component.radioResourceConfigCommonSCell.nonUlConfiguration.pdschConfigCommon.pb = 0;
        component.radioResourceConfigCommonSCell.haveUlConfiguration = true;
        component.radioResourceConfigCommonSCell.ulConfiguration.ulFreqInfo.ulCarrierFreq =
            eNbCcm->GetUlEarfcn();
        component.radioResourceConfigCommonSCell.ulConfiguration.ulFreqInfo.ulBandwidth =
            eNbCcm->GetUlBandwidth();
        component.radioResourceConfigCommonSCell.ulConfiguration.ulPowerControlCommonSCell.alpha =
            0;
        // component.radioResourceConfigCommonSCell.ulConfiguration.soundingRsUlConfigCommon.type =
        // LteRrcSap::SoundingRsUlConfigDedicated::SETUP;
        component.radioResourceConfigCommonSCell.ulConfiguration.soundingRsUlConfigCommon
            .srsBandwidthConfig = 0;
        component.radioResourceConfigCommonSCell.ulConfiguration.soundingRsUlConfigCommon
            .srsSubframeConfig = 0;
        component.radioResourceConfigCommonSCell.ulConfiguration.prachConfigSCell.index = 0;

        component.haveRadioResourceConfigDedicatedSCell = true;
        component.radioResourceConfigDedicatedSCell.physicalConfigDedicatedSCell
            .haveNonUlConfiguration = true;
        component.radioResourceConfigDedicatedSCell.physicalConfigDedicatedSCell
            .haveAntennaInfoDedicated = true;
        component.radioResourceConfigDedicatedSCell.physicalConfigDedicatedSCell.antennaInfo
            .transmissionMode = m_rrc->m_defaultTransmissionMode;
        component.radioResourceConfigDedicatedSCell.physicalConfigDedicatedSCell
            .crossCarrierSchedulingConfig = false;
        component.radioResourceConfigDedicatedSCell.physicalConfigDedicatedSCell
            .havePdschConfigDedicated = true;
        component.radioResourceConfigDedicatedSCell.physicalConfigDedicatedSCell
            .pdschConfigDedicated.pa = LteRrcSap::PdschConfigDedicated::dB0;
        component.radioResourceConfigDedicatedSCell.physicalConfigDedicatedSCell
            .haveUlConfiguration = true;
        component.radioResourceConfigDedicatedSCell.physicalConfigDedicatedSCell
            .haveAntennaInfoUlDedicated = true;
        component.radioResourceConfigDedicatedSCell.physicalConfigDedicatedSCell.antennaInfoUl
            .transmissionMode = m_rrc->m_defaultTransmissionMode;
        component.radioResourceConfigDedicatedSCell.physicalConfigDedicatedSCell
            .pushConfigDedicatedSCell.nPuschIdentity = 0;
        component.radioResourceConfigDedicatedSCell.physicalConfigDedicatedSCell
            .ulPowerControlDedicatedSCell.pSrsOffset = 0;
        component.radioResourceConfigDedicatedSCell.physicalConfigDedicatedSCell
            .haveSoundingRsUlConfigDedicated = true;
        component.radioResourceConfigDedicatedSCell.physicalConfigDedicatedSCell
            .soundingRsUlConfigDedicated.srsConfigIndex = GetSrsConfigurationIndex();
        component.radioResourceConfigDedicatedSCell.physicalConfigDedicatedSCell
            .soundingRsUlConfigDedicated.type = LteRrcSap::SoundingRsUlConfigDedicated::SETUP;
        component.radioResourceConfigDedicatedSCell.physicalConfigDedicatedSCell
            .soundingRsUlConfigDedicated.srsBandwidth = 0;

        ncec.sCellToAddModList.push_back(component);
    }

    return ncec;
}

///////////////////////////////////////////
// eNB RRC methods
///////////////////////////////////////////

NS_OBJECT_ENSURE_REGISTERED(NoriLteEnbRrc);

NoriLteEnbRrc::NoriLteEnbRrc()
    : m_x2SapProvider(nullptr),
      m_cmacSapProvider(0),
      m_handoverManagementSapProvider(nullptr),
      m_ccmRrcSapProvider(nullptr),
      m_anrSapProvider(nullptr),
      m_ffrRrcSapProvider(0),
      m_rrcSapUser(nullptr),
      m_macSapProvider(nullptr),
      m_s1SapProvider(nullptr),
      m_cphySapProvider(0),
      m_configured(false),
      m_lastAllocatedRnti(0),
      m_srsCurrentPeriodicityId(0),
      m_lastAllocatedConfigurationIndex(0),
      m_reconfigureUes(false),
      m_numberOfComponentCarriers(0),
      m_carriersConfigured(false)
{
    NS_LOG_FUNCTION(this);
    m_cmacSapUser.push_back(new EnbRrcMemberLteEnbCmacSapUser(this, 0));
    m_handoverManagementSapUser = new MemberLteHandoverManagementSapUser<NoriLteEnbRrc>(this);
    m_anrSapUser = new MemberLteAnrSapUser<NoriLteEnbRrc>(this);
    m_ffrRrcSapUser.push_back(new MemberLteFfrRrcSapUser<NoriLteEnbRrc>(this));
    m_rrcSapProvider = new MemberLteEnbRrcSapProvider<NoriLteEnbRrc>(this);
    m_x2SapUser = new EpcX2SpecificEpcX2SapUser<NoriLteEnbRrc>(this);
    m_s1SapUser = new MemberEpcEnbS1SapUser<NoriLteEnbRrc>(this);
    m_cphySapUser.push_back(new MemberLteEnbCphySapUser<NoriLteEnbRrc>(this));
    m_ccmRrcSapUser = new MemberLteCcmRrcSapUser<NoriLteEnbRrc>(this);
}

void
NoriLteEnbRrc::ConfigureCarriers(std::map<uint8_t, Ptr<ComponentCarrierBaseStation>> ccPhyConf)
{
    NS_ASSERT_MSG(!m_carriersConfigured, "Secondary carriers can be configured only once.");
    m_componentCarrierPhyConf = ccPhyConf;
    NS_ABORT_MSG_IF(m_numberOfComponentCarriers != m_componentCarrierPhyConf.size(),
                    " Number of component carriers "
                    "are not equal to the number of he component carrier configuration provided");

    for (uint16_t i = 1; i < m_numberOfComponentCarriers; i++)
    {
        m_cphySapUser.push_back(new MemberLteEnbCphySapUser<NoriLteEnbRrc>(this));
        m_cmacSapUser.push_back(new EnbRrcMemberLteEnbCmacSapUser(this, i));
        m_ffrRrcSapUser.push_back(new MemberLteFfrRrcSapUser<NoriLteEnbRrc>(this));
    }
    m_carriersConfigured = true;
    Object::DoInitialize();
}

NoriLteEnbRrc::~NoriLteEnbRrc()
{
    NS_LOG_FUNCTION(this);
}

void
NoriLteEnbRrc::DoDispose()
{
    NS_LOG_FUNCTION(this);
    for (uint16_t i = 0; i < m_numberOfComponentCarriers; i++)
    {
        delete m_cphySapUser[i];
        delete m_cmacSapUser[i];
        delete m_ffrRrcSapUser[i];
    }
    // delete m_cphySapUser;
    m_cphySapUser.erase(m_cphySapUser.begin(), m_cphySapUser.end());
    m_cphySapUser.clear();
    // delete m_cmacSapUser;
    m_cmacSapUser.erase(m_cmacSapUser.begin(), m_cmacSapUser.end());
    m_cmacSapUser.clear();
    // delete m_ffrRrcSapUser;
    m_ffrRrcSapUser.erase(m_ffrRrcSapUser.begin(), m_ffrRrcSapUser.end());
    m_ffrRrcSapUser.clear();
    m_ueMap.clear();
    delete m_handoverManagementSapUser;
    delete m_ccmRrcSapUser;
    delete m_anrSapUser;
    delete m_rrcSapProvider;
    delete m_x2SapUser;
    delete m_s1SapUser;
}

TypeId
NoriLteEnbRrc::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::LteEnbRrc")
            .SetParent<Object>()
            .SetGroupName("Lte")
            .AddConstructor<NoriLteEnbRrc>()
            .AddAttribute("UeMap",
                          "List of UeManager by C-RNTI.",
                          ObjectMapValue(),
                          MakeObjectMapAccessor(&NoriLteEnbRrc::m_ueMap),
                          MakeObjectMapChecker<UeManager>())
            .AddAttribute("DefaultTransmissionMode",
                          "The default UEs' transmission mode (0: SISO)",
                          UintegerValue(0), // default tx-mode
                          MakeUintegerAccessor(&NoriLteEnbRrc::m_defaultTransmissionMode),
                          MakeUintegerChecker<uint8_t>())
            .AddAttribute(
                "EpsBearerToRlcMapping",
                "Specify which type of RLC will be used for each type of EPS bearer.",
                EnumValue(RLC_SM_ALWAYS),
                MakeEnumAccessor<LteEpsBearerToRlcMapping_t>(&NoriLteEnbRrc::m_epsBearerToRlcMapping),
                MakeEnumChecker(RLC_SM_ALWAYS,
                                "RlcSmAlways",
                                RLC_UM_ALWAYS,
                                "RlcUmAlways",
                                RLC_AM_ALWAYS,
                                "RlcAmAlways",
                                PER_BASED,
                                "PacketErrorRateBased"))
            .AddAttribute("SystemInformationPeriodicity",
                          "The interval for sending system information (Time value)",
                          TimeValue(MilliSeconds(80)),
                          MakeTimeAccessor(&NoriLteEnbRrc::m_systemInformationPeriodicity),
                          MakeTimeChecker())

            // SRS related attributes
            .AddAttribute(
                "SrsPeriodicity",
                "The SRS periodicity in milliseconds",
                UintegerValue(40),
                MakeUintegerAccessor(&NoriLteEnbRrc::SetSrsPeriodicity, &NoriLteEnbRrc::GetSrsPeriodicity),
                MakeUintegerChecker<uint32_t>())

            // Timeout related attributes
            .AddAttribute("ConnectionRequestTimeoutDuration",
                          "After a RA attempt, if no RRC CONNECTION REQUEST is "
                          "received before this time, the UE context is destroyed. "
                          "Must account for reception of RAR and transmission of "
                          "RRC CONNECTION REQUEST over UL GRANT. The value of this"
                          "timer should not be greater than T300 timer at UE RRC",
                          TimeValue(MilliSeconds(15)),
                          MakeTimeAccessor(&NoriLteEnbRrc::m_connectionRequestTimeoutDuration),
                          MakeTimeChecker(MilliSeconds(1), MilliSeconds(15)))
            .AddAttribute("ConnectionSetupTimeoutDuration",
                          "After accepting connection request, if no RRC CONNECTION "
                          "SETUP COMPLETE is received before this time, the UE "
                          "context is destroyed. Must account for the UE's reception "
                          "of RRC CONNECTION SETUP and transmission of RRC CONNECTION "
                          "SETUP COMPLETE.",
                          TimeValue(MilliSeconds(150)),
                          MakeTimeAccessor(&NoriLteEnbRrc::m_connectionSetupTimeoutDuration),
                          MakeTimeChecker())
            .AddAttribute("ConnectionRejectedTimeoutDuration",
                          "Time to wait between sending a RRC CONNECTION REJECT and "
                          "destroying the UE context",
                          TimeValue(MilliSeconds(30)),
                          MakeTimeAccessor(&NoriLteEnbRrc::m_connectionRejectedTimeoutDuration),
                          MakeTimeChecker())
            .AddAttribute("HandoverJoiningTimeoutDuration",
                          "After accepting a handover request, if no RRC CONNECTION "
                          "RECONFIGURATION COMPLETE is received before this time, the "
                          "UE context is destroyed. Must account for reception of "
                          "X2 HO REQ ACK by source eNB, transmission of the Handover "
                          "Command, non-contention-based random access and reception "
                          "of the RRC CONNECTION RECONFIGURATION COMPLETE message.",
                          TimeValue(MilliSeconds(200)),
                          MakeTimeAccessor(&NoriLteEnbRrc::m_handoverJoiningTimeoutDuration),
                          MakeTimeChecker())
            .AddAttribute("HandoverLeavingTimeoutDuration",
                          "After issuing a Handover Command, if neither RRC "
                          "CONNECTION RE-ESTABLISHMENT nor X2 UE Context Release has "
                          "been previously received, the UE context is destroyed.",
                          TimeValue(MilliSeconds(500)),
                          MakeTimeAccessor(&NoriLteEnbRrc::m_handoverLeavingTimeoutDuration),
                          MakeTimeChecker())

            // Cell selection related attribute
            .AddAttribute("QRxLevMin",
                          "One of information transmitted within the SIB1 message, "
                          "indicating the required minimum RSRP level that any UE must "
                          "receive from this cell before it is allowed to camp to this "
                          "cell. The default value -70 corresponds to -140 dBm and is "
                          "the lowest possible value as defined by Section 6.3.4 of "
                          "3GPP TS 36.133. This restriction, however, only applies to "
                          "initial cell selection and EPC-enabled simulation.",
                          TypeId::ATTR_GET | TypeId::ATTR_CONSTRUCT,
                          IntegerValue(-70),
                          MakeIntegerAccessor(&NoriLteEnbRrc::m_qRxLevMin),
                          MakeIntegerChecker<int8_t>(-70, -22))
            .AddAttribute("NumberOfComponentCarriers",
                          "Number of Component Carriers",
                          UintegerValue(1),
                          MakeIntegerAccessor(&NoriLteEnbRrc::m_numberOfComponentCarriers),
                          MakeIntegerChecker<int16_t>(MIN_NO_CC, MAX_NO_CC))

            // Handover related attributes
            .AddAttribute("AdmitHandoverRequest",
                          "Whether to admit an X2 handover request from another eNB",
                          BooleanValue(true),
                          MakeBooleanAccessor(&NoriLteEnbRrc::m_admitHandoverRequest),
                          MakeBooleanChecker())
            .AddAttribute("AdmitRrcConnectionRequest",
                          "Whether to admit a connection request from a UE",
                          BooleanValue(true),
                          MakeBooleanAccessor(&NoriLteEnbRrc::m_admitRrcConnectionRequest),
                          MakeBooleanChecker())

            // UE measurements related attributes
            .AddAttribute("RsrpFilterCoefficient",
                          "Determines the strength of smoothing effect induced by "
                          "layer 3 filtering of RSRP in all attached UE; "
                          "if set to 0, no layer 3 filtering is applicable",
                          // i.e. the variable k in 3GPP TS 36.331 section 5.5.3.2
                          UintegerValue(4),
                          MakeUintegerAccessor(&NoriLteEnbRrc::m_rsrpFilterCoefficient),
                          MakeUintegerChecker<uint8_t>(0))
            .AddAttribute("RsrqFilterCoefficient",
                          "Determines the strength of smoothing effect induced by "
                          "layer 3 filtering of RSRQ in all attached UE; "
                          "if set to 0, no layer 3 filtering is applicable",
                          // i.e. the variable k in 3GPP TS 36.331 section 5.5.3.2
                          UintegerValue(4),
                          MakeUintegerAccessor(&NoriLteEnbRrc::m_rsrqFilterCoefficient),
                          MakeUintegerChecker<uint8_t>(0))

            // Trace sources
            .AddTraceSource("NewUeContext",
                            "Fired upon creation of a new UE context.",
                            MakeTraceSourceAccessor(&NoriLteEnbRrc::m_newUeContextTrace),
                            "ns3::LteEnbRrc::NewUeContextTracedCallback")
            .AddTraceSource("ConnectionEstablished",
                            "Fired upon successful RRC connection establishment.",
                            MakeTraceSourceAccessor(&NoriLteEnbRrc::m_connectionEstablishedTrace),
                            "ns3::LteEnbRrc::ConnectionHandoverTracedCallback")
            .AddTraceSource("ConnectionReconfiguration",
                            "trace fired upon RRC connection reconfiguration",
                            MakeTraceSourceAccessor(&NoriLteEnbRrc::m_connectionReconfigurationTrace),
                            "ns3::LteEnbRrc::ConnectionHandoverTracedCallback")
            .AddTraceSource("HandoverStart",
                            "trace fired upon start of a handover procedure",
                            MakeTraceSourceAccessor(&NoriLteEnbRrc::m_handoverStartTrace),
                            "ns3::LteEnbRrc::HandoverStartTracedCallback")
            .AddTraceSource("HandoverEndOk",
                            "trace fired upon successful termination of a handover procedure",
                            MakeTraceSourceAccessor(&NoriLteEnbRrc::m_handoverEndOkTrace),
                            "ns3::LteEnbRrc::ConnectionHandoverTracedCallback")
            .AddTraceSource("RecvMeasurementReport",
                            "trace fired when measurement report is received",
                            MakeTraceSourceAccessor(&NoriLteEnbRrc::m_recvMeasurementReportTrace),
                            "ns3::LteEnbRrc::ReceiveReportTracedCallback")
            .AddTraceSource("NotifyConnectionRelease",
                            "trace fired when an UE is released",
                            MakeTraceSourceAccessor(&NoriLteEnbRrc::m_connectionReleaseTrace),
                            "ns3::LteEnbRrc::ConnectionHandoverTracedCallback")
            .AddTraceSource("RrcTimeout",
                            "trace fired when a timer expires",
                            MakeTraceSourceAccessor(&NoriLteEnbRrc::m_rrcTimeoutTrace),
                            "ns3::LteEnbRrc::TimerExpiryTracedCallback")
            .AddTraceSource(
                "HandoverFailureNoPreamble",
                "trace fired upon handover failure due to non-allocation of non-contention based "
                "preamble at eNB for UE to handover due to max count reached",
                MakeTraceSourceAccessor(&NoriLteEnbRrc::m_handoverFailureNoPreambleTrace),
                "ns3::LteEnbRrc::HandoverFailureTracedCallback")
            .AddTraceSource(
                "HandoverFailureMaxRach",
                "trace fired upon handover failure due to max RACH attempts from UE to target eNB",
                MakeTraceSourceAccessor(&NoriLteEnbRrc::m_handoverFailureMaxRachTrace),
                "ns3::LteEnbRrc::HandoverFailureTracedCallback")
            .AddTraceSource(
                "HandoverFailureLeaving",
                "trace fired upon handover failure due to handover leaving timeout at source eNB",
                MakeTraceSourceAccessor(&NoriLteEnbRrc::m_handoverFailureLeavingTrace),
                "ns3::LteEnbRrc::HandoverFailureTracedCallback")
            .AddTraceSource(
                "HandoverFailureJoining",
                "trace fired upon handover failure due to handover joining timeout at target eNB",
                MakeTraceSourceAccessor(&NoriLteEnbRrc::m_handoverFailureJoiningTrace),
                "ns3::LteEnbRrc::HandoverFailureTracedCallback");
    return tid;
}

void
NoriLteEnbRrc::SetEpcX2SapProvider(EpcX2SapProvider* s)
{
    NS_LOG_FUNCTION(this << s);
    m_x2SapProvider = s;
}

EpcX2SapUser*
NoriLteEnbRrc::GetEpcX2SapUser()
{
    NS_LOG_FUNCTION(this);
    return m_x2SapUser;
}

void
NoriLteEnbRrc::SetLteEnbCmacSapProvider(LteEnbCmacSapProvider* s)
{
    NS_LOG_FUNCTION(this << s);
    m_cmacSapProvider.at(0) = s;
}

void
NoriLteEnbRrc::SetLteEnbCmacSapProvider(LteEnbCmacSapProvider* s, uint8_t pos)
{
    NS_LOG_FUNCTION(this << s);
    if (m_cmacSapProvider.size() > pos)
    {
        m_cmacSapProvider.at(pos) = s;
    }
    else
    {
        m_cmacSapProvider.push_back(s);
        NS_ABORT_IF(m_cmacSapProvider.size() - 1 != pos);
    }
}

LteEnbCmacSapUser*
NoriLteEnbRrc::GetLteEnbCmacSapUser()
{
    NS_LOG_FUNCTION(this);
    return m_cmacSapUser.at(0);
}

LteEnbCmacSapUser*
NoriLteEnbRrc::GetLteEnbCmacSapUser(uint8_t pos)
{
    NS_LOG_FUNCTION(this);
    return m_cmacSapUser.at(pos);
}

void
NoriLteEnbRrc::SetLteHandoverManagementSapProvider(LteHandoverManagementSapProvider* s)
{
    NS_LOG_FUNCTION(this << s);
    m_handoverManagementSapProvider = s;
}

LteHandoverManagementSapUser*
NoriLteEnbRrc::GetLteHandoverManagementSapUser()
{
    NS_LOG_FUNCTION(this);
    return m_handoverManagementSapUser;
}

void
NoriLteEnbRrc::SetLteCcmRrcSapProvider(LteCcmRrcSapProvider* s)
{
    NS_LOG_FUNCTION(this << s);
    m_ccmRrcSapProvider = s;
}

LteCcmRrcSapUser*
NoriLteEnbRrc::GetLteCcmRrcSapUser()
{
    NS_LOG_FUNCTION(this);
    return m_ccmRrcSapUser;
}

void
NoriLteEnbRrc::SetLteAnrSapProvider(LteAnrSapProvider* s)
{
    NS_LOG_FUNCTION(this << s);
    m_anrSapProvider = s;
}

LteAnrSapUser*
NoriLteEnbRrc::GetLteAnrSapUser()
{
    NS_LOG_FUNCTION(this);
    return m_anrSapUser;
}

void
NoriLteEnbRrc::SetLteFfrRrcSapProvider(LteFfrRrcSapProvider* s)
{
    NS_LOG_FUNCTION(this << s);
    if (!m_ffrRrcSapProvider.empty())
    {
        m_ffrRrcSapProvider.at(0) = s;
    }
    else
    {
        m_ffrRrcSapProvider.push_back(s);
    }
}

void
NoriLteEnbRrc::SetLteFfrRrcSapProvider(LteFfrRrcSapProvider* s, uint8_t index)
{
    NS_LOG_FUNCTION(this << s);
    if (m_ffrRrcSapProvider.size() > index)
    {
        m_ffrRrcSapProvider.at(index) = s;
    }
    else
    {
        m_ffrRrcSapProvider.push_back(s);
        NS_ABORT_MSG_IF(m_ffrRrcSapProvider.size() - 1 != index,
                        "You meant to store the pointer at position "
                            << static_cast<uint32_t>(index) << " but it went to "
                            << m_ffrRrcSapProvider.size() - 1);
    }
}

LteFfrRrcSapUser*
NoriLteEnbRrc::GetLteFfrRrcSapUser()
{
    NS_LOG_FUNCTION(this);
    return m_ffrRrcSapUser.at(0);
}

LteFfrRrcSapUser*
NoriLteEnbRrc::GetLteFfrRrcSapUser(uint8_t index)
{
    NS_LOG_FUNCTION(this);
    NS_ASSERT_MSG(index < m_numberOfComponentCarriers,
                  "Invalid component carrier index:"
                      << index << " provided in order to obtain FfrRrcSapUser.");
    return m_ffrRrcSapUser.at(index);
}

void
NoriLteEnbRrc::SetLteEnbRrcSapUser(LteEnbRrcSapUser* s)
{
    NS_LOG_FUNCTION(this << s);
    m_rrcSapUser = s;
}

LteEnbRrcSapProvider*
NoriLteEnbRrc::GetLteEnbRrcSapProvider()
{
    NS_LOG_FUNCTION(this);
    return m_rrcSapProvider;
}

void
NoriLteEnbRrc::SetLteMacSapProvider(LteMacSapProvider* s)
{
    NS_LOG_FUNCTION(this);
    m_macSapProvider = s;
}

void
NoriLteEnbRrc::SetS1SapProvider(EpcEnbS1SapProvider* s)
{
    m_s1SapProvider = s;
}

EpcEnbS1SapUser*
NoriLteEnbRrc::GetS1SapUser()
{
    return m_s1SapUser;
}

void
NoriLteEnbRrc::SetLteEnbCphySapProvider(LteEnbCphySapProvider* s)
{
    NS_LOG_FUNCTION(this << s);
    if (!m_cphySapProvider.empty())
    {
        m_cphySapProvider.at(0) = s;
    }
    else
    {
        m_cphySapProvider.push_back(s);
    }
}

LteEnbCphySapUser*
NoriLteEnbRrc::GetLteEnbCphySapUser()
{
    NS_LOG_FUNCTION(this);
    return m_cphySapUser.at(0);
}

void
NoriLteEnbRrc::SetLteEnbCphySapProvider(LteEnbCphySapProvider* s, uint8_t pos)
{
    NS_LOG_FUNCTION(this << s);
    if (m_cphySapProvider.size() > pos)
    {
        m_cphySapProvider.at(pos) = s;
    }
    else
    {
        m_cphySapProvider.push_back(s);
        NS_ABORT_IF(m_cphySapProvider.size() - 1 != pos);
    }
}

LteEnbCphySapUser*
NoriLteEnbRrc::GetLteEnbCphySapUser(uint8_t pos)
{
    NS_LOG_FUNCTION(this);
    return m_cphySapUser.at(pos);
}

bool
NoriLteEnbRrc::HasUeManager(uint16_t rnti) const
{
    NS_LOG_FUNCTION(this << (uint32_t)rnti);
    auto it = m_ueMap.find(rnti);
    return (it != m_ueMap.end());
}

Ptr<NoriUeManager>
NoriLteEnbRrc::GetUeManager(uint16_t rnti)
{
    NS_LOG_FUNCTION(this << (uint32_t)rnti);
    NS_ASSERT(0 != rnti);
    auto it = m_ueMap.find(rnti);
    NS_ASSERT_MSG(it != m_ueMap.end(), "UE manager for RNTI " << rnti << " not found");
    return it->second;
}

std::vector<uint8_t>
NoriLteEnbRrc::AddUeMeasReportConfig(LteRrcSap::ReportConfigEutra config)
{
    NS_LOG_FUNCTION(this);

    // SANITY CHECK

    NS_ASSERT_MSG(
        m_ueMeasConfig.measIdToAddModList.size() ==
            m_ueMeasConfig.reportConfigToAddModList.size() * m_numberOfComponentCarriers,
        "Measurement identities and reporting configuration should not have different quantity");

    if (Simulator::Now() != Seconds(0))
    {
        NS_FATAL_ERROR("AddUeMeasReportConfig may not be called after the simulation has run");
    }

    // INPUT VALIDATION

    switch (config.triggerQuantity)
    {
    case LteRrcSap::ReportConfigEutra::RSRP:
        if ((config.eventId == LteRrcSap::ReportConfigEutra::EVENT_A5) &&
            (config.threshold2.choice != LteRrcSap::ThresholdEutra::THRESHOLD_RSRP))
        {
            NS_FATAL_ERROR(
                "The given triggerQuantity (RSRP) does not match with the given threshold2.choice");
        }

        if (((config.eventId == LteRrcSap::ReportConfigEutra::EVENT_A1) ||
             (config.eventId == LteRrcSap::ReportConfigEutra::EVENT_A2) ||
             (config.eventId == LteRrcSap::ReportConfigEutra::EVENT_A4) ||
             (config.eventId == LteRrcSap::ReportConfigEutra::EVENT_A5)) &&
            (config.threshold1.choice != LteRrcSap::ThresholdEutra::THRESHOLD_RSRP))
        {
            NS_FATAL_ERROR(
                "The given triggerQuantity (RSRP) does not match with the given threshold1.choice");
        }
        break;

    case LteRrcSap::ReportConfigEutra::RSRQ:
        if ((config.eventId == LteRrcSap::ReportConfigEutra::EVENT_A5) &&
            (config.threshold2.choice != LteRrcSap::ThresholdEutra::THRESHOLD_RSRQ))
        {
            NS_FATAL_ERROR(
                "The given triggerQuantity (RSRQ) does not match with the given threshold2.choice");
        }

        if (((config.eventId == LteRrcSap::ReportConfigEutra::EVENT_A1) ||
             (config.eventId == LteRrcSap::ReportConfigEutra::EVENT_A2) ||
             (config.eventId == LteRrcSap::ReportConfigEutra::EVENT_A4) ||
             (config.eventId == LteRrcSap::ReportConfigEutra::EVENT_A5)) &&
            (config.threshold1.choice != LteRrcSap::ThresholdEutra::THRESHOLD_RSRQ))
        {
            NS_FATAL_ERROR(
                "The given triggerQuantity (RSRQ) does not match with the given threshold1.choice");
        }
        break;

    default:
        NS_FATAL_ERROR("unsupported triggerQuantity");
        break;
    }

    if (config.purpose != LteRrcSap::ReportConfigEutra::REPORT_STRONGEST_CELLS)
    {
        NS_FATAL_ERROR("Only REPORT_STRONGEST_CELLS purpose is supported");
    }

    if (config.reportQuantity != LteRrcSap::ReportConfigEutra::BOTH)
    {
        NS_LOG_WARN("reportQuantity = BOTH will be used instead of the given reportQuantity");
    }

    uint8_t nextId = m_ueMeasConfig.reportConfigToAddModList.size() + 1;

    // create the reporting configuration
    LteRrcSap::ReportConfigToAddMod reportConfig;
    reportConfig.reportConfigId = nextId;
    reportConfig.reportConfigEutra = config;

    // add reporting configuration to UE measurement configuration
    m_ueMeasConfig.reportConfigToAddModList.push_back(reportConfig);

    std::vector<uint8_t> measIds;

    // create measurement identities, linking reporting configuration to all objects
    for (uint16_t componentCarrier = 0; componentCarrier < m_numberOfComponentCarriers;
         componentCarrier++)
    {
        LteRrcSap::MeasIdToAddMod measIdToAddMod;

        uint8_t measId = m_ueMeasConfig.measIdToAddModList.size() + 1;

        measIdToAddMod.measId = measId;
        measIdToAddMod.measObjectId = componentCarrier + 1;
        measIdToAddMod.reportConfigId = nextId;

        m_ueMeasConfig.measIdToAddModList.push_back(measIdToAddMod);
        measIds.push_back(measId);
    }

    return measIds;
}

void
NoriLteEnbRrc::ConfigureCell(std::map<uint8_t, Ptr<ComponentCarrierBaseStation>> ccPhyConf)
{
    auto it = ccPhyConf.begin();
    NS_ASSERT(it != ccPhyConf.end());
    uint16_t ulBandwidth = it->second->GetUlBandwidth();
    uint16_t dlBandwidth = it->second->GetDlBandwidth();
    uint32_t ulEarfcn = it->second->GetUlEarfcn();
    uint32_t dlEarfcn = it->second->GetDlEarfcn();
    NS_LOG_FUNCTION(this << ulBandwidth << dlBandwidth << ulEarfcn << dlEarfcn);
    NS_ASSERT(!m_configured);

    for (const auto& it : ccPhyConf)
    {
        m_cphySapProvider.at(it.first)->SetBandwidth(it.second->GetUlBandwidth(),
                                                     it.second->GetDlBandwidth());
        m_cphySapProvider.at(it.first)->SetEarfcn(it.second->GetUlEarfcn(),
                                                  it.second->GetDlEarfcn());
        m_cphySapProvider.at(it.first)->SetCellId(it.second->GetCellId());
        m_cmacSapProvider.at(it.first)->ConfigureMac(it.second->GetUlBandwidth(),
                                                     it.second->GetDlBandwidth());
        if (m_ffrRrcSapProvider.size() > it.first)
        {
            m_ffrRrcSapProvider.at(it.first)->SetCellId(it.second->GetCellId());
            m_ffrRrcSapProvider.at(it.first)->SetBandwidth(it.second->GetUlBandwidth(),
                                                           it.second->GetDlBandwidth());
        }
    }

    m_dlEarfcn = dlEarfcn;
    m_ulEarfcn = ulEarfcn;
    m_dlBandwidth = dlBandwidth;
    m_ulBandwidth = ulBandwidth;

    /*
     * Initializing the list of measurement objects.
     * Only intra-frequency measurements are supported,
     * so one measurement object is created for each carrier frequency.
     */
    for (const auto& it : ccPhyConf)
    {
        LteRrcSap::MeasObjectToAddMod measObject;
        measObject.measObjectId = it.first + 1;
        measObject.measObjectEutra.carrierFreq = it.second->GetDlEarfcn();
        measObject.measObjectEutra.allowedMeasBandwidth = it.second->GetDlBandwidth();
        measObject.measObjectEutra.presenceAntennaPort1 = false;
        measObject.measObjectEutra.neighCellConfig = 0;
        measObject.measObjectEutra.offsetFreq = 0;
        measObject.measObjectEutra.haveCellForWhichToReportCGI = false;

        m_ueMeasConfig.measObjectToAddModList.push_back(measObject);
    }

    m_ueMeasConfig.haveQuantityConfig = true;
    m_ueMeasConfig.quantityConfig.filterCoefficientRSRP = m_rsrpFilterCoefficient;
    m_ueMeasConfig.quantityConfig.filterCoefficientRSRQ = m_rsrqFilterCoefficient;
    m_ueMeasConfig.haveMeasGapConfig = false;
    m_ueMeasConfig.haveSmeasure = false;
    m_ueMeasConfig.haveSpeedStatePars = false;

    m_sib1.clear();
    m_sib1.reserve(ccPhyConf.size());
    for (const auto& it : ccPhyConf)
    {
        // Enabling MIB transmission
        LteRrcSap::MasterInformationBlock mib;
        mib.dlBandwidth = it.second->GetDlBandwidth();
        mib.systemFrameNumber = 0;
        m_cphySapProvider.at(it.first)->SetMasterInformationBlock(mib);

        // Enabling SIB1 transmission with default values
        LteRrcSap::SystemInformationBlockType1 sib1;
        sib1.cellAccessRelatedInfo.cellIdentity = it.second->GetCellId();
        sib1.cellAccessRelatedInfo.csgIndication = false;
        sib1.cellAccessRelatedInfo.csgIdentity = 0;
        sib1.cellAccessRelatedInfo.plmnIdentityInfo.plmnIdentity = 0; // not used
        sib1.cellSelectionInfo.qQualMin = -34;          // not used, set as minimum value
        sib1.cellSelectionInfo.qRxLevMin = m_qRxLevMin; // set as minimum value
        m_sib1.push_back(sib1);
        m_cphySapProvider.at(it.first)->SetSystemInformationBlockType1(sib1);
    }
    /*
     * Enabling transmission of other SIB. The first time System Information is
     * transmitted is arbitrarily assumed to be at +0.016s, and then it will be
     * regularly transmitted every 80 ms by default (set the
     * SystemInformationPeriodicity attribute to configure this).
     */
    Simulator::Schedule(MilliSeconds(16), &NoriLteEnbRrc::SendSystemInformation, this);

    m_configured = true;
}

void
NoriLteEnbRrc::SetCellId(uint16_t cellId)
{
    // update SIB1
    m_sib1.at(0).cellAccessRelatedInfo.cellIdentity = cellId;
    m_cphySapProvider.at(0)->SetSystemInformationBlockType1(m_sib1.at(0));
}

void
NoriLteEnbRrc::SetCellId(uint16_t cellId, uint8_t ccIndex)
{
    // update SIB1
    m_sib1.at(ccIndex).cellAccessRelatedInfo.cellIdentity = cellId;
    m_cphySapProvider.at(ccIndex)->SetSystemInformationBlockType1(m_sib1.at(ccIndex));
}

uint8_t
NoriLteEnbRrc::CellToComponentCarrierId(uint16_t cellId)
{
    NS_LOG_FUNCTION(this << cellId);
    for (auto& it : m_componentCarrierPhyConf)
    {
        if (it.second->GetCellId() == cellId)
        {
            return it.first;
        }
    }
    NS_FATAL_ERROR("Cell " << cellId << " not found in CC map");
}

uint16_t
NoriLteEnbRrc::ComponentCarrierToCellId(uint8_t componentCarrierId)
{
    NS_LOG_FUNCTION(this << +componentCarrierId);
    return m_componentCarrierPhyConf.at(componentCarrierId)->GetCellId();
}

bool
NoriLteEnbRrc::HasCellId(uint16_t cellId) const
{
    for (auto& it : m_componentCarrierPhyConf)
    {
        if (it.second->GetCellId() == cellId)
        {
            return true;
        }
    }
    return false;
}

bool
NoriLteEnbRrc::SendData(Ptr<Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);
    EpsBearerTag tag;
    bool found = packet->RemovePacketTag(tag);
    NS_ASSERT_MSG(found, "no EpsBearerTag found in packet to be sent");
    Ptr<NoriUeManager> ueManager = GetUeManager(tag.GetRnti());

    NS_LOG_INFO("Sending a packet of " << packet->GetSize() << " bytes to IMSI "
                                       << ueManager->GetImsi() << ", RNTI " << ueManager->GetRnti()
                                       << ", BID " << (uint16_t)tag.GetBid());
    ueManager->SendData(tag.GetBid(), packet);

    return true;
}

void
NoriLteEnbRrc::SetForwardUpCallback(Callback<void, Ptr<Packet>> cb)
{
    m_forwardUpCallback = cb;
}

void
NoriLteEnbRrc::ConnectionRequestTimeout(uint16_t rnti)
{
    NS_LOG_FUNCTION(this << rnti);
    NS_ASSERT_MSG(GetUeManager(rnti)->GetState() == NoriUeManager::INITIAL_RANDOM_ACCESS,
                  "ConnectionRequestTimeout in unexpected state "
                      << ToString(GetUeManager(rnti)->GetState()));
    m_rrcTimeoutTrace(GetUeManager(rnti)->GetImsi(),
                      rnti,
                      ComponentCarrierToCellId(GetUeManager(rnti)->GetComponentCarrierId()),
                      "ConnectionRequestTimeout");
    RemoveUe(rnti);
}

void
NoriLteEnbRrc::ConnectionSetupTimeout(uint16_t rnti)
{
    NS_LOG_FUNCTION(this << rnti);
    NS_ASSERT_MSG(GetUeManager(rnti)->GetState() == NoriUeManager::CONNECTION_SETUP,
                  "ConnectionSetupTimeout in unexpected state "
                      << ToString(GetUeManager(rnti)->GetState()));
    m_rrcTimeoutTrace(GetUeManager(rnti)->GetImsi(),
                      rnti,
                      ComponentCarrierToCellId(GetUeManager(rnti)->GetComponentCarrierId()),
                      "ConnectionSetupTimeout");
    RemoveUe(rnti);
}

void
NoriLteEnbRrc::ConnectionRejectedTimeout(uint16_t rnti)
{
    NS_LOG_FUNCTION(this << rnti);
    NS_ASSERT_MSG(GetUeManager(rnti)->GetState() == NoriUeManager::CONNECTION_REJECTED,
                  "ConnectionRejectedTimeout in unexpected state "
                      << ToString(GetUeManager(rnti)->GetState()));
    m_rrcTimeoutTrace(GetUeManager(rnti)->GetImsi(),
                      rnti,
                      ComponentCarrierToCellId(GetUeManager(rnti)->GetComponentCarrierId()),
                      "ConnectionRejectedTimeout");
    RemoveUe(rnti);
}

void
NoriLteEnbRrc::HandoverJoiningTimeout(uint16_t rnti)
{
    NS_LOG_FUNCTION(this << rnti);
    NS_ASSERT_MSG(GetUeManager(rnti)->GetState() == NoriUeManager::HANDOVER_JOINING,
                  "HandoverJoiningTimeout in unexpected state "
                      << ToString(GetUeManager(rnti)->GetState()));
    m_handoverFailureJoiningTrace(
        GetUeManager(rnti)->GetImsi(),
        rnti,
        ComponentCarrierToCellId(GetUeManager(rnti)->GetComponentCarrierId()));
    // check if the RNTI to be removed is not stale
    if (HasUeManager(rnti))
    {
        /**
         * When the handover joining timer expires at the target cell,
         * then notify the source cell to release the RRC connection and
         * delete the UE context at eNodeB and SGW/PGW. The
         * HandoverPreparationFailure message is reused to notify the source cell
         * through the X2 interface instead of creating a new message.
         */
        Ptr<NoriUeManager> ueManager = GetUeManager(rnti);
        EpcX2Sap::HandoverPreparationFailureParams msg = ueManager->BuildHoPrepFailMsg();
        m_x2SapProvider->SendHandoverPreparationFailure(msg);
        RemoveUe(rnti);
    }
}

void
NoriLteEnbRrc::HandoverLeavingTimeout(uint16_t rnti)
{
    NS_LOG_FUNCTION(this << rnti);
    NS_ASSERT_MSG(GetUeManager(rnti)->GetState() == NoriUeManager::HANDOVER_LEAVING,
                  "HandoverLeavingTimeout in unexpected state "
                      << ToString(GetUeManager(rnti)->GetState()));
    m_handoverFailureLeavingTrace(
        GetUeManager(rnti)->GetImsi(),
        rnti,
        ComponentCarrierToCellId(GetUeManager(rnti)->GetComponentCarrierId()));
    // check if the RNTI to be removed is not stale
    if (HasUeManager(rnti))
    {
        /**
         * Send HO cancel msg to the target eNB and release the RRC connection
         * with the UE and also delete UE context at the source eNB and bearer
         * info at SGW and PGW.
         */
        Ptr<NoriUeManager> ueManager = GetUeManager(rnti);
        EpcX2Sap::HandoverCancelParams msg = ueManager->BuildHoCancelMsg();
        m_x2SapProvider->SendHandoverCancel(msg);
        ueManager->SendRrcConnectionRelease();
    }
}

void
NoriLteEnbRrc::SendHandoverRequest(uint16_t rnti, uint16_t cellId)
{
    NS_LOG_FUNCTION(this << rnti << cellId);
    NS_LOG_LOGIC("Request to send HANDOVER REQUEST");
    NS_ASSERT(m_configured);

    Ptr<NoriUeManager> ueManager = GetUeManager(rnti);
    ueManager->PrepareHandover(cellId);
}

void
NoriLteEnbRrc::DoCompleteSetupUe(uint16_t rnti, LteEnbRrcSapProvider::CompleteSetupUeParameters params)
{
    NS_LOG_FUNCTION(this << rnti);
    GetUeManager(rnti)->CompleteSetupUe(params);
}

void
NoriLteEnbRrc::DoRecvRrcConnectionRequest(uint16_t rnti, LteRrcSap::RrcConnectionRequest msg)
{
    NS_LOG_FUNCTION(this << rnti);
    GetUeManager(rnti)->RecvRrcConnectionRequest(msg);
}

void
NoriLteEnbRrc::DoRecvRrcConnectionSetupCompleted(uint16_t rnti,
                                             LteRrcSap::RrcConnectionSetupCompleted msg)
{
    NS_LOG_FUNCTION(this << rnti);
    GetUeManager(rnti)->RecvRrcConnectionSetupCompleted(msg);
}

void
NoriLteEnbRrc::DoRecvRrcConnectionReconfigurationCompleted(
    uint16_t rnti,
    LteRrcSap::RrcConnectionReconfigurationCompleted msg)
{
    NS_LOG_FUNCTION(this << rnti);
    GetUeManager(rnti)->RecvRrcConnectionReconfigurationCompleted(msg);
}

void
NoriLteEnbRrc::DoRecvRrcConnectionReestablishmentRequest(
    uint16_t rnti,
    LteRrcSap::RrcConnectionReestablishmentRequest msg)
{
    NS_LOG_FUNCTION(this << rnti);
    GetUeManager(rnti)->RecvRrcConnectionReestablishmentRequest(msg);
}

void
NoriLteEnbRrc::DoRecvRrcConnectionReestablishmentComplete(
    uint16_t rnti,
    LteRrcSap::RrcConnectionReestablishmentComplete msg)
{
    NS_LOG_FUNCTION(this << rnti);
    GetUeManager(rnti)->RecvRrcConnectionReestablishmentComplete(msg);
}

void
NoriLteEnbRrc::DoRecvMeasurementReport(uint16_t rnti, LteRrcSap::MeasurementReport msg)
{
    NS_LOG_FUNCTION(this << rnti);
    GetUeManager(rnti)->RecvMeasurementReport(msg);
}

void
NoriLteEnbRrc::DoInitialContextSetupRequest(EpcEnbS1SapUser::InitialContextSetupRequestParameters msg)
{
    NS_LOG_FUNCTION(this);
    Ptr<NoriUeManager> ueManager = GetUeManager(msg.rnti);
    ueManager->InitialContextSetupRequest();
}

void
NoriLteEnbRrc::DoRecvIdealUeContextRemoveRequest(uint16_t rnti)
{
    NS_LOG_FUNCTION(this << rnti);

    // check if the RNTI to be removed is not stale
    if (HasUeManager(rnti))
    {
        Ptr<NoriUeManager> ueManager = GetUeManager(rnti);

        if (ueManager->GetState() == NoriUeManager::HANDOVER_JOINING)
        {
            m_handoverFailureMaxRachTrace(
                GetUeManager(rnti)->GetImsi(),
                rnti,
                ComponentCarrierToCellId(GetUeManager(rnti)->GetComponentCarrierId()));
            /**
             * During the HO, when the RACH failure due to the maximum number of
             * re-attempts is reached the UE request the target eNB to deletes its
             * context. Upon which, the target eNB sends handover preparation
             * failure to the source eNB.
             */
            EpcX2Sap::HandoverPreparationFailureParams msg = ueManager->BuildHoPrepFailMsg();
            m_x2SapProvider->SendHandoverPreparationFailure(msg);
        }

        GetUeManager(rnti)->RecvIdealUeContextRemoveRequest(rnti);
        // delete the UE context at the eNB
        RemoveUe(rnti);
    }
}

void
NoriLteEnbRrc::DoDataRadioBearerSetupRequest(
    EpcEnbS1SapUser::DataRadioBearerSetupRequestParameters request)
{
    NS_LOG_FUNCTION(this);
    Ptr<NoriUeManager> ueManager = GetUeManager(request.rnti);
    ueManager->SetupDataRadioBearer(request.bearer,
                                    request.bearerId,
                                    request.gtpTeid,
                                    request.transportLayerAddress);
}

void
NoriLteEnbRrc::DoPathSwitchRequestAcknowledge(
    EpcEnbS1SapUser::PathSwitchRequestAcknowledgeParameters params)
{
    NS_LOG_FUNCTION(this);
    Ptr<NoriUeManager> ueManager = GetUeManager(params.rnti);
    ueManager->SendUeContextRelease();
}

void
NoriLteEnbRrc::DoRecvHandoverRequest(EpcX2SapUser::HandoverRequestParams req)
{
    NS_LOG_FUNCTION(this);

    NS_LOG_LOGIC("Recv X2 message: HANDOVER REQUEST");

    NS_LOG_LOGIC("oldEnbUeX2apId = " << req.oldEnbUeX2apId);
    NS_LOG_LOGIC("sourceCellId = " << req.sourceCellId);
    NS_LOG_LOGIC("targetCellId = " << req.targetCellId);
    NS_LOG_LOGIC("mmeUeS1apId = " << req.mmeUeS1apId);

    // if no SRS index is available, then do not accept the handover
    if (!m_admitHandoverRequest || IsMaxSrsReached())
    {
        NS_LOG_INFO("rejecting handover request from cellId " << req.sourceCellId);
        EpcX2Sap::HandoverPreparationFailureParams res;
        res.oldEnbUeX2apId = req.oldEnbUeX2apId;
        res.sourceCellId = req.sourceCellId;
        res.targetCellId = req.targetCellId;
        res.cause = 0;
        res.criticalityDiagnostics = 0;
        m_x2SapProvider->SendHandoverPreparationFailure(res);
        return;
    }

    uint8_t componentCarrierId = CellToComponentCarrierId(req.targetCellId);
    uint16_t rnti = AddUe(NoriUeManager::HANDOVER_JOINING, componentCarrierId);
    Ptr<NoriUeManager> ueManager = GetUeManager(rnti);
    ueManager->SetSource(req.sourceCellId, req.oldEnbUeX2apId);
    ueManager->SetImsi(req.mmeUeS1apId);
    LteEnbCmacSapProvider::AllocateNcRaPreambleReturnValue anrcrv =
        m_cmacSapProvider.at(componentCarrierId)->AllocateNcRaPreamble(rnti);
    if (!anrcrv.valid)
    {
        NS_LOG_INFO(
            this
            << " failed to allocate a preamble for non-contention based RA => cannot accept HO");
        m_handoverFailureNoPreambleTrace(
            GetUeManager(rnti)->GetImsi(),
            rnti,
            ComponentCarrierToCellId(GetUeManager(rnti)->GetComponentCarrierId()));
        /**
         * When the maximum non-contention based preambles is reached, then it is considered
         * handover has failed and source cell is notified to release the RRC connection and delete
         * the UE context at eNodeB and SGW/PGW.
         */
        Ptr<NoriUeManager> ueManager = GetUeManager(rnti);
        EpcX2Sap::HandoverPreparationFailureParams msg = ueManager->BuildHoPrepFailMsg();
        m_x2SapProvider->SendHandoverPreparationFailure(msg);
        RemoveUe(rnti); // remove the UE from the target eNB
        return;
    }

    EpcX2SapProvider::HandoverRequestAckParams ackParams;
    ackParams.oldEnbUeX2apId = req.oldEnbUeX2apId;
    ackParams.newEnbUeX2apId = rnti;
    ackParams.sourceCellId = req.sourceCellId;
    ackParams.targetCellId = req.targetCellId;

    for (auto it = req.bearers.begin(); it != req.bearers.end(); ++it)
    {
        ueManager->SetupDataRadioBearer(it->erabLevelQosParameters,
                                        it->erabId,
                                        it->gtpTeid,
                                        it->transportLayerAddress);
        EpcX2Sap::ErabAdmittedItem i;
        i.erabId = it->erabId;
        ackParams.admittedBearers.push_back(i);
    }

    LteRrcSap::RrcConnectionReconfiguration handoverCommand =
        ueManager->GetRrcConnectionReconfigurationForHandover(componentCarrierId);

    handoverCommand.mobilityControlInfo.newUeIdentity = rnti;
    handoverCommand.mobilityControlInfo.haveRachConfigDedicated = true;
    handoverCommand.mobilityControlInfo.rachConfigDedicated.raPreambleIndex = anrcrv.raPreambleId;
    handoverCommand.mobilityControlInfo.rachConfigDedicated.raPrachMaskIndex =
        anrcrv.raPrachMaskIndex;

    LteEnbCmacSapProvider::RachConfig rc =
        m_cmacSapProvider.at(componentCarrierId)->GetRachConfig();
    handoverCommand.mobilityControlInfo.radioResourceConfigCommon.rachConfigCommon.preambleInfo
        .numberOfRaPreambles = rc.numberOfRaPreambles;
    handoverCommand.mobilityControlInfo.radioResourceConfigCommon.rachConfigCommon.raSupervisionInfo
        .preambleTransMax = rc.preambleTransMax;
    handoverCommand.mobilityControlInfo.radioResourceConfigCommon.rachConfigCommon.raSupervisionInfo
        .raResponseWindowSize = rc.raResponseWindowSize;
    handoverCommand.mobilityControlInfo.radioResourceConfigCommon.rachConfigCommon.txFailParam
        .connEstFailCount = rc.connEstFailCount;

    Ptr<Packet> encodedHandoverCommand = m_rrcSapUser->EncodeHandoverCommand(handoverCommand);

    ackParams.rrcContext = encodedHandoverCommand;

    NS_LOG_LOGIC("Send X2 message: HANDOVER REQUEST ACK");

    NS_LOG_LOGIC("oldEnbUeX2apId = " << ackParams.oldEnbUeX2apId);
    NS_LOG_LOGIC("newEnbUeX2apId = " << ackParams.newEnbUeX2apId);
    NS_LOG_LOGIC("sourceCellId = " << ackParams.sourceCellId);
    NS_LOG_LOGIC("targetCellId = " << ackParams.targetCellId);

    m_x2SapProvider->SendHandoverRequestAck(ackParams);
}

void
NoriLteEnbRrc::DoRecvHandoverRequestAck(EpcX2SapUser::HandoverRequestAckParams params)
{
    NS_LOG_FUNCTION(this);

    NS_LOG_LOGIC("Recv X2 message: HANDOVER REQUEST ACK");

    NS_LOG_LOGIC("oldEnbUeX2apId = " << params.oldEnbUeX2apId);
    NS_LOG_LOGIC("newEnbUeX2apId = " << params.newEnbUeX2apId);
    NS_LOG_LOGIC("sourceCellId = " << params.sourceCellId);
    NS_LOG_LOGIC("targetCellId = " << params.targetCellId);

    uint16_t rnti = params.oldEnbUeX2apId;
    Ptr<NoriUeManager> ueManager = GetUeManager(rnti);
    ueManager->RecvHandoverRequestAck(params);
}

void
NoriLteEnbRrc::DoRecvHandoverPreparationFailure(EpcX2SapUser::HandoverPreparationFailureParams params)
{
    NS_LOG_FUNCTION(this);

    NS_LOG_LOGIC("Recv X2 message: HANDOVER PREPARATION FAILURE");

    NS_LOG_LOGIC("oldEnbUeX2apId = " << params.oldEnbUeX2apId);
    NS_LOG_LOGIC("sourceCellId = " << params.sourceCellId);
    NS_LOG_LOGIC("targetCellId = " << params.targetCellId);
    NS_LOG_LOGIC("cause = " << params.cause);
    NS_LOG_LOGIC("criticalityDiagnostics = " << params.criticalityDiagnostics);

    uint16_t rnti = params.oldEnbUeX2apId;

    // check if the RNTI is not stale
    if (HasUeManager(rnti))
    {
        Ptr<NoriUeManager> ueManager = GetUeManager(rnti);
        ueManager->RecvHandoverPreparationFailure(params.targetCellId);
    }
}

void
NoriLteEnbRrc::DoRecvSnStatusTransfer(EpcX2SapUser::SnStatusTransferParams params)
{
    NS_LOG_FUNCTION(this);

    NS_LOG_LOGIC("Recv X2 message: SN STATUS TRANSFER");

    NS_LOG_LOGIC("oldEnbUeX2apId = " << params.oldEnbUeX2apId);
    NS_LOG_LOGIC("newEnbUeX2apId = " << params.newEnbUeX2apId);
    NS_LOG_LOGIC("erabsSubjectToStatusTransferList size = "
                 << params.erabsSubjectToStatusTransferList.size());

    uint16_t rnti = params.newEnbUeX2apId;

    // check if the RNTI to receive SN transfer for is not stale
    if (HasUeManager(rnti))
    {
        Ptr<NoriUeManager> ueManager = GetUeManager(rnti);
        ueManager->RecvSnStatusTransfer(params);
    }
}

void
NoriLteEnbRrc::DoRecvUeContextRelease(EpcX2SapUser::UeContextReleaseParams params)
{
    NS_LOG_FUNCTION(this);

    NS_LOG_LOGIC("Recv X2 message: UE CONTEXT RELEASE");

    NS_LOG_LOGIC("oldEnbUeX2apId = " << params.oldEnbUeX2apId);
    NS_LOG_LOGIC("newEnbUeX2apId = " << params.newEnbUeX2apId);

    uint16_t rnti = params.oldEnbUeX2apId;

    // check if the RNTI to be removed is not stale
    if (HasUeManager(rnti))
    {
        GetUeManager(rnti)->RecvUeContextRelease(params);
        RemoveUe(rnti);
    }
}

void
NoriLteEnbRrc::DoRecvLoadInformation(EpcX2SapUser::LoadInformationParams params)
{
    NS_LOG_FUNCTION(this);

    NS_LOG_LOGIC("Recv X2 message: LOAD INFORMATION");

    NS_LOG_LOGIC("Number of cellInformationItems = " << params.cellInformationList.size());

    NS_ABORT_IF(m_ffrRrcSapProvider.empty());
    m_ffrRrcSapProvider.at(0)->RecvLoadInformation(params);
}

void
NoriLteEnbRrc::DoRecvResourceStatusUpdate(EpcX2SapUser::ResourceStatusUpdateParams params)
{
    NS_LOG_FUNCTION(this);

    NS_LOG_LOGIC("Recv X2 message: RESOURCE STATUS UPDATE");

    NS_LOG_LOGIC(
        "Number of cellMeasurementResultItems = " << params.cellMeasurementResultList.size());

    NS_ASSERT("Processing of RESOURCE STATUS UPDATE X2 message IS NOT IMPLEMENTED");
}

void
NoriLteEnbRrc::DoRecvUeData(EpcX2SapUser::UeDataParams params)
{
    NS_LOG_FUNCTION(this);

    NS_LOG_LOGIC("Recv UE DATA FORWARDING through X2 interface");
    NS_LOG_LOGIC("sourceCellId = " << params.sourceCellId);
    NS_LOG_LOGIC("targetCellId = " << params.targetCellId);
    NS_LOG_LOGIC("gtpTeid = " << params.gtpTeid);
    NS_LOG_LOGIC("ueData = " << params.ueData);
    NS_LOG_LOGIC("ueData size = " << params.ueData->GetSize());

    auto teidInfoIt = m_x2uTeidInfoMap.find(params.gtpTeid);
    if (teidInfoIt != m_x2uTeidInfoMap.end())
    {
        GetUeManager(teidInfoIt->second.rnti)->SendData(teidInfoIt->second.drbid, params.ueData);
    }
    else
    {
        NS_FATAL_ERROR("X2-U data received but no X2uTeidInfo found");
    }
}

void
NoriLteEnbRrc::DoRecvHandoverCancel(EpcX2SapUser::HandoverCancelParams params)
{
    NS_LOG_FUNCTION(this);

    NS_LOG_LOGIC("Recv X2 message: HANDOVER CANCEL");

    NS_LOG_LOGIC("oldEnbUeX2apId = " << params.oldEnbUeX2apId);
    NS_LOG_LOGIC("newEnbUeX2apId = " << params.newEnbUeX2apId);
    NS_LOG_LOGIC("sourceCellId = " << params.sourceCellId);
    NS_LOG_LOGIC("targetCellId = " << params.targetCellId);
    NS_LOG_LOGIC("cause = " << params.cause);

    uint16_t rnti = params.newEnbUeX2apId;
    if (HasUeManager(rnti))
    {
        Ptr<NoriUeManager> ueManager = GetUeManager(rnti);
        ueManager->RecvHandoverCancel(params);
        GetUeManager(rnti)->RecvIdealUeContextRemoveRequest(rnti);
    }
}

uint16_t
NoriLteEnbRrc::DoAllocateTemporaryCellRnti(uint8_t componentCarrierId)
{
    NS_LOG_FUNCTION(this << +componentCarrierId);
    // if no SRS index is available, then do not create a new UE context.
    if (IsMaxSrsReached())
    {
        NS_LOG_WARN("Not enough SRS configuration indices, UE context not created");
        return 0; // return 0 since new RNTI was not assigned for the received preamble
    }
    return AddUe(NoriUeManager::INITIAL_RANDOM_ACCESS, componentCarrierId);
}

void
NoriLteEnbRrc::DoRrcConfigurationUpdateInd(LteEnbCmacSapUser::UeConfig cmacParams)
{
    Ptr<NoriUeManager> ueManager = GetUeManager(cmacParams.m_rnti);
    ueManager->CmacUeConfigUpdateInd(cmacParams);
}

void
NoriLteEnbRrc::DoNotifyLcConfigResult(uint16_t rnti, uint8_t lcid, bool success)
{
    NS_LOG_FUNCTION(this << (uint32_t)rnti);
    NS_FATAL_ERROR("not implemented");
}

std::vector<uint8_t>
NoriLteEnbRrc::DoAddUeMeasReportConfigForHandover(LteRrcSap::ReportConfigEutra reportConfig)
{
    NS_LOG_FUNCTION(this);
    std::vector<uint8_t> measIds = AddUeMeasReportConfig(reportConfig);
    m_handoverMeasIds.insert(measIds.begin(), measIds.end());
    return measIds;
}

uint8_t
NoriLteEnbRrc::DoAddUeMeasReportConfigForComponentCarrier(LteRrcSap::ReportConfigEutra reportConfig)
{
    NS_LOG_FUNCTION(this);
    uint8_t measId = AddUeMeasReportConfig(reportConfig).at(0);
    m_componentCarrierMeasIds.insert(measId);
    return measId;
}

void
NoriLteEnbRrc::DoSetNumberOfComponentCarriers(uint16_t numberOfComponentCarriers)
{
    m_numberOfComponentCarriers = numberOfComponentCarriers;
}

void
NoriLteEnbRrc::DoTriggerHandover(uint16_t rnti, uint16_t targetCellId)
{
    NS_LOG_FUNCTION(this << rnti << targetCellId);

    bool isHandoverAllowed = true;

    Ptr<NoriUeManager> ueManager = GetUeManager(rnti);
    NS_ASSERT_MSG(ueManager, "Cannot find UE context with RNTI " << rnti);

    if (m_anrSapProvider != nullptr && !HasCellId(targetCellId))
    {
        // ensure that proper neighbour relationship exists between source and target cells
        bool noHo = m_anrSapProvider->GetNoHo(targetCellId);
        bool noX2 = m_anrSapProvider->GetNoX2(targetCellId);
        NS_LOG_DEBUG(this << " cellId="
                          << ComponentCarrierToCellId(ueManager->GetComponentCarrierId())
                          << " targetCellId=" << targetCellId << " NRT.NoHo=" << noHo
                          << " NRT.NoX2=" << noX2);

        if (noHo || noX2)
        {
            isHandoverAllowed = false;
            NS_LOG_LOGIC(this << " handover to cell " << targetCellId << " is not allowed by ANR");
        }
    }

    if (ueManager->GetState() != NoriUeManager::CONNECTED_NORMALLY)
    {
        isHandoverAllowed = false;
        NS_LOG_LOGIC(this << " handover is not allowed because the UE"
                          << " rnti=" << rnti << " is in " << ToString(ueManager->GetState())
                          << " state");
    }

    if (isHandoverAllowed)
    {
        // initiate handover execution
        ueManager->PrepareHandover(targetCellId);
    }
}

uint8_t
NoriLteEnbRrc::DoAddUeMeasReportConfigForAnr(LteRrcSap::ReportConfigEutra reportConfig)
{
    NS_LOG_FUNCTION(this);
    uint8_t measId = AddUeMeasReportConfig(reportConfig).at(0);
    m_anrMeasIds.insert(measId);
    return measId;
}

uint8_t
NoriLteEnbRrc::DoAddUeMeasReportConfigForFfr(LteRrcSap::ReportConfigEutra reportConfig)
{
    NS_LOG_FUNCTION(this);
    uint8_t measId = AddUeMeasReportConfig(reportConfig).at(0);
    m_ffrMeasIds.insert(measId);
    return measId;
}

void
NoriLteEnbRrc::DoSetPdschConfigDedicated(uint16_t rnti,
                                     LteRrcSap::PdschConfigDedicated pdschConfigDedicated)
{
    NS_LOG_FUNCTION(this);
    Ptr<NoriUeManager> ueManager = GetUeManager(rnti);
    ueManager->SetPdschConfigDedicated(pdschConfigDedicated);
}

void
NoriLteEnbRrc::DoSendLoadInformation(EpcX2Sap::LoadInformationParams params)
{
    NS_LOG_FUNCTION(this);

    m_x2SapProvider->SendLoadInformation(params);
}

uint16_t
NoriLteEnbRrc::AddUe(NoriUeManager::State state, uint8_t componentCarrierId)
{
    NS_LOG_FUNCTION(this);
    bool found = false;
    uint16_t rnti;
    for (rnti = m_lastAllocatedRnti + 1; (rnti != m_lastAllocatedRnti - 1) && (!found); ++rnti)
    {
        if ((rnti != 0) && (m_ueMap.find(rnti) == m_ueMap.end()))
        {
            found = true;
            break;
        }
    }

    NS_ASSERT_MSG(found, "no more RNTIs available (do you have more than 65535 UEs in a cell?)");
    m_lastAllocatedRnti = rnti;
    Ptr<NoriUeManager> ueManager = CreateObject<NoriUeManager>(this, rnti, state, componentCarrierId);
    m_ccmRrcSapProvider->AddUe(rnti, (uint8_t)state);
    m_ueMap.insert(std::pair<uint16_t, Ptr<NoriUeManager>>(rnti, ueManager));
    ueManager->Initialize();
    const uint16_t cellId = ComponentCarrierToCellId(componentCarrierId);
    NS_LOG_DEBUG(this << " New UE RNTI " << rnti << " cellId " << cellId << " srs CI "
                      << ueManager->GetSrsConfigurationIndex());
    m_newUeContextTrace(cellId, rnti);
    return rnti;
}

void
NoriLteEnbRrc::RemoveUe(uint16_t rnti)
{
    NS_LOG_FUNCTION(this << (uint32_t)rnti);
    auto it = m_ueMap.find(rnti);
    NS_ASSERT_MSG(it != m_ueMap.end(), "request to remove UE info with unknown rnti " << rnti);
    uint64_t imsi = it->second->GetImsi();
    uint16_t srsCi = (*it).second->GetSrsConfigurationIndex();
    // cancel pending events
    it->second->CancelPendingEvents();
    // fire trace upon connection release
    m_connectionReleaseTrace(imsi,
                             ComponentCarrierToCellId(it->second->GetComponentCarrierId()),
                             rnti);
    m_ueMap.erase(it);
    for (uint16_t i = 0; i < m_numberOfComponentCarriers; i++)
    {
        m_cmacSapProvider.at(i)->RemoveUe(rnti);
        m_cphySapProvider.at(i)->RemoveUe(rnti);
    }
    if (m_s1SapProvider != nullptr)
    {
        m_s1SapProvider->UeContextRelease(rnti);
    }
    m_ccmRrcSapProvider->RemoveUe(rnti);
    // need to do this after UeManager has been deleted
    if (srsCi != 0)
    {
        RemoveSrsConfigurationIndex(srsCi);
    }

    m_rrcSapUser->RemoveUe(rnti); // Remove UE context at RRC protocol
}

TypeId
NoriLteEnbRrc::GetRlcType(EpsBearer bearer)
{
    switch (m_epsBearerToRlcMapping)
    {
    case RLC_SM_ALWAYS:
        return LteRlcSm::GetTypeId();

    case RLC_UM_ALWAYS:
        return NoriLteRlcUm::GetTypeId();

    case RLC_AM_ALWAYS:
        return NoriLteRlcAm::GetTypeId();

    case PER_BASED:
        if (bearer.GetPacketErrorLossRate() > 1.0e-5)
        {
            return NoriLteRlcUm::GetTypeId();
        }
        else
        {
            return NoriLteRlcAm::GetTypeId();
        }

    default:
        return LteRlcSm::GetTypeId();
    }
}

void
NoriLteEnbRrc::AddX2Neighbour(uint16_t cellId)
{
    NS_LOG_FUNCTION(this << cellId);

    if (m_anrSapProvider != nullptr)
    {
        m_anrSapProvider->AddNeighbourRelation(cellId);
    }
}

void
NoriLteEnbRrc::SetCsgId(uint32_t csgId, bool csgIndication)
{
    NS_LOG_FUNCTION(this << csgId << csgIndication);
    for (std::size_t componentCarrierId = 0; componentCarrierId < m_sib1.size();
         componentCarrierId++)
    {
        m_sib1.at(componentCarrierId).cellAccessRelatedInfo.csgIdentity = csgId;
        m_sib1.at(componentCarrierId).cellAccessRelatedInfo.csgIndication = csgIndication;
        m_cphySapProvider.at(componentCarrierId)
            ->SetSystemInformationBlockType1(m_sib1.at(componentCarrierId));
    }
}

/// Number of distinct SRS periodicity plus one.
static const uint8_t SRS_ENTRIES = 9;
/**
 * Sounding Reference Symbol (SRS) periodicity (TSRS) in milliseconds. Taken
 * from 3GPP TS 36.213 Table 8.2-1. Index starts from 1.
 */
static const uint16_t g_srsPeriodicity[SRS_ENTRIES] = {0, 2, 5, 10, 20, 40, 80, 160, 320};
/**
 * The lower bound (inclusive) of the SRS configuration indices (ISRS) which
 * use the corresponding SRS periodicity (TSRS). Taken from 3GPP TS 36.213
 * Table 8.2-1. Index starts from 1.
 */
static const uint16_t g_srsCiLow[SRS_ENTRIES] = {0, 0, 2, 7, 17, 37, 77, 157, 317};
/**
 * The upper bound (inclusive) of the SRS configuration indices (ISRS) which
 * use the corresponding SRS periodicity (TSRS). Taken from 3GPP TS 36.213
 * Table 8.2-1. Index starts from 1.
 */
static const uint16_t g_srsCiHigh[SRS_ENTRIES] = {0, 1, 6, 16, 36, 76, 156, 316, 636};

void
NoriLteEnbRrc::SetSrsPeriodicity(uint32_t p)
{
    NS_LOG_FUNCTION(this << p);
    for (uint32_t id = 1; id < SRS_ENTRIES; ++id)
    {
        if (g_srsPeriodicity[id] == p)
        {
            m_srsCurrentPeriodicityId = id;
            return;
        }
    }
    // no match found
    std::ostringstream allowedValues;
    for (uint32_t id = 1; id < SRS_ENTRIES; ++id)
    {
        allowedValues << g_srsPeriodicity[id] << " ";
    }
    NS_FATAL_ERROR("illecit SRS periodicity value " << p
                                                    << ". Allowed values: " << allowedValues.str());
}

uint32_t
NoriLteEnbRrc::GetSrsPeriodicity() const
{
    NS_LOG_FUNCTION(this);
    NS_ASSERT(m_srsCurrentPeriodicityId > 0);
    NS_ASSERT(m_srsCurrentPeriodicityId < SRS_ENTRIES);
    return g_srsPeriodicity[m_srsCurrentPeriodicityId];
}

uint16_t
NoriLteEnbRrc::GetNewSrsConfigurationIndex()
{
    NS_LOG_FUNCTION(this << m_ueSrsConfigurationIndexSet.size());
    // SRS
    NS_ASSERT(m_srsCurrentPeriodicityId > 0);
    NS_ASSERT(m_srsCurrentPeriodicityId < SRS_ENTRIES);
    NS_LOG_DEBUG(this << " SRS p " << g_srsPeriodicity[m_srsCurrentPeriodicityId] << " set "
                      << m_ueSrsConfigurationIndexSet.size());
    if (m_ueSrsConfigurationIndexSet.size() >= g_srsPeriodicity[m_srsCurrentPeriodicityId])
    {
        NS_FATAL_ERROR("too many UEs ("
                       << m_ueSrsConfigurationIndexSet.size() + 1
                       << ") for current SRS periodicity "
                       << g_srsPeriodicity[m_srsCurrentPeriodicityId]
                       << ", consider increasing the value of ns3::LteEnbRrc::SrsPeriodicity");
    }

    if (m_ueSrsConfigurationIndexSet.empty())
    {
        // first entry
        m_lastAllocatedConfigurationIndex = g_srsCiLow[m_srsCurrentPeriodicityId];
        m_ueSrsConfigurationIndexSet.insert(m_lastAllocatedConfigurationIndex);
    }
    else
    {
        // find a CI from the available ones
        auto rit = m_ueSrsConfigurationIndexSet.rbegin();
        NS_ASSERT(rit != m_ueSrsConfigurationIndexSet.rend());
        NS_LOG_DEBUG(this << " lower bound " << (*rit) << " of "
                          << g_srsCiHigh[m_srsCurrentPeriodicityId]);
        if ((*rit) < g_srsCiHigh[m_srsCurrentPeriodicityId])
        {
            // got it from the upper bound
            m_lastAllocatedConfigurationIndex = (*rit) + 1;
            m_ueSrsConfigurationIndexSet.insert(m_lastAllocatedConfigurationIndex);
        }
        else
        {
            // look for released ones
            for (uint16_t srcCi = g_srsCiLow[m_srsCurrentPeriodicityId];
                 srcCi < g_srsCiHigh[m_srsCurrentPeriodicityId];
                 srcCi++)
            {
                auto it = m_ueSrsConfigurationIndexSet.find(srcCi);
                if (it == m_ueSrsConfigurationIndexSet.end())
                {
                    m_lastAllocatedConfigurationIndex = srcCi;
                    m_ueSrsConfigurationIndexSet.insert(srcCi);
                    break;
                }
            }
        }
    }
    return m_lastAllocatedConfigurationIndex;
}

void
NoriLteEnbRrc::RemoveSrsConfigurationIndex(uint16_t srcCi)
{
    NS_LOG_FUNCTION(this << srcCi);
    auto it = m_ueSrsConfigurationIndexSet.find(srcCi);
    NS_ASSERT_MSG(it != m_ueSrsConfigurationIndexSet.end(),
                  "request to remove unknown SRS CI " << srcCi);
    m_ueSrsConfigurationIndexSet.erase(it);
}

bool
NoriLteEnbRrc::IsMaxSrsReached()
{
    NS_ASSERT(m_srsCurrentPeriodicityId > 0);
    NS_ASSERT(m_srsCurrentPeriodicityId < SRS_ENTRIES);
    NS_LOG_DEBUG(this << " SRS p " << g_srsPeriodicity[m_srsCurrentPeriodicityId] << " set "
                      << m_ueSrsConfigurationIndexSet.size());
    return m_ueSrsConfigurationIndexSet.size() >= g_srsPeriodicity[m_srsCurrentPeriodicityId];
}

uint8_t
NoriLteEnbRrc::GetLogicalChannelGroup(EpsBearer bearer)
{
    if (bearer.GetResourceType() > 0) // 1, 2 for GBR and DC-GBR
    {
        return 1;
    }
    else
    {
        return 2;
    }
}

uint8_t
NoriLteEnbRrc::GetLogicalChannelPriority(EpsBearer bearer)
{
    return bearer.qci;
}

void
NoriLteEnbRrc::SendSystemInformation()
{
    // NS_LOG_FUNCTION (this);

    for (auto& it : m_componentCarrierPhyConf)
    {
        uint8_t ccId = it.first;

        LteRrcSap::SystemInformation si;
        si.haveSib2 = true;
        si.sib2.freqInfo.ulCarrierFreq = it.second->GetUlEarfcn();
        si.sib2.freqInfo.ulBandwidth = it.second->GetUlBandwidth();
        si.sib2.radioResourceConfigCommon.pdschConfigCommon.referenceSignalPower =
            m_cphySapProvider.at(ccId)->GetReferenceSignalPower();
        si.sib2.radioResourceConfigCommon.pdschConfigCommon.pb = 0;

        LteEnbCmacSapProvider::RachConfig rc = m_cmacSapProvider.at(ccId)->GetRachConfig();
        LteRrcSap::RachConfigCommon rachConfigCommon;
        rachConfigCommon.preambleInfo.numberOfRaPreambles = rc.numberOfRaPreambles;
        rachConfigCommon.raSupervisionInfo.preambleTransMax = rc.preambleTransMax;
        rachConfigCommon.raSupervisionInfo.raResponseWindowSize = rc.raResponseWindowSize;
        rachConfigCommon.txFailParam.connEstFailCount = rc.connEstFailCount;
        si.sib2.radioResourceConfigCommon.rachConfigCommon = rachConfigCommon;

        m_rrcSapUser->SendSystemInformation(it.second->GetCellId(), si);
    }

    /*
     * For simplicity, we use the same periodicity for all SIBs. Note that in real
     * systems the periodicy of each SIBs could be different.
     */
    Simulator::Schedule(m_systemInformationPeriodicity, &NoriLteEnbRrc::SendSystemInformation, this);
}

bool
NoriLteEnbRrc::IsRandomAccessCompleted(uint16_t rnti)
{
    NS_LOG_FUNCTION(this << (uint32_t)rnti);
    Ptr<NoriUeManager> ueManager = GetUeManager(rnti);
    switch (ueManager->GetState())
    {
    case NoriUeManager::CONNECTED_NORMALLY:
    case NoriUeManager::CONNECTION_RECONFIGURATION:
        return true;
    default:
        return false;
    }
}

} // namespace ns3
