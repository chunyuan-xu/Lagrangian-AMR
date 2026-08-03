---
name: "MPI_BUG_SOD_AMR"
description: "Records the exact context and findings of the MPI divergence bug in Sod AMR at step 3."
---

# MPI Bug Context: Sod AMR Divergence

## Overview
A divergence between serial and parallel (MPI, n=2) execution was detected in the **Sod AMR** problem (`which_case = 7`). The discrepancy manifests precisely at **Step 3**, immediately after the `RiemannSolver` completes its calculations.

## Configuration (`param.ini`)
The exact configuration used to reproduce this bug is as follows:

```ini
which_case = 7
start_time = 0.0
end_time = 0.2
delta_time = 1e-5
minus_level = 5
max_level = 7
max_time_step = 3
refine_err = 0.1
coarsen_error = 0.05
refine_period = 1
write_interval_time = 0.2
write_interval_step = 1
enable_amr = true
```

## The Crime Scene ("案发现场")
The discrepancy was isolated to a specific quadrant, which corresponds to `quadid == 397` in the serial run. 

**Quadrant Details:**
- **Coordinates:** `x = 134217728`, `y = 528482304`
- **Location:** At a partition boundary where parent-child hanging nodes exist.
- **Timing:** Immediately after `RiemannSolver` in step 3.

**Symptom:**
The Riemann solver updates corner velocities (`idcnVelocity_cur`). Comparing the four corner velocities (`P4EST_CHILDREN`) of this exact quadrant between serial and parallel runs reveals that Corners 2 and 3 are receiving corrupted/mismatched flux updates in the MPI run.

### Serial Run Output
```text
SERIAL 397 (x=134217728, y=528482304) corner velocities:
  Corner 0: vx=0.406346, vy=0.184776
  Corner 1: vx=0.369208, vy=0.369314
  Corner 2: vx=0.276940, vy=0.277038
  Corner 3: vx=0.249376, vy=0.138642
```

### Parallel Run Output (MPI n=2)
```text
PARALLEL MATCH (x=134217728, y=528482304) corner velocities:
  Corner 0: vx=0.406346, vy=0.184776
  Corner 1: vx=0.369208, vy=0.369314
  Corner 2: vx=0.315114, vy=0.315167   <--- Mismatch!
  Corner 3: vx=0.268463, vy=0.157707   <--- Mismatch!
```

## Root Cause Analysis
The mismatched velocity updates at Corners 2 and 3 trace back to the `quadrant_parent_edge_matrix_callback`. In this function, the flag `IsParentChildBoun` triggers specific boundary reconstructions. Due to missing or misaligned ghost layer information across MPI boundaries, the parallel run evaluates `IsParentChildBoun == true` for different edges/directions (`k`) than the serial run. This leads to incorrect divergence (`Divergence`) and corner impedance (`Zcp`) values being fed into the Riemann solver for this cell.
