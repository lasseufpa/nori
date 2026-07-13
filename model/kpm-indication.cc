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

#include "kpm-indication.h"

#include "asn1c-types.h"

#include "ns3/log.h"

extern "C"
{
#include "E2SM-KPM-Indication_Hearder-Format1.h
#include "E2SM-KPM-IndicationMessage-Format1.h"
#include "TimeStamp.h"
}

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("KpmIndication");

KpmIndicationHeader::KpmIndicationHeader(KpmRicIndicationHeaderValues values)
{
    auto* descriptor = new E2SM_KPM_IndicationHeader_t;
    FillAndEncodeKpmRicIndicationHeader(descriptor, values);
    delete descriptor;
}

KpmIndicationHeader::~KpmIndicationHeader()
{
    NS_LOG_FUNCTION(this);
    free(m_buffer);
    m_size = 0;
}

void
KpmIndicationHeader::Encode(E2SM_KPM_IndicationHeader_t* descriptor)
{
    asn_codec_ctx_t* opt_cod = nullptr; // disable stack bounds checking
    asn_encode_to_new_buffer_result_s encodedHeader =
        asn_encode_to_new_buffer(opt_cod,
                                 ATS_ALIGNED_BASIC_PER,
                                 &asn_DEF_E2SM_KPM_IndicationHeader,
                                 descriptor);

    if (encodedHeader.result.encoded < 0)
    {
        NS_FATAL_ERROR("Error during the encoding of the RIC Indication Header, errno: "
                       << strerror(errno) << ", failed_type "
                       << encodedHeader.result.failed_type->name << ", structure_ptr "
                       << encodedHeader.result.structure_ptr);
    }

    m_buffer = encodedHeader.buffer;
    m_size = encodedHeader.result.encoded;
}

void
KpmIndicationHeader::FillAndEncodeKpmRicIndicationHeader(E2SM_KPM_IndicationHeader_t* descriptor,
                                                         KpmRicIndicationHeaderValues values)
{
    auto* ind_header =
        (E2SM_KPM_IndicationHeader_Format1_t*)calloc(1,
                                                     sizeof(E2SM_KPM_IndicationHeader_Format1_t));

    NS_LOG_DEBUG("Timestamp received: " << values.m_timestamp);
    long bigEndianTimestamp = htobe64(values.m_timestamp);
    NS_LOG_DEBUG("Timestamp inverted: " << bigEndianTimestamp);

    Ptr<OctetString> ts = Create<OctetString>((void*)&bigEndianTimestamp, TIMESTAMP_LIMIT_SIZE);
    

    ind_header->colletStartTime= ts->GetValue();

    //TODO: adicioanr ifs paara dar suporte aos parametros opcionais (vamos usar?)
    
    if  (!values.m_fileFormatVersion.empty()){
        ind_header->fileFormatVersion = (PrintableString_t*)calloc(1, sizeof(PrintableString_t));
        OCTET_STRING_fromString(ind_header->fileFormatVersion, values.m_fileFormatVersion.c_str());
    }
    if (!values.m_senderName.empty()){
        ind_header->senderName = (PrintableString_t*)calloc(1, sizeof(PrintableString_t));
        OCTET_STRING_fromString(ind_header->senderName, values.m_senderName.c_str());
    }
    if (!values.m_senderType.empty()){
        ind_header->senderType = (PrintableString_t*)calloc(1, sizeof(PrintableString_t));
        OCTET_STRING_fromString(ind_header->senderType, values.m_senderType.c_str());
    }
    if (!values.m_vendorName.empty()){
        ind_header->vendorName = (PrintableString_t*)calloc(1, sizeof(PrintableString_t));
        OCTET_STRING_fromString(ind_header->vendorName, values.m_vendorName.c_str());
    }

            
    NS_LOG_INFO(xer_fprint(stderr, &asn_DEF_E2SM_KPM_IndicationHeader_Format1, ind_header));

    descriptor->indicationHeader_formats.present = E2SM_KPM_IndiciationHeader__indicationHeader_Formats_PR_indicationHeader_Format1;
    descriptor->inficationHeader_formats.choice.indicationHeader_Format1 = ind_header;

    //descriptor->present = E2SM_KPM_IndicationHeader_PR_indicationHeader_Format1;
    //descriptor->choice.indicationHeader_Format1 = ind_header;

    Encode(descriptor);
    ASN_STRUCT_FREE(asn_DEF_E2SM_KPM_IndicationHeader_Format1, ind_header);
}

KpmIndicationMessage::KpmIndicationMessage(KpmIndicationMessageValues values)
{
    auto* descriptor = new E2SM_KPM_IndicationMessage_t();
    CheckConstraints(values);
    FillAndEncodeKpmIndicationMessage(descriptor, values);
    delete descriptor;
}

KpmIndicationMessage::~KpmIndicationMessage()
{
    free(m_buffer);
    m_size = 0;
}

void
KpmIndicationMessage::CheckConstraints(KpmIndicationMessageValues values)
{
    // TODO remove?
    // if (values.m_crnti.length () != 2)
    //   {
    //     NS_FATAL_ERROR ("C-RNTI should have length 2");
    //   }
    // if (values.m_plmId.length () != 3)
    //   {
    //     NS_FATAL_ERROR ("PLMID should have length 3");
    //   }
    // if (values.m_nrCellId.length () != 5)
    //   {
    //     NS_FATAL_ERROR ("NR Cell ID should have length 5");
    //   }
    // TODO add other constraints
}

void
KpmIndicationMessage::Encode(E2SM_KPM_IndicationMessage_t* descriptor)
{
    asn_codec_ctx_t* opt_cod = 0; // disable stack bounds checking
    asn_encode_to_new_buffer_result_s encodedMsg =
        asn_encode_to_new_buffer(opt_cod,
                                 ATS_ALIGNED_BASIC_PER,
                                 &asn_DEF_E2SM_KPM_IndicationMessage,
                                 descriptor);

    if (encodedMsg.result.encoded < 0)
    {
        NS_FATAL_ERROR("Error during the encoding of the RIC Indication Message, errno: "
                       << strerror(errno) << ", failed_type " << encodedMsg.result.failed_type->name
                       << ", structure_ptr " << encodedMsg.result.structure_ptr);
    }

    m_buffer = encodedMsg.buffer;
    m_size = encodedMsg.result.encoded;
}

void
KpmIndicationMessage::FillPmContainer(PF_Container_t* ranContainer, Ptr<PmContainerValues> values)
{
    Ptr<OCuUpContainerValues> cuUpVal = DynamicCast<OCuUpContainerValues>(values);
    Ptr<OCuCpContainerValues> cuCpVal = DynamicCast<OCuCpContainerValues>(values);
    Ptr<ODuContainerValues> duVal = DynamicCast<ODuContainerValues>(values);

    if (cuUpVal)
    {
        FillOCuUpContainer(ranContainer, cuUpVal);
    }
    else if (cuCpVal)
    {
        FillOCuCpContainer(ranContainer, cuCpVal);
    }
    else if (duVal)
    {
        FillODuContainer(ranContainer, duVal);
    }
    else
    {
        NS_FATAL_ERROR("Unknown PM Container type");
    }
}

void
KpmIndicationMessage::FillOCuUpContainer(PF_Container_t* ranContainer,
                                         Ptr<OCuUpContainerValues> values)
{
    auto* ocuup = (OCUUP_PF_Container_t*)calloc(1, sizeof(OCUUP_PF_Container_t));
    auto* pcli = (PF_ContainerListItem_t*)calloc(1, sizeof(PF_ContainerListItem_t));
    pcli->interface_type = NI_Type_x2_u;

    auto* cuuppmc = (CUUPMeasurement_Container_t*)calloc(1, sizeof(CUUPMeasurement_Container_t));
    auto* plmnItem = (PlmnID_Item_t*)calloc(1, sizeof(PlmnID_Item_t));
    Ptr<OctetString> plmnidstr = Create<OctetString>(values->m_plmId, 3);
    plmnItem->pLMN_Identity = plmnidstr->GetValue();

    auto* cuuppmf = (EPC_CUUP_PM_Format_t*)calloc(1, sizeof(EPC_CUUP_PM_Format_t));
    plmnItem->cu_UP_PM_EPC = cuuppmf;
    auto* pqrli = (PerQCIReportListItemFormat_t*)calloc(1, sizeof(PerQCIReportListItemFormat_t));
    pqrli->drbqci = 0;

    auto* pDCPBytesDL = (INTEGER_t*)calloc(1, sizeof(INTEGER_t));
    auto* pDCPBytesUL = (INTEGER_t*)calloc(1, sizeof(INTEGER_t));

    asn_long2INTEGER(pDCPBytesDL, values->m_pDCPBytesDL);
    asn_long2INTEGER(pDCPBytesUL, values->m_pDCPBytesUL);

    pqrli->pDCPBytesDL = pDCPBytesDL;
    pqrli->pDCPBytesUL = pDCPBytesUL;

    ASN_SEQUENCE_ADD(&cuuppmf->perQCIReportList_cuup.list, pqrli);

    ASN_SEQUENCE_ADD(&cuuppmc->plmnList.list, plmnItem);

    pcli->o_CU_UP_PM_Container = *cuuppmc;
    ASN_SEQUENCE_ADD(&ocuup->pf_ContainerList, pcli);
    ranContainer->choice.oCU_UP = ocuup;
    ranContainer->present = PF_Container_PR_oCU_UP;

    free(cuuppmc);
}

void
KpmIndicationMessage::FillOCuCpContainer(PF_Container_t* ranContainer,
                                         Ptr<OCuCpContainerValues> values)
{
    OCUCP_PF_Container_t* ocucp = (OCUCP_PF_Container_t*)calloc(1, sizeof(OCUCP_PF_Container_t));
    long* numActiveUes = (long*)calloc(1, sizeof(long));
    *numActiveUes = long(values->m_numActiveUes);
    ocucp->cu_CP_Resource_Status.numberOfActive_UEs = numActiveUes;
    ranContainer->choice.oCU_CP = ocucp;
    ranContainer->present = PF_Container_PR_oCU_CP;
}

void
KpmIndicationMessage::FillODuContainer(PF_Container_t* ranContainer, Ptr<ODuContainerValues> values)
{
    ODU_PF_Container_t* odu = (ODU_PF_Container_t*)calloc(1, sizeof(ODU_PF_Container_t));

    for (auto cellReport : values->m_cellResourceReportItems)
    {
        NS_LOG_LOGIC("O-DU: Add Cell Resource Report Item");
        CellResourceReportListItem_t* crrli =
            (CellResourceReportListItem_t*)calloc(1, sizeof(CellResourceReportListItem_t));

        Ptr<OctetString> plmnid = Create<OctetString>(cellReport->m_plmId, 3);
        Ptr<NrCellId> nrcellid = Create<NrCellId>(cellReport->m_nrCellId);
        crrli->nRCGI.pLMN_Identity = plmnid->GetValue();
        crrli->nRCGI.nRCellIdentity = nrcellid->GetValue();

        long* dlAvailablePrbs = (long*)calloc(1, sizeof(long));
        *dlAvailablePrbs = cellReport->dlAvailablePrbs;
        crrli->dl_TotalofAvailablePRBs = dlAvailablePrbs;

        long* ulAvailablePrbs = (long*)calloc(1, sizeof(long));
        *ulAvailablePrbs = cellReport->ulAvailablePrbs;
        crrli->ul_TotalofAvailablePRBs = ulAvailablePrbs;
        ASN_SEQUENCE_ADD(&odu->cellResourceReportList.list, crrli);

        for (auto servedPlmnCell : cellReport->m_servedPlmnPerCellItems)
        {
            NS_LOG_LOGIC("O-DU: Add Served Plmn Per Cell Item");
            auto* sppcl =
                (ServedPlmnPerCellListItem_t*)calloc(1, sizeof(ServedPlmnPerCellListItem_t));
            Ptr<OctetString> servedPlmnId = Create<OctetString>(servedPlmnCell->m_plmId, 3);
            sppcl->pLMN_Identity = servedPlmnId->GetValue();

            auto* edpc = (EPC_DU_PM_Container_t*)calloc(1, sizeof(EPC_DU_PM_Container_t));

            for (auto perQciReportItem : servedPlmnCell->m_perQciReportItems)
            {
                NS_LOG_LOGIC("O-DU: Add Per QCI Report Item");
                auto* pqrl = (PerQCIReportListItem_t*)calloc(1, sizeof(PerQCIReportListItem_t));
                pqrl->qci = perQciReportItem->m_qci;

                NS_ABORT_MSG_IF((perQciReportItem->m_dlPrbUsage < 0) |
                                    (perQciReportItem->m_dlPrbUsage > 100),
                                "As per ASN definition, dl_PRBUsage should be between 0 and 100");
                long* dlUsedPrbs = (long*)calloc(1, sizeof(long));
                *dlUsedPrbs = perQciReportItem->m_dlPrbUsage;
                pqrl->dl_PRBUsage = dlUsedPrbs;
                NS_LOG_LOGIC("DL PRBs " << dlUsedPrbs);

                NS_ABORT_MSG_IF((perQciReportItem->m_ulPrbUsage < 0) |
                                    (perQciReportItem->m_ulPrbUsage > 100),
                                "As per ASN definition, ul_PRBUsage should be between 0 and 100");
                long* ulUsedPrbs = (long*)calloc(1, sizeof(long));
                *ulUsedPrbs = perQciReportItem->m_ulPrbUsage;
                pqrl->ul_PRBUsage = ulUsedPrbs;
                ASN_SEQUENCE_ADD(&edpc->perQCIReportList_du.list, pqrl);
            }

            sppcl->du_PM_EPC = edpc;
            ASN_SEQUENCE_ADD(&crrli->servedPlmnPerCellList.list, sppcl);
        }
    }
    ranContainer->choice.oDU = odu;
    ranContainer->present = PF_Container_PR_oDU;
}

void
KpmIndicationMessage::FillAndEncodeKpmIndicationMessage(E2SM_KPM_IndicationMessage_t* descriptor,
                                                        KpmIndicationMessageValues values)
{
    // Create and fill the RAN Container
    auto* ranContainer = (PF_Container_t*)calloc(1, sizeof(PF_Container_t));
    FillPmContainer(ranContainer, values.m_pmContainerValues);

    //------- now fill the message
    auto* containers_list = (PM_Containers_Item_t*)calloc(1, sizeof(PM_Containers_Item_t));
    containers_list->performanceContainer = ranContainer;

    auto* format =
        (E2SM_KPM_IndicationMessage_Format1_t*)calloc(1,
                                                      sizeof(E2SM_KPM_IndicationMessage_Format1_t));

    ASN_SEQUENCE_ADD(&format->pm_Containers.list, containers_list);

    // Cell Object ID
    CellObjectID_t* cellObjectID = (CellObjectID_t*)calloc(1, sizeof(CellObjectID_t));
    cellObjectID->size = values.m_cellObjectId.length();
    cellObjectID->buf = (uint8_t*)calloc(1, cellObjectID->size);
    memcpy(cellObjectID->buf, values.m_cellObjectId.c_str(), values.m_cellObjectId.length());
    format->cellObjectID = *cellObjectID;

    // Measurement Information List
    if (values.m_cellMeasurementItems)
    {
        format->list_of_PM_Information =
            (E2SM_KPM_IndicationMessage_Format1::
                 E2SM_KPM_IndicationMessage_Format1__list_of_PM_Information*)
                calloc(1,
                       sizeof(E2SM_KPM_IndicationMessage_Format1::
                                  E2SM_KPM_IndicationMessage_Format1__list_of_PM_Information));
        for (auto item : values.m_cellMeasurementItems->GetItems())
        {
            ASN_SEQUENCE_ADD(&format->list_of_PM_Information->list, item->GetPointer());
        }
    }

    // List of matched UEs
    if (values.m_ueIndications.size() > 0)
    {
        format->list_of_matched_UEs = (E2SM_KPM_IndicationMessage_Format1_t::
                                           E2SM_KPM_IndicationMessage_Format1__list_of_matched_UEs*)
            calloc(1,
                   sizeof(E2SM_KPM_IndicationMessage_Format1_t::
                              E2SM_KPM_IndicationMessage_Format1__list_of_matched_UEs));

        for (auto ueIndication : values.m_ueIndications)
        {
            PerUE_PM_Item_t* perUEItem = (PerUE_PM_Item_t*)calloc(1, sizeof(PerUE_PM_Item_t));

            // UE Identity
            perUEItem->ueId = ueIndication->GetId();
            // xer_fprint (stderr, &asn_DEF_UE_Identity, &perUEItem->ueId);
            // NS_LOG_UNCOND ("Values " << ueIndication->m_drbIPLateDlUEID);

            // List of Measurements PM information
            perUEItem->list_of_PM_Information =
                (PerUE_PM_Item::PerUE_PM_Item__list_of_PM_Information*)
                    calloc(1, sizeof(PerUE_PM_Item::PerUE_PM_Item__list_of_PM_Information));

            for (auto measurementItem : ueIndication->GetItems())
            {
                ASN_SEQUENCE_ADD(&perUEItem->list_of_PM_Information->list,
                                 measurementItem->GetPointer());
            }
            ASN_SEQUENCE_ADD(&format->list_of_matched_UEs->list, perUEItem);
        }
    }

    descriptor->present = E2SM_KPM_IndicationMessage_PR_indicationMessage_Format1;
    descriptor->choice.indicationMessage_Format1 = format;

    NS_LOG_INFO(xer_fprint(stderr, &asn_DEF_E2SM_KPM_IndicationMessage_Format1, format));

    // xer_fprint (stderr, &asn_DEF_PF_Container, ranContainer);
    Encode(descriptor);

    free(cellObjectID);
    // free (ranContainer);
    ASN_STRUCT_FREE(asn_DEF_E2SM_KPM_IndicationMessage_Format1, format);
}

MeasurementItemList::MeasurementItemList()
{
    m_id = NULL;
}

MeasurementItemList::MeasurementItemList(std::string id)
{
    m_id = Create<OctetString>(id, id.length());
}

MeasurementItemList::~MeasurementItemList(){};

std::vector<Ptr<MeasurementItem>>
MeasurementItemList::GetItems()
{
    return m_items;
}

OCTET_STRING_t
MeasurementItemList::GetId()
{
    NS_ABORT_IF(m_id == nullptr);
    return m_id->GetValue();
}

} // namespace ns3
