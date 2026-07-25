# Mathematical Formulation for Cell-Centered Lagrangian Hydrodynamics

## 1. 2D Lagrangian Compressible Fluid Dynamics Governing Equations

For a 2D inviscid, compressible fluid, the integral conservation laws over a moving control volume $\Omega(t)$ with boundary $\partial\Omega(t)$ and outward unit normal $\mathbf{N}$ are given by:

- **Geometric Conservation Law (GCL) / Volume:**
  $$ \frac{d}{dt} \int_{\Omega} d\Omega - \int_{\partial\Omega} \mathbf{U} \cdot \mathbf{N} dL = 0 $$
- **Conservation of Mass:**
  $$ \frac{d}{dt} \int_{\Omega} \rho d\Omega = 0 $$
- **Conservation of Momentum:**
  $$ \frac{d}{dt} \int_{\Omega} \rho \mathbf{U} d\Omega + \int_{\partial\Omega} P \mathbf{N} dL = \mathbf{0} $$
- **Conservation of Total Energy:**
  $$ \frac{d}{dt} \int_{\Omega} \rho E d\Omega + \int_{\partial\Omega} P \mathbf{U} \cdot \mathbf{N} dL = 0 $$

The system is closed by the ideal gas Equation of State (EOS):
$$ P = (\gamma - 1)\rho e, \quad e = E - \frac{1}{2}|\mathbf{U}|^2 $$

## 2. Corner Nodal Velocity Solver & Decoupled Kinematic-Constrained Formulation

### 2.1 Nodal Solver Matrix Equations
For a regular node $p$, the nodal velocity $\mathbf{U}_p$ is given by:
$$ \mathbf{U}_p = \mathbf{M}_p^{-1} \sum_{c \in \mathcal{C}(p)} \left( \mathbf{M}_p^c \mathbf{U}_c - P_c L_p^c \mathbf{N}_p^c \right) $$
where $\mathbf{M}_p^c$ is the subcell acoustic impedance matrix and $\mathbf{M}_p = \sum_c \mathbf{M}_p^c$.

### 2.2 Hanging Node Kinematic Constraint
For a hanging node $h$ between master nodes $p_1$ and $p_2$, the decoupled collinearity constraint uniquely dictates its velocity:
$$ \mathbf{U}_h = \frac{1}{2} (\mathbf{U}_{p_1} + \mathbf{U}_{p_2}) $$
The constraint force (Lagrange multiplier) required to enforce this collinearity is computed *a posteriori*:
$$ \boldsymbol{\lambda}_h = -\mathbf{M}_h \mathbf{U}_h + \sum_{c \in \mathcal{C}(h)} \left( \mathbf{M}_h^c \mathbf{U}_c - P_c L_h^c \mathbf{N}_h^c \right) $$

### 2.3 Thermal-Inertia Internal-Energy-Weighted Partition
To redistribute the constraint force conservatively and robustly, we use internal-energy-weighted partition coefficients:
$$ \alpha_{ch} = \frac{m_c e_c}{\sum_{k \in \mathcal{C}(h)} m_k e_k} $$

## 3. Spatio-Temporal Discretization Scheme

The semi-discrete finite volume equations are advanced using a **1st-order explicit forward Euler scheme**.

1. **Geometry Initialization:** Compute half-edge lengths $L_k^c$ and normals $\mathbf{N}_k^c$.
2. **Standard Nodal Solver:** Calculate $\mathbf{U}_p$ for regular nodes.
3. **Hanging Nodal Solver:** Calculate $\mathbf{U}_h = \frac{1}{2}(\mathbf{U}_{p_1} + \mathbf{U}_{p_2})$ and constraint force $\boldsymbol{\lambda}_h$.
4. **State Update (Momentum & Energy):**
   $$ m_c \frac{\mathbf{U}_c^{n+1} - \mathbf{U}_c^n}{\Delta t} = - \sum_{p} \mathbf{F}_{cp} - \sum_{h} \alpha_{ch}\boldsymbol{\lambda}_h $$
   $$ m_c \frac{E_c^{n+1} - E_c^n}{\Delta t} = - \sum_{p} \mathbf{F}_{cp} \cdot \mathbf{U}_p - \sum_{h} \alpha_{ch}\boldsymbol{\lambda}_h \cdot \mathbf{U}_h $$
5. **Coordinate Update:** $\mathbf{X}_k^{n+1} = \mathbf{X}_k^n + \Delta t \mathbf{U}_k^n$
6. **Thermodynamic Update:** Update $V_c^{n+1}$, $\rho_c^{n+1} = m_c/V_c^{n+1}$, and $P_c^{n+1}$.

### Adaptation Time Step Limiters
The time step $\Delta t^n$ obeys the standard CFL condition ($C_{CFL} = 0.2$), limited by:
- Maximum relative volume change: $|\Delta V_c / V_c| \leq 10\%$
- Growth limit: $\Delta t^{n+1} \leq 1.1 \Delta t^n$

## 4. Mathematical Criteria for Mesh Adaptation (`p4est`)

- **Refinement/Coarsening Indicators:** Gradient-based indicators of density ($\nabla \rho$) or pressure ($\nabla P$), mapped to predefined threshold levels.
- **Topology Balancing:** Maintains a strict 2:1 quadtree balance rule (at most one hanging node per edge).
- **Intersection-Based Conservative Remap (Coarsening):**
  When fine child cells merge into a parent cell, mass, momentum, and energy are conserved algebraically:
  $$ m_p = \sum_i m_i, \quad (\mathbf{mU})_p = \sum_i m_i \mathbf{U}_i, \quad (\mathbf{mE})_p = \sum_i m_i E_i $$

## 5. Physical Variable Mapping Table

Based on `src/variable.h` and `src/defines.h`, the C++ solver variables are classified to facilitate data structure refactoring:

| Variable Description | Category | Struct/Array Mapping |
| :--- | :--- | :--- |
| **Mass** ($m_c$) | Primary State | `quad_data_t.m_vara.DouCData[idMass]` |
| **Density** ($\rho_c$) | Primary State | `quad_data_t.m_vara.DouCData[idDensity_cur]` |
| **Pressure** ($P_c$) | Primary State | `quad_data_t.m_vara.DouCData[idPressure_cur]` |
| **Internal Energy** ($e_c$) | Primary State | `quad_data_t.m_vara.DouCData[idInternalEnergy_cur]` |
| **Total Energy** ($E_c$) | Primary State | `quad_data_t.m_vara.DouCData[idTotalEnergy_cur]` |
| **Cell Velocity** ($\mathbf{U}_c$) | Primary State | `quad_data_t.m_vara.VecCData[idCentroidVelo_cur]` |
| **Node Coordinates** ($\mathbf{X}_p$) | Primary State | `quad_data_t.init_node_coords`, `points.hanging_coord` |
| **Node Velocity** ($\mathbf{U}_p$) | Primary State | `quad_data_t.points[i].velo_lag`, `VecCnData[idcnVelocity_cur]` |
| **Cell Volume** ($V_c$) | Geometrical | `quad_data_t.m_vara.DouCData[idVolume]` |
| **Half-Edge Lengths** ($L_{cp}$) | Geometrical | `CCorner_data.hdata[].Lcp` |
| **Outward Normals** ($\mathbf{N}_{cp}$) | Geometrical | `CCorner_data.hdata[].Ncp` |
| **Cell Centroid** | Geometrical | `quad_data_t.m_vara.VecCData[idCentroidCoord_cur]` |
| **Subcell Matrix** ($\mathbf{M}_p^c$) | Solver Cache | `quad_data_t.m_vara.MarCnData[idcnMcp]` |
| **Nodal Matrix** ($\mathbf{M}_p$) | Solver Cache | `quad_data_t.points[i].MatrixP` |
| **Right-Hand Side** ($\mathbf{RHS}$) | Solver Cache | `quad_data_t.points[i].RHS`, `VecCnData[idcnRHS]` |
| **Constraint Force** ($\boldsymbol{\lambda}_h$) | Solver Cache | Represented inside `RHS` closures and `Fcp` |
| **Predictor/Half States** | Solver Cache | `DouCData[idPressure_half]`, `VecCData[idCentroidVelo_half]` |
