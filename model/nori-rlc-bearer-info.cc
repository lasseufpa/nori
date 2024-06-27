
#include "ns3/lte-radio-bearer-info.h"
#include "ns3/lte-ue-rrc.h"
#include "ns3/lte-rlc.h"
#include "ns3/lte-pdcp.h"
#include "ns3/nori-rlc-bearer-info.h"

namespace ns3 {

TypeId 
RlcBearerInfo::GetTypeId (void)
{
  static TypeId
    tid =
    TypeId ("ns3::RlcBearerInfo")
    .SetParent<Object> ()
    .AddConstructor<RlcBearerInfo> ()
    .AddAttribute ("LteRlc", "RLC instance of the secondary connection.",
                   PointerValue (),
                   MakePointerAccessor (&RlcBearerInfo::m_rlc),
                   MakePointerChecker<LteRlc> ())
    ;
  return tid;
}
}