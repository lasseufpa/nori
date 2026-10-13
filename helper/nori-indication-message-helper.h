/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2022 Northeastern University
 * Copyright (c) 2022 Sapienza, University of Rome
 * Copyright (c) 2022 University of Padova
 * Copyright (c) 2026 LASSE/UFPA
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * E2SM-KPM v3 Indication Message Helper for NR (nori)
 */

#pragma once

#include "indication-message-helper.h"

extern "C"
{
#include "MeasurementRecordItem.h"
#include "UEID.h"
#include "UEID-GNB.h"
#include "GUAMI.h"
#include "AMF-UE-NGAP-ID.h"
}

namespace ns3
{

class NoriIndicationMessageHelper : public IndicationMessageHelper
{
    public:
        NoriIndicationMessageHelper(IndicationMessageType type, bool isOffline, bool reducedPmValues);
        ~NoriIndicationMessageHelper();

        void SetGranularityPeriod(unsigned long periodMs);

        void AddNodeMeasurementInteger(const std::string& measName, unsigned long value);
        
        void AddNodeMeasurementReal(const std::string& measName, double value);

        void BeginUeReport(uint64_t amfUeNgapId,
                    const std::string& plmnId,
                    uint8_t amfRegionId,
                    uint16_t amfSetId,
                    uint8_t amfPointer);

        void AddUeMeasurementInteger(const std::string& measName, unsigned long value);

        void AddUeMeasurementReal(const std::string& measName, double value);


    private:
            UEID_t* BuildGnbUeId(uint64_t amfUeNgapId,
                        const std::string& plmnId,
                        uint8_t amfRegionId,
                        uint16_t amfSetId,
                        uint8_t amfPointer);
};

} // namespace ns3