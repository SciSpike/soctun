# soctun

## Why

`soctun` was created to overcome Docker for Mac's known issue: [i can't ping my containers](https://docs.docker.com/docker-for-mac/networking/#/i-can-t-ping-my-containers). 

## How

We need to create a tunnel from your local Mac to a Docker network. This is done by running [socat](http://www.dest-unreach.org/socat/) in the Docker container which exposes a tunneling port. Then `soctun` runs on your local Mac which connects to the published port on Docker for Mac. You will then have a tunnel to the Docker network.


```ditaa
+-------------------------------------------------------------------------------+
|-|Your Mac|--------------------------------------------------------------------|
+-------------------------------------------------------------------------------+
|                                                                               |
| +-------------------------------------+   +--------------------------------+  |
| | Terminal                            |   | Docker for Mac                 |  |
| +-------------------------------------+   +--------------------------------+  |
| |$ ping 172.18.0.9                    |   |  +--------------------------+  |  |
| |64 bytes from 172.18.0.9: time=.90 ms|   |  | Docker network           |  |  |
| |                                     |   |  +--------------------------+  |  |
| |                                     |   |  | subnet 172.18/16         |  |  |
| +-------------------------------------+   |  |                          |  |  |
|                                           |  |  +-------------------+   |  |  |
|                                           |  |  | vpn_container     |   |  |  |
|                                           |  |  +-------------------+   |  |  |
|                                           |  |  |-----------------+ |   |  |  |
+-------------------+                       |  |  || eth0 172.18.0.2| |   |  |  |
|| en0 10.0.0.11    |                       |  |  |-----------------+ |   |  |  |
|-------------------+                       |  |  |-----------------+ |   |  |  |
|-------------------+                       |  |  || tun0 172.18.0.2| |   |  |  |
|| utun0 172.18.1.1 |                       |  |  +--------+--------+ |   |  |  |
+----------^--------+                       |  |  |        |          |   |  |  |
|          |                                |  |  | +------+--------+ |   |  |  |
| +--------+--------+                       |  |  | | socat         | |   |  |  |
| | soctun          |                       |  |  | +---------------+ |   |  |  |
| +-----------------+                       |  |  | | TCP:4444 TUN  | |   |  |  |
| | connect:TCP:RAND|                       |  |  | +------^--------+ |   |  |  |
| | create utun0    |                       |  |  +-------------------+   |  |  |
| +--------+--------+                       |  |           |              |  |  |
|          |                                |  +--------------------------+  |  |
|          |                                |              |                 |  |
|          |                                +  +-----------+----------+      |  |
|          +--------+0.0.0.0:RANDOM+-------> <-+ publish              |      |  |
|                                           +  +----------------------+      |  |
|                                           |  | vpn RANDOM:4444      |      |  |
|                                           |  +----------------------+      |  |
|                                           +--------------------------------+  |
+-------------------------------------------------------------------------------+


```

## Install

```
curl -L https://github.com/SciSpike/soctun/releases/download/${SOCTUN_VERSION}/soctun.tar.gz > /tmp/soctun-${SOCTUN_VERSION}.tar.gz
tar xf /tmp/soctun-${SOCTUN_VERSION}.tar.gz
mv /tmp/soctun/soctun bin/soctun
```

## Build

Make sure you have Xcode Developer Tools installed.

```
make soctun
```

## Example

Add this to your `docker-compose.yml`.

```docker-compose.yml
services:
  vpn:
    privileged: true
    ports:
      - 4444
    image: bobrik/socat
    command: -d TCP-LISTEN:4444,fork TUN
```
Assuming docker-compose chose 172.18 subnet, 172.18.0.2 for vpn service, and mapped 4444 to 32776.

```shell
sudo bin/soctun -t 0 -h 0.0.0.0 -p 32776 -m 1500 -n
sudo ifconfig utun0 inet 172.18.1.1 172.18.0.2 mtu 1500 up # setup the link
docker exec -i test_vpn_1 sh -c 'ip link set tun0 up && ip link set mtu 1500 tun0 && ip addr add 172.18.0.2/16 peer 172.18.1.1 dev tun0 && arp -sD 172.18.1.1 eth0 pub' # setup remote link
sudo route -n add 172.18.0.0 -interface utun0
```

You now have direct access to your Docker containers.

```shell
ping 172.18.0.9 #assuming .9 goes somewhere
```

## General solution

It is possible that this tool can, or could be adapted to, be a more general socket/tunneling solution.
