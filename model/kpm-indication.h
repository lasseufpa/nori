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
#include "ESM-KPM-IndicationHeader-Format1.h"
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

    struct KpmRicIndicationHearderValues
    {
      uint64_t m_timestamp;
      std::string m_fileFormatVersion;
      std::string m_senderName;
      std::string m_senderType;
      std::string m_vendorName;
    };

    KpmIndicationHeader(KpmRicIndicationHearderValues values);
    ~KpmIndicationHeader();

    void* m_buffer;
    size_t m_size;

  private:
    void FillAndEncodeIndicationHearder(E2SM_KPM_IndicationHeader_t* descriptor, KpmRicIndicationHearderValues values);
    void Encode(E2SM_KPM_IndicationHeader_t* descriptor);

};

class MeasurementItemList : public SimpleRefCount<MeasurementItemList>
{
  private:
    Ptr<OctetString>
        m_id; // ID, contains the UE IMSI if used to carry UE-specific measurement items
    std::vector<Ptr<MeasurementItem>> m_items; //!< list of Measurement Information Items
  public:
    MeasurementItemList();
    MeasurementItemList(std::string ueId);
    ~MeasurementItemList();

    // NOTE defined here to avoid undefined references
    template <class T>
    void AddItem(std::string name, T value)
    {
        Ptr<MeasurementItem> item = Create<MeasurementItem>(name, value);
        m_items.push_back(item);
    }

    std::vector<Ptr<MeasurementItem>> GetItems();
    OCTET_STRING_t GetId();
};

/**
 * Base class to carry PM Container values
 */
class PmContainerValues : public SimpleRefCount<PmContainerValues>
{
  public:
    virtual ~PmContainerValues() = default;
};

/**
 * Contains the values to be inserted in the O-CU-CP Measurement Container
 */
class OCuCpContainerValues : public PmContainerValues
{
  public:
    uint16_t m_numActiveUes; //!< mean number of RRC connections
};

/**
 * Contains the values to be inserted in the O-CU-UP Measurement Container
 */
class OCuUpContainerValues : public PmContainerValues
{
  public:
    std::string m_plmId; //!< PLMN identity, octet string, 3 bytes
    long m_pDCPBytesUL;  //!< total PDCP bytes transmitted UL
    long m_pDCPBytesDL;  //!< total PDCP bytes transmitted DL
};

/**
 * Contains the values to be inserted in the O-DU EPC Measurement Container
 */
class EpcDuPmContainer : public SimpleRefCount<EpcDuPmContainer>
{
  public:
    long m_qci;        //!< QCI value
    long m_dlPrbUsage; //!< Used number of PRBs in an average of DL for the monitored slice during
                       //!< E2 reporting period
    long m_ulPrbUsage; //!< Used number of PRBs in an average of UL for the monitored slice during
                       //!< E2 reporting period
    virtual ~EpcDuPmContainer() = default;
};

/**
 * Contains the values to be inserted in the O-DU 5GC Measurement Container
 */
class FiveGcDuPmContainer : public SimpleRefCount<FiveGcDuPmContainer>
{
  public:
    // Snssai m_sliceId; //!< S-NSSAI
    long m_fiveQi;     //!< 5QI value
    long m_dlPrbUsage; //!< Used number of PRBs in an average of DL for the monitored slice during
                       //!< E2 reporting period
    long m_ulPrbUsage; //!< Used number of PRBs in an average of UL for the monitored slice during
                       //!< E2 reporting period
    virtual ~FiveGcDuPmContainer() = default;
};

class ServedPlmnPerCell : public SimpleRefCount<ServedPlmnPerCell>
{
  public:
    std::string m_plmId; //!< PLMN identity, octet string, 3 bytes
    uint16_t m_nrCellId;
    std::set<Ptr<EpcDuPmContainer>> m_perQciReportItems;
};

class CellResourceReport : public SimpleRefCount<CellResourceReport>
{
  public:
    std::string m_plmId; //!< PLMN identity, octet string, 3 bytes
    uint16_t m_nrCellId;
    long dlAvailablePrbs;
    long ulAvailablePrbs;
    std::set<Ptr<ServedPlmnPerCell>> m_servedPlmnPerCellItems;
};

/**
 * Contains the values to be inserted in the O-DU Measurement Container
 */
class ODuContainerValues : public PmContainerValues
{
  public:
    std::set<Ptr<CellResourceReport>> m_cellResourceReportItems;
};

class KpmIndicationMessage : public SimpleRefCount<KpmIndicationMessage>
{
  public:
    /**
     * Holds the values to be used to fill the RIC Indication Message
     */
    struct KpmIndicationMessageValues
    {
        std::string m_cellObjectId; //!< Cell Object ID
        Ptr<PmContainerValues>
            m_pmContainerValues; //!< struct containing values to be inserted in the PM Container
        Ptr<MeasurementItemList>
            m_cellMeasurementItems; //!< list of cell-specific Measurement Information Items
        std::set<Ptr<MeasurementItemList>>
            m_ueIndications; //!< list of Measurement Information Items
    };

    KpmIndicationMessage(KpmIndicationMessageValues values);
    ~KpmIndicationMessage();

    void* m_buffer;
    size_t m_size;

  private:
    static void CheckConstraints(KpmIndicationMessageValues values);
    void FillPmContainer(PF_Container_t* ranContainer, Ptr<PmContainerValues> values);
    void FillOCuUpContainer(PF_Container_t* ranContainer, Ptr<OCuUpContainerValues> values);
    void FillOCuCpContainer(PF_Container_t* ranContainer, Ptr<OCuCpContainerValues> values);
    void FillODuContainer(PF_Container_t* ranContainer, Ptr<ODuContainerValues> values);
    void FillAndEncodeKpmIndicationMessage(E2SM_KPM_IndicationMessage_t* descriptor,
                                           KpmIndicationMessageValues values);
    void Encode(E2SM_KPM_IndicationMessage_t* descriptor);
};
} // namespace ns3
