<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="doc/source/_static/logo/nori-logo-dark.png">
    <img src="doc/source/_static/logo/nori-logo-light.png" alt="NORI" width="380">
  </picture>
</p>

<p align="center">
  <b>O-RAN E2 interface for ns-3 and 5G-LENA</b><br>
  Connect simulated 5G NR cells to a Near-RT RIC and test real xApps against them.
</p>

<p align="center">
  <a href="https://github.com/lasseufpa/nori/actions/workflows/ci.yml"><img src="https://github.com/lasseufpa/nori/actions/workflows/ci.yml/badge.svg?branch=main" alt="ci"></a>
  <a href="https://github.com/lasseufpa/nori/actions/workflows/docs.yml"><img src="https://github.com/lasseufpa/nori/actions/workflows/docs.yml/badge.svg?branch=main" alt="docs"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-GPL--2.0--only-blue" alt="License: GPL-2.0-only"></a>
  <img src="https://img.shields.io/badge/ns--3-3.48-2B5240" alt="ns-3.48">
  <img src="https://img.shields.io/badge/nr%20(5G--LENA)-v5.1-2B5240" alt="nr v5.1">
  <img src="https://img.shields.io/badge/O--RAN%20SC-Release%20I-4E9A31" alt="O-RAN SC Release I">
</p>

<p align="center">
  <a href="https://lasseufpa.github.io/nori/">Manual</a> ·
  <a href="https://lasseufpa.github.io/nori/api/">API reference</a> ·
  <a href="#quick-start-with-docker">Quick start</a> ·
  <a href="#examples">Examples</a> ·
  <a href="CHANGES.md">Changes</a> ·
  <a href="https://github.com/lasseufpa/nori/wiki">Wiki</a>
</p>

---

**NORI** (New Open RAN Interface) is an [ns-3](https://www.nsnam.org) contrib module. It adds an
O-RAN **E2 termination** to the gNBs of the [nr (5G-LENA)](https://gitlab.com/cttc-lena/nr) module,
so a simulated network talks E2AP over SCTP to a real **Near-RT RIC** (O-RAN SC Release I): xApps
receive measurements from the simulation and send controls back, without changes to the ns-3 core.

## What you get

| Service model | RAN function ID | What the xApp sees or controls |
|---|:---:|---|
| **E2SM-KPM** v03.00 | 200 | Per-UE and per-cell measurements: throughput, PDCP volume, PRB usage and more, at the granularity period the xApp requests |
| **E2SM-RC** v03.01 | 300 | Slice PRB quotas (Control Style 2, Action 6, RAN parameters 1–13), answered with RIC Control Acknowledge or Failure |
| **E2SM-CCC** v07.00 | 4 | Cell configuration in JSON: `O-RRMPolicyRatio` slice quotas and MIMO rank limit |

It also brings a slice-aware MAC scheduler (`NrRLMacSchedulerOfdma`), multi-cell scenarios with X2
handover, and runs either in simulated time or synchronized with the wall clock for live RIC tests.

## How it works

The [manual](https://lasseufpa.github.io/nori/) describes the E2AP procedures, each KPM measurement
and how RC and CCC controls reach the scheduler.

## Compatibility

Each NORI release is tested against one ns-3 release and one nr version.

| NORI | ns-3 | nr (5G-LENA) | E2AP / E2SM-KPM / E2SM-RC |
|---|---|---|---|
| **v3.00** | ns-3.48 | v5.1 | v03.01 / v03.00 / v03.01 |
| v2.00 | ns-3.42 | not pinned | — |
| v1.00 | ns-3.42 | not pinned | — |

> [!NOTE]
> Between v2.00 and v3.00, `main` used ns-3.42 with nr `78b7179e`. Results from that environment
> and from v3.00 are not directly comparable: nr v4.0 to v5.1 changed the RLC, HARQ, TB size,
> MIMO SINR and scheduler models.

## Quick start with Docker

The Docker environment builds every dependency (Ubuntu 24.04, GCC 13, Nokia `asn1c`, the patched
O-RAN SC `e2sim` and the ns-3/nr versions above) and mounts this repository at
`ns-3-dev/contrib/nori`, so you edit on the host and build in the container.

```bash
git clone https://github.com/lasseufpa/nori.git && cd nori
docker compose up -d                 # builds the image nori:dev on the first run
docker compose exec nori-dev bash

# inside the container (/home/developer/ns-3-dev)
./ns3 configure --enable-examples --enable-modules=nori
./ns3 build
./ns3 run "nori-testbed-scenario --simTime=5"     # runs without a RIC
```

<details>
<summary><b>Optimized standalone image</b> (<code>-d optimized</code>, for long runs)</summary>

```bash
docker build -t nori .
docker run -it --name nori --network host nori bash
./ns3 run "nori-rc-slicing-demo --ipE2TermRic=<E2TERM_IP>"
```

The image copies the working tree and builds every module with `-O3`. Release environments are
also published as `ghcr.io/lasseufpa/nori:X.YY-dev`; the image labels `org.nsnam.ns3.version`
and `org.cttc-lena.nr.version` record the versions inside.
</details>

## Installing as an ns-3 module

NORI follows the ns-3 contrib layout: clone it into `contrib/` of an ns-3 tree that has nr.

```bash
git clone https://gitlab.com/nsnam/ns-3-dev.git
cd ns-3-dev && git checkout ns-3.48
git clone https://gitlab.com/cttc-lena/nr.git contrib/nr && git -C contrib/nr checkout v5.1
git clone https://github.com/lasseufpa/nori.git contrib/nori

./ns3 configure --enable-examples --enable-modules=nori
./ns3 build
./ns3 run nori-print-attributes
```

<details>
<summary><b>Requirements</b></summary>

- C++23 compiler (GCC 13), CMake ≥ 3.25, Python ≥ 3.10
- `libsctp-dev`, Boost, Eigen 3, nlohmann-json
- The O-RAN SC `e2sim` library (`/usr/local/lib/libe2sim.a`) compiled with the ASN.1 files of
  `asn/v03`, the Nokia `asn1c` and a patch for CHOICE extensions. Stages 2 and 4 of the
  [`Dockerfile`](Dockerfile) show the commands.
</details>

## Examples

| Example | What it shows | Needs a RIC |
|---|---|:---:|
| `nori-testbed-scenario` | Copy of the LASSE OCUDU testbed: 1 gNB (n78, 100 MHz), 3 UEs, 3 slices, KPM + RC + CCC | optional (`--enableE2`) |
| `nori-rc-slicing-demo` | 2 saturated slices; PRB quotas from an E2SM-RC xApp | yes |
| `nori-ccc-slicing-demo` | Same topology; PRB quotas through E2SM-CCC | yes |
| `nori-handover-scenario` | 3 gNBs, 2 moving UEs, X2 handover seen by a KPM xApp | optional (`--enableE2`) |
| `nori-benchmark-scenario` | Scalable UMi topology for execution-time measurements | optional (`--enableE2`) |
| `nori-sample` | Minimal KPM example: 1 gNB, 1 UE | yes |
| `nori-mimo-demo` | MIMO; the E2SM-CCC `maxMimoLayers` changes the rank limit | yes |
| `nori-print-attributes` | Prints the NORI attributes as reStructuredText for the manual | no |

Examples that talk to a RIC take `--ipE2TermRic` and `--ipE2TermRicPort`; `--PrintHelp` lists every
option.

```bash
./ns3 run "nori-rc-slicing-demo --ipE2TermRic=<E2TERM_IP> --simTime=0"   # 0 = until Ctrl+C
```

> [!TIP]
> Find the E2Term address with `kubectl get pods -A -o wide` and follow its log with
> `kubectl logs -n ricplt deployment-ricplt-e2term-alpha-<id>`. When the link is up, the ns-3 output
> shows E2 Setup, RIC Subscription, RIC Indication and RIC Control messages.

Ready-made environments with ns-3, nr, NORI and sample xApps (**blueprints**) are described in the
[project wiki](https://github.com/lasseufpa/nori/wiki).

## Documentation and development

- **Manual** (Sphinx): <https://lasseufpa.github.io/nori/> · **API** (Doxygen): <https://lasseufpa.github.io/nori/api/>
- **Changes**: [CHANGES.md](CHANGES.md) lists API and behavior changes; [RELEASE_NOTES.md](RELEASE_NOTES.md) lists features, fixes and known issues.

<details>
<summary><b>Build the documentation locally</b></summary>

```bash
python3 -m pip install -r doc/requirements.txt
make -C doc html       # doc/build/html
make -C doc doxygen    # doc/build/doxygen/html (needs doxygen and graphviz)
```
</details>

<details>
<summary><b>Continuous integration</b></summary>

| Workflow | Trigger | What it does |
|---|---|---|
| `ci` | push to `main`, pull requests | Builds `libnori` and the examples in the CI image and runs the examples that work without a RIC for 2 simulated seconds |
| `image` | changes to `Dockerfile` or `asn/`, release tags `nori-v*` | Builds the `ci` and `dev` targets of the Dockerfile and publishes `ghcr.io/lasseufpa/nori:ci`, `:dev` and `:X.YY-dev` |
| `docs` | changes to `doc/`, `model/`, `helper/` | Builds the manual and the API reference; publishes on GitHub Pages from `main` |

The same scripts run in the dev container (they reconfigure ns-3 with `-d default --enable-modules=nori`):

```bash
docker compose run --rm nori-dev bash -c \
  "contrib/nori/.github/scripts/ci-build.sh && contrib/nori/.github/scripts/smoke.sh"
```
</details>

## License

NORI is licensed under the GNU General Public License, version 2 only (`GPL-2.0-only`), the same
license as ns-3 and nr; see [LICENSE](LICENSE). The exception is `model/e2sim-mod.h`, adapted from the
O-RAN SC `e2sim` and licensed under Apache-2.0 ([LICENSES/Apache-2.0.txt](LICENSES/Apache-2.0.txt)).
Code derived from ns-O-RAN (Northeastern University) and nr (CTTC), external dependencies and the
origin of the O-RAN ASN.1 files are listed in [NOTICE.md](NOTICE.md).

## Contact

Developed at LASSE, Federal University of Pará (UFPA).
Technical contact: andrey.oliveira@itec.ufpa.br
