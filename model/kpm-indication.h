/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2022 Northeastern University
 * Copyright (c) 2022 Sapienza, University of Rome
 * Copyright (c) 2022 University of Padova
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 *
 *
 * Author: Andrea Lacava <thecave003@gmail.com>
 *         Tommaso Zugno <tommasozugno@gmail.com>
 *         Michele Polese <michele.polese@gmail.com>
 */

#pragma once

#include "ns3/object.h"

#include <set>

extern "C"
{
#include "E2SM-KPM-IndicationHeader.h"
#include "E2SM-KPM-IndicationMessage.h"
#include "E2SM-KPM-RANfunction-Description.h"
#include "asn1c-types.h"
}

namespace ns3
{

class KpmIndicationHeader : public SimpleRefCount<KpmIndicationHeader>
{
  public:
    const int TIMESTAMP_LIMIT_SIZE = 8;

    /**
     * Holds the values to be used to fill the RIC Indication header
     */
    struct KpmRicIndicationHeaderValues
    {
        // E2SM-KPM Indication Header Format 1
        // KPM Node ID IE
        std::string m_fileFormatVersion; //Optional, PrintableString

        std::string m_senderName; //Optional, PrintableString

        std::string m_senderType; //Optional, PrintableString

        std::string m_vendorName; //Optional, PrintableString

        // CollectionTimeStamp
        uint64_t m_timestamp;
    };

    KpmIndicationHeader(KpmRicIndicationHeaderValues values);
    ~KpmIndicationHeader();
    void* m_buffer;
    size_t m_size;

  private:
    /**
     * Fills the KPM INDICATION Header descriptor
     * This function fills the RIC Indication Header with the provided
     * values
     *
     * @param descriptor object representing the KPM INDICATION Header
     * @param values struct holding the values to be used to fill the header
     */
    void FillAndEncodeKpmRicIndicationHeader(E2SM_KPM_IndicationHeader_t* descriptor,
                                             KpmRicIndicationHeaderValues values);

    void Encode(E2SM_KPM_IndicationHeader_t* descriptor);

};

struct KpmMeasurementLabelValues
{
    // Opcionals
    std::string m_plmId;      //ServedPlmnPerCell on old version
    uint64_t m_nrCellId;      //CellResourceReport on old version
    uint8_t m_fiveQi;         //FiveGcDuPmContainer on old version
    uint8_t m_qci;            // EpcDuPmContainer on old version
    uint8_t m_qfi;            
    uint32_t m_sNssai;       // Slice ID
    uint32_t m_qciMax;
    uint32_t m_qciMin;
    uint32_t m_arpMax;

};

struct KpmMeasurementInfoItemValues
{
    std::string m_measName;                // Ex: "RRU.PrbUsedDl"
    uint16_t m_measID;
    KpmMeasurementLabelValues m_labels;    //5QI, CellID, etc
};

struct KpmMeasurementRecordValues
{
    //the order is the same of m_measInfoList
    std::vector<double> m_values; 
};


class KpmIndicationMessage : public SimpleRefCount<KpmIndicationMessage>
{
  public:
    /**
     * Holds the values to be used to fill the RIC Indication Message
     */
    struct KpmIndicationMessageValues
    {
      //Structure: measData > measInfoList > GranulPeriod


      std::vector<KpmMeasurementRecordValues> m_measData; //Measurements Record. Mandatory
      std::vector<KpmMeasurementInfoItemValues> m_measInfoList; //Measurement Information List. Mandatory
      uint32_t m_granularityPeriod; //Granularity Period. Optional 

    };

    KpmIndicationMessage(KpmIndicationMessageValues values);
    ~KpmIndicationMessage();

    void* m_buffer;
    size_t m_size;

  private:
    static void CheckConstraints(KpmIndicationMessageValues values);
    void FillAndEncodeIndicationMessageFormat1(E2SM_KPM_IndicationMessage_Format1_t* format1, 
                     KpmIndicationMessageValues values);
    void FillAndEncodeKpmIndicationMessage(E2SM_KPM_IndicationMessage_t* descriptor,
                                           KpmIndicationMessageValues values);
    void Encode(E2SM_KPM_IndicationMessage_t* descriptor);
};
} // namespace ns3
