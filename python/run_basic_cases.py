"""Isolated distance-AMR basic tests; build with Makefile first (stdlib only).

PASS means completion/output sanity, NOT golden equality or accuracy certification.
Never modifies root param.ini or reference/. See docs/basic-test-cases.md.
"""

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import time
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
CASES = {name: ROOT / 'cases' / 'basic' / (name + '.ini') for name in (
    'sedov-distance-l4-l6', 'noh-distance-l4-l6')}


def read_config(path):
    values = {}
    for line in path.read_text(encoding='utf-8-sig').splitlines():
        line = line.split('#', 1)[0].strip()
        if not line:
            continue
        key, value = (part.strip() for part in line.split('=', 1))
        if key in values:
            raise ValueError(f'duplicate parameter: {key}')
        values[key] = value
    return values


def numeric_rows(path, columns):
    # Existing writer uses literal backslashes; accept whitespace too.
    rows = []
    for line in path.read_text(encoding='utf-8-sig').splitlines():
        if not line.strip():
            continue
        row = [float(v) for v in re.split(r'[\\\s,]+', line.strip()) if v]
        if len(row) != columns or not all(math.isfinite(v) for v in row):
            raise ValueError(f'{path.name}: invalid/nonfinite row')
        rows.append(row)
    if not rows:
        raise ValueError(f'{path.name}: empty data')
    return rows


def inspect_run(folder, config, ranks):
    log = (folder / 'run.log').read_text(encoding='utf-8', errors='replace')
    steps = re.findall(r'simulation_step=\s*(\d+),\s*delta_time\s*=\s*([^,]+),'
                       r'\s*simulation_time\s*=\s*(\S+)', log)
    if not steps:
        raise ValueError('no completed simulation step')
    step, dt, final_time = steps[-1]
    dt, final_time = float(dt), float(final_time)
    end = float(config['end_time'])
    if not all(math.isfinite(v) for v in (dt, final_time)) or dt <= 0:
        raise ValueError('invalid final clock')
    # Log time has 6 decimal places; solver may overshoot end by one dt.
    if not end - 1e-6 <= final_time <= end + dt + 1e-6:
        raise ValueError(f'incomplete/invalid final time: {final_time}, requested {end}')
    profiles = numeric_rows(folder / 'DistanceProfiles.plt', 5)
    if any(row[0] < 0 or row[1] <= 0 or row[2] < 0 for row in profiles):
        raise ValueError('invalid radius/density/pressure in final profile')
    energy = numeric_rows(folder / 'EnergyError.plt', 2)
    frames = list((folder / 'output').glob('p4est_Lagrangian_*.pvtu'))
    if not frames:
        raise ValueError('missing PVTU output')
    frame = max(frames, key=lambda p: int(p.stem.rsplit('_', 1)[1]))
    tree = ET.parse(frame)
    pieces = tree.findall('.//Piece')
    if len(pieces) != ranks:
        raise ValueError('wrong number of PVTU rank pieces')
    counts = []
    for piece in pieces:
        piece_tree = ET.parse(frame.parent / piece.attrib['Source'])
        counts.append(sum(int(p.attrib['NumberOfCells'])
                          for p in piece_tree.findall('.//Piece')))
    if sum(counts) != len(profiles):
        raise ValueError('final profile / last VTU cell counts disagree')
    stamp = tree.find('.//DataArray[@Name="TimeValue"]')
    if stamp is None:
        raise ValueError('missing VTU time metadata')
    output_time = float(stamp.text)
    if not math.isfinite(output_time) or not 0 <= final_time - output_time <= dt + 2e-6:
        raise ValueError('last VTU is stale relative to final step')
    for operation in ('refine', 'coarsen', 'partition'):
        if f'Into p4est_{operation}' not in log:
            raise ValueError(f'no {operation} activity')
    return dict(final_step=int(step), final_time=final_time,
                last_vtu_time=output_time, last_vtu=str(frame),
                final_cells=sum(counts), rank_cells=counts,
                cell_imbalance=max(counts) / (sum(counts) / ranks),
                max_abs_step_energy_error=max(abs(row[1]) for row in energy))


def export_distance(folder):
    rows = numeric_rows(folder / 'DistanceProfiles.plt', 5)
    (folder / 'distance.plt').write_text(
        'VARIABLES = "radius", "density", "pressure", "internal_energy", "total_energy"\n'
        + f'ZONE I={len(rows)}, F=POINT\n'
        + ''.join(' '.join(format(v, '.17g') for v in row) + '\n' for row in rows),
        encoding='ascii')


def environment():
    env = dict(os.environ)
    if os.name == 'nt':
        env['PATH'] = os.pathsep.join([
            'C:/msys64/usr/bin', 'C:/msys64/ucrt64/bin',
            'C:/Program Files/Microsoft MPI/Bin', env.get('PATH', '')])
    for key in list(env):
        if key.startswith('LAGRANGIAN_'):
            env.pop(key)
    return env


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--case', choices=['all', *CASES], default='all')
    parser.add_argument('--ranks', type=int, default=4)
    parser.add_argument('--solver', type=Path, default=ROOT / 'bin' / 'AMR_Solver.exe')
    parser.add_argument('--mpiexec', default='mpiexec')
    parser.add_argument('--timeout', type=float, default=1800,
                        help='per-case timeout in seconds')
    parser.add_argument('--output-root', type=Path, default=ROOT / '.tmp' / 'basic-tests')
    args = parser.parse_args()
    if args.ranks < 1 or args.timeout <= 0:
        parser.error('ranks and timeout must be positive')
    solver = args.solver.resolve()
    if not solver.is_file():
        parser.error(f'build first: solver missing: {solver}')
    env = environment()
    launcher = shutil.which(args.mpiexec, path=env['PATH'])
    if not launcher:
        parser.error(f'MPI launcher not found: {args.mpiexec}')
    args.output_root.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix=time.strftime('%Y%m%d-%H%M%S-'),
                                 dir=args.output_root.resolve()))
    summary = dict(kind='basic-completion-not-golden', ranks=args.ranks,
                   solver=str(solver), solver_sha256=sha256(solver), cases=[])
    print(f'Results: {root}', flush=True)
    selected = CASES if args.case == 'all' else {args.case: CASES[args.case]}
    for name, config_path in selected.items():
        folder = root / name
        folder.mkdir()
        (folder / 'output').mkdir()
        shutil.copy2(config_path, folder / 'param.ini')
        result = dict(name=name, directory=str(folder), status='FAIL',
                      config_sha256=sha256(config_path))
        started = time.perf_counter()
        try:
            config = read_config(config_path)
            command = [launcher, '-n', str(args.ranks), str(solver)]
            result['command'] = command
            print(f'Running {name} ({args.ranks} ranks)...', flush=True)
            with (folder / 'run.log').open('w', encoding='utf-8') as log:
                proc = subprocess.Popen(command, cwd=folder, env=env,
                                        stdout=log, stderr=subprocess.STDOUT)
                try:
                    result['exit_code'] = proc.wait(timeout=args.timeout)
                except (subprocess.TimeoutExpired, KeyboardInterrupt):
                    if os.name == 'nt':
                        subprocess.run(['taskkill', '/PID', str(proc.pid), '/T', '/F'],
                                       capture_output=True)
                    else:
                        proc.terminate()
                    proc.wait()
                    raise
            result['wall_seconds'] = time.perf_counter() - started
            if result['exit_code'] != 0:
                raise ValueError(f'solver exit code {result["exit_code"]}')
            result.update(inspect_run(folder, config, args.ranks))
            export_distance(folder)
            result['status'] = 'PASS'
        except (OSError, ValueError, ET.ParseError, subprocess.TimeoutExpired,
                KeyboardInterrupt) as error:
            result['error'] = str(error) or type(error).__name__
        summary['cases'].append(result)
        summary['status'] = 'PASS' if all(r['status'] == 'PASS'
                                        for r in summary['cases']) else 'FAIL'
        (root / 'summary.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
        print(f'{name}: {result["status"]}', flush=True)
        if result['status'] != 'PASS':
            print(result['error'], flush=True)
            return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
