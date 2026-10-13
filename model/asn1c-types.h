/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2022 Northeastern University
 * Copyright (c) 2022 Sapienza, University of Rome
 * Copyright (c) 2022 University of Padova
 * Copyright (c) 2026 LASSE/UFPA - Universidade Federal do Pará
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 *
 *
 * Author: Andrea Lacava <thecave003@gmail.com>
 *         Tommaso Zugno <tommasozugno@gmail.com>
 *         Michele Polese <michele.polese@gmail.com>
 *         Andrey Adailso <andreyadailsom@gmail.com>
 */

#pragma once

#include "ns3/math.h"
#include "ns3/object.h"

#include <string>
#include <vector>

extern "C"
{
#include "BIT_STRING.h"
#include "BOOLEAN.h"
#include "NativeInteger.h"
#include "NativeReal.h"
#include "OCTET_STRING.h"
#include "PrintableString.h"
#include "RANParameter-ID.h"
#include "RANParameter-LIST.h"
#include "RANParameter-STRUCTURE-Item.h"
#include "RANParameter-STRUCTURE.h"
#include "RANParameter-Value.h"
#include "RANParameter-ValueType-Choice-ElementFalse.h"
#include "RANParameter-ValueType-Choice-ElementTrue.h"
#include "RANParameter-ValueType-Choice-List.h"
#include "RANParameter-ValueType-Choice-Structure.h"
#include "RANParameter-ValueType.h"
#include "S-NSSAI.h"
}

namespace ns3
{

/**
 * Wrapper for class for OCTET STRING
 */
class OctetString : public SimpleRefCount<OctetString>
{
  public:
    OctetString(std::string value, size_t size);
    OctetString(void* value, size_t size);
    ~OctetString();
    OCTET_STRING_t* GetPointer();
    OCTET_STRING_t GetValue();
    std::string DecodeContent();

  private:
    void CreateBaseOctetString(size_t size);
    OCTET_STRING_t* m_octetString;
};

/**
 * Wrapper for class for BIT STRING
 */
class BitString : public SimpleRefCount<BitString>
{
  public:
    BitString(std::string value, size_t size);
    BitString(std::string value, size_t size, size_t bits_unused);
    ~BitString();
    BIT_STRING_t* GetPointer();
    BIT_STRING_t GetValue();
    // TODO maybe a to string or a decode method should be created

  private:
    BIT_STRING_t* m_bitString;
};

class NrCellId : public SimpleRefCount<NrCellId>
{
  public:
    NrCellId(uint16_t value);
    virtual ~NrCellId();
    BIT_STRING_t* GetPointer();
    BIT_STRING_t GetValue();

  private:
    Ptr<BitString> m_bitString;
};

/**
 * Wrapper for class for S-NSSAI
 */
class Snssai : public SimpleRefCount<Snssai>
{
  public:
    Snssai(std::string sst);
    Snssai(std::string sst, std::string sd);
    ~Snssai();
    S_NSSAI_t* GetPointer();
    S_NSSAI_t GetValue();

  private:
    OCTET_STRING_t* m_sst;
    OCTET_STRING_t* m_sd;
    S_NSSAI_t* m_sNssai;
};

// /**
//  * Wrapper for class for MeasQuantityResults_t
//  */
// class MeasQuantityResultsWrap : public SimpleRefCount<MeasQuantityResultsWrap>
// {
//   public:
//     MeasQuantityResultsWrap();
//     ~MeasQuantityResultsWrap();
//     MeasQuantityResults_t* GetPointer();
//     MeasQuantityResults_t GetValue();
//     void AddRsrp(long rsrp);
//     void AddRsrq(long rsrq);
//     void AddSinr(long sinr);
// 
//   private:
//     MeasQuantityResults_t* m_measQuantityResults;
// };
// 
// /**
//  * Wrapper for class for ResultsPerCSI_RS_Index_t
//  */
// class ResultsPerCsiRsIndex : public SimpleRefCount<ResultsPerCsiRsIndex>
// {
//   public:
//     ResultsPerCsiRsIndex(long csiRsIndex, MeasQuantityResults_t* csiRsResults);
//     ResultsPerCsiRsIndex(long csiRsIndex);
//     ResultsPerCSI_RS_Index_t* GetPointer();
//     ResultsPerCSI_RS_Index_t GetValue();
// 
//   private:
//     ResultsPerCSI_RS_Index_t* m_resultsPerCsiRsIndex;
// };
// 
// /**
//  * Wrapper for class for ResultsPerSSB_Index_t
//  */
// class ResultsPerSSBIndex : public SimpleRefCount<ResultsPerSSBIndex>
// {
//   public:
//     ResultsPerSSBIndex(long ssbIndex, MeasQuantityResults_t* ssbResults);
//     ResultsPerSSBIndex(long ssbIndex);
//     ResultsPerSSB_Index_t* GetPointer();
//     ResultsPerSSB_Index_t GetValue();
// 
//   private:
//     ResultsPerSSB_Index_t* m_resultsPerSSBIndex;
// };
// 
// /**
//  * Wrapper for class for MeasResultNR_t
//  */
// class MeasResultNr : public SimpleRefCount<MeasResultNr>
// {
//   public:
//     enum ResultCell
//     {
//         SSB = 0,
//         CSI_RS = 1
//     };
// 
//     MeasResultNr(long physCellId);
//     MeasResultNr();
//     ~MeasResultNr();
//     MeasResultNR_t* GetPointer();
//     MeasResultNR_t GetValue();
//     void AddCellResults(ResultCell cell, MeasQuantityResults_t* results);
//     void AddPerSsbIndexResults(ResultsPerSSB_Index_t* resultsSsbIndex);
//     void AddPerCsiRsIndexResults(ResultsPerCSI_RS_Index_t* resultsCsiRsIndex);
//     void AddPhyCellId(long physCellId);
// 
//   private:
//     MeasResultNR_t* m_measResultNr;
//     bool m_shouldFree;
// };
// 
// /**
//  * Wrapper for class for MeasResultEUTRA_t
//  */
// class MeasResultEutra : public SimpleRefCount<MeasResultEutra>
// {
//   public:
//     MeasResultEutra(long eutraPhysCellId, long rsrp, long rsrq, long sinr);
//     MeasResultEutra(long eutraPhysCellId);
//     MeasResultEUTRA_t* GetPointer();
//     MeasResultEUTRA_t GetValue();
//     void AddRsrp(long rsrp);
//     void AddRsrq(long rsrq);
//     void AddSinr(long sinr);
// 
//   private:
//     MeasResultEUTRA_t* m_measResultEutra;
// };
// 
// /**
//  * Wrapper for class for MeasResultPCell_t
//  */
// class MeasResultPCellWrap : public SimpleRefCount<MeasResultPCellWrap>
// {
//   public:
//     MeasResultPCellWrap(long eutraPhysCellId, long rsrpResult, long rsrqResult);
//     MeasResultPCellWrap(long eutraPhysCellId);
//     MeasResultPCell_t* GetPointer();
//     MeasResultPCell_t GetValue();
//     void AddRsrpResult(long rsrpResult);
//     void AddRsrqResult(long rsrqResult);
// 
//   private:
//     MeasResultPCell_t* m_measResultPCell;
// };
// 
// /**
//  * Wrapper for class for MeasResultServMO_t
//  */
// class MeasResultServMo : public SimpleRefCount<MeasResultServMo>
// {
//   public:
//     MeasResultServMo(long servCellId,
//                      MeasResultNR_t measResultServingCell,
//                      MeasResultNR_t* measResultBestNeighCell);
//     MeasResultServMo(long servCellId, MeasResultNR_t measResultServingCell);
//     MeasResultServMO_t* GetPointer();
//     MeasResultServMO_t GetValue();
// 
//   private:
//     MeasResultServMO_t* m_measResultServMo;
// };
// 
// /**
//  * Wrapper for class for ServingCellMeasurements_t
//  */
// class ServingCellMeasurementsWrap : public SimpleRefCount<ServingCellMeasurementsWrap>
// {
//   public:
//     ServingCellMeasurementsWrap(ServingCellMeasurements_PR present);
//     ServingCellMeasurements_t* GetPointer();
//     ServingCellMeasurements_t GetValue();
//     void AddMeasResultPCell(MeasResultPCell_t* measResultPCell);
//     void AddMeasResultServMo(MeasResultServMO_t* measResultServMO);
// 
//   private:
//     ServingCellMeasurements_t* m_servingCellMeasurements;
//     MeasResultServMOList_t* m_nr_measResultServingMOList;
// };

/**
 * Wrapper for class for L3 RRC Measurements
 */
class L3RrcMeasurements : public SimpleRefCount<L3RrcMeasurements>
{
  public:
    int MAX_MEAS_RESULTS_ITEMS = 8; // Maximum 8 per UE (standard)
    // L3RrcMeasurements(RRCEvent_t rrcEvent);
    // L3RrcMeasurements(L3_RRC_Measurements_t* l3RrcMeasurements);
    // ~L3RrcMeasurements();
    // L3_RRC_Measurements_t* GetPointer();
    // L3_RRC_Measurements_t GetValue();

    // void AddMeasResultEUTRANeighCells(MeasResultEUTRA_t* measResultItemEUTRA);
    // void AddMeasResultNRNeighCells(MeasResultNR_t* measResultItemNR);
    // void AddServingCellMeasurement(ServingCellMeasurements_t* servingCellMeasurements);
    // void AddNeighbourCellMeasurement(long neighCellId, long sinr);

    // static Ptr<L3RrcMeasurements> CreateL3RrcUeSpecificSinrServing(long servingCellId,
    //                                                                long physCellId,
    //                                                                long sinr);

    // static Ptr<L3RrcMeasurements> CreateL3RrcUeSpecificSinrNeigh();

    // static void ExtractMeasurementsFromL3RrcMeas(L3_RRC_Measurements_t* l3RrcMeasurements);

    /**
     * Returns the input SINR on a 0-127 scale
     *
     * Refer to 3GPP TS 38.133 V17.2.0(2021-06), Table 10.1.16.1-1: SS-SINR and CSI-SINR measurement
     * report mapping
     *
     * @param sinr
     * @return double
     */
    static double ThreeGppMapSinr(double sinr);

  private:
    // void addMeasResultNeighCells(MeasResultNeighCells_PR present);
    // L3_RRC_Measurements_t* m_l3RrcMeasurements;
    // MeasResultListEUTRA_t* m_measResultListEUTRA;
    // MeasResultListNR_t* m_measResultListNR;
    int m_measItemsCounter{0};
};



/**
 * Wrapper class for RANParameterItem (E2SM-RC v3)
 */
class RANParameterItem : public SimpleRefCount<RANParameterItem>
{
  public:
    enum class ValueType
    {
        Nothing = 0,
        Boolean = 1,
        Int = 2,
        Real = 3,
        BitString = 4,
        OctetString = 5,
        PrintableString = 6
    };

    RANParameterItem();
    RANParameterItem(long paramId, const RANParameter_Value_t& val);
    ~RANParameterItem();

    long m_paramId{0};
    ValueType m_valueType{ValueType::Nothing};
    bool m_valueBool{false};
    long m_valueInt{0};
    double m_valueReal{0.0};
    Ptr<BitString> m_valueBitStr{nullptr};
    Ptr<OctetString> m_valueOctStr{nullptr};
    std::string m_valuePrtStr{""};

    static std::vector<RANParameterItem> ExtractRANParametersFromValueType(
        long paramId,
        const RANParameter_ValueType_t* valueType);

    static std::vector<RANParameterItem> ExtractRANParametersFromStructure(
        const RANParameter_STRUCTURE_t* structure);

    static std::vector<RANParameterItem> ExtractRANParametersFromList(
        const RANParameter_LIST_t* list);
};

} // namespace ns3
