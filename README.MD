For testing, flash and run each target, and run these commands in their serial consoles:

| Command | Explanation | Example |
|---|---|---|
| `txpower`| Set the transmitting power (either 2, 5 or 7 dBm) | `txpower 7` |
| `dur <t>` | Set the test duration (in seconds). | `dur 20` |
| `server <udp\tcp>` | Start the UDP iperf server. Needs to be restarted after each test | `server udp` |
| `test udp <rate>/tcp>` | Send traffic in either udp or tcp. In case of UDP, give target rate for example "20M" to try to reach 20 Mbps | `test udp 20M` |