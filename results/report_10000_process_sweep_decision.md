# 10000x10000x10000 Process Sweep Evidence

All runs use `mpi_2d`, seed `2026`, `--tile 10000`, 3 WSL nodes, and the same matrix shape A(10000x10000) * B(10000x10000).

| processes | runs | min time (s) | avg time (s) | stddev (s) | best GFLOP/s | checksum | mapping |
|---:|---:|---:|---:|---:|---:|---|---|
| 4 | 1 | 172.477 | 172.477 | 0.000 | 11.596 | `c14405814301863a` | `slots_2_1_1_panel_10000` |
| 9 | 3 | 144.972 | 152.407 | 8.710 | 13.796 | `c14405814301863a` | `slots_3_3_3_panel_10000` |
| 16 | 3 | 189.611 | 193.133 | 4.345 | 10.548 | `c14405814301863a` | `slots_4_6_6_panel_10000` |
| 25 | 1 | 214.462 | 214.462 | 0.000 | 9.326 | `c14405814301863a` | `slots_7_9_9_panel_10000` |
| 36 | 1 | 255.644 | 255.644 | 0.000 | 7.823 | `c14405814301863a` | `slots_10_13_13_panel_10000` |
| 44 | 1 | 304.251 | 304.251 | 0.000 | 6.574 | `c14405814301863a` | `slots_12_16_16_panel_10000` |

## Result
- Best observed configuration: `9` processes, min time `144.972s`.
- The requested hypothesis ?16 processes is best? is not supported by the measurements.
- Compared with the best 16-process run (`189.611s`), `9` processes is `23.54%` faster by minimum runtime.
- All process counts produced the same checksum hash, so the timing comparison is over equivalent numerical results.

## Interpretation
- `mpi_2d` scales best here when the 2D grid remains small and communication/packing overhead is controlled.
- More ranks increase MPI overhead and produce less favorable grids for this WSL cluster; 25, 36, and 44 processes are slower despite using more cores.
- For the report, present this as an empirical optimum on the available 3-node WSL cluster, not as a universal hardware-independent optimum.
