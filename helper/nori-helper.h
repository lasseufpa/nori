/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */

// Copyright (c) 2019 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
//
// SPDX-License-Identifier: GPL-2.0-only

#ifndef NORI_HELPER_H
#define NORI_HELPER_H

#include "ns3/cc-bwp-helper.h"
#include "ns3/ideal-beamforming-helper.h"
#include "ns3/nr-bearer-stats-connector.h"
#include "ns3/nr-mac-scheduling-stats.h"

#include <ns3/eps-bearer.h>
#include <ns3/net-device-container.h>
#include <ns3/node-container.h>
#include <ns3/nr-control-messages.h>
#include <ns3/nr-spectrum-phy.h>
#include <ns3/object-factory.h>
#include <ns3/three-gpp-propagation-loss-model.h>
#include <ns3/three-gpp-spectrum-propagation-loss-model.h>

#include <ns3/nr-helper.h>
#include "nori-bearer-stats-connector.h"

namespace ns3
{

class NrUePhy;
class NrGnbPhy;
class SpectrumChannel;
class NrSpectrumValueHelper;
class NrGnbMac;
class EpcHelper;
class EpcTft;
class NoriBearerStatsCalculator;
class NrMacRxTrace;
class NrPhyRxTrace;
class ComponentCarrierEnb;
class ComponentCarrier;
class NrMacScheduler;
class NoriGnbNetDevice;
class NrUeNetDevice;
class NrUeMac;
class BwpManagerGnb;
class BwpManagerUe;

class NoriHelper : public NrHelper
{
  public:
    /**
     * \brief NoriHelper constructor
     */
    NoriHelper();
    /**
     * \brief ~NoriHelper
     */
    ~NoriHelper() override;

    /**
     * \brief GetTypeId
     * \return the type id of the object
     */
    static TypeId GetTypeId();

    /**
     * \brief Install one (or more) UEs
     * \param c Node container with the UEs
     * \param allBwps The spectrum configuration that comes from CcBwpHelper
     * \return a NetDeviceContainer with the net devices that have been installed.
     *
     */

    void EnableE2PdcpTraces (void);
    Ptr<NoriBearerStatsCalculator> GetE2PdcpStats (void);
    void EnableE2RlcTraces (void);
    Ptr<NoriBearerStatsCalculator> GetE2RlcStats (void);
    
    void SetBasicCellId (uint16_t basicCellId);
    uint16_t GetBasicCellId () const;
    

    /**
     * \brief Get the RLC stats calculator object
     *
     * \return The NrBearerStatsCalculator stats calculator object to write RLC traces
     */
    Ptr<NoriBearerStatsCalculator> GetRlcStatsCalculator();

    /**
     * \brief Get the PDCP stats calculator object
     *
     * \return The NrBearerStatsCalculator stats calculator object to write PDCP traces
     */
    Ptr<NoriBearerStatsCalculator> GetPdcpStatsCalculator();

    void AttachToClosestEnb(NetDeviceContainer ueDevices, NetDeviceContainer enbDevices);


  private:
    Ptr<NrGnbPhy> CreateGnbPhy(const Ptr<Node>& n,
                               const std::unique_ptr<BandwidthPartInfo>& bwp,
                               const Ptr<NoriGnbNetDevice>& dev,
                               const NrSpectrumPhy::NrPhyRxCtrlEndOkCallback& phyEndCtrlCallback);
    

    Ptr<NoriBearerStatsCalculator> m_e2PdcpStats;
    Ptr<NoriBearerStatsCalculator> m_e2PdcpStatsLte;
    Ptr<NoriBearerStatsCalculator> m_e2RlcStats;
    Ptr<NoriBearerStatsCalculator> m_e2RlcStatsLte;

    /**
    * The `E2Mode` attribute. If true, enable E2 interface
    */
    bool m_e2mode_nr;
    bool m_e2mode_lte;
    std::string m_e2ip;
    uint16_t m_e2port;
    uint16_t m_e2localPort;

    bool m_enableMimoFeedback{false}; ///< Let UE compute MIMO feedback with PMI and RI
    ObjectFactory m_pmSearchFactory;  ///< Factory for precoding matrix search algorithm

    /**
     * Assign a fixed random variable stream number to the channel and propagation
     * objects. This function will save the objects to which it has assigned stream
     * to not overwrite assignment, because these objects are shared by gNB and UE
     * devices.
     *
     * The InstallGnbDevice() or InstallUeDevice method should have previously
     * been called by the user on the given devices.
     *
     *
     * \param c NetDeviceContainer of the set of net devices for which the
     *          LteNetDevice should be modified to use a fixed stream
     * \param stream first stream index to use
     * \return the number of stream indices (possibly zero) that have been assigned
     */
    int64_t DoAssignStreamsToChannelObjects(Ptr<NrSpectrumPhy> phy, int64_t currentStream);

    uint64_t GetStartTime (void);

    /**
     *  \brief The actual function to trigger a manual bearer de-activation
     *  \param ueDevice the UE on which bearer to be de-activated must be of the type LteUeNetDevice
     *  \param enbDevice eNB, must be of the type LteEnbNetDevice
     *  \param bearerId Bearer Identity which is to be de-activated
     *
     *  This method is normally scheduled by DeActivateDedicatedEpsBearer() to run at a specific
     *  time when a manual bearer de-activation is desired by the simulation user.
     */
    void DoDeActivateDedicatedEpsBearer(Ptr<NetDevice> ueDevice,
                                        Ptr<NetDevice> enbDevice,
                                        uint8_t bearerId);

    Ptr<NrGnbPhy> CreateGnbPhy(const Ptr<Node>& n,
                               const std::unique_ptr<BandwidthPartInfo>& bwp,
                               const Ptr<NrGnbNetDevice>& dev,
                               const NrSpectrumPhy::NrPhyRxCtrlEndOkCallback& phyEndCtrlCallback);
    Ptr<NrMacScheduler> CreateGnbSched();
    Ptr<NrGnbMac> CreateGnbMac();

    Ptr<NrUeMac> CreateUeMac() const;
    Ptr<NrUePhy> CreateUePhy(const Ptr<Node>& n,
                             const std::unique_ptr<BandwidthPartInfo>& bwp,
                             const Ptr<NrUeNetDevice>& dev,
                             const NrSpectrumPhy::NrPhyDlHarqFeedbackCallback& dlHarqCallback,
                             const NrSpectrumPhy::NrPhyRxCtrlEndOkCallback& phyRxCtrlCallback);

    Ptr<NetDevice> InstallSingleUeDevice(
        const Ptr<Node>& n,
        const std::vector<std::reference_wrapper<BandwidthPartInfoPtr>> allBwps);
    Ptr<NetDevice> InstallSingleGnbDevice(
        const Ptr<Node>& n,
        const std::vector<std::reference_wrapper<BandwidthPartInfoPtr>> allBwps);
    void AttachToClosestEnb(Ptr<NetDevice> ueDevice, NetDeviceContainer enbDevices);

    ObjectFactory m_gnbNetDeviceFactory;            //!< NetDevice factory for gnb
    ObjectFactory m_ueNetDeviceFactory;             //!< NetDevice factory for ue
    ObjectFactory m_channelFactory;                 //!< Channel factory
    ObjectFactory m_ueMacFactory;                   //!< UE MAC factory
    ObjectFactory m_gnbMacFactory;                  //!< GNB MAC factory
    ObjectFactory m_ueSpectrumFactory;              //!< UE Spectrum factory
    ObjectFactory m_gnbSpectrumFactory;             //!< GNB spectrum factory
    ObjectFactory m_uePhyFactory;                   //!< UE PHY factory
    ObjectFactory m_gnbPhyFactory;                  //!< GNB PHY factory
    ObjectFactory m_ueChannelAccessManagerFactory;  //!< UE Channel access manager factory
    ObjectFactory m_gnbChannelAccessManagerFactory; //!< GNB Channel access manager factory
    ObjectFactory m_schedFactory;                   //!< Scheduler factory
    ObjectFactory m_ueAntennaFactory;               //!< UE antenna factory
    ObjectFactory m_gnbAntennaFactory;              //!< UE antenna factory
    ObjectFactory m_gnbBwpManagerAlgoFactory;       //!< BWP manager algorithm factory
    ObjectFactory m_ueBwpManagerAlgoFactory;        //!< BWP manager algorithm factory
    ObjectFactory m_channelConditionModelFactory;   //!< Channel condition factory
    ObjectFactory m_spectrumPropagationFactory;     //!< Spectrum Factory
    ObjectFactory m_pathlossModelFactory;           //!< Pathloss factory
    ObjectFactory m_gnbDlAmcFactory;                //!< DL AMC factory
    ObjectFactory m_gnbUlAmcFactory;                //!< UL AMC factory
    ObjectFactory m_gnbBeamManagerFactory;          //!< gNb Beam manager factory
    ObjectFactory m_ueBeamManagerFactory;           //!< UE beam manager factory

    uint16_t m_basicCellId;
    uint64_t m_imsiCounter{0};   //!< Imsi counter
    uint16_t m_cellIdCounter{1}; //!< CellId Counter
    uint64_t m_startTime; //!< starting time of the NoriHelper life in epoch format

    Ptr<EpcHelper> m_epcHelper{nullptr};                     //!< Ptr to the EPC helper (optional)
    Ptr<BeamformingHelperBase> m_beamformingHelper{nullptr}; //!< Ptr to the beamforming helper

    bool m_harqEnabled{false};
    bool m_snrTest{false};

    Ptr<NrPhyRxTrace> m_phyStats; //!< Pointer to the PhyRx stats
    Ptr<NrMacRxTrace> m_macStats; //!< Pointer to the MacRx stats

    NrBearerStatsConnector
        m_radioBearerStatsConnectorSimpleTraces; //!< RLC and PDCP statistics connector for simple
                                                 //!< file statistics
    NrBearerStatsConnector
        m_radioBearerStatsConnectorCalculator; //!< RLC and PDCP statistics connector for complex
                                               //!< calculator statistics
    
    NoriBearerStatsConnector m_radioBearerStatsConnector;

    std::map<uint8_t, ComponentCarrier> m_componentCarrierPhyParams; //!< component carrier map
    std::vector<Ptr<Object>>
        m_channelObjectsWithAssignedStreams; //!< channel and propagation objects to which NrHelper
    //!< has assigned streams in order to avoid double
    //!< assignments
    Ptr<NrMacSchedulingStats> m_macSchedStats; //!<< Pointer to NrMacStatsCalculator


};

} // namespace ns3

#endif /* NR_HELPER_H */
