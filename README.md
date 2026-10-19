<h1 align="center">PHASE AWARE UNIFIED L2 CACHE REPLACEMENT POLICY</h1>

<p align="center">
  <strong>Parthsinh R. Thakor</strong><br>
  Department of Computer Science and Engineering<br>
  IIT Bombay<br>
  parthsinh@cse.iitb.ac.in
</p>


## Overview

A unified L2 cache holds instruction blocks, data blocks, and blocks containing page-table entries (PTEs). These classes have different reuse patterns, and their relative cache requirements can change as a program moves between execution phases.

This project investigates a replacement policy that uses runtime cache behaviour to adapt the retention priority of all three classes. The policy will be implemented and evaluated in ChampSim against baseline replacement policies and fixed class-priority orderings.

## Project Plan

### Checkpoint I

- [ ] Establish identification of instruction, data, and PTE blocks.
- [ ] Evaluate baseline policies and collect class-wise cache behaviour over execution windows.
- [ ] Test all six fixed class-priority orderings:
  - [ ] Instruction > Data > PTE
  - [ ] Instruction > PTE > Data
  - [ ] Data > Instruction > PTE
  - [ ] Data > PTE > Instruction
  - [ ] PTE > Instruction > Data
  - [ ] PTE > Data > Instruction
- [ ] Explore runtime signals and window sizes that can guide priority changes.
- [ ] Implement an initial adaptive policy controlling insertion, promotion, and eviction.

### Checkpoint II

- [ ] Refine the policy using findings from Checkpoint I.
- [ ] Compare IPC and cache misses against baselines and fixed class priorities.
- [ ] Evaluate insertion, promotion, and eviction separately to measure each component's contribution.
- [ ] Study different workloads, execution-window sizes, and cache sizes.
- [ ] Analyze performance limitations and implementation overhead.

## Planned Workloads

The proposal targets SPEC CPU2017, TPC-C, Wikipedia, Tomcat, Kafka, and Qualcomm server traces.

Place local traces in `traces/`, or configure another trace directory in the build script. Trace files and generated build artifacts are excluded from version control.

## Requirements

- Ubuntu 18.04 with GCC/G++ 7.5.
- GNU Make and Bash.

If these requirements are unavailable on your local system, use Docker with an Ubuntu environment containing the required compiler and tools.

Use the same environment, compiler, trace, and instruction counts when comparing policies.

## Configure and Run

Edit the settings at the top of `build_champsim.sh`:

```bash
L2C_REPLACEMENT="lru"
BINARY_NAME="lru"
WARMUP_INSTRUCTIONS=25000000
SIMULATION_INSTRUCTIONS=25000000
TRACE_FOLDER="traces"
TRACE_NAME="Trace1.gz"
```

`L2C_REPLACEMENT` selects the policy module. `BINARY_NAME` selects only the executable's name. Set the trace filename to an actual `.gz` or `.xz` file in the configured directory.

From the project directory, run:

```bash
make run
```

`make run` calls `build_champsim.sh`, which:

1. Reads the policy, binary name, instruction counts, and trace settings from the top of the script.
2. Copies the selected replacement policy into the simulator's build source.
3. Displays `Building Binary...` and runs `make clean` followed by a fresh build.
4. Creates `bin/<BINARY_NAME>`. Other build output is hidden; errors are displayed if the build fails.
5. Automatically starts the configured simulation after a successful build and displays its output.

All L1I, L1D, L2, LLC, ITLB, DTLB, and STLB prefetchers use the supplied `no` implementations. Prefetching remains disabled for these experiments.


## Repository Layout

| Path | Contents |
| --- | --- |
| `src/` | CPU, cache, page-table walker, and memory-controller implementation |
| `inc/` | Simulator headers and configuration |
| `replacement/` | Replacement-policy templates and generated implementation files |
| `prefetcher/` | Disabled-prefetcher templates and implementation files |
| `branch/` | Branch-predictor template and implementation |
| `traces/` | Local workload traces |
| `bin/` | Generated executables |
| `obj/` | Generated objects and dependency files |


## Related Work in the Proposal

- **iTP+xPTP, ASPLOS 2025:** Coordinates TLB and L2 replacement for instruction delivery and address translation, using translation pressure to adapt L2 replacement.
- **EMISSARY, ISCA 2023:** Protects instruction blocks associated with costly decode-stage stalls.
- **CLIP, HPCA 2015:** Dynamically protects instruction blocks to reduce instruction-fetch delays.
- **TRRIP, MICRO 2025:** Uses compiler-provided code hotness to prioritize instruction blocks through RRIP rules.
