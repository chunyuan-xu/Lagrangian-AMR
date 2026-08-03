import sys
import glob

def parse_file(filename, data):
    with open(filename, 'r') as f:
        for line in f:
            if not line.strip(): continue
            parts = line.strip().split(',')
            # Step 1, GlobalID 1025, Corner 0, P=1.000000e+00, Rho=1.000000e+00, Velo=(0.000000e+00, 0.000000e+00)
            try:
                step = int(parts[0].replace('Step', '').strip())
                gid = int(parts[1].replace('GlobalID', '').strip())
                cnid = int(parts[2].replace('Corner', '').strip())
                p = float(parts[3].replace('P=', '').strip())
                rho = float(parts[4].replace('Rho=', '').strip())
                # Velo=(x
                vx = float(parts[5].replace('Velo=(', '').strip())
                # y)
                vy = float(parts[6].replace(')', '').strip())
                
                key = (step, gid, cnid)
                data[key] = {'P': p, 'Rho': rho, 'Vx': vx, 'Vy': vy}
            except Exception as e:
                print(f"Error parsing line: {line}\n{e}")

serial_data = {}
parse_file('serial_vars.txt', serial_data)

mpi_data = {}
for fname in glob.glob('debug_vars_rank_*.txt'):
    parse_file(fname, mpi_data)

print(f"Total keys in serial: {len(serial_data)}")
print(f"Total keys in MPI: {len(mpi_data)}")

max_diff = {}
max_diff_gid = {}

for key in serial_data:
    if key not in mpi_data:
        continue
    
    s_val = serial_data[key]
    m_val = mpi_data[key]
    
    step_key = f"Step {key[0]}"
    if step_key not in max_diff:
        max_diff[step_key] = {'P': 0, 'Rho': 0, 'Vx': 0, 'Vy': 0}
        max_diff_gid[step_key] = {'P': None, 'Rho': None, 'Vx': None, 'Vy': None}
        
    for var in ['P', 'Rho', 'Vx', 'Vy']:
        diff = abs(s_val[var] - m_val[var])
        if diff > max_diff[step_key][var]:
            max_diff[step_key][var] = diff
            max_diff_gid[step_key][var] = key

for step in sorted(max_diff.keys(), key=lambda x: int(x.split()[1])):
    print(f"\n--- {step} ---")
    for var in ['P', 'Rho', 'Vx', 'Vy']:
        print(f"Max Diff {var}: {max_diff[step][var]:.4e} at {max_diff_gid[step][var]}")
