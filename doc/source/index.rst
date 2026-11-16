NORI Manual
===========

NORI (New Open RAN Interface) is an ns-3 module that connects gNBs simulated with
the 5G-LENA (``nr``) module to a Near-RT RIC through the O-RAN E2 interface. xApps
running on a real RIC (O-RAN Software Community, Release I) receive E2SM-KPM
measurements from the simulation and send E2SM-RC and E2SM-CCC controls back to it.

This manual describes the model, the supported service models and how to run the
examples. The API reference is generated with Doxygen (see :doc:`user-guide`).

.. toctree::
   :maxdepth: 2
   :numbered:
   :caption: Contents

   overview
   architecture
   e2ap
   kpm
   rc
   ccc
   scheduler
   user-guide
   examples
   validation
   attributes
