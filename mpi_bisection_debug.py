import os
import subprocess
import re
import sys

def get_env():
    env = os.environ.copy()
    env["PATH"] = r"C:\msys64\usr\bin;C:\msys64\ucrt64\bin;C:\Program Files\Microsoft MPI\Bin;" + env.get("PATH", "")
    return env

def write_param(which_case, enable_amr):
    param = f"""which_case = {which_case}
start_time = 0.0
end_time = 0.5
delta_time = 1e-5
minus_level = 5
max_level = 7
max_time_step = 15
refine_err = 1.0
coarsen_error = 0.8
refine_period = 4
write_interval_time = 0.5
write_interval_step = 100
enable_amr = {'true' if enable_amr else 'false'}
"""
    with open("C:/Lagrangian-AMR/param.ini", "w") as f:
        f.write(param)

def run_test(case_name):
    print(f"\n--- Running {case_name} ---")
    print("Running 1-core...")
    subprocess.run("mpiexec -n 1 ./bin/AMR_Solver.exe > log1.txt 2>&1", cwd="C:/Lagrangian-AMR", shell=True, check=True, env=get_env())
    print("Running 4-core...")
    subprocess.run("mpiexec -n 4 ./bin/AMR_Solver.exe > log4.txt 2>&1", cwd="C:/Lagrangian-AMR", shell=True, check=True, env=get_env())

def parse_checksums(logfile):
    checksums = {}
    with open(logfile, "r") as f:
        for line in f:
            match = re.search(r"Checksum \[(.*?)\]: Mass = (.*?), E = (.*?), Rho = (.*?), V = (.*?), W = (.*)", line)
            if match:
                label = match.group(1).strip()
                mass = float(match.group(2))
                energy = float(match.group(3))
                rho = float(match.group(4))
                velo = float(match.group(5))
                work = float(match.group(6))
                if label not in checksums:
                    checksums[label] = []
                checksums[label].append((mass, energy, rho, velo, work))
    return checksums

def compare(case_name):
    chk1 = parse_checksums("C:/Lagrangian-AMR/log1.txt")
    chk4 = parse_checksums("C:/Lagrangian-AMR/log4.txt")
    
    # dynamically get labels from 1-core log in order of appearance
    labels_order = []
    with open("C:/Lagrangian-AMR/log1.txt", "r") as f:
        for line in f:
            match = re.search(r"Checksum \[(.*?)\]:", line)
            if match:
                label = match.group(1).strip()
                if label not in labels_order:
                    labels_order.append(label)
    
    for label in labels_order:
        if label not in chk1 or label not in chk4:
            continue
            
        c1_list = chk1[label]
        c4_list = chk4[label]
        
        for i, vals1 in enumerate(c1_list):
            if i >= len(c4_list):
                print(f"[{case_name}] 4-core log is shorter than 1-core at {label} step {i}")
                return False
            vals4 = c4_list[i]
            
            diffs = [abs(vals1[k] - vals4[k]) for k in range(5)]
            if any(d > 1e-12 for d in diffs):
                print(f"[{case_name}] Mismatch at {label}, step {i}:")
                print(f"  1-core: {vals1}")
                print(f"  4-core: {vals4}")
                print(f"  Diffs:  {diffs}")
                return False
    return True

if __name__ == "__main__":
    print("Building project...")
    subprocess.run(["make"], cwd="C:/Lagrangian-AMR", check=True, shell=True, env=get_env())
    
    # Test Noh (Non-AMR)
    write_param(which_case=4, enable_amr=False)
    run_test("Noh_NonAMR")
    if not compare("Noh_NonAMR"):
        print("FAIL: Noh_NonAMR Checksums diverge.")
        sys.exit(1)
    
    # Test Sod (AMR)
    write_param(which_case=7, enable_amr=True)
    run_test("Sod_AMR")
    if not compare("Sod_AMR"):
        print("FAIL: Sod_AMR Checksums diverge.")
        sys.exit(1)
        
    print("PASS: All checkpoints match for both Noh and Sod!")

