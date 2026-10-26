# netbench

TCP throughput benchmark for the board's two network interfaces. Consists of
a tiny single-threaded TCP server (`netbench`, deployed to
`/usr/local/bin/netbench` on the board) and a stdlib-only Python client
(`netbench_client.py`) that runs unchanged on WSL and native Windows.

Protocol: client connects to port 5201, sends `W` then a byte stream (server
reads/discards and reports), or `R<len>\n` (server streams `<len>` bytes
back and reports).

## Build & run

```sh
. ./env.sh && cd apps/netbench
make deploy                       # cross-builds, installs, board side
ssh root@<board> 'nohup /usr/local/bin/netbench >/tmp/netbench.log 2>&1 &'
python3 netbench_client.py upload   <board> 5201 100
python3 netbench_client.py download <board> 5201 100
```

On Windows: `python netbench_client.py ...` (same script, native mDNS name
`stm32mp1-dk1.local` works from Windows).

## Topology during the recorded run

- PC <-> router: **Wi-Fi 6** (both PC Ethernet ports disconnected)
- board <-> router: GbE (end0, DHCP 192.168.5.165)
- board <-> PC: direct USB cable (gadget NCM), Windows adapter 192.168.7.210,
  board usb0 = 192.168.7.1

## Results — 100 MiB per transfer (except as noted)

| # | Board interface | Client        | Direction | Mbps (client) | Mbps (server) |
|---|-----------------|---------------|-----------|---------------|---------------|
| 1 | GbE via router  | WSL2          | upload    | 87.4          | 84.7          |
| 2 | GbE via router  | WSL2          | download  | 65.6          | 66.1          |
| 3 | GbE via router  | Windows       | upload    | 234.2         | 224.9         |
| 4 | GbE via router  | Windows       | download  | 330.7         | 331.9         |
| 5 | USB NCM direct  | WSL2          | upload    | 253.3 (20 MiB)| 217.2         |
| 6 | USB NCM direct  | Windows       | upload    | 226.4         | 222.0         |
| 7 | USB NCM direct  | Windows       | download  | 313.8         | 314.0         |

(USB download/upload from WSL2 also works — traffic is NAT-routed through
the Windows host.)

## Reading the numbers

- Rows 1-2 are limited by the **Wi-Fi path + WSL2 NAT**, not by the board's
  GbE: the same transfer from native Windows (rows 3-4) reaches
  ~230/330 Mbps, which is the Wi-Fi 6 link's practical ceiling on this
  setup.
- The **USB NCM gadget delivers ~220-315 Mbps** — in this topology on par
  with the Wi-Fi-routed GbE path, and far above the naive "USB 2.0 = 480
  Mbps theoretical / ~35 MB/s assumption" for this peripheral: the MP1 NCM
  gadget is fast.
- To measure the board's true GbE capability (~940 Mbps), connect the PC to
  the board with a direct Ethernet cable and use static IPs on both ends
  (PC has a 2.5 GbE Realtek port). The router/Wi-Fi path cannot exceed the
  wireless link.

## Board-side log

The server appends each session report to its stdout (start it with
`nohup ... >/tmp/netbench.log` to keep it).
