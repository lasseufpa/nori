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

#include "asn1c-types.h"

#include "ns3/log.h"

NS_LOG_COMPONENT_DEFINE("Asn1Types");

namespace ns3
{

OctetString::OctetString(std::string value, size_t size)
{
    NS_LOG_FUNCTION(this);
    CreateBaseOctetString(size);
    memcpy(m_octetString->buf, value.c_str(), size);
}

void
OctetString::CreateBaseOctetString(size_t size)
{
    NS_LOG_FUNCTION(this);
    m_octetString = (OCTET_STRING_t*)calloc(1, sizeof(OCTET_STRING_t));
    m_octetString->buf = (uint8_t*)calloc(1, size);
    m_octetString->size = size;
}

OctetString::OctetString(void* value, size_t size)
{
    NS_LOG_FUNCTION(this);
    CreateBaseOctetString(size);
    memcpy(m_octetString->buf, value, size);
}

OctetString::~OctetString()
{
    NS_LOG_FUNCTION(this);
    // if (m_octetString->buf != NULL)
    // free (m_octetString->buf);
    free(m_octetString);
}

OCTET_STRING_t*
OctetString::GetPointer()
{
    return m_octetString;
}

OCTET_STRING_t
OctetString::GetValue()
{
    return *m_octetString;
}

std::string
OctetString::DecodeContent()
{
    // int size = this->GetValue().size;
    // char out[size + 1];
    // std::memcpy(out, this->GetValue().buf, size);
    // out[size] = '\0';
    std::string out(reinterpret_cast<char*>(this->GetValue().buf), this->GetValue().size);
    return out;
}

BitString::BitString(std::string value, size_t size)

{
    NS_LOG_FUNCTION(this);
    m_bitString = (BIT_STRING_t*)calloc(1, sizeof(BIT_STRING_t));
    m_bitString->buf = (uint8_t*)calloc(1, size);
    m_bitString->size = size;
    memcpy(m_bitString->buf, value.c_str(), size);
}

BitString::BitString(std::string value, size_t size, size_t bits_unused)
    : BitString::BitString(value, size)
{
    NS_LOG_FUNCTION(this);
    m_bitString->bits_unused = bits_unused;
}

BitString::~BitString()
{
    NS_LOG_FUNCTION(this);
    free(m_bitString);
}

BIT_STRING_t*
BitString::GetPointer()
{
    return m_bitString;
}

BIT_STRING_t
BitString::GetValue()
{
    return *m_bitString;
}

NrCellId::NrCellId(uint16_t value)
{
    NS_LOG_FUNCTION(this);

    // TODO check why with more than 15 cells is not working
    // if (value > 15)
    // {
    //   NS_FATAL_ERROR ("TODO: update the encoding to support more than 15 cells");
    // }

    // convert value to a char array
    // char ar [5] {};
    // ar [4] = value * 16; // multiply by 16 to obtain a left shift of 4 bits
    uint16_t shifted = value * 16;
    std::string str_shift = std::to_string(shifted);
    m_bitString = Create<BitString>(str_shift, 5, 4);
}

NrCellId::~NrCellId()
{
}

BIT_STRING_t
NrCellId::GetValue()
{
    return m_bitString->GetValue();
}

BIT_STRING_t*
NrCellId::GetPointer()
{
    return m_bitString->GetPointer();
}

Snssai::Snssai(std::string sst)
{
    m_sNssai = (S_NSSAI_t*)calloc(1, sizeof(S_NSSAI_t));
    m_sst = (OCTET_STRING_t*)calloc(1, sizeof(OCTET_STRING_t));
    m_sst->buf = (uint8_t*)calloc(1, sst.size());
    m_sst->size = sst.size();
    memcpy(m_sst->buf, sst.c_str(), sst.size());
    m_sNssai->sST = *m_sst;
}

Snssai::Snssai(std::string sst, std::string sd)
    : Snssai(sst)
{
    m_sd = (OCTET_STRING_t*)calloc(1, sizeof(OCTET_STRING_t));
    m_sd->buf = (uint8_t*)calloc(1, sd.size());
    m_sd->size = sd.size();
    memcpy(m_sd->buf, sd.c_str(), sd.size());
    m_sNssai->sD = m_sd;
}

Snssai::~Snssai()
{
    if (m_sNssai != nullptr)
        ASN_STRUCT_FREE(asn_DEF_S_NSSAI, m_sNssai);
}

S_NSSAI_t*
Snssai::GetPointer()
{
    return m_sNssai;
}

S_NSSAI_t
Snssai::GetValue()
{
    return *m_sNssai;
}

// void
// MeasQuantityResultsWrap::AddRsrp(long rsrp)
// {
//     m_measQuantityResults->rsrp = (RSRP_Range_t*)calloc(1, sizeof(RSRP_Range_t));
//     *m_measQuantityResults->rsrp = rsrp;
// }
// 
// void
// MeasQuantityResultsWrap::AddRsrq(long rsrq)
// {
//     m_measQuantityResults->rsrq = (RSRQ_Range_t*)calloc(1, sizeof(RSRQ_Range_t));
//     *m_measQuantityResults->rsrq = rsrq;
// }
// 
// void
// MeasQuantityResultsWrap::AddSinr(long sinr)
// {
//     m_measQuantityResults->sinr = (SINR_Range_t*)calloc(1, sizeof(SINR_Range_t));
//     *m_measQuantityResults->sinr = sinr;
// }
// 
// MeasQuantityResultsWrap::MeasQuantityResultsWrap()
// {
//     m_measQuantityResults = (MeasQuantityResults_t*)calloc(1, sizeof(MeasQuantityResults_t));
// }
// 
// MeasQuantityResultsWrap::~MeasQuantityResultsWrap()
// {
// }
// 
// MeasQuantityResults_t*
// MeasQuantityResultsWrap::GetPointer()
// {
//     return m_measQuantityResults;
// }
// 
// MeasQuantityResults_t
// MeasQuantityResultsWrap::GetValue()
// {
//     return *m_measQuantityResults;
// }
// 
// ResultsPerCsiRsIndex::ResultsPerCsiRsIndex(long csiRsIndex, MeasQuantityResults_t* csiRsResults)
// {
//     m_resultsPerCsiRsIndex =
//         (ResultsPerCSI_RS_Index_t*)calloc(1, sizeof(ResultsPerCSI_RS_Index_t));
//     m_resultsPerCsiRsIndex->csi_RS_Index = csiRsIndex;
//     m_resultsPerCsiRsIndex->csi_RS_Results = csiRsResults;
// }
// 
// ResultsPerCsiRsIndex::ResultsPerCsiRsIndex(long csiRsIndex)
// {
//     m_resultsPerCsiRsIndex =
//         (ResultsPerCSI_RS_Index_t*)calloc(1, sizeof(ResultsPerCSI_RS_Index_t));
//     m_resultsPerCsiRsIndex->csi_RS_Index = csiRsIndex;
// }
// 
// ResultsPerCSI_RS_Index_t*
// ResultsPerCsiRsIndex::GetPointer()
// {
//     return m_resultsPerCsiRsIndex;
// }
// 
// ResultsPerCSI_RS_Index_t
// ResultsPerCsiRsIndex::GetValue()
// {
//     return *m_resultsPerCsiRsIndex;
// }
// 
// ResultsPerSSBIndex::ResultsPerSSBIndex(long ssbIndex, MeasQuantityResults_t* ssbResults)
// {
//     m_resultsPerSSBIndex = (ResultsPerSSB_Index_t*)calloc(1, sizeof(ResultsPerSSB_Index_t));
//     m_resultsPerSSBIndex->ssb_Index = ssbIndex;
//     m_resultsPerSSBIndex->ssb_Results = ssbResults;
// }
// 
// ResultsPerSSBIndex::ResultsPerSSBIndex(long ssbIndex)
// {
//     m_resultsPerSSBIndex = (ResultsPerSSB_Index_t*)calloc(1, sizeof(ResultsPerSSB_Index_t));
//     m_resultsPerSSBIndex->ssb_Index = ssbIndex;
// }
// 
// ResultsPerSSB_Index_t*
// ResultsPerSSBIndex::GetPointer()
// {
//     return m_resultsPerSSBIndex;
// }
// 
// ResultsPerSSB_Index_t
// ResultsPerSSBIndex::GetValue()
// {
//     return *m_resultsPerSSBIndex;
// }
// 
// void
// MeasResultNr::AddCellResults(MeasResultNr::ResultCell cell, MeasQuantityResults_t* results)
// {
//     if (m_measResultNr->measResult.cellResults == NULL)
//     {
//         m_measResultNr->measResult.cellResults =
//             (MeasResultNR::MeasResultNR__measResult::
//                  MeasResultNR__measResult__cellResults*)calloc(1, sizeof(MeasResultNR::MeasResultNR__measResult::MeasResultNR__measResult__cellResults));
//     }
// 
//     switch (cell)
//     {
//     case MeasResultNr::ResultCell::SSB: {
//         m_measResultNr->measResult.cellResults->resultsSSB_Cell = results;
//         break;
//     }
//     case MeasResultNr::ResultCell::CSI_RS: {
//         m_measResultNr->measResult.cellResults->resultsCSI_RS_Cell = results;
//         break;
//     }
//     }
// }
// 
// void
// MeasResultNr::AddPerSsbIndexResults(ResultsPerSSB_Index_t* resultsSSB_Index)
// {
// }
// 
// void
// MeasResultNr::AddPerCsiRsIndexResults(ResultsPerCSI_RS_Index_t* resultsCSI_RS_Index)
// {
// }
// 
// void
// MeasResultNr::AddPhyCellId(long physCellId)
// {
//     m_measResultNr->physCellId = (PhysCellId_t*)calloc(1, sizeof(PhysCellId_t));
//     *m_measResultNr->physCellId = physCellId;
// }
// 
// MeasResultNr::MeasResultNr(long physCellId)
//     : MeasResultNr()
// {
//     AddPhyCellId(physCellId);
// }
// 
// MeasResultNr::MeasResultNr()
// {
//     m_measResultNr = (MeasResultNR_t*)calloc(1, sizeof(MeasResultNR_t));
//     m_shouldFree = false;
// }
// 
// MeasResultNr::~MeasResultNr()
// {
// }
// 
// MeasResultNR_t*
// MeasResultNr::GetPointer()
// {
//     return m_measResultNr;
// }
// 
// MeasResultNR_t
// MeasResultNr::GetValue()
// {
//     return *m_measResultNr;
// }
// 
// MeasResultEutra::MeasResultEutra(long eutraPhysCellId, long rsrp, long rsrq, long sinr)
// {
//     m_measResultEutra = (MeasResultEUTRA_t*)calloc(1, sizeof(MeasResultEUTRA_t));
//     m_measResultEutra->eutra_PhysCellId = eutraPhysCellId;
//     AddRsrp(rsrp);
//     AddRsrq(rsrq);
//     AddSinr(sinr);
// }
// 
// MeasResultEutra::MeasResultEutra(long eutraPhysCellId)
// {
//     m_measResultEutra = (MeasResultEUTRA_t*)calloc(1, sizeof(MeasResultEUTRA_t));
//     m_measResultEutra->eutra_PhysCellId = eutraPhysCellId;
// }
// 
// MeasResultEUTRA_t*
// MeasResultEutra::GetPointer()
// {
//     return m_measResultEutra;
// }
// 
// MeasResultEUTRA_t
// MeasResultEutra::GetValue()
// {
//     return *m_measResultEutra;
// }
// 
// void
// MeasResultEutra::AddRsrp(long rsrp)
// {
//     m_measResultEutra->measResult.rsrpResult = (RSRP_Range_t*)calloc(1, sizeof(RSRP_Range_t));
//     *m_measResultEutra->measResult.rsrpResult = rsrp;
// }
// 
// void
// MeasResultEutra::AddRsrq(long rsrq)
// {
//     m_measResultEutra->measResult.rsrqResult = (RSRQ_Range_t*)calloc(1, sizeof(RSRQ_Range_t));
//     *m_measResultEutra->measResult.rsrqResult = rsrq;
// }
// 
// void
// MeasResultEutra::AddSinr(long sinr)
// {
//     m_measResultEutra->measResult.sinrResult = (SINR_Range_t*)calloc(1, sizeof(SINR_Range_t));
//     *m_measResultEutra->measResult.sinrResult = sinr;
// }
// 
// MeasResultPCellWrap::MeasResultPCellWrap(long eutraPhysCellId, long rsrpResult, long rsrqResult)
// {
//     m_measResultPCell = (MeasResultPCell_t*)calloc(1, sizeof(MeasResultPCell_t));
//     m_measResultPCell->eutra_PhysCellId = eutraPhysCellId;
//     AddRsrpResult(rsrpResult);
//     AddRsrqResult(rsrqResult);
// }
// 
// MeasResultPCellWrap::MeasResultPCellWrap(long eutraPhysCellId)
// {
//     m_measResultPCell = (MeasResultPCell_t*)calloc(1, sizeof(MeasResultPCell_t));
//     m_measResultPCell->eutra_PhysCellId = eutraPhysCellId;
// }
// 
// MeasResultPCell_t*
// MeasResultPCellWrap::GetPointer()
// {
//     return m_measResultPCell;
// }
// 
// MeasResultPCell_t
// MeasResultPCellWrap::GetValue()
// {
//     return *m_measResultPCell;
// }
// 
// void
// MeasResultPCellWrap::AddRsrpResult(long rsrpResult)
// {
//     m_measResultPCell->rsrpResult = rsrpResult;
// }
// 
// void
// MeasResultPCellWrap::AddRsrqResult(long rsrqResult)
// {
//     m_measResultPCell->rsrqResult = rsrqResult;
// }
// 
// MeasResultServMo::MeasResultServMo(long servCellId,
//                                     MeasResultNR_t measResultServingCell,
//                                     MeasResultNR_t* measResultBestNeighCell)
// {
//     m_measResultServMo = (MeasResultServMO_t*)calloc(1, sizeof(MeasResultServMO_t));
//     m_measResultServMo->servCellId = servCellId;
//     m_measResultServMo->measResultServingCell = measResultServingCell;
//     m_measResultServMo->measResultBestNeighCell = measResultBestNeighCell;
// }
// 
// MeasResultServMo::MeasResultServMo(long servCellId, MeasResultNR_t measResultServingCell)
// {
//     m_measResultServMo = (MeasResultServMO_t*)calloc(1, sizeof(MeasResultServMO_t));
//     m_measResultServMo->servCellId = servCellId;
//     m_measResultServMo->measResultServingCell = measResultServingCell;
// }
// 
// MeasResultServMO_t*
// MeasResultServMo::GetPointer()
// {
//     return m_measResultServMo;
// }
// 
// MeasResultServMO_t
// MeasResultServMo::GetValue()
// {
//     return *m_measResultServMo;
// }
// 
// void
// ServingCellMeasurementsWrap::AddMeasResultPCell(MeasResultPCell_t* measResultPCell)
// {
//     m_servingCellMeasurements->choice.eutra_measResultPCell = measResultPCell;
// }
// 
// void
// ServingCellMeasurementsWrap::AddMeasResultServMo(MeasResultServMO_t* measResultServMO)
// {
//     ASN_SEQUENCE_ADD(&m_nr_measResultServingMOList->list, measResultServMO);
// }
// 
// ServingCellMeasurementsWrap::ServingCellMeasurementsWrap(ServingCellMeasurements_PR present)
// {
//     m_servingCellMeasurements =
//         (ServingCellMeasurements_t*)calloc(1, sizeof(ServingCellMeasurements_t));
//     m_servingCellMeasurements->present = present;
// 
//     if (m_servingCellMeasurements->present == ServingCellMeasurements_PR_nr_measResultServingMOList)
//     {
//         m_nr_measResultServingMOList =
//             (MeasResultServMOList_t*)calloc(1, sizeof(MeasResultServMOList_t));
//         m_servingCellMeasurements->choice.nr_measResultServingMOList = m_nr_measResultServingMOList;
//     }
// }
// 
// ServingCellMeasurements_t*
// ServingCellMeasurementsWrap::GetPointer()
// {
//     return m_servingCellMeasurements;
// }
// 
// ServingCellMeasurements_t
// ServingCellMeasurementsWrap::GetValue()
// {
//     return *m_servingCellMeasurements;
// }

// Ptr<L3RrcMeasurements>
// L3RrcMeasurements::CreateL3RrcUeSpecificSinrServing(long servingCellId, long physCellId, long sinr)
// {
//     Ptr<L3RrcMeasurements> l3RrcMeasurement = Create<L3RrcMeasurements>(RRCEvent_b1);
//     Ptr<ServingCellMeasurementsWrap> servingCellMeasurements =
//         Create<ServingCellMeasurementsWrap>(ServingCellMeasurements_PR_nr_measResultServingMOList);
// 
//     Ptr<MeasResultNr> measResultNr = Create<MeasResultNr>(physCellId);
//     Ptr<MeasQuantityResultsWrap> measQuantityResultWrap = Create<MeasQuantityResultsWrap>();
//     measQuantityResultWrap->AddSinr(sinr);
//     measResultNr->AddCellResults(MeasResultNr::SSB, measQuantityResultWrap->GetPointer());
//     Ptr<MeasResultServMo> measResultServMo =
//         Create<MeasResultServMo>(servingCellId, measResultNr->GetValue());
//     servingCellMeasurements->AddMeasResultServMo(measResultServMo->GetPointer());
//     l3RrcMeasurement->AddServingCellMeasurement(servingCellMeasurements->GetPointer());
//     return l3RrcMeasurement;
// }
// 
// Ptr<L3RrcMeasurements>
// L3RrcMeasurements::CreateL3RrcUeSpecificSinrNeigh()
// {
//     return Create<L3RrcMeasurements>(RRCEvent_b1);
// }
// 
// void
// L3RrcMeasurements::AddNeighbourCellMeasurement(long neighCellId, long sinr)
// {
//     Ptr<MeasResultNr> measResultNr = Create<MeasResultNr>(neighCellId);
//     Ptr<MeasQuantityResultsWrap> measQuantityResultWrap = Create<MeasQuantityResultsWrap>();
//     measQuantityResultWrap->AddSinr(sinr);
//     measResultNr->AddCellResults(MeasResultNr::SSB, measQuantityResultWrap->GetPointer());
// 
//     this->AddMeasResultNRNeighCells(measResultNr->GetPointer()); // MAX 8 UE per message (standard)
// }
// 
// void
// L3RrcMeasurements::AddServingCellMeasurement(ServingCellMeasurements_t* servingCellMeasurements)
// {
//     m_l3RrcMeasurements->servingCellMeasurements = servingCellMeasurements;
// }
// 
// void
// L3RrcMeasurements::AddMeasResultEUTRANeighCells(MeasResultEUTRA_t* measResultItemEUTRA)
// {
//     if (m_measItemsCounter == L3RrcMeasurements::MAX_MEAS_RESULTS_ITEMS)
//     {
//         NS_LOG_ERROR("Maximum number of items ("
//                      << L3RrcMeasurements::MAX_MEAS_RESULTS_ITEMS
//                      << ")for the standard reached. This item will not be "
//                         "inserted in the list");
//         return;
//     }
// 
//     if (m_l3RrcMeasurements->measResultNeighCells == NULL)
//     {
//         addMeasResultNeighCells(MeasResultNeighCells_PR_measResultListEUTRA);
//     }
// 
//     if (m_l3RrcMeasurements->measResultNeighCells->present !=
//         MeasResultNeighCells_PR_measResultListEUTRA)
//     {
//         NS_LOG_ERROR("Wrong measurement item for this list, it will not be added.");
//         return;
//     }
// 
//     m_measItemsCounter++;
//     ASN_SEQUENCE_ADD(&m_measResultListEUTRA->list, measResultItemEUTRA);
// }
// 
// void
// L3RrcMeasurements::AddMeasResultNRNeighCells(MeasResultNR_t* measResultItemNR)
// {
//     if (m_measItemsCounter == L3RrcMeasurements::MAX_MEAS_RESULTS_ITEMS)
//     {
//         NS_LOG_ERROR("Maximum number of items ("
//                      << L3RrcMeasurements::MAX_MEAS_RESULTS_ITEMS
//                      << ")for the standard reached. This item will not be "
//                         "inserted in the list");
//         return;
//     }
// 
//     if (m_l3RrcMeasurements->measResultNeighCells == NULL)
//     {
//         addMeasResultNeighCells(MeasResultNeighCells_PR_measResultListNR);
//     }
// 
//     if (m_l3RrcMeasurements->measResultNeighCells->present !=
//         MeasResultNeighCells_PR_measResultListNR)
//     {
//         NS_LOG_ERROR("Wrong measurement item for this list, it will not be added.");
//         return;
//     }
// 
//     m_measItemsCounter++;
//     ASN_SEQUENCE_ADD(&m_measResultListNR->list, measResultItemNR);
// }
// 
// void
// L3RrcMeasurements::addMeasResultNeighCells(MeasResultNeighCells_PR present)
// {
//     m_l3RrcMeasurements->measResultNeighCells =
//         (MeasResultNeighCells_t*)calloc(1, sizeof(MeasResultNeighCells_t));
//     m_l3RrcMeasurements->measResultNeighCells->present = present;
// 
//     switch (present)
//     {
//     case MeasResultNeighCells_PR_measResultListEUTRA: {
//         m_measResultListEUTRA = (MeasResultListEUTRA_t*)calloc(1, sizeof(MeasResultListEUTRA_t));
//         m_l3RrcMeasurements->measResultNeighCells->choice.measResultListEUTRA =
//             m_measResultListEUTRA;
//         break;
//     }
// 
//     case MeasResultNeighCells_PR_measResultListNR: {
//         m_measResultListNR = (MeasResultListNR_t*)calloc(1, sizeof(MeasResultListNR_t));
//         m_l3RrcMeasurements->measResultNeighCells->choice.measResultListNR = m_measResultListNR;
//         break;
//     }
// 
//     default: {
//         NS_LOG_ERROR("Unrecognized present for Measurment result.");
//         break;
//     }
//     }
// }
// 
// L3RrcMeasurements::L3RrcMeasurements(RRCEvent_t rrcEvent)
// {
//     m_l3RrcMeasurements = (L3_RRC_Measurements_t*)calloc(1, sizeof(L3_RRC_Measurements_t));
//     m_l3RrcMeasurements->rrcEvent = rrcEvent;
//     m_measItemsCounter = 0;
// }
// 
// L3RrcMeasurements::L3RrcMeasurements(L3_RRC_Measurements_t* l3RrcMeasurements)
// {
//     m_l3RrcMeasurements = l3RrcMeasurements;
// }
// 
// L3RrcMeasurements::~L3RrcMeasurements()
// {
// }
// 
// L3_RRC_Measurements*
// L3RrcMeasurements::GetPointer()
// {
//     return m_l3RrcMeasurements;
// }
// 
// L3_RRC_Measurements
// L3RrcMeasurements::GetValue()
// {
//     return *m_l3RrcMeasurements;
// }
// 
// void
// L3RrcMeasurements::ExtractMeasurementsFromL3RrcMeas(L3_RRC_Measurements_t* l3RrcMeasurements)
// {
// }

double
L3RrcMeasurements::ThreeGppMapSinr(double sinr)
{
    double inputEnd = 40;
    double inputStart = -23;
    double outputStart = 0;
    double outputEnd = 127;
    double outputSinr;
    double slope = (outputEnd - outputStart) / (inputEnd - inputStart);

    if (sinr < inputStart)
    {
        outputSinr = outputStart;
    }
    else if (sinr > inputEnd)
    {
        outputSinr = outputEnd;
    }
    else
    {
        outputSinr = outputStart + std::round(slope * (sinr - inputStart));
    }

    NS_LOG_DEBUG("input sinr" << sinr << " output sinr" << outputSinr);

    return outputSinr;
}



RANParameterItem::RANParameterItem()
{
}

RANParameterItem::RANParameterItem(long paramId, const RANParameter_Value_t& val)
    : m_paramId(paramId)
{
    switch (val.present)
    {
    case RANParameter_Value_PR_valueBoolean:
        m_valueType = ValueType::Boolean;
        m_valueBool = (val.choice.valueBoolean != 0);
        break;
    case RANParameter_Value_PR_valueInt:
        m_valueType = ValueType::Int;
        m_valueInt = val.choice.valueInt;
        break;
    case RANParameter_Value_PR_valueReal:
        m_valueType = ValueType::Real;
        m_valueReal = val.choice.valueReal;
        break;
    case RANParameter_Value_PR_valueBitS:
        m_valueType = ValueType::BitString;
        if (val.choice.valueBitS.buf != nullptr)
        {
            m_valueBitStr = Create<BitString>(
                std::string(reinterpret_cast<char*>(val.choice.valueBitS.buf),
                            val.choice.valueBitS.size),
                val.choice.valueBitS.size,
                val.choice.valueBitS.bits_unused);
        }
        break;
    case RANParameter_Value_PR_valueOctS:
        m_valueType = ValueType::OctetString;
        if (val.choice.valueOctS.buf != nullptr)
        {
            m_valueOctStr = Create<OctetString>(val.choice.valueOctS.buf,
                                                val.choice.valueOctS.size);
        }
        break;
    case RANParameter_Value_PR_valuePrintableString:
        m_valueType = ValueType::PrintableString;
        if (val.choice.valuePrintableString.buf != nullptr)
        {
            m_valuePrtStr.assign(
                reinterpret_cast<char*>(val.choice.valuePrintableString.buf),
                val.choice.valuePrintableString.size);
        }
        break;
    case RANParameter_Value_PR_NOTHING:
    default:
        m_valueType = ValueType::Nothing;
        break;
    }
}

RANParameterItem::~RANParameterItem()
{
}

std::vector<RANParameterItem>
RANParameterItem::ExtractRANParametersFromValueType(
    long paramId,
    const RANParameter_ValueType_t* valueType)
{
    std::vector<RANParameterItem> list;
    if (valueType == nullptr)
    {
        return list;
    }

    switch (valueType->present)
    {
    case RANParameter_ValueType_PR_ranP_Choice_ElementTrue:
        if (valueType->choice.ranP_Choice_ElementTrue != nullptr)
        {
            list.emplace_back(paramId,
                              valueType->choice.ranP_Choice_ElementTrue->ranParameter_value);
        }
        break;

    case RANParameter_ValueType_PR_ranP_Choice_ElementFalse:
        if (valueType->choice.ranP_Choice_ElementFalse != nullptr &&
            valueType->choice.ranP_Choice_ElementFalse->ranParameter_value != nullptr)
        {
            list.emplace_back(paramId,
                              *valueType->choice.ranP_Choice_ElementFalse->ranParameter_value);
        }
        break;

    case RANParameter_ValueType_PR_ranP_Choice_Structure:
        if (valueType->choice.ranP_Choice_Structure != nullptr &&
            valueType->choice.ranP_Choice_Structure->ranParameter_Structure != nullptr)
        {
            auto subList = ExtractRANParametersFromStructure(
                valueType->choice.ranP_Choice_Structure->ranParameter_Structure);
            list.insert(list.end(), subList.begin(), subList.end());
        }
        break;

    case RANParameter_ValueType_PR_ranP_Choice_List:
        if (valueType->choice.ranP_Choice_List != nullptr &&
            valueType->choice.ranP_Choice_List->ranParameter_List != nullptr)
        {
            auto subList = ExtractRANParametersFromList(
                valueType->choice.ranP_Choice_List->ranParameter_List);
            list.insert(list.end(), subList.begin(), subList.end());
        }
        break;

    default:
        break;
    }

    return list;
}

std::vector<RANParameterItem>
RANParameterItem::ExtractRANParametersFromStructure(
    const RANParameter_STRUCTURE_t* structure)
{
    std::vector<RANParameterItem> list;
    if (structure == nullptr || structure->sequence_of_ranParameters == nullptr)
    {
        return list;
    }

    int count = structure->sequence_of_ranParameters->list.count;
    for (int i = 0; i < count; i++)
    {
        auto* item = structure->sequence_of_ranParameters->list.array[i];
        if (item != nullptr && item->ranParameter_valueType != nullptr)
        {
            auto subList = ExtractRANParametersFromValueType(
                item->ranParameter_ID,
                item->ranParameter_valueType);
            list.insert(list.end(), subList.begin(), subList.end());
        }
    }
    return list;
}

std::vector<RANParameterItem>
RANParameterItem::ExtractRANParametersFromList(
    const RANParameter_LIST_t* list)
{
    std::vector<RANParameterItem> result;
    if (list == nullptr)
    {
        return result;
    }

    int count = list->list_of_ranParameter.list.count;
    for (int i = 0; i < count; i++)
    {
        auto* structItem = list->list_of_ranParameter.list.array[i];
        if (structItem != nullptr)
        {
            auto subList = ExtractRANParametersFromStructure(structItem);
            result.insert(result.end(), subList.begin(), subList.end());
        }
    }
    return result;
}

} // namespace ns3
