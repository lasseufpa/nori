/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */

#pragma once

#include <cstdint>
#include <map>
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
     * @param when             Time at which the mapping must be applied.
     * @param enableRanSlicing Flag indicating whether RAN slicing is enabled.
     * @param uesPerSlice      Vector with the number of UEs per slice.
     * @param sstPerSlice      Vector with the SST value per slice (same size as uesPerSlice).
     * @param gNbDevs          gNB devices.
     * @param ueDevs           UE devices.
     */
    static void ScheduleSliceMapping(Time when,
                     bool enableRanSlicing,
                     const std::vector<int>& uesPerSlice,
                     const std::vector<uint8_t>& sstPerSlice,
                     NetDeviceContainer gNbDevs,
                     NetDeviceContainer ueDevs);

            /**
             * \brief Get the SST associated to a given UE RNTI.
             *
             * This function is the official entry point for KPM/RC/scheduler
             * components that need to translate a UE RNTI into its SST. The
             * internal representation may still use per-slice lists, but the
             * mapping RNTI -> SST is centralized here.
             *
             * \param rnti 16-bit UE RNTI.
             * \return SST value (0 means unknown / not mapped).
             */
            static uint8_t GetSstForRnti(uint16_t rnti);

  private:
    /**
     * \brief Internal function that applies the slice mapping configuration.
     *
     * It is invoked by Simulator::Schedule from ScheduleSliceMapping().
     */
    static void ConfigureSliceMapping(bool enableRanSlicing,
                      std::vector<int> uesPerSlice,
                      std::vector<uint8_t> sstPerSlice,
                      NetDeviceContainer gNbDevs,
                      NetDeviceContainer ueDevs);

    /**
     * \brief Helper used internally to register the RNTI->SST mapping
     *        whenever a new slice mapping is configured.
     *
     * @param sliceUeRntiMap Per-slice list of UE RNTIs.
    * @param sstPerSlice    Per-slice SST values (must match number of slices).
     */
    static void RegisterSstMapping(const std::vector<std::vector<uint32_t>>& sliceUeRntiMap,
                const std::vector<uint8_t>& sstPerSlice);

    /**
     * \brief Global RNTI -> SST mapping (single source of truth).
     */
    static std::map<uint16_t, uint8_t> m_rntiToSst;
};

} // namespace ns3
