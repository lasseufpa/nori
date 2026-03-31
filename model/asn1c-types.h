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

#include <string>

extern "C"
{
#include "BIT_STRING.h"
#include "MeasurementDataItem.h"
#include "MeasurementInfoItem.h"
#include "MeasurementRecordItem.h"
#include "MeasurementType.h"
#include "OCTET_STRING.h"
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

class MeasurementRecordItemWrap : public SimpleRefCount<MeasurementRecordItemWrap>
{
  public:
    MeasurementRecordItemWrap();
    MeasurementRecordItemWrap(long value);
    MeasurementRecordItemWrap(double value);
    ~MeasurementRecordItemWrap();
    MeasurementRecordItem_t* GetPointer();
    MeasurementRecordItem_t GetValue();

  private:
    MeasurementRecordItem_t* m_measurementRecordItem;
};

class MeasurementDataItemWrap : public SimpleRefCount<MeasurementDataItemWrap>
{
  public:
    MeasurementDataItemWrap();
    ~MeasurementDataItemWrap();
    MeasurementDataItem_t* GetPointer();
    MeasurementDataItem_t GetValue();
    void AddRecordItem(Ptr<MeasurementRecordItemWrap> record);
    void SetIncompleteFlag(bool isIncomplete);

  private:
    MeasurementDataItem_t* m_measurementDataItem;
};

class MeasurementInfoItemWrap : public SimpleRefCount<MeasurementInfoItemWrap>
{
  public:
    MeasurementInfoItemWrap(std::string metricName);
    ~MeasurementInfoItemWrap();
    MeasurementInfoItem_t* GetPointer();
    MeasurementInfoItem_t GetValue();

  private:
    MeasurementInfoItem_t* m_measurementInfoItem;
};

} // namespace ns3
