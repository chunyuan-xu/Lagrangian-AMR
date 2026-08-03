import sys
import glob

def parse_file(filename, data):
    try:
        with open(filename, 'r') as f:
            for line in f:
                if not line.strip(): continue
                parts = line.strip().split(',')
                try:
                    step = int(parts[0].replace('Step', '').strip())
                    gid = int(parts[1].replace('GlobalID', '').strip())
                    cnid = int(parts[2].replace('Corner', '').strip())
                    p = float(parts[3].replace('P=', '').strip())
                    rho = float(parts[4].replace('Rho=', '').strip())
                    vx = float(parts[5].replace('Velo=(', '').strip())
                    vy = float(parts[6].replace(')', '').strip())
                    
                    key = (gid, cnid)
                    data[key] = {'P': p, 'Rho': rho, 'Vx': vx, 'Vy': vy}
                except Exception as e:
                    pass
    except:
        pass

for loc in [3, 4]:
    serial_data = {}
    parse_file(f'serial_vars_loc_{loc}.txt', serial_data)
    
    mpi_data = {}
    for fname in glob.glob(f'debug_vars_loc_{loc}_rank_*.txt'):
        parse_file(fname, mpi_data)
        
    print(f'\n--- Location {loc} (1=Before Riemann, 2=After Riemann) ---')
    print(f'Total keys in serial: {len(serial_data)}')
    print(f'Total keys in MPI: {len(mpi_data)}')
    
    max_diff = {'P': 0, 'Rho': 0, 'Vx': 0, 'Vy': 0}
    max_diff_gid = {'P': None, 'Rho': None, 'Vx': None, 'Vy': None}
    
    for key in serial_data:
        if key not in mpi_data:
            continue
        s_val = serial_data[key]
        m_val = mpi_data[key]
        for var in ['P', 'Rho', 'Vx', 'Vy']:
            diff = abs(s_val[var] - m_val[var])
            if diff > max_diff[var]:
                max_diff[var] = diff
                max_diff_gid[var] = key

    for var in ['P', 'Rho', 'Vx', 'Vy']:
        print(f'Max Diff {var}: {max_diff[var]:.4e} at {max_diff_gid[var]}')
