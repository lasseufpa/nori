# NORI - New Open RAN Interface (Module for ns-3)

[![ci](https://github.com/lasseufpa/nori/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/lasseufpa/nori/actions/workflows/ci.yml)
[![docs](https://github.com/lasseufpa/nori/actions/workflows/docs.yml/badge.svg?branch=main)](https://github.com/lasseufpa/nori/actions/workflows/docs.yml)

**NORI** (New Open RAN Interface) is a module for the **ns-3** simulator that integrates the **NR 5G LENA** module with a **Near-RT RIC (Release I)** from the **O-RAN** architecture via the standardized **E2 interface**.

This project enables:
- Metric collection via **KPM Service Model (E2SM-KPM v3.00)**
- Network control and dynamic slicing via **RAN Control Service Model (E2SM-RC v3.01)**
- Integration with custom xApps
- Operation in simulated environments without modifying the ns-3 core

## Compatibility

NORI is an ns-3 contrib module (`ns-3-dev/contrib/nori`). Each NORI release is tested
against one ns-3 release and one nr (5G-LENA) version:

| NORI | ns-3 | nr (5G-LENA) | E2AP / E2SM-KPM / E2SM-RC |
|---|---|---|---|
| **v3.00** | ns-3.48 | v5.1 | v03.01 / v03.00 / v03.01 |
| v2.00 | ns-3.42 | not pinned | — |
| v1.00 | ns-3.42 | not pinned | — |

Between v2.00 and v3.00, `main` used ns-3.42 with nr `78b7179e`. Results obtained with
that environment and with v3.00 are not directly comparable: nr v4.0 to v5.1 changed
the RLC, HARQ, TB size, MIMO SINR and scheduler models.

---

## Documentation

- User manual (Sphinx): architecture, E2AP procedures, E2SM-KPM measurements, E2SM-RC and E2SM-CCC controls, slicing scheduler, examples and validation.
- API reference (Doxygen).

Published by the `docs` workflow at [https://lasseufpa.github.io/nori/](https://lasseufpa.github.io/nori/) (manual) and [https://lasseufpa.github.io/nori/api/](https://lasseufpa.github.io/nori/api/) (API). To build locally:

```bash
python3 -m pip install -r doc/requirements.txt
make -C doc html       # doc/build/html
make -C doc doxygen    # doc/build/doxygen/html (needs doxygen and graphviz)
```

API and behavior changes are listed in [CHANGES.md](CHANGES.md); features, fixes and known issues in [RELEASE_NOTES.md](RELEASE_NOTES.md).

## Continuous Integration

| Workflow | Trigger | What it does |
|---|---|---|
| `ci` | push to `main`, pull requests | Builds `libnori` and the examples in the CI image and runs the examples that work without a RIC for 2 simulated seconds |
| `image` | changes to `Dockerfile` or `asn/`, release tags `nori-v*` | Builds the `ci` and `dev` targets of the Dockerfile and publishes `ghcr.io/lasseufpa/nori:ci`, `:dev` and `:X.YY-dev` |
| `docs` | changes to `doc/`, `model/`, `helper/` | Builds the manual and the API reference; publishes on GitHub Pages from `main` |

The same scripts run locally in the dev container (they reconfigure ns-3 with `-d default --enable-modules=nori`):

```bash
docker compose run --rm nori-dev bash -c \
  "contrib/nori/.github/scripts/ci-build.sh && contrib/nori/.github/scripts/smoke.sh"
```

---

## Installing NORI as an ns-3 module

NORI follows the ns-3 contrib module layout: clone it into `contrib/` of an ns-3 tree
that has the nr module, then enable it with `./ns3 configure`.

```bash
# 1. ns-3 and nr (versions from the Compatibility table)
git clone https://gitlab.com/nsnam/ns-3-dev.git
cd ns-3-dev && git checkout ns-3.48
cd contrib
git clone https://gitlab.com/cttc-lena/nr.git && git -C nr checkout v5.1

# 2. NORI
git clone https://github.com/lasseufpa/nori.git
cd ..

# 3. Build and check
./ns3 configure --enable-examples --enable-modules=nori
./ns3 build
./ns3 run nori-print-attributes
```

Requirements: a C++23 compiler (GCC 13), CMake >= 3.25, Python >= 3.10, `libsctp-dev`,
Boost, Eigen 3 and nlohmann-json. NORI also links the O-RAN SC `e2sim` library
(`/usr/local/lib/libe2sim.a`), compiled with the ASN.1 files of `asn/v03`, the Nokia
`asn1c` and a patch for CHOICE extensions; stages 2 and 4 of the `Dockerfile` show the
commands. The Docker environment below builds all of it.

---

## Getting Started with Docker

NORI is containerized with **Docker** and **Docker Compose**, providing a pre-configured environment with all necessary toolchains (Ubuntu 24.04, GCC 13, CMake 3.28, Nokia ASN1C, E2AP/E2SM-KPM/E2SM-RC v3.01 ASN.1 compilation, and patched `e2sim-dev`).

The image is `nori:dev` (ns-3 and nr versions as in the Compatibility table, recorded in
the image labels `org.nsnam.ns3.version` and `org.cttc-lena.nr.version`). Release
environments are also published as `ghcr.io/lasseufpa/nori:X.YY-dev`. The previous
environment (ns-3.42, nr `78b7179e`) is kept locally as `nori-nori-dev:ns3.42-nr78b7179e`.

### Option 1: Development Mode with Live Code Mounting (Recommended)
Edit source files directly on your host machine while building and running inside Docker in real-time:

```bash
# Start the container with host volume mounted into ns-3-dev/contrib/nori
docker compose up -d

# Open a shell inside the container
docker compose exec nori-dev bash

# Inside the container:
./ns3 configure --enable-examples
./ns3 build
./ns3 run nori-rc-slicing-demo -- --simTime=0
```

### Option 2: High-Performance Standalone Image (`-d optimized`)
Builds a fully self-contained Docker image with `-d optimized` (`-O3` compiler optimizations) using your local working tree for maximum simulation execution speed:

```bash
# Build the standalone image
docker build -t nori .

# Run the container
docker run -it --name nori --network host nori bash

# Inside the container:
./ns3 run nori-rc-slicing-demo -- --simTime=0
```

---

## Running Examples

Example scenarios are provided to demonstrate the core functionality of the **NORI** module:

### 1. `nori-rc-slicing-demo` (E2SM-RC v3.01 Dynamic Slicing)

Simulates 2 network slices (eMBB on SST=1, URLLC on SST=2) with real-time PRB quota adjustments received from a Near-RT RIC xApp via E2SM-RC v3.01:

```bash
./ns3 run nori-rc-slicing-demo -- --ipE2TermRic="YOUR_E2TERM_IP" --simTime=0
```

### 2. `nori-sample` (E2SM-KPM v3.00 Telemetry)

Simulates one **gNB** and one **UE**, with UDP traffic and KPM metric reporting enabled.

```bash
./ns3 run nori-sample -- --ipE2TermRic="YOUR_E2TERM_IP"
```

> You can find the E2Term pod IP in your Kubernetes cluster with:
> ```bash
> kubectl get pods -A -o wide
> ```
> To follow the E2Term logs:
> ```bash
> kubectl logs deployment-ricplt-e2term-alpha-XYZ -n ricplt
> ```

### 3. `nori-mimo-demo`

A variation of the sample scenario with **MIMO** (multiple antennas) support.

```bash
./ns3 run nori-mimo-demo -- --IpE2TermRic="YOUR_E2TERM_IP"
```

---

## Available Blueprints

To simplify the setup and experimentation with the **NORI** module, we provide pre-configured **blueprints** including **ns-3**, **NR 5G LENA**, **NORI**, and sample **xApps**:

Access the full documentation in the project’s **Wiki**:
[https://github.com/lasseufpa/nori/wiki](https://github.com/lasseufpa/nori/wiki)

---

## Tips

* Ensure the **Near-RT RIC stack** is up and running before executing ns-3 examples.
* The ns-3 terminal output will log **E2-SETUP**, **RIC Subscription**, **RIC Indication**, and **RIC Control** messages upon successful operation.

---

## License

NORI is licensed under the GNU General Public License, version 2 only (`GPL-2.0-only`), the same license as ns-3 and nr. See [LICENSE](LICENSE). The exception is `model/e2sim-mod.h`, adapted from the O-RAN SC `e2sim` and licensed under Apache-2.0 ([LICENSES/Apache-2.0.txt](LICENSES/Apache-2.0.txt)). Code derived from ns-O-RAN (Northeastern University) and nr (CTTC), external dependencies with other licenses (O-RAN SC `e2sim`, Apache-2.0) and the origin of the O-RAN ASN.1 files are listed in [NOTICE.md](NOTICE.md).

---

## 📬 Contact
* Technical contact: [andrey.oliveira@itec.ufpa.br](mailto:andrey.oliveira@itec.ufpa.br)
