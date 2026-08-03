"""Serial anchor regression: verify Noh/Sod/Sedov roll back to the GOLDEN-PASS
reference VTUs. Avoids safe-delete blocking on shutil.rmtree('output') by rotating
the output dir aside (os.replace/rename). Restores param.ini afterward.

Run with the NumPy-enabled venv python (compare_vtu.py needs numpy):
    <venv>/python.exe _anchor_regression.py

GOLDEN anchor params (may live partly in source, not just param.ini):
    refine_err = 1.0, coarsen_error = 0.8, refine_period = 4
    refine_coarsen_time = 0.0001   (source default was changed to 0.0; set here)
For efficiency, write_interval_step is pushed very high so only the final
terminal frame is emitted (the comparison needs only that one).
"""
import os, re, shutil, subprocess, datetime, time, sys
from pathlib import Path

# Resolve sibling tools relative to this script so python/ is self-contained.
_THIS = Path(__file__).resolve().parent
COMPARE_VTU = str(_THIS / 'compare_vtu.py')

MSYS_PATHS = [
    "C:/msys64/usr/bin",
    "C:/msys64/ucrt64/bin",
    "C:/Program Files/Microsoft MPI/Bin",
]


def solver_env():
    env = dict(os.environ)
    cur = env.get("PATH", "")
    env["PATH"] = os.pathsep.join(MSYS_PATHS + [cur])
    return env


def update_param(updates):
    with open('param.ini') as f:
        c = f.read()
    for k, v in updates.items():
        if re.search(rf'^{k}\s*=', c, flags=re.MULTILINE):
            c = re.sub(rf'^{k}\s*=.*', f'{k} = {v}', c, flags=re.MULTILINE)
        else:
            c = c.rstrip('\n') + f'\n{k} = {v}\n'
    with open('param.ini', 'w') as f:
        f.write(c)


def rotate_output():
    if os.path.exists('output'):
        ts = datetime.datetime.now().strftime('%H%M%S%f')
        os.replace('output', f'output_rot_{ts}')
    os.makedirs('output', exist_ok=True)


def run_test(name, case_id, end_time, amr, minus, maxlvl, ref_file, ref_enum=0):
    print(f"\n{'='*50}\nRunning {name}\n{'='*50}", flush=True)
    rotate_output()
    update_param({
        'which_case': case_id, 'end_time': end_time, 'enable_amr': amr,
        'minus_level': minus, 'max_level': maxlvl,
        'refine_coarsen_enum': ref_enum,
        'refine_err': 1.0, 'coarsen_error': 0.8,   # match GOLDEN-PASS anchor threshold
        'refine_period': 4,                        # match GOLDEN-PASS anchor AMR schedule
        'refine_coarsen_time': 0.0001,             # match GOLDEN-PASS anchor AMR start time
        'write_interval_time': end_time,
        'write_interval_step': 200000,             # regression needs only the final frame
        'max_time_step': 200000,
    })
    t0 = time.time()
    r = subprocess.run(['./bin/AMR_Solver.exe'], capture_output=True, text=True,
                       env=solver_env())
    solve_s = time.time() - t0
    print(f"[solve] {name} took {solve_s:.1f}s", flush=True)
    if r.returncode != 0:
        print(f"FAILED: solver crash for {name}\n{r.stdout[-800:]}\n{r.stderr[-800:]}")
        return False
    outs = sorted(f for f in os.listdir('output') if f.endswith('.vtu') and f.startswith('p4est_Lagrangian'))
    if not outs:
        print("FAILED: no VTU output")
        return False
    latest = f"output/{outs[-1]}"
    print(f"Comparing {latest} vs reference/{ref_file}", flush=True)
    c = subprocess.run([sys.executable, COMPARE_VTU, '--target', latest,
                        '--ref', f'reference/{ref_file}', '--tol', '1e-12'],
                       capture_output=True, text=True)
    if c.returncode != 0:
        print(f"FAILED: compare for {name}\n{c.stdout}\n{c.stderr}")
        return False
    print(f"PASS: {name}")
    return True


def main():
    if not os.path.exists('bin/AMR_Solver.exe'):
        print("Error: bin/AMR_Solver.exe not found. Please compile first. (Need MSYS2 UCRT64 g++, not CMake)")
        sys.exit(2)

    # robust backup: save the real current ini, restore even on failure
    with open('param.ini') as f:
        orig = f.read()
    results = []
    try:
        results.append(run_test("Noh Uniform (Serial Non-AMR)", case_id=4, end_time=0.6,
                                amr="false", minus=5, maxlvl=5, ref_file="Noh_32x32.vtu"))
        results.append(run_test("Sod AMR (Serial AMR)", case_id=7, end_time=0.2,
                                amr="true", minus=5, maxlvl=7, ref_file="SodAMR.vtu"))
        results.append(run_test("Sedov AMR (Serial AMR)", case_id=1, end_time=0.5,
                                amr="true", minus=5, maxlvl=7, ref_file="SedovAMR.vtu"))
    finally:
        with open('param.ini', 'w') as f:
            f.write(orig)
        print("param.ini restored to the original contents")

    print("\n==== SUMMARY ====")
    names = ["Noh Uniform", "Sod AMR", "Sedov AMR"]
    for n, ok in zip(names, results):
        print(f"{n}: {'PASS (back to anchor)' if ok else 'FAIL'}")
    print("ALL" if all(results) else "SOME-FAILED")
    sys.exit(0 if all(results) else 1)


if __name__ == '__main__':
    main()
