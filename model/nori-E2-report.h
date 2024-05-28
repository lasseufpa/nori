#ifndef NORI_E2_REPORT_H_
#define NORI_E2_REPORT_H_
#include <ns3/nr-bearer-stats-calculator.h>
#include <ns3/nr-phy-mac-common.h>

namespace ns3
{

typedef std::pair<uint16_t, uint16_t> RntiCellIdPair_t;

class NoriE2Report : public Object
{
  public:
    /**
     * Constructor
     */
    NoriE2Report();

    /**
     * TypeId
     */
    static TypeId GetTypeId();

    /**
     * Destructor
     */
    ~NoriE2Report() override;

    /**
     * Gets the number of MAC PDUs, UE specific
     * @param rnti
     * @param cellId
     * @return the number of MAC PDUs, UE specific
     */
    uint32_t GetMacPduUeSpecific(uint16_t rnti, uint16_t cellId);

    /**
     * Gets the number of MAC PDUs (initial tx), UE specific
     * @param rnti
     * @param cellId
     * @return the number of MAC PDUs (initial tx), UE specific
     */
    uint32_t GetMacPduInitialTransmissionUeSpecific(uint16_t rnti, uint16_t cellId);

    /**
     * Gets the number of MAC PDUs (retx), UE specific
     * @param rnti
     * @param cellId
     * @return the number of MAC PDUs (retx), UE specific
     */
    uint32_t GetMacPduRetransmissionUeSpecific(uint16_t rnti, uint16_t cellId);

    /**
     * Gets MAC volume (amount of TXed bytes), UE specific
     * @param rnti
     * @param cellId
     * @return amount of TXed bytes
     */
    uint32_t GetMacVolumeUeSpecific(uint16_t rnti, uint16_t cellId);

    /**
     * Gets the number of MAC PDUs with QPSK, UE specific
     * @param rnti
     * @param cellId
     * @return number of MAC PDUs with QPSK
     */
    uint32_t GetMacPduQpskUeSpecific(uint16_t rnti, uint16_t cellId);

    /**
     * Gets the number of MAC PDUs with 16QAM, UE specific
     * @param rnti
     * @param cellId
     * @return number of MAC PDUs with 16QAM
     */
    uint32_t GetMacPdu16QamUeSpecific(uint16_t rnti, uint16_t cellId);

    /**
     * Gets the number of MAC PDUs with 64QAM, UE specific
     * @param rnti
     * @param cellId
     * @return number of MAC PDUs with 64QAM
     */
    uint32_t GetMacPdu64QamUeSpecific(uint16_t rnti, uint16_t cellId);

    /**
     * Gets the number of symbols, UE specific
     * @param rnti
     * @param cellId
     * @return number of symbols
     */
    uint32_t GetMacNumberOfSymbolsUeSpecific(uint16_t rnti, uint16_t cellId);

    /**
     * Gets the number of TX with MCS 0-4
     * @param rnti
     * @param cellId
     * @return number of TX with MCS 0-4
     */
    uint32_t GetMacMcs04UeSpecific(uint16_t rnti, uint16_t cellId);

    /**
     * Gets the number of TX with MCS 5-9
     * @param rnti
     * @param cellId
     * @return number of TX with MCS 5-9
     */
    uint32_t GetMacMcs59UeSpecific(uint16_t rnti, uint16_t cellId);

    /**
     * Gets the number of TX with MCS 10-14
     * @param rnti
     * @param cellId
     * @return number of TX with MCS 10-14
     */
    uint32_t GetMacMcs1014UeSpecific(uint16_t rnti, uint16_t cellId);

    /**
     * Gets the number of TX with MCS 15-19
     * @param rnti
     * @param cellId
     * @return number of TX with MCS 15-19
     */
    uint32_t GetMacMcs1519UeSpecific(uint16_t rnti, uint16_t cellId);

    /**
     * Gets the number of TX with MCS 20-24
     * @param rnti
     * @param cellId
     * @return number of TX with MCS 20-24
     */
    uint32_t GetMacMcs2024UeSpecific(uint16_t rnti, uint16_t cellId);

    /**
     * Gets the number of TX with MCS 25-29
     * @param rnti
     * @param cellId
     * @return number of TX with MCS 25-29
     */
    uint32_t GetMacMcs2529UeSpecific(uint16_t rnti, uint16_t cellId);

    /**
     * Gets the number of TX with SINR < 6 dB
     * @param rnti
     * @param cellId
     * @return number of TX with SINR < 6 dB
     */
    uint32_t GetMacSinrBin1UeSpecific(uint16_t rnti, uint16_t cellId);

    /**
     * Gets the number of TX with SINR 0-6 dB
     * @param rnti
     * @param cellId
     * @return number of TX with SINR 0-6 dB
     */
    uint32_t GetMacSinrBin2UeSpecific(uint16_t rnti, uint16_t cellId);

    /**
     * Gets the number of TX with SINR 6-12 dB
     * @param rnti
     * @param cellId
     * @return number of TX with SINR 6-12 dB
     */
    uint32_t GetMacSinrBin3UeSpecific(uint16_t rnti, uint16_t cellId);

    /**
     * Gets the number of TX with SINR 12-18 dB
     * @param rnti
     * @param cellId
     * @return number of TX with SINR 12-18 dB
     */
    uint32_t GetMacSinrBin4UeSpecific(uint16_t rnti, uint16_t cellId);

    /**
     * Gets the number of TX with SINR 12-18 dB
     * @param rnti
     * @param cellId
     * @return number of TX with SINR 12-18 dB
     */
    uint32_t GetMacSinrBin5UeSpecific(uint16_t rnti, uint16_t cellId);

    /**
     * Gets the number of TX with SINR 18-24 dB
     * @param rnti
     * @param cellId
     * @return number of TX with SINR 18-24 dB
     */
    uint32_t GetMacSinrBin6UeSpecific(uint16_t rnti, uint16_t cellId);

    /**
     * Gets the number of TX with SINR > 24 dB
     * @param rnti
     * @param cellId
     * @return number of TX with SINR > 24 dB
     */
    uint32_t GetMacSinrBin7UeSpecific(uint16_t rnti, uint16_t cellId);

    /**
     * Reset the counters for a specific UE
     * @param rnti
     * @param cellId
     */
    void ResetPhyTracesForRntiCellId(uint16_t rnti, uint16_t cellId);

    /**
     * Get last reset time
     * @param rnti
     * @param cellId
     */
    Time GetLastResetTime(uint16_t rnti, uint16_t cellId);

    void UpdateTraces(RxPacketTraceParams params);

  private:
    std::map<RntiCellIdPair_t, uint32_t> m_macPduUeSpecific;
    std::map<RntiCellIdPair_t, uint32_t> m_macPduInitialTransmissionUeSpecific;
    std::map<RntiCellIdPair_t, uint32_t> m_macPduRetransmissionUeSpecific;
    std::map<RntiCellIdPair_t, uint32_t> m_macVolumeUeSpecific;   //!< UE specific MAC volume
    std::map<RntiCellIdPair_t, uint32_t> m_macPduQpskUeSpecific;  //!< UE specific MAC PDUs QPSK
    std::map<RntiCellIdPair_t, uint32_t> m_macPdu16QamUeSpecific; //!< UE specific MAC PDUs 16QAM
    std::map<RntiCellIdPair_t, uint32_t> m_macPdu64QamUeSpecific; //!< UE specific MAC PDUs 64QAM
    std::map<RntiCellIdPair_t, uint32_t> m_macMcs04UeSpecific;    //!< UE specific TX with MCS 0-4
    std::map<RntiCellIdPair_t, uint32_t> m_macMcs59UeSpecific;    //!< UE specific TX with MCS 5-9
    std::map<RntiCellIdPair_t, uint32_t> m_macMcs1014UeSpecific;  //!< UE specific TX with MCS 10-14
    std::map<RntiCellIdPair_t, uint32_t> m_macMcs1519UeSpecific;  //!< UE specific TX with MCS 15-19
    std::map<RntiCellIdPair_t, uint32_t> m_macMcs2024UeSpecific;  //!< UE specific TX with MCS 20-24
    std::map<RntiCellIdPair_t, uint32_t> m_macMcs2529UeSpecific;  //!< UE specific TX with MCS 25-29

    std::map<RntiCellIdPair_t, uint32_t>
        m_macSinrBin1UeSpecific; //!< UE specific TX with SINR < -6dB
    std::map<RntiCellIdPair_t, uint32_t>
        m_macSinrBin2UeSpecific; //!< UE specific TX with SINR -6dB to 0 dB
    std::map<RntiCellIdPair_t, uint32_t>
        m_macSinrBin3UeSpecific; //!< UE specific TX with SINR 0 dB to 6 dB
    std::map<RntiCellIdPair_t, uint32_t>
        m_macSinrBin4UeSpecific; //!< UE specific TX with SINR 6 dB to 12 dB
    std::map<RntiCellIdPair_t, uint32_t>
        m_macSinrBin5UeSpecific; //!< UE specific TX with SINR 12 dB to 18 dB
    std::map<RntiCellIdPair_t, uint32_t>
        m_macSinrBin6UeSpecific; //!< UE specific TX with SINR 18 dB to 24 dB
    std::map<RntiCellIdPair_t, uint32_t>
        m_macSinrBin7UeSpecific; //!< UE specific TX with SINR > 24 dB

    std::map<RntiCellIdPair_t, uint32_t> m_macNumberOfSymbols; //!< UE specific number of symbols

    std::map<RntiCellIdPair_t, Time> m_lastReset; //! last time UE was reset

    /**
     * Update the value of an entry in a map
     * @param map
     * @param key
     * @param newValue
     */
    std::map<RntiCellIdPair_t, uint32_t> UpdateMapValue(std::map<RntiCellIdPair_t, uint32_t> map,
                                                        RntiCellIdPair_t key,
                                                        uint32_t newValue);
    /**
     * Increase the value of an entry in a map by 1
     * @param map
     * @param key
     * @param newValue
     */
    std::map<RntiCellIdPair_t, uint32_t> IncreaseMapValue(std::map<RntiCellIdPair_t, uint32_t> map,
                                                          RntiCellIdPair_t key,
                                                          uint32_t value);

    /**
     * Get an entry in a map
     * @param map
     * @param key
     */
    uint32_t GetMapValue(std::map<RntiCellIdPair_t, uint32_t> map, RntiCellIdPair_t key);

    /**
     * Erase an entry in a map
     * @param map
     * @param key
     */
    std::map<RntiCellIdPair_t, uint32_t> ResetMapValue(std::map<RntiCellIdPair_t, uint32_t> map,
                                                       RntiCellIdPair_t key);
};

} // namespace ns3
#endif /* NORI_E2_REPORT_H_ */
