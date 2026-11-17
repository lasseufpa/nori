# NORI notices

NORI is Copyright (c) 2024-2026 LASSE/UFPA and contributors, licensed under the
GNU General Public License, version 2 only (`GPL-2.0-only`); see [LICENSE](LICENSE).
Each source file states its copyright holders and license in an SPDX header.
All files are GPL-2.0-only except `model/e2sim-mod.h`, which is Apache-2.0.

## Code derived from other projects

| Project | Copyright | License | Files |
|---|---|---|---|
| ns-O-RAN E2 module ([o-ran-sc/sim-ns3-o-ran-e2](https://github.com/o-ran-sc/sim-ns3-o-ran-e2)) | Northeastern University | GPL-2.0-only | `model/oran-interface.*`, `model/asn1c-types.*`, `model/function-description.*`, `model/kpm-*`, `model/ric-control-*`, `helper/*indication-message-helper.*`, `examples/oran-interface-example.cc` |
| O-RAN SC `e2sim` ([sim/e2-interface](https://gerrit.o-ran-sc.org/r/sim/e2-interface), commit `da6f82f`) | AT&T Intellectual Property; Samsung Electronics | **Apache-2.0** (see [LICENSES/Apache-2.0.txt](LICENSES/Apache-2.0.txt)) | `model/e2sim-mod.h` (the file as a whole; two functions adapted from `encode_e2apv1.cpp`, modified by LASSE/UFPA) |
| nr / 5G-LENA ([cttc-lena/nr](https://gitlab.com/cttc-lena/nr)) | Centre Tecnològic de Telecomunicacions de Catalunya (CTTC) | GPL-2.0-only | `helper/E2-term-helper.*`, `model/nr-rl-mac-scheduler-ofdma.h`, `examples/nori-sample.cc`, `examples/nori-mimo-demo.cc`, `examples/nori-simple-rl-sched.cc` |

## External dependencies (not included in this repository)

| Component | License | How NORI uses it |
|---|---|---|
| [ns-3](https://gitlab.com/nsnam/ns-3-dev) | GPL-2.0-only | NORI is an ns-3 contrib module |
| [nr (5G-LENA)](https://gitlab.com/cttc-lena/nr) | GPL-2.0-only | ns-3 module linked by NORI |
| O-RAN SC `e2sim` ([sim/e2-interface](https://gerrit.o-ran-sc.org/r/sim/e2-interface), commit `da6f82f`) | Apache-2.0 | Downloaded and built by the `Dockerfile`; linked as `libe2sim.a` |
| [Nokia asn1c](https://github.com/nokia/asn1c) | BSD-2-Clause | Compiles the ASN.1 files into the e2sim library |
| [nlohmann/json](https://github.com/nlohmann/json) | MIT | E2SM-CCC JSON encoding |

The repository distributes source code only. Binaries that combine NORI with
`libe2sim.a` (for example the `final` target of the `Dockerfile`) combine
GPL-2.0-only and Apache-2.0 code; they are built by the user and are not
published by this project.

## O-RAN specification files

The files below are transcribed from O-RAN Alliance specifications. They remain
subject to the O-RAN Alliance terms and are included only to build the E2
encoders and decoders.

| File | Specification |
|---|---|
| `asn/v03/e2ap-*-v03.01.asn` | O-RAN WG3 E2AP v03.01 |
| `asn/v03/e2sm-v03.01.asn` | O-RAN WG3 E2SM v03.01 |
| `asn/v03/e2sm-kpm-v03.00.asn` | O-RAN WG3 E2SM-KPM v03.00 |
| `asn/v03/e2sm-rc-v03.01.asn` | O-RAN WG3 E2SM-RC v03.01 |
| `asn/E2SM-CCC-v07.00.json` | O-RAN WG3 E2SM-CCC v07.00 |
