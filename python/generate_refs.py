
import os, subprocess, shutil, re
def update_param(filepath, updates):
    with open(filepath, 'r') as f:
        content = f.read()
    for k, v in updates.items():
        content = re.sub(rf'^{k}\s*=.*', f'{k} = {v}', content, flags=re.MULTILINE)
    with open(filepath, 'w') as f:
        f.write(content)

def run_and_save(case_id, end_time, m_level, max_level, ref_name):
    if os.path.exists('output'):
        shutil.rmtree('output')
    os.makedirs('output', exist_ok=True)
    
    update_param('param.ini', {
        'which_case': case_id,
        'end_time': end_time,
        'minus_level': m_level,
        'max_level': max_level,
        'write_interval_time': end_time,
        'max_time_step': 200000
    })
    subprocess.run(['./bin/AMR_Solver.exe'])
    outputs = [f for f in os.listdir('output') if f.endswith('.vtu')]
    outputs.sort()
    shutil.copy(f'output/{outputs[-1]}', f'reference/{ref_name}')
    print(f'Generated {ref_name}')

run_and_save(4, 0.6, 5, 5, 'Noh_32x32.vtu')
run_and_save(7, 0.2, 5, 7, 'SodAMR.vtu')
run_and_save(1, 0.5, 5, 7, 'SedovAMR.vtu')

