import os
import sys
import subprocess
import re
import shutil
from pathlib import Path

# Project root = parent of python/ (where param.ini, bin/, reference/ live).
# Resolve paths from here so the script works regardless of the invoking cwd.
ROOT = Path(__file__).resolve().parent.parent
_COMPARE_VTU = str(Path(__file__).resolve().parent / 'compare_vtu.py')


def _rooted(p):
    return str(ROOT / p)


def update_param(filepath, updates):
    with open(filepath, 'r') as f:
        content = f.read()
    for k, v in updates.items():
        content = re.sub(rf'^{k}\s*=.*', f'{k} = {v}', content, flags=re.MULTILINE)
    with open(filepath, 'w') as f:
        f.write(content)

def run_test(name, case_id, end_time, amr_enabled, minus_level, max_level, ref_file, refine_coarsen_enum=0):
    print(f"\n{'='*50}\nRunning {name}\n{'='*50}")
    
    # Clean output dir
    out_dir = _rooted('output')
    if os.path.exists(out_dir):
        shutil.rmtree(out_dir)
    os.makedirs(out_dir, exist_ok=True)
    
    update_param(_rooted('param.ini'), {
        'which_case': case_id,
        'end_time': end_time,
        'enable_amr': amr_enabled,
        'minus_level': minus_level,
        'max_level': max_level,
        'refine_coarsen_enum': refine_coarsen_enum,
        'write_interval_time': end_time,
        'max_time_step': 200000
    })
    
    # Run solver
    print("Executing solver...")
    result = subprocess.run([_rooted('bin/AMR_Solver.exe')], capture_output=True, text=True, cwd=str(ROOT))
    if result.returncode != 0:
        print(f"FAILED: Solver crashed for {name}!\nSTDOUT:\n{result.stdout[-1000:]}\nSTDERR:\n{result.stderr[-1000:]}")
        return False
        
    # Get output file
    outputs = [f for f in os.listdir(_rooted('output')) if f.endswith('.vtu') and f.startswith('p4est_Lagrangian')]
    if not outputs:
        print("FAILED: No VTU output generated!")
        return False
    outputs.sort()
    latest_out = _rooted(f"output/{outputs[-1]}")
    
    # Check comparison
    print(f"Comparing {latest_out} against reference/{ref_file}")
    comp_result = subprocess.run([sys.executable, _COMPARE_VTU, '--target', latest_out,
                                  '--ref', _rooted(f'reference/{ref_file}'), '--tol', '1e-12'],
                                 capture_output=True, text=True, cwd=str(ROOT))
    
    if comp_result.returncode != 0:
        print(f"FAILED: Comparison failed for {name}!\n{comp_result.stdout}\n{comp_result.stderr}")
        return False
        
    print(f"PASS: {name}")
    return True

def main():
    if not os.path.exists(_rooted('bin/AMR_Solver.exe')):
        print("Error: bin/AMR_Solver.exe not found. Please compile first.")
        sys.exit(1)
        
    # Strictly sequenced execution, aborting on first failure.
    
    # Phase 1: Serial Uniform (Non-AMR) - Tests basic physics solver
    if not run_test("Noh Uniform (Serial Non-AMR)", case_id=4, end_time=0.6, amr_enabled="false", minus_level=5, max_level=5, ref_file="Noh_32x32.vtu"):
        print("\nABORT: Failed at Phase 1: Serial Uniform (Non-AMR). Stopping further tests.")
        sys.exit(1)
        
    # Phase 2: Serial AMR - Tests adaptive mesh logic
    if not run_test("Sod AMR (Serial AMR)", case_id=7, end_time=0.2, amr_enabled="true", minus_level=5, max_level=7, ref_file="SodAMR.vtu"):
        print("\nABORT: Failed at Phase 2: Serial AMR (Sod). Stopping further tests.")
        sys.exit(1)
        
    if not run_test("Sedov AMR (Serial AMR)", case_id=1, end_time=0.5, amr_enabled="true", minus_level=5, max_level=7, ref_file="SedovAMR.vtu"):
        print("\nABORT: Failed at Phase 2: Serial AMR (Sedov). Stopping further tests.")
        sys.exit(1)
    
    print("\nALL REGRESSION TESTS PASSED SUCCESSFULLY!")
    sys.exit(0)

if __name__ == '__main__':
    main()
