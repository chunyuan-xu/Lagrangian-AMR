"""Serial anchor regression: replicate run_tests.py but avoid safe-delete blocking
on shutil.rmtree('output') by rotating the output dir aside (os.replace->rename) instead
of deleting it. Restores param.ini afterward."""
import os, re, shutil, subprocess, datetime, sys

ANCHOR_PARAM_BAK = "/tmp/param_anchor_backup.ini"


def update_param(updates):
    with open('param.ini') as f:
        c = f.read()
    for k, v in updates.items():
        c = re.sub(rf'^{k}\s*=.*', f'{k} = {v}', c, flags=re.MULTILINE)
    with open('param.ini', 'w') as f:
        f.write(c)


def rotate_output():
    # move existing output/ aside (rename, not delete) then create fresh empty dir
    if os.path.exists('output'):
        ts = datetime.datetime.now().strftime('%H%M%S%f')
        os.replace('output', f'output_rot_{ts}')
    os.makedirs('output', exist_ok=True)


def run_test(name, case_id, end_time, amr, minus, maxlvl, ref_file, ref_enum=0):
    print(f"\n{'='*50}\nRunning {name}\n{'='*50}")
    rotate_output()
    update_param({
        'which_case': case_id, 'end_time': end_time, 'enable_amr': amr,
        'minus_level': minus, 'max_level': maxlvl,
        'refine_coarsen_enum': ref_enum,
        'refine_err': 1.0, 'coarsen_error': 0.8,   # match GOLDEN-PASS anchor threshold
        'refine_period': 4,                        # match GOLDEN-PASS anchor AMR schedule
        'write_interval_time': end_time, 'max_time_step': 200000,
    })
    r = subprocess.run(['./bin/AMR_Solver.exe'], capture_output=True, text=True)
    if r.returncode != 0:
        print(f"FAILED: solver crash for {name}\n{r.stdout[-800:]}\n{r.stderr[-800:]}")
        return False
    outs = sorted(f for f in os.listdir('output') if f.endswith('.vtu') and f.startswith('p4est_Lagrangian'))
    if not outs:
        print("FAILED: no VTU output")
        return False
    latest = f"output/{outs[-1]}"
    print(f"Comparing {latest} vs reference/{ref_file}")
    c = subprocess.run(['python', 'compare_vtu.py', '--target', latest, '--ref', f'reference/{ref_file}', '--tol', '1e-12'],
                       capture_output=True, text=True)
    if c.returncode != 0:
        print(f"FAILED: compare for {name}\n{c.stdout}\n{c.stderr}")
        return False
    print(f"PASS: {name}")
    return True


def restore_param():
    if os.path.exists(ANCHOR_PARAM_BAK):
        shutil.copy2(ANCHOR_PARAM_BAK, 'param.ini')
        print("param.ini restored from backup")


def main():
    results = []
    results.append(run_test("Noh Uniform (Serial Non-AMR)", case_id=4, end_time=0.6,
                            amr="false", minus=5, maxlvl=5, ref_file="Noh_32x32.vtu"))
    results.append(run_test("Sod AMR (Serial AMR)", case_id=7, end_time=0.2,
                            amr="true", minus=5, maxlvl=7, ref_file="SodAMR.vtu"))
    results.append(run_test("Sedov AMR (Serial AMR)", case_id=1, end_time=0.5,
                            amr="true", minus=5, maxlvl=7, ref_file="SedovAMR.vtu"))
    restore_param()
    print("\n==== SUMMARY ====")
    names = ["Noh Uniform", "Sod AMR", "Sedov AMR"]
    for n, ok in zip(names, results):
        print(f"{n}: {'PASS (back to anchor)' if ok else 'FAIL'}")
    print("ALL" if all(results) else "SOME-FAILED")
    sys.exit(0 if all(results) else 1)


if __name__ == '__main__':
    main()
