
#include "ns3/lte-radio-bearer-info.h"
#include "ns3/lte-ue-rrc.h"
#include "ns3/nori-lte-rlc.h"
#include "ns3/lte-pdcp.h"
#include "ns3/nori-rlc-bearer-info.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("NoriRlcBearerInfo");
NS_OBJECT_ENSURE_REGISTERED(NoriRlcBearerInfo);

TypeId 
NoriRlcBearerInfo::GetTypeId (void)
{
  static TypeId
    tid =
    TypeId ("ns3::RlcBearerInfo")
    .SetParent<Object> ()
    .AddConstructor<NoriRlcBearerInfo> ()
    .AddAttribute ("LteRlc", "RLC instance of the secondary connection.",
                   PointerValue (),
                   MakePointerAccessor (&NoriRlcBearerInfo::m_rlc),
                   MakePointerChecker<NoriLteRlc> ())
    ;
  return tid;
}
}