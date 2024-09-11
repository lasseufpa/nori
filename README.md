# Quick Start

NORI(NEW OPEN RAN INTERFACE) é um modulo NS-3 que conecta o modulo NR a um interface e2 de um Near-RT RIC

# Requisitos

Sistema Ubuntu 20.04.6 LTS

Requisitos para o e2sim

```jsx
sudo apt-get install -y build-essential git cmake libsctp-dev autoconf automake libtool bison flex libboost-all-dev
```

Para ns-3 

```jsx
sudo apt install -y git gcc python3 cmake g++
```

Para o NR-5G Lena

```jsx
sudo apt install libc6-dev sqlite sqlite3 libsqlite3-dev libeigen3-dev
```

Para o projeto funcionar o gcc e o g++ 11 deve estar instalado, geralmente no Ubuntu 20.04 apenas a versão 9 está dinsponivel. 

Siga os passos para instalar a versão 11 do gcc e g++

```python
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt update
sudo apt install gcc-11 g++-11
```

```python
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-9 10
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-11 20

sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-9 10
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-11 20
```

# Instalar e2sim (Atualmente usando o E2sim do Lacava com o ASN1c modificado)

```jsx
git clone http://gitlab.lasse.ufpa.br/openran/e2sim.git
```

```jsx
cd e2sim/e2sim/
mkdir build
./build_e2sim.sh 3
```

```cpp
cd ../..
```

# Instalar ns-3

```jsx
git clone https://gitlab.com/nsnam/ns-3-dev.git
```

## Instalar NR 5G Lena

```jsx
git clone https://gitlab.com/cttc-lena/nr.git
cd ns-3-dev
git checkout -b ns-3.42 ns-3.42
```

# Instalar NORI

```jsx
git clone http://gitlab.lasse.ufpa.br/openran/nori.git
```

# Build ns-3

```jsx
./ns3 configure --enable-example
```

```jsx
./ns3 build -j 2
```

# Executando o exemplo

```jsx
./ns3 run E2-example
```

O log do e2term pode der obsevado com:

```jsx
kubectl logs deployment-ricplt-e2term-alpha-5dc768bcb7-ppcql -n ricplt
```
