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

#include "mmwave-indication-message-helper.h"

namespace ns3
{

MmWaveIndicationMessageHelper::MmWaveIndicationMessageHelper(
                                                             bool isOffline,
                                                             bool reducedPmValues)
    : IndicationMessageHelper(isOffline, reducedPmValues)
{
}

void
MmWaveIndicationMessageHelper::AddCuUpUePmItem(std::string ueImsiComplete,
                                               std::string plmId, // Adicionado como parâmetro
                                               long txPdcpPduBytesNrRlc,
                                               long txPdcpPduNrRlc,
                                               double pdcpThroughput)
{
    KpmMeasurementLabelValues labels;
    labels.m_plmId = plmId;
    labels.m_noUEID = ueImsiComplete; // Vínculo com o Usuário

    if (!m_reducedPmValues)
    {
        AddMetricValue(KPM_SUPPORTED_METRICS[PDCP_VOL_DL], (double)txPdcpPduBytesNrRlc, labels);
        AddMetricValue(KPM_SUPPORTED_METRICS[PDCP_PDU_NBR], (double)txPdcpPduNrRlc, labels);
        AddMetricValue(KPM_SUPPORTED_METRICS[PDCP_THP_DL], pdcpThroughput, labels);
    }
}


void
MmWaveIndicationMessageHelper::AddDuUePmItem(std::string ueImsiComplete,
                                             uint64_t nrCellId, 
                                             long macPduUe,
                                             long macPduInitialUe,
                                             long macQpsk,
                                             long mac16Qam,
                                             long mac64Qam,
                                             long macRetx,
                                             // CORREÇÃO 1: Removido o 'macVolume' daqui!
                                             long macPrb,
                                             long macMac04, long macMac59, long macMac1014,
                                             long macMac1519, long macMac2024, long macMac2529,
                                             long macSinrBin1, long macSinrBin2, long macSinrBin3,
                                             long macSinrBin4, long macSinrBin5, long macSinrBin6,
                                             long macSinrBin7,
                                             long rlcBufferOccup,
                                             double drbThrDlUeid)
{
    KpmMeasurementLabelValues labels;
    labels.m_noUEID = ueImsiComplete; 
    labels.m_nrCellId = nrCellId;

    if (!m_reducedPmValues)
    {
        AddMetricValue(KPM_SUPPORTED_METRICS[TB_TOT_NBR_DL], (double)macPduUe, labels);
        AddMetricValue(KPM_SUPPORTED_METRICS[TB_TOT_NBR_DL_INIT], (double)macPduInitialUe, labels);
        AddMetricValue(KPM_SUPPORTED_METRICS[TB_INIT_QPSK], (double)macQpsk, labels);
        AddMetricValue(KPM_SUPPORTED_METRICS[TB_INIT_16QAM], (double)mac16Qam, labels);
        AddMetricValue(KPM_SUPPORTED_METRICS[TB_INIT_64QAM], (double)mac64Qam, labels);
        AddMetricValue(KPM_SUPPORTED_METRICS[TB_ERR_RETX], (double)macRetx, labels);
        AddMetricValue(KPM_SUPPORTED_METRICS[RRU_PRB_UE], (double)std::ceil(macPrb), labels);

        AddMetricValue(KPM_SUPPORTED_METRICS[MCS_BIN_1], (double)macMac04, labels);
        AddMetricValue(KPM_SUPPORTED_METRICS[MCS_BIN_2], (double)macMac59, labels);
        AddMetricValue(KPM_SUPPORTED_METRICS[MCS_BIN_3], (double)macMac1014, labels);
        AddMetricValue(KPM_SUPPORTED_METRICS[MCS_BIN_4], (double)macMac1519, labels);
        AddMetricValue(KPM_SUPPORTED_METRICS[MCS_BIN_5], (double)macMac2024, labels);
        AddMetricValue(KPM_SUPPORTED_METRICS[MCS_BIN_6], (double)macMac2529, labels);

        // CORREÇÃO 2: Adicionado o 'R' em SINR_BIN
        AddMetricValue(KPM_SUPPORTED_METRICS[SINR_BIN_1], (double)macSinrBin1, labels);
        AddMetricValue(KPM_SUPPORTED_METRICS[SINR_BIN_2], (double)macSinrBin2, labels);
        AddMetricValue(KPM_SUPPORTED_METRICS[SINR_BIN_3], (double)macSinrBin3, labels);
        AddMetricValue(KPM_SUPPORTED_METRICS[SINR_BIN_4], (double)macSinrBin4, labels);
        AddMetricValue(KPM_SUPPORTED_METRICS[SINR_BIN_5], (double)macSinrBin5, labels);
        AddMetricValue(KPM_SUPPORTED_METRICS[SINR_BIN_6], (double)macSinrBin6, labels);
        AddMetricValue(KPM_SUPPORTED_METRICS[SINR_BIN_7], (double)macSinrBin7, labels);
        
        AddMetricValue("DRB.BufferSize.Qos.UEID", (double)rlcBufferOccup, labels);
    }

    AddMetricValue(KPM_SUPPORTED_METRICS[DRB_THP_UE], drbThrDlUeid, labels);
}

void
MmWaveIndicationMessageHelper::AddDuCellPmItem(uint64_t nrCellId, 
                                               double prbUtilizationDl,
                                               long activeUeDl)
{
    KpmMeasurementLabelValues labels;
    labels.m_nrCellId = nrCellId;

    AddMetricValue(KPM_SUPPORTED_METRICS[PRB_UTIL_CELL], (double)std::ceil(prbUtilizationDl), labels);
    AddMetricValue(KPM_SUPPORTED_METRICS[ACTIVE_UES_CELL], (double)activeUeDl, labels);
}

// void
// MmWaveIndicationMessageHelper::AddDuCellResRepPmItem(uint64_t nrCellId, Ptr<CellResourceReport> cellResRep)
// {
//     // 1. Criar a Label da Célula
//     KpmMeasurementLabelValues labels;
//     labels.m_nrCellId = nrCellId;

//     // 2. Extrair os dados do objeto CellResourceReport
//     // Supondo que o objeto tenha um método para pegar o uso total de PRB
//     double prbUsage = cellResRep->GetTotalPrbUsage(); 

//     // 3. Enviar como uma métrica de rádio padrão (definida no seu kpm-metrics-defs.h)
//     AddMetricValue(KPM_SUPPORTED_METRICS[RRU_PRB_USED_CELL], prbUsage, labels);

    
//     for (auto const& sliceReport : cellResRep->GetSliceReports()) {
//         KpmMeasurementLabelValues sliceLabels = labels;
//         sliceLabels.m_sNssai = sliceReport.GetSnssai();
//         AddMetricValue("RRU.PrbUsedDl.Slice", sliceReport.GetUsage(), sliceLabels);
//     }

// }
void
MmWaveIndicationMessageHelper::AddCuCpUePmItem(std::string ueImsiComplete,
                                               long numDrb,
                                               long drbRelAct)
{
    KpmMeasurementLabelValues labels;
    labels.m_noUEID = ueImsiComplete;

    if (!m_reducedPmValues)
    {
        // Usando o mapeamento do arquivo de métricas
        AddMetricValue(KPM_SUPPORTED_METRICS[DRB_ESTAB_SUCCESS], (double)numDrb, labels);
        AddMetricValue(KPM_SUPPORTED_METRICS[DRB_REL_ACT], (double)drbRelAct, labels);
    }
}

MmWaveIndicationMessageHelper::~MmWaveIndicationMessageHelper()
{
}

} // namespace ns3
