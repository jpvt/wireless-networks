# Wireless Networks

Repository dedicated to the Wireless Networks course took at the Federal University of Paraíba. On this class, we created experiments using ns-3. The project's structure is based on [Alejandro Gomez's Network Simulator](https://github.com/alejandrogomez314/network-simulator). The experiments are implemented in the scenarios directory, where each scenario has it's own docker-compose file to setup the necessary containers and volumes. The simulations can be run using scripts that sets up th required linux networking interfaceson host and connects them to the running containers and simulation.

## Table of Contents
- [Wireless Networks](#wireless-networks)
  - [Table of Contents](#table-of-contents)
  - [Requeriments](#requeriments)
  - [Installation](#installation)
  - [Scenarios](#scenarios)

## Requeriments

1. Linux or MacOS
   
   :warning: Warning: This installation was only tested on `Linux` (but should work fine for `MacOS` as well).

2. `docker` -  https://docs.docker.com/get-docker/

3. `docker compose` -  https://docs.docker.com/compose/install/

4. `git` installed by your distribution Linux or macOS (e.g. `HomeBrew`)

## Installation

1. Install [`docker`](https://docs.docker.com/get-docker/) and verify installation. Should be possible to run the `hello-world` image:

    ```console
    foor@bar# sudo docker run hello-world

    Hello from Docker!
    This message shows that your installation appears to be working correctly.

    To generate this message, Docker took the following steps:
    1. The Docker client contacted the Docker daemon.
    2. The Docker daemon pulled the "hello-world" image from the Docker Hub.
        (amd64)
    3. The Docker daemon created a new container from that image which runs the
        executable that produces the output you are currently reading.
    4. The Docker daemon streamed that output to the Docker client, which sent it
        to your terminal.

    To try something more ambitious, you can run an Ubuntu container with:
    $ docker run -it ubuntu bash

    Share images, automate workflows, and more with a free Docker ID:
    https://hub.docker.com/

    For more examples and ideas, visit:
    https://docs.docker.com/get-started/
    ```

2. Clone the repo. Example using SSH:

    ```console
    foo@bar# git clone git@github.com:jpvt/wireless-networks.git
    ```

3. Prepare the `hello_world.sh` script to run:

    ```console
    foo@bar# cd wireless-networks/scripts
    foo@bar# chmod a+x hello_world.sh
    ```

4. Run the `hello_world.sh`  script:

    :warning: Warning: On the first run this should take about ~22 minutes on a 300Mbps connection.

    ```console
    foo@bar# ./scripts/hello_world.sh

    ns3  | -- Configuring done
    ns3  | -- Generating done
    ns3  | -- Build files have been written to: /usr/local/ns-allinone-3.37/ns-3.37/cmake-cache
    ns3  | Scanning dependencies of target scratch_hello
    ns3  | [  0%] Building CXX object scratch/CMakeFiles/scratch_hello.dir/hello.cc.o
    ns3  | [  0%] Linking CXX executable ../../build/scratch/ns3.37-hello-default
    ns3  | At time +2s client sent 1024 bytes to 10.1.1.2 port 9
    ns3  | At time +2.00369s server received 1024 bytes from 10.1.1.1 port 49153
    ns3  | At time +2.00369s server sent 1024 bytes to 10.1.1.1 port 49153
    ns3  | At time +2.00737s client received 1024 bytes from 10.1.1.2 port 9
    ns3 exited with code 0
    ```

## Scenarios

In construction