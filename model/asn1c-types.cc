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

#include "asn1c-types.h"

#include "ns3/log.h"

#include <cstring>

NS_LOG_COMPONENT_DEFINE("Asn1Types");

namespace ns3
{

OctetString::OctetString(std::string value, size_t size)
{
    NS_LOG_FUNCTION(this);
    CreateBaseOctetString(size);
    std::memcpy(m_octetString->buf, value.c_str(), size);
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
    std::memcpy(m_octetString->buf, value, size);
}

OctetString::~OctetString()
{
    NS_LOG_FUNCTION(this);
    if (m_octetString != nullptr)
    {
        free(m_octetString->buf);
        free(m_octetString);
    }
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
    return std::string(reinterpret_cast<char*>(this->GetValue().buf), this->GetValue().size);
}

BitString::BitString(std::string value, size_t size)
{
    NS_LOG_FUNCTION(this);
    m_bitString = (BIT_STRING_t*)calloc(1, sizeof(BIT_STRING_t));
    m_bitString->buf = (uint8_t*)calloc(1, size);
    m_bitString->size = size;
    std::memcpy(m_bitString->buf, value.c_str(), size);
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
    if (m_bitString != nullptr)
    {
        free(m_bitString->buf);
        free(m_bitString);
    }
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
    uint16_t shifted = value * 16;
    std::string strShift = std::to_string(shifted);
    m_bitString = Create<BitString>(strShift, 5, 4);
}

NrCellId::~NrCellId() = default;

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
    m_sNssai = (SNSSAI_t*)calloc(1, sizeof(SNSSAI_t));
    m_sst = (OCTET_STRING_t*)calloc(1, sizeof(OCTET_STRING_t));
    m_sst->buf = (uint8_t*)calloc(1, sst.size());
    m_sst->size = sst.size();
    std::memcpy(m_sst->buf, sst.c_str(), sst.size());
    m_sNssai->sST = *m_sst;
}

Snssai::Snssai(std::string sst, std::string sd)
    : Snssai(sst)
{
    m_sd = (OCTET_STRING_t*)calloc(1, sizeof(OCTET_STRING_t));
    m_sd->buf = (uint8_t*)calloc(1, sd.size());
    m_sd->size = sd.size();
    std::memcpy(m_sd->buf, sd.c_str(), sd.size());
    m_sNssai->sD = m_sd;
}

Snssai::~Snssai()
{
    if (m_sNssai != nullptr)
    {
        ASN_STRUCT_FREE(asn_DEF_SNSSAI, m_sNssai);
    }
}

SNSSAI_t*
Snssai::GetPointer()
{
    return m_sNssai;
}

SNSSAI_t
Snssai::GetValue()
{
    return *m_sNssai;
}

MeasurementRecordItemWrap::MeasurementRecordItemWrap()
{
    m_measurementRecordItem = (MeasurementRecordItem_t*)calloc(1, sizeof(MeasurementRecordItem_t));
    m_measurementRecordItem->present = MeasurementRecordItem_PR_noValue;
}

MeasurementRecordItemWrap::MeasurementRecordItemWrap(long value)
    : MeasurementRecordItemWrap()
{
    m_measurementRecordItem->present = MeasurementRecordItem_PR_integer;
    m_measurementRecordItem->choice.integer = static_cast<unsigned long>(value);
}

MeasurementRecordItemWrap::MeasurementRecordItemWrap(double value)
    : MeasurementRecordItemWrap()
{
    m_measurementRecordItem->present = MeasurementRecordItem_PR_real;
    m_measurementRecordItem->choice.real = value;
}

MeasurementRecordItemWrap::~MeasurementRecordItemWrap()
{
    if (m_measurementRecordItem != nullptr)
    {
        ASN_STRUCT_FREE(asn_DEF_MeasurementRecordItem, m_measurementRecordItem);
    }
}

MeasurementRecordItem_t*
MeasurementRecordItemWrap::GetPointer()
{
    return m_measurementRecordItem;
}

MeasurementRecordItem_t
MeasurementRecordItemWrap::GetValue()
{
    return *m_measurementRecordItem;
}

MeasurementDataItemWrap::MeasurementDataItemWrap()
{
    m_measurementDataItem = (MeasurementDataItem_t*)calloc(1, sizeof(MeasurementDataItem_t));
}

MeasurementDataItemWrap::~MeasurementDataItemWrap()
{
    if (m_measurementDataItem != nullptr)
    {
        ASN_STRUCT_FREE(asn_DEF_MeasurementDataItem, m_measurementDataItem);
    }
}

MeasurementDataItem_t*
MeasurementDataItemWrap::GetPointer()
{
    return m_measurementDataItem;
}

MeasurementDataItem_t
MeasurementDataItemWrap::GetValue()
{
    return *m_measurementDataItem;
}

void
MeasurementDataItemWrap::AddRecordItem(Ptr<MeasurementRecordItemWrap> record)
{
    ASN_SEQUENCE_ADD(&m_measurementDataItem->measRecord.list, record->GetPointer());
}

void
MeasurementDataItemWrap::SetIncompleteFlag(bool isIncomplete)
{
    if (isIncomplete)
    {
        if (m_measurementDataItem->incompleteFlag == nullptr)
        {
            m_measurementDataItem->incompleteFlag = (long*)calloc(1, sizeof(long));
        }
        *m_measurementDataItem->incompleteFlag = MeasurementDataItem__incompleteFlag_true;
        return;
    }

    if (m_measurementDataItem->incompleteFlag != nullptr)
    {
        free(m_measurementDataItem->incompleteFlag);
        m_measurementDataItem->incompleteFlag = nullptr;
    }
}

MeasurementInfoItemWrap::MeasurementInfoItemWrap(std::string metricName)
{
    m_measurementInfoItem = (MeasurementInfoItem_t*)calloc(1, sizeof(MeasurementInfoItem_t));

    m_measurementInfoItem->measType.present = MeasurementType_PR_measName;
    m_measurementInfoItem->measType.choice.measName.buf = (uint8_t*)calloc(1, metricName.size());
    m_measurementInfoItem->measType.choice.measName.size = metricName.size();
    std::memcpy(m_measurementInfoItem->measType.choice.measName.buf,
                metricName.c_str(),
                metricName.size());
}

MeasurementInfoItemWrap::~MeasurementInfoItemWrap()
{
    if (m_measurementInfoItem != nullptr)
    {
        ASN_STRUCT_FREE(asn_DEF_MeasurementInfoItem, m_measurementInfoItem);
    }
}

MeasurementInfoItem_t*
MeasurementInfoItemWrap::GetPointer()
{
    return m_measurementInfoItem;
}

MeasurementInfoItem_t
MeasurementInfoItemWrap::GetValue()
{
    return *m_measurementInfoItem;
}

RANParameterItem::RANParameterItem(RANParameter_Item_t* ranParameterItem)
{
    m_ranParameterItem = ranParameterItem;
}

RANParameterItem::~RANParameterItem()
{
    if (m_ranParameterItem != nullptr)
    {
        ASN_STRUCT_FREE(asn_DEF_RANParameter_Item, m_ranParameterItem);
    }
}

RANParameter_Item_t*
RANParameterItem::GetPointer()
{
    return m_ranParameterItem;
}

RANParameter_Item_t
RANParameterItem::GetValue()
{
    return *m_ranParameterItem;
}

std::vector<RANParameterItem>
RANParameterItem::ExtractRANParametersFromRANParameter(RANParameter_Item_t* ranParameterItem)
{
    std::vector<RANParameterItem> ranParameterList;

    switch (ranParameterItem->ranParameterItem_valueType->present)
    {
    case RANParameter_ValueType_PR_NOTHING:
        break;

    case RANParameter_ValueType_PR_ranParameter_Element: {
        RANParameterItem newItem = RANParameterItem(ranParameterItem);
        RANParameter_ELEMENT_t* ranParameterElement =
            ranParameterItem->ranParameterItem_valueType->choice.ranParameter_Element;
        newItem.m_keyFlag = &ranParameterElement->keyFlag;

        switch (ranParameterElement->ranParameter_Value.present)
        {
        case RANParameter_Value_PR_NOTHING:
            newItem.m_valueType = ValueType::Nothing;
            break;

        case RANParameter_Value_PR_valueInt:
            newItem.m_valueInt = ranParameterElement->ranParameter_Value.choice.valueInt;
            newItem.m_valueType = ValueType::Int;
            break;

        case RANParameter_Value_PR_valueOctS:
            newItem.m_valueStr = Create<OctetString>(
                (void*)ranParameterElement->ranParameter_Value.choice.valueOctS.buf,
                ranParameterElement->ranParameter_Value.choice.valueOctS.size);
            newItem.m_valueType = ValueType::OctectString;
            break;
        }

        ranParameterList.push_back(newItem);
        break;
    }

    case RANParameter_ValueType_PR_ranParameter_Structure: {
        RANParameter_STRUCTURE_t* ranParameterStructure =
            ranParameterItem->ranParameterItem_valueType->choice.ranParameter_Structure;
        int count = ranParameterStructure->sequence_of_ranParameters.list.count;
        for (int i = 0; i < count; i++)
        {
            RANParameter_Item_t* childRanItem =
                ranParameterStructure->sequence_of_ranParameters.list.array[i];

            for (RANParameterItem extractedParameter :
                 ExtractRANParametersFromRANParameter(childRanItem))
            {
                ranParameterList.push_back(extractedParameter);
            }
        }
        break;
    }

    case RANParameter_ValueType_PR_ranParameter_List:
        break;
    }

    return ranParameterList;
}

} // namespace ns3
