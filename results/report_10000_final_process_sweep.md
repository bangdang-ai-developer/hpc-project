# Final 10000x10000x10000 Process Sweep

All runs use `mpi_2d`, seed `2026`, `--tile 10000`, 3 WSL nodes, and shape A(10000x10000) * B(10000x10000).

| processes | 2D grid | runs | min time (s) | avg time (s) | stddev (s) | best GFLOP/s | checksum | mapping |
|---:|---|---:|---:|---:|---:|---:|---|---|
| 4 | 2x2 | 1 | 172.477 | 172.477 | 0.000 | 11.596 | `c14405814301863a` | `slots_2_1_1_panel_10000` |
| 9 | 3x3 | 3 | 144.972 | 152.407 | 8.710 | 13.796 | `c14405814301863a` | `slots_3_3_3_panel_10000` |
| 12 | 3x4 | 1 | 148.039 | 148.039 | 0.000 | 13.510 | `c14405814301863a` | `slots_4_4_4_panel_10000` |
| 16 | 4x4 | 3 | 189.611 | 193.133 | 4.345 | 10.548 | `c14405814301863a` | `slots_4_6_6_panel_10000` |
| 25 | 5x5 | 1 | 214.462 | 214.462 | 0.000 | 9.326 | `c14405814301863a` | `slots_7_9_9_panel_10000` |
| 36 | 6x6 | 1 | 255.644 | 255.644 | 0.000 | 7.823 | `c14405814301863a` | `slots_10_13_13_panel_10000` |
| 44 | 4x11 | 1 | 304.251 | 304.251 | 0.000 | 6.574 | `c14405814301863a` | `slots_12_16_16_panel_10000` |

## Final Conclusion
- Best measured process count: **9 processes**, min time **144.972s**, best throughput **13.796 GFLOP/s**.
- `p12` is close but still slower than `p9`: 148.039s vs 144.972s, about 2.12% slower by best runtime.
- `p9` is about 23.54% faster than the best `p16` run (144.972s vs 189.611s).
- All runs share checksum `c14405814301863a`, so performance differences are not caused by different numerical outputs.

## Report Writing Plan
1. Describe the experimental cluster: 3 WSL nodes, 44 logical cores available, OpenMPI over SSH/TCP.
2. Explain `mpi_2d`: root packs A row blocks and B column blocks, then each rank computes one C block; checksum is reduced without gathering full C.
3. Present process sweep table above as the main 10000^3 result.
4. Explain why more processes are not always faster in this implementation: root-side sequential pack/send and more MPI messages dominate after p9/p12.
5. State empirical optimum: `p9` on this cluster/code version; `p12` is runner-up and `p16+` is slower.
