#include "ns3/object.h"
#include "ns3/ptr.h"
#include "ns3/packet.h"
//#include "ns3/lte-common"
#include "ns3/lte-rrc-sap.h"
#include "ns3/lte-enb-cmac-sap.h"
#include "ns3/lte-rlc.h"

namespace ns3 {
    class RlcBearerInfo : public Object{
        public:
          RlcBearerInfo (void);
          virtual ~RlcBearerInfo (void);
          static TypeId GetTypeId (void);
        
            uint16_t    sourceCellId;
            uint16_t    targetCellId;
            uint32_t    gtpTeid;
            uint16_t    mmWaveRnti;
            uint16_t    lteRnti;
            uint8_t     drbid;
            uint8_t     logicalChannelIdentity;
            LteRrcSap::RlcConfig rlcConfig;
            LteRrcSap::LogicalChannelConfig logicalChannelConfig;
            LteEnbCmacSapProvider::LcInfo lcinfo;
            Ptr<LteRlc> m_rlc;
        };
}
