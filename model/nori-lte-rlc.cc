/*
 * Copyright (c) 2011 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
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
 * Author: Nicola Baldo <nbaldo@cttc.es>
 */

#include "nori-lte-rlc.h"

#include "ns3/lte-rlc-sap.h"
#include "ns3/lte-rlc-tag.h"
// #include "lte-mac-sap.h"
// #include "ff-mac-sched-sap.h"

#include "ns3/log.h"
#include "ns3/simulator.h"


namespace ns3
{

NS_LOG_COMPONENT_DEFINE("LteRlc");

/// LteRlcSpecificLteMacSapUser class
class LteRlcSpecificLteMacSapUser : public LteMacSapUser
{
  public:
    /**
     * Constructor
     *
     * \param rlc the RLC
     */
    LteRlcSpecificLteMacSapUser(NoriLteRlc* rlc);

    // Interface implemented from LteMacSapUser
    void NotifyTxOpportunity(LteMacSapUser::TxOpportunityParameters params) override;
    void NotifyHarqDeliveryFailure() override;
    void ReceivePdu(LteMacSapUser::ReceivePduParameters params) override;

  private:
    LteRlcSpecificLteMacSapUser();
    NoriLteRlc* m_rlc; ///< the RLC
};

LteRlcSpecificLteMacSapUser::LteRlcSpecificLteMacSapUser(NoriLteRlc* rlc)
    : m_rlc(rlc)
{
}

LteRlcSpecificLteMacSapUser::LteRlcSpecificLteMacSapUser()
{
}

void
LteRlcSpecificLteMacSapUser::NotifyTxOpportunity(TxOpportunityParameters params)
{
    m_rlc->DoNotifyTxOpportunity(params);
}

void
LteRlcSpecificLteMacSapUser::NotifyHarqDeliveryFailure()
{
    m_rlc->DoNotifyHarqDeliveryFailure();
}

void
LteRlcSpecificLteMacSapUser::ReceivePdu(LteMacSapUser::ReceivePduParameters params)
{
    m_rlc->DoReceivePdu(params);
}

///////////////////////////////////////

NS_OBJECT_ENSURE_REGISTERED(NoriLteRlc);

NoriLteRlc::NoriLteRlc()
    : m_rlcSapUser(nullptr),
      m_macSapProvider(nullptr),
      m_rnti(0),
      m_lcid(0),
      m_imsi(0),
      m_txPacketsInReportingPeriod(0),
      m_txBytesInReportingPeriod(0)
{
    NS_LOG_FUNCTION(this);
    m_rlcSapProvider = new LteRlcSpecificLteRlcSapProvider<NoriLteRlc>(this);
    m_macSapUser = new LteRlcSpecificLteMacSapUser(this);
}

NoriLteRlc::~NoriLteRlc()
{
    NS_LOG_FUNCTION(this);
}

TypeId
NoriLteRlc::GetTypeId()
{
    static TypeId tid = TypeId("ns3::LteRlc")
                            .SetParent<Object>()
                            .SetGroupName("Lte")
                            .AddTraceSource("TxPDU",
                                            "PDU transmission notified to the MAC.",
                                            MakeTraceSourceAccessor(&NoriLteRlc::m_txPdu),
                                            "ns3::LteRlc::NotifyTxTracedCallback")
                            .AddTraceSource("RxPDU",
                                            "PDU received.",
                                            MakeTraceSourceAccessor(&NoriLteRlc::m_rxPdu),
                                            "ns3::LteRlc::ReceiveTracedCallback")
                            .AddTraceSource("TxDrop",
                                            "Trace source indicating a packet "
                                            "has been dropped before transmission",
                                            MakeTraceSourceAccessor(&NoriLteRlc::m_txDropTrace),
                                            "ns3::Packet::TracedCallback");
    return tid;
}

void
NoriLteRlc::DoDispose()
{
    NS_LOG_FUNCTION(this);
    delete (m_rlcSapProvider);
    delete (m_macSapUser);
}

void
NoriLteRlc::SetRnti(uint16_t rnti)
{
    NS_LOG_FUNCTION(this << (uint32_t)rnti);
    m_rnti = rnti;
}

void
NoriLteRlc::SetLcId(uint8_t lcId)
{
    NS_LOG_FUNCTION(this << (uint32_t)lcId);
    m_lcid = lcId;
}

void
NoriLteRlc::SetPacketDelayBudgetMs(uint16_t packetDelayBudget)
{
    NS_LOG_FUNCTION(this << +packetDelayBudget);
    m_packetDelayBudgetMs = packetDelayBudget;
}

void
NoriLteRlc::SetLteRlcSapUser(LteRlcSapUser* s)
{
    NS_LOG_FUNCTION(this << s);
    m_rlcSapUser = s;
}

void NoriLteRlc::SetImsi(uint64_t imsi)
{
    NS_LOG_FUNCTION(this << imsi);
    m_imsi = imsi;
}

LteRlcSapProvider*
NoriLteRlc::GetLteRlcSapProvider()
{
    NS_LOG_FUNCTION(this);
    return m_rlcSapProvider;
}

void
NoriLteRlc::SetLteMacSapProvider(LteMacSapProvider* s)
{
    NS_LOG_FUNCTION(this << s);
    m_macSapProvider = s;
}

LteMacSapUser*
NoriLteRlc::GetLteMacSapUser()
{
    NS_LOG_FUNCTION(this);
    return m_macSapUser;
}

////////////////////////////////////////

NS_OBJECT_ENSURE_REGISTERED(NoriLteRlcSm);

NoriLteRlc::NoriLteRlc()
{
    NS_LOG_FUNCTION(this);
}

NoriLteRlc::~NoriLteRlc()
{
    NS_LOG_FUNCTION(this);
}

TypeId
NoriLteRlc::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::LteRlcSm").SetParent<NoriLteRlc>().SetGroupName("Lte").AddConstructor<NoriLteRlcSm>();
    return tid;
}

void
NoriLteRlcSm::DoInitialize()
{
    NS_LOG_FUNCTION(this);
    ReportBufferStatus();
}

void
NoriLteRlcSm::DoDispose()
{
    NS_LOG_FUNCTION(this);
    NoriLteRlc::DoDispose();
}

void
NoriLteRlcSm::DoTransmitPdcpPdu(Ptr<Packet> p)
{
    NS_LOG_FUNCTION(this << p);
}

void
NoriLteRlcSm::DoReceivePdu(LteMacSapUser::ReceivePduParameters rxPduParams)
{
    NS_LOG_FUNCTION(this << rxPduParams.p);
    // RLC Performance evaluation
    RlcTag rlcTag;
    Time delay;
    bool ret = rxPduParams.p->FindFirstMatchingByteTag(rlcTag);
    NS_ASSERT_MSG(ret, "RlcTag is missing");
    delay = Simulator::Now() - rlcTag.GetSenderTimestamp();
    NS_LOG_LOGIC(" RNTI=" << m_rnti << " LCID=" << (uint32_t)m_lcid << " size="
                          << rxPduParams.p->GetSize() << " delay=" << delay.As(Time::NS));
    m_rxPdu(m_rnti, m_lcid, rxPduParams.p->GetSize(), delay.GetNanoSeconds());
}

void
NoriLteRlcSm::DoNotifyTxOpportunity(LteMacSapUser::TxOpportunityParameters txOpParams)
{
    NS_LOG_FUNCTION(this << txOpParams.bytes);
    LteMacSapProvider::TransmitPduParameters params;
    RlcTag tag(Simulator::Now());

    params.pdu = Create<Packet>(txOpParams.bytes);
    NS_ABORT_MSG_UNLESS(txOpParams.bytes > 0, "Bytes must be > 0");
    /**
     * For RLC SM, the packets are not passed to the upper layers, therefore,
     * in the absence of an header we can safely byte tag the entire packet.
     */
    params.pdu->AddByteTag(tag, 1, params.pdu->GetSize());

    params.rnti = m_rnti;
    params.lcid = m_lcid;
    params.layer = txOpParams.layer;
    params.harqProcessId = txOpParams.harqId;
    params.componentCarrierId = txOpParams.componentCarrierId;

    // RLC Performance evaluation
    NS_LOG_LOGIC(" RNTI=" << m_rnti << " LCID=" << (uint32_t)m_lcid
                          << " size=" << txOpParams.bytes);
    m_txPdu(m_rnti, m_lcid, txOpParams.bytes);

    m_macSapProvider->TransmitPdu(params);
    ReportBufferStatus();
}

void
NoriLteRlcSm::DoNotifyHarqDeliveryFailure()
{
    NS_LOG_FUNCTION(this);
}

void
NoriLteRlcSm::ReportBufferStatus()
{
    NS_LOG_FUNCTION(this);
    LteMacSapProvider::ReportBufferStatusParameters p;
    p.rnti = m_rnti;
    p.lcid = m_lcid;
    p.txQueueSize = 80000;
    p.txQueueHolDelay = 10;
    p.retxQueueSize = 0;
    p.retxQueueHolDelay = 0;
    p.statusPduSize = 0;
    m_macSapProvider->ReportBufferStatus(p);
}

//////////////////////////////////////////

// LteRlcTm::~LteRlcTm ()
// {
// }

//////////////////////////////////////////

// LteRlcUm::~LteRlcUm ()
// {
// }

//////////////////////////////////////////

// LteRlcAm::~LteRlcAm ()
// {
// }

} // namespace ns3
