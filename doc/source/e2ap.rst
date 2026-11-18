E2AP procedures
===============

This chapter lists the E2AP procedures that NORI implements and how each one maps to
the code. The service model payloads are described in :doc:`kpm`, :doc:`rc` and
:doc:`ccc`.

.. code-block:: text

   E2 node (NORI)                                   Near-RT RIC
   --------------                                   -----------
   E2 Setup Request (3 RAN functions)  ------------>
                                       <------------ E2 Setup Response
                                       <------------ RIC Subscription Request (KPM/CCC)
   RIC Subscription Response           ------------>
   RIC Indication (every reportingPeriod) -------->
                                       <------------ RIC Control Request (RC/CCC)
   RIC Control Acknowledge / Failure   ------------>

E2 Setup
--------

``E2Termination::Start()`` connects to the RIC and sends the E2 Setup Request with
the RAN function definitions registered by ``E2TermHelper``:

.. list-table::
   :header-rows: 1
   :widths: 12 22 30 36

   * - ID
     - Short name
     - OID
     - Content
   * - 200
     - ``ORAN-E2SM-KPM``
     - ``1.3.6.1.4.1.53148.1.3.2.2``
     - Event trigger style 1 (periodic); report styles 1 and 4 (:doc:`kpm`)
   * - 300
     - ``ORAN-E2SM-RC``
     - ``1.3.6.1.4.1.53148.1.3.2.3``
     - Control style 2 "Radio Resource Allocation Control", action 6
       "Slice-level PRB quota" (:doc:`rc`)
   * - 4
     - ``ORAN-E2SM-CCC``
     - ``1.3.6.1.4.1.53148.1.6.2.4``
     - JSON RAN function definition (:doc:`ccc`)

The gNB opens its SCTP association from local port ``E2LocalPort + cellId``
(default 38470 + cell ID). The socket is bound to the local address of the route to
the RIC, so the SCTP INIT does not announce unreachable addresses (for example
Docker or VPN interfaces).

Reconnection
^^^^^^^^^^^^

Each gNB keeps trying to reach the RIC while the simulation runs. A new attempt
(SCTP connect + E2 Setup Request) starts ``ReconnectInterval`` after any of these
events:

- ``connect()`` fails (RIC not reachable, port closed);
- the RIC answers with E2 Setup Failure;
- no E2 Setup answer arrives within ``SetupTimeout``;
- the RIC closes or resets the association, or a send/receive on it fails.

The typical case is a simulation stopped and started again: the RIC may still hold
the old association and reject or reset the new one for a while; the gNB connects as
soon as the RIC accepts it. Socket errors no longer terminate the ns-3 process (the
e2sim ``sctp_receive_data()``/``sctp_send_data()`` functions call ``exit(1)``; NORI
uses its own receive and send).

The O-RAN SC ``e2mgr`` (checked in 6.0.4) rejects an E2 Setup Request that arrives
less than 10 s after the last disconnection of the same node ("nodeB entity
disconnection in progress"), and the rejection itself is recorded as a new
disconnection. With a retry interval below 10 s the node is rejected forever; the
default of 15 s connects on the second attempt after a restart.

Both times are wall-clock times. While the association is down, KPM reports and CCC
indications are skipped. Subscriptions belong to the association on which they were
received: after a new E2 Setup the RIC subscribes again and the periodic reports of
the old subscriptions stop (log ``Stopping KPM reports of requestorId ...``).

.. list-table::
   :header-rows: 1
   :widths: 25 15 60

   * - ``E2TermHelper`` attribute
     - Default
     - Meaning
   * - ``ReconnectInterval``
     - 15 s
     - Wait before a new attempt. Zero makes a single attempt (previous behavior).
       Keep it above 10 s with the O-RAN SC RIC (see above).
   * - ``SetupTimeout``
     - 10 s
     - Maximum wait for the E2 Setup answer. Zero waits forever.

Several gNBs in one scenario
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Some E2 terminators delay the E2 Setup of associations that a single host opens at
the same instant. ``E2TermHelper`` has the ``StartStagger`` attribute: cell *N*
starts its E2 Setup at ``(N - 1) x StartStagger``. The default (0 s) starts all
gNBs together. ``nori-handover-scenario`` exposes it as ``--e2StartStagger``.

RIC Subscription
----------------

``E2Termination::ProcessRicSubscriptionRequest()`` runs on the SCTP thread:

* it reads the RIC request ID (requestor and instance) and the RAN function ID;
* it accepts the **first** action of type REPORT or INSERT and ignores the others;
* it sends the RIC Subscription Response immediately.

For KPM, ``E2Interface::FunctionServiceSubscriptionCallback()`` also decodes:

* ``reportingPeriod`` from the E2SM-KPM Event Trigger Definition Format 1;
* ``granulPeriod`` from the Action Definition (Formats 1 to 5).

The subscription is then queued and the first report is sent by the simulation
thread (see :doc:`architecture`).

RIC Indication
--------------

For each accepted KPM subscription, ``E2Interface::BuildAndSendReportMessage()`` runs
every reporting period and sends two RIC Indications with the subscription's request
ID, RAN function ID and action ID:

#. node-level message, E2SM-KPM Indication Message **Format 1** (report style 1);
#. UE-level message, E2SM-KPM Indication Message **Format 3** (report style 4).
   It is skipped when no UE is attached to the gNB.

The reporting period is the ``reportingPeriod`` of the subscription. When the
subscription does not carry a valid one, the ``E2Periodicity`` attribute of
``E2Interface`` is used (default 0.01 s) and NORI logs a warning.

.. note::

   NORI sends both formats and all measurements of :doc:`kpm` for every
   subscription. The action definition format and its measurement list are not used
   to filter the report. xApps must select the measurements they need.

RIC Control
-----------

Controls arrive on the SCTP thread:

* E2SM-RC (RAN function 300): ``E2Interface::ControlMessageReceivedCallback()``
  decodes the header and the message with ``RicControlMessage``. Invalid or
  unsupported requests are answered immediately with **RIC Control Failure**.
  Valid ones are queued.
* E2SM-CCC (RAN function 4): ``E2Interface::CccSubscriptionCallback()`` handles both
  subscriptions and controls of this RAN function. Valid controls are queued.

On the simulation thread, ``DrainSctpQueues()`` applies the quotas to the scheduler
and answers:

.. list-table::
   :header-rows: 1
   :widths: 25 35 40

   * - Service model
     - Answer
     - Fields
   * - E2SM-RC
     - RIC Control Acknowledge, or RIC Control Failure when the quotas cannot be
       applied (cause ``control-failed-to-execute``)
     - RIC request ID and RAN function ID copied from the request
   * - E2SM-CCC
     - RIC Control Acknowledge
     - RIC request ID (requestor and instance) and RAN function ID copied from the
       request

Routing of the acknowledge in the RIC
-------------------------------------

The OSC RIC routes ``RIC_CONTROL_ACK`` (12041) and ``RIC_CONTROL_FAILURE`` (12042)
by message type and, when configured, by subscription ID, which for these messages
is the ``ricInstanceID`` of the request. NORI copies the ``ricInstanceID`` of each
request into its answer. When two xApps send controls through the same RIC, each
one should use its own ``ricInstanceID`` and register the route
``12041|<ricInstanceID>`` in the routing manager; otherwise the acknowledge can be
delivered to the wrong xApp.
