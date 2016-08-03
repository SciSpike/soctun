# soctun

## how to use with Docker for Mac

add something like this to your docker-compose.yml

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
sudo bin/soctun -t 17218 -h 0.0.0.0 -p 32776 -m 1500 -n
sudo ifconfig utun17218 inet 172.18.1.1 172.18.0.2 mtu 1500 up # setup the link
docker exec -i test_vpn_1 sh -c 'ip link set tun0 up && ip link set mtu 1500 tun0 && ip addr add 172.18.0.2/16 peer 172.18.1.1 dev tun0 && arp -sD  172.18.1.1 eth0 pub' # setup remote link
sudo route -n add 172.18.0.0 -interface utun17218
```

now you have direct access to your docker containers

```shell
ping 172.18.0.2
```
