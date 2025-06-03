# NORI - New Open RAN Interface (Module for ns-3)

**NORI** (New Open RAN Interface) is a module for the **ns-3** simulator that integrates the **NR 5G LENA** module with a **Near-RT RIC (Release I)** from the **O-RAN** architecture via the standardized **E2 interface**.

This project enables:
- Metric collection via **KPM Service Model**
- Network control using the **RAN Control Service Model**
- Integration with custom xApps
- Operation in simulated environments without modifying the ns-3 core

## 📘 Available Blueprints

To simplify the setup and experimentation with the **NORI** module, we provide **blueprints** with pre-configured environments, including **ns-3**, the **NR 5G LENA** module, **NORI**, and the necessary **xApps**.

These blueprints are ideal for quick testing, prototyping, and hands-on learning with the RIC stack.

▶️ Access the full documentation in the project’s **Wiki**:
[https://github.com/lasseufpa/nori/wiki](https://github.com/lasseufpa/nori/wiki)

> 💡 **However, if you prefer to test the module in a different environment**, just follow the steps below to manually install the required components and run the examples directly on your system.

---

## 📦 Requirements

Recommended OS: **Ubuntu 20.04.6 LTS**

### E2Sim dependencies

```bash
sudo apt-get install -y build-essential git cmake libsctp-dev autoconf automake libtool bison flex libboost-all-dev
````

### ns-3 dependencies

```bash
sudo apt install -y git gcc python3 cmake g++
```

### NR 5G LENA dependencies

```bash
sudo apt install -y libc6-dev sqlite sqlite3 libsqlite3-dev libeigen3-dev
```

### ⚠️ GCC/G++ 11 Required

This project requires **GCC/G++ version 11**. To install on Ubuntu 20.04:

```bash
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt update
sudo apt install gcc-11 g++-11
```

Set GCC 11 as default:

```bash
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-9 10
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-11 20
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-9 10
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-11 20
```

---

## 🧩 Installing Components

### 1. E2Sim (Lacava’s fork with modified ASN1c)

```bash
git clone https://github.com/lasseufpa/e2sim.git
cd e2sim/e2sim/
mkdir build
./build_e2sim.sh 3
cd ../..
```

### 2. ns-3

```bash
git clone https://gitlab.com/nsnam/ns-3-dev.git
cd ns-3-dev
git checkout -b ns-3.42 ns-3.42
```

### 3. NR 5G LENA

```bash
git clone https://gitlab.com/cttc-lena/nr.git
```

### 4. NORI

```bash
git clone https://github.com/lasseufpa/nori.git
```

---

## 🔧 Building ns-3

From the `ns-3-dev` directory:

```bash
./ns3 configure --enable-examples
./ns3 build -j 2
```

---

## 🚀 Running Examples

Two example scenarios are provided to demonstrate the core functionality of the **NORI** module:

### 1. `nori-sample`

Simulates one **gNB** and one **UE**, with UDP traffic and KPM metrics enabled.

```bash
./ns3 run nori-sample -- --IpE2TermRic="YOUR_E2TERM_IP"
```

> 🔎 You can get the E2Term pod IP with:

```bash
kubectl get pods -A -o wide
```

To view the E2Term logs:

```bash
kubectl logs deployment-ricplt-e2term-alpha-XYZ -n ricplt
```

### 2. `nori-mimo-demo`

A variation of the sample with **MIMO** (multiple antennas) support.

```bash
./ns3 run nori-mimo-demo -- --IpE2TermRic="YOUR_E2TERM_IP"
```

---

## 🧠 Tips

* Ensure the **RIC stack** is running before executing ns-3 examples.
* The ns-3 terminal will show **E2-SETUP** and **RIC Indication** messages when working correctly.

---

## 📬 Contact
* Technical contact: [andrey.oliveira@itec.ufpa.br](mailto:andrey.oliveira@itec.ufpa.br)
