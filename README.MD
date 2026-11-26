For testing, flash and run each target, and run these commands in their serial consoles:

| Target | Command |
| :--- | :--- |
| AP | `iperf -s -i 1` |
| Station | `iperf -c 192.168.4.1 -t 60 -i 1` |
| Station reverse direction | `iperf -c 192.168.4.1 -t 60 -i 1 -R` |

Change `i` value for changing report interval in seconds, change `t` value to change test length.