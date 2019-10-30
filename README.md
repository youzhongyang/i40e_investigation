# i40e_investigation
<h2>Summary</h2>

This repo is created for investigating a performance issue observed on servers using i40e cards. See <a href="https://github.com/joyent/illumos-joyent/issues/197">Issue 197</a> for the initial report.

<h2>Reproduction Steps</h2>

1. The server has one aggregated interface created over 4 i40e instances. Two additional VNICs are created over aggr0.
```
# dladm show-aggr -L
LINK        PORT         AGGREGATABLE SYNC COLL DIST DEFAULTED EXPIRED
aggr0       i40e0        yes          yes  yes  yes  no        no
--          i40e1        yes          yes  yes  yes  no        no
--          i40e2        yes          yes  yes  yes  no        no
--          i40e3        yes          yes  yes  yes  no        no

# dladm show-vnic
LINK         OVER       SPEED MACADDRESS        MACADDRTYPE VID  ZONE
admin0       aggr0      10000 2:8:20:43:4e:9c   random      0    --
admin1       aggr0      10000 2:8:20:33:31:10   random      0    --

# ipadm show-addr
ADDROBJ           TYPE     STATE        ADDR
lo0/v4            static   ok           127.0.0.1/8
aggr0/_a          static   ok           172.30.115.32/24
admin0/_a         static   ok           172.30.115.36/24
admin1/_a         static   ok           172.30.115.47/24
lo0/v6            static   ok           ::1/128
```

2. Build the source programs tcp-client.cpp and tcp-server.cpp.

3. On the server run 'tcp-server'.

4. On a client host, either Linux or illumos, connect to one of the IP addresses.

```
# /var/tmp/tcp-client 172.30.115.32
initial request size = 65652
request size = 131188
CONNECTED
503485 K in 1001 ms
466076 K in 1001 ms
485933 K in 1001 ms
504638 K in 1001 ms
Too fast, retry: 1
CONNECTED
975198 K in 1001 ms
466716 K in 1001 ms
456851 K in 1001 ms
451343 K in 1001 ms
Too fast, retry: 2
CONNECTED
862202 K in 1001 ms
410987 K in 1001 ms
412012 K in 1001 ms
412012 K in 1001 ms
Too fast, retry: 3
^C

# /var/tmp/tcp-client 172.30.115.47
initial request size = 65652
request size = 131188
CONNECTED
3971 K in 1067 ms
896 K in 1647 ms
1921 K in 1260 ms
1665 K in 1250 ms
1281 K in 1300 ms
1921 K in 1260 ms
2562 K in 1239 ms
2434 K in 1250 ms
2306 K in 1620 ms
1921 K in 1260 ms
2177 K in 1240 ms
3330 K in 1240 ms
1921 K in 1240 ms
^C

# /var/tmp/tcp-client 172.30.115.36
initial request size = 65652
request size = 131188
CONNECTED
2690 K in 1004 ms
1921 K in 1290 ms
2177 K in 1229 ms
2306 K in 1250 ms
1921 K in 1240 ms
2306 K in 1230 ms
2306 K in 1230 ms
1665 K in 1230 ms
2177 K in 1230 ms
2049 K in 1230 ms
2177 K in 1250 ms
2818 K in 1230 ms
2562 K in 1230 ms
2434 K in 1250 ms
^C
```

<h2>Observation</h2>

It is about 100 times slower when sending data to the server through a VNIC. By observing the pcap files (see server-capture.pcap and client-capture.pcapng), it's obvious that there is around 300ms delay of some incoming packets, and these packets happen to be near the end of a series of tcp segments. The delayed receiving of these packets causes the client to retransmit.

Based on my understanding of the i40e driver in illumos, the aggr0 interface uses the default VSI (and thus the default, first rx group). It seems for some reason the default VSI is more efficient than others.
