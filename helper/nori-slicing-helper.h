/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */

#pragma once

#include <vector>

#include "ns3/net-device-container.h"
#include "ns3/nstime.h"

namespace ns3
{

/**
 * \brief Static helper to configure slice mapping (RNTI -> slice).
 *
 * This helper retrieves the UEs' RNTIs and configures the RL scheduler
 * (NrRLMacSchedulerOfdma) with the UE/slice mapping in a scheduled event
 * after the random access / RRC procedure has completed.
 */
class NoriSlicingHelper
{
  public:
    /**
     * \brief Schedule slice mapping configuration.
     *
     * @param when            Time at which the mapping must be applied.
     * @param enableRanSlicing Flag indicating whether RAN slicing is enabled.
     * @param uesPerSlice     Vector with the number of UEs per slice.
     * @param gNbDevs         gNB devices.
     * @param ueDevs          UE devices.
     */
    static void ScheduleSliceMapping(Time when,
                                     bool enableRanSlicing,
                                     const std::vector<int>& uesPerSlice,
                                     NetDeviceContainer gNbDevs,
                                     NetDeviceContainer ueDevs);

  private:
    /**
     * \brief Internal function that applies the slice mapping configuration.
     *
     * It is invoked by Simulator::Schedule from ScheduleSliceMapping().
     */
    static void ConfigureSliceMapping(bool enableRanSlicing,
                                      std::vector<int> uesPerSlice,
                                      NetDeviceContainer gNbDevs,
                                      NetDeviceContainer ueDevs);
};

} // namespace ns3
