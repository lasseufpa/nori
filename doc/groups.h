// Copyright (c) 2026 LASSE/UFPA
//
// SPDX-License-Identifier: GPL-2.0-only

/**
 * @defgroup nori NORI
 *
 * O-RAN E2 interface for gNBs simulated with the nr (5G-LENA) module of ns-3.
 *
 * Each gNB with an E2 termination (installed by ns3::E2TermHelper) behaves as an E2
 * node of a Near-RT RIC: it answers the E2 Setup, accepts E2SM-KPM and E2SM-CCC
 * subscriptions, sends periodic RIC Indications and applies E2SM-RC and E2SM-CCC
 * slice PRB quotas in ns3::NrRLMacSchedulerOfdma.
 *
 * The user manual (doc/source) describes the model; this reference documents the
 * classes.
 */

/**
 * @defgroup nori-e2 E2 termination and interface
 * @ingroup nori
 *
 * ns3::E2Termination (E2 agent, SCTP association, e2sim), ns3::E2Interface (E2 node
 * logic, queues filled by the SCTP thread and drained by the simulation thread) and
 * ns3::E2TermHelper.
 */

/**
 * @defgroup nori-kpm E2SM-KPM
 * @ingroup nori
 *
 * RAN function definition, indication header and message encoders, and
 * ns3::NoriE2Report (per-UE MAC/PHY counters).
 */

/**
 * @defgroup nori-rc E2SM-RC
 * @ingroup nori
 *
 * Control Style 2, Action 6 (slice-level PRB quota): ns3::RicControlFunctionDescription
 * and ns3::RicControlMessage.
 */

/**
 * @defgroup nori-ccc E2SM-CCC
 * @ingroup nori
 *
 * JSON RAN function definition, indications and O-RRMPolicyRatio controls.
 */

/**
 * @defgroup nori-slicing RAN slicing
 * @ingroup nori
 *
 * ns3::NrRLMacSchedulerOfdma (dedicated / min / max RBG shares per slice) and
 * ns3::NoriSlicingHelper (RNTI to SST mapping).
 */
