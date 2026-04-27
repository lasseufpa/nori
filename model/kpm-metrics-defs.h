/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef KPM_METRICS_DEFS_H
#define KPM_METRICS_DEFS_H

#include <vector>
#include <string>

namespace ns3 {

/**
 * @brief Enumeração para facilitar o acesso aos índices do vetor de métricas.
 */
enum KpmMetricIndex {
    
    PDCP_VOL_DL = 0,       
    PDCP_PDU_NBR,           
    PDCP_THP_DL,            

   
    DRB_ESTAB_SUCCESS,      
    DRB_REL_ACT,            
   
    TB_TOT_NBR_DL,        
    TB_TOT_NBR_DL_INIT,     
    TB_INIT_QPSK,           
    TB_INIT_16QAM,          
    TB_INIT_64QAM,          
    TB_ERR_RETX,            
    RRU_PRB_UE,             
    DRB_THP_UE,             
    RLC_BUFFER_OCC,         

    
    MCS_BIN_1, MCS_BIN_2, MCS_BIN_3, MCS_BIN_4, MCS_BIN_5, MCS_BIN_6,

    
    SINR_BIN_1, SINR_BIN_2, SINR_BIN_3, SINR_BIN_4, SINR_BIN_5, SINR_BIN_6, SINR_BIN_7,

   
    PRB_UTIL_CELL,          
    ACTIVE_UES_CELL,        
    TB_TOT_CELL,             

    
    NUM_KPM_METRICS        
};



inline const std::vector<std::string> KPM_SUPPORTED_METRICS = {
    "QosFlow.PdcpPduVolumeDL_Filter.UEID", // PDCP_VOL_DL
    "DRB.PdcpPduNbrDl.Qos.UEID",           // PDCP_PDU_NBR
    "DRB.PdcpSduBitRateDl.UEID",           // PDCP_THP_DL

    "DRB.EstabSucc.5QI.UEID",              // DRB_ESTAB_SUCCESS
    "DRB.RelActNbr.5QI.UEID",              // DRB_REL_ACT

    "TB.TotNbrDl.1.UEID",                  // TB_TOT_NBR_DL
    "TB.TotNbrDlInitial.UEID",             // TB_TOT_NBR_DL_INIT
    "TB.TotNbrDlInitial.Qpsk.UEID",        // TB_INIT_QPSK
    "TB.TotNbrDlInitial.16Qam.UEID",       // TB_INIT_16QAM
    "TB.TotNbrDlInitial.64Qam.UEID",       // TB_INIT_64QAM
    "TB.ErrTotalNbrDl.1.UEID",             // TB_ERR_RETX
    "RRU.PrbUsedDl.UEID",                  // RRU_PRB_UE
    "DRB.UEThpDl.UEID",                    // DRB_THP_UE
    "DRB.BufferSize.Qos.UEID",             // RLC_BUFFER_OCC

    "CARR.PDSCHMCSDist.Bin1.UEID",         // MCS_BIN_1
    "CARR.PDSCHMCSDist.Bin2.UEID",         // MCS_BIN_2
    "CARR.PDSCHMCSDist.Bin3.UEID",         // MCS_BIN_3
    "CARR.PDSCHMCSDist.Bin4.UEID",         // MCS_BIN_4
    "CARR.PDSCHMCSDist.Bin5.UEID",         // MCS_BIN_5
    "CARR.PDSCHMCSDist.Bin6.UEID",         // MCS_BIN_6

    "L1M.RS-SINR.Bin34.UEID",              // SINR_BIN_1
    "L1M.RS-SINR.Bin46.UEID",              // SINR_BIN_2
    "L1M.RS-SINR.Bin58.UEID",              // SINR_BIN_3
    "L1M.RS-SINR.Bin70.UEID",              // SINR_BIN_4
    "L1M.RS-SINR.Bin82.UEID",              // SINR_BIN_5
    "L1M.RS-SINR.Bin94.UEID",              // SINR_BIN_6
    "L1M.RS-SINR.Bin127.UEID",             // SINR_BIN_7

    "RRU.PrbUsedDl.Total",                 // PRB_UTIL_CELL
    "DRB.MeanActiveUeDl",                  // ACTIVE_UES_CELL
    "TB.TotNbrDl.1",                       // TB_TOT_CELL
    "RRU.PrbUsedDl"                        // RRU_PRB_USED_CELL
};


} // namespace ns3

#endif // KPM_METRICS_DEFS_H