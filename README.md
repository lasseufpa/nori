# NORI - New Open RAN Interface (Module for ns-3)

**NORI** (New Open RAN Interface) is a module for the **ns-3** simulator that integrates the **NR 5G LENA** module with a **Near-RT RIC (Release I)** from the **O-RAN** architecture via the standardized **E2 interface**.

This project enables:
- Metric collection via **KPM Service Model (E2SM-KPM v3.00)**
- Network control and dynamic slicing via **RAN Control Service Model (E2SM-RC v3.01)**
- Integration with custom xApps
- Operation in simulated environments without modifying the ns-3 core

---

## Getting Started with Docker

NORI is containerized with **Docker** and **Docker Compose**, providing a pre-configured environment with all necessary toolchains (GCC-11, Nokia ASN1C, E2AP/E2SM-KPM/E2SM-RC v3.01 ASN.1 compilation, and patched `e2sim-dev`).

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

## 📬 Contact
* Technical contact: [andrey.oliveira@itec.ufpa.br](mailto:andrey.oliveira@itec.ufpa.br)
