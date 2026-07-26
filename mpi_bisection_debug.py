import os
import subprocess
import re

param_content = """
which_case = 4
start_time = 0.0
end_time = 0.5
delta_time = 1e-5
minus_level = 6
max_level = 6
max_time_step = 10
refine_err = 1.0
coarsen_error = 0.8
refine_period = 400
write_interval_time = 0.5
write_interval_step = 100
"""

def get_env():
    env = os.environ.copy()
    env["PATH"] = r"C:\msys64\usr\bin;C:\msys64\ucrt64\bin;C:\Program Files\Microsoft MPI\Bin;" + env.get("PATH", "")
    return env

def setup_and_build():
    with open("C:/Lagrangian-AMR/param.ini", "w") as f:
        f.write(param_content)
    
    print("Building project...")
    subprocess.run(["make"], cwd="C:/Lagrangian-AMR", check=True, shell=True, env=get_env())

def run_test():
    print("Running 1-core...")
    subprocess.run("mpiexec -n 1 ./bin/AMR_Solver.exe > log1.txt 2>&1", cwd="C:/Lagrangian-AMR", shell=True, check=True, env=get_env())
    print("Running 4-core...")
    subprocess.run("mpiexec -n 4 ./bin/AMR_Solver.exe > log4.txt 2>&1", cwd="C:/Lagrangian-AMR", shell=True, check=True, env=get_env())

def parse_checksums(logfile):
    checksums = {}
    with open(logfile, "r") as f:
        for line in f:
            match = re.search(r"Checksum \[(.*?)\]: Mass = (.*?), Energy = (.*)", line)
            if match:
                label = match.group(1).strip()
                mass = float(match.group(2))
                energy = float(match.group(3))
                if label not in checksums:
                    checksums[label] = []
                checksums[label].append((mass, energy))
    return checksums

def compare():
    chk1 = parse_checksums("C:/Lagrangian-AMR/log1.txt")
    chk4 = parse_checksums("C:/Lagrangian-AMR/log4.txt")
    
    success = True
    labels_order = ["Checkpoint 1: Start time loop", "Checkpoint 2: AMR", "Checkpoint 3: Predict", "Checkpoint 4: RiemannSolver", "Checkpoint 5: Update"]
    
    for label in labels_order:
        if label not in chk1 or label not in chk4:
            print(f"Missing label {label} in logs")
            success = False
            continue
        
        c1_list = chk1[label]
        c4_list = chk4[label]
        
        for i, (m1, e1) in enumerate(c1_list):
            m4, e4 = c4_list[i]
            diff_m = abs(m1 - m4)
            diff_e = abs(e1 - e4)
            if diff_m > 1e-12 or diff_e > 1e-12:
                print(f"Mismatch at {label}, step {i}:")
                print(f"  1-core: Mass={m1}, Energy={e1}")
                print(f"  4-core: Mass={m4}, Energy={e4}")
                print(f"  Diff:   Mass={diff_m}, Energy={diff_e}")
                success = False
            else:
                pass
    return success

if __name__ == "__main__":
    setup_and_build()
    run_test()
    if compare():
        print("PASS: All checkpoints match.")
    else:
        print("FAIL: Checksums diverge.")
