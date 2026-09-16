"""Isolated, manifest-backed sensor comparisons; no reference updates."""
import argparse, hashlib, json, os, re, shutil, subprocess, time
from pathlib import Path

PACKAGE=Path(__file__).resolve().parents[1]
REPO=PACKAGE.parents[1]
def digest(p): return hashlib.sha256(p.read_bytes()).hexdigest()

def run(case,mode,end,refine,coarsen,label,limit):
    out=REPO/'.tmp/weno-linear-study-20260916'/label
    out.mkdir(parents=True,exist_ok=False)
    (out/'output').mkdir()
    preset_case=case if case in ('noh','sedov') else 'sedov'
    config=(PACKAGE/'configs'/(preset_case+'.ini')).read_text()
    for key,val in [('which_case',dict(noh='NohCartesian',sedov='SedovCartesian',sod='SodCartesian',sod1d='Sod1DCartesian')[case]),
                    ('minus_level',5),('max_level',8),('end_time',end),('write_interval_time',min(end/5,.01))]:
        config=re.sub(rf'(?m)^{key}\s*=.*$',f'{key} = {val}',config)
    (out/'param.ini').write_text(config)
    presets=json.loads((PACKAGE/'configs/presets.json').read_text())
    setting=presets['common_environment'].copy()
    setting.update({k:v for k,v in presets['cases'][preset_case]['environment'].items()
                    if not k.endswith('_AUDIT')})
    # All routes, including matched mode=0 control, use identical hydro/transfer settings.
    setting.update(AMR_WENO_SENSOR=str(int(mode!=0)),AMR_WENO_MODE=str(mode),
                   AMR_WENO_REFINE=str(refine),AMR_WENO_COARSEN=str(coarsen))
    env={k:v for k,v in os.environ.items() if not k.startswith('AMR_') and k!='LAGRANGIAN_CHECK_STATE_INVARIANTS'}
    env.update(setting)
    env['PATH']='C:/msys64/ucrt64/bin;C:/Program Files/Microsoft MPI/Bin;'+env['PATH']
    exe=out/'AMR_Solver.exe'
    shutil.copy2(PACKAGE/'bin/AMR_Solver.exe',exe)
    shutil.copytree(PACKAGE/'src',out/'source')
    manifest=dict(case=case,mode=mode,end=end,screen_time_limit=limit,environment=setting,exe_sha256=digest(exe),
                  config_sha256=digest(out/'param.ini'),
                  source_sha256={str(p.relative_to(out/'source')):digest(p) for p in (out/'source').rglob('*') if p.is_file()})
    (out/'manifest.json').write_text(json.dumps(manifest,indent=2))
    started=time.perf_counter()
    with (out/'solver.log').open('wb') as stdout, (out/'solver.err').open('wb') as stderr:
        proc=subprocess.Popen(['C:/Program Files/Microsoft MPI/Bin/mpiexec.exe','-n','1',str(exe)],cwd=out,env=env,stdout=stdout,stderr=stderr)
        (out/'process.json').write_text(json.dumps(dict(pid=proc.pid)))
        timed_out=False
        try:
            code=proc.wait(timeout=limit or None)
        except subprocess.TimeoutExpired:
            # Kill only this runner's owned MPI process tree, never other runs.
            timed_out=True
            subprocess.run(['taskkill','/PID',str(proc.pid),'/T','/F'],stdout=subprocess.DEVNULL)
            code=proc.wait()
    log=(out/'solver.log').read_text(errors='replace')
    steps=re.findall(r'simulation_step=\s*(\d+), delta_time = ([^,]+), simulation_time = (\S+)',log)
    counts=[int(x) for x in re.findall(r'Done p4est_(?:refine|coarsen|balance)[^\n]*with (\d+) total quadrants',log)]
    energies=[abs(float(x)) for x in re.findall(r'the total energy error is (\S+)',log)]
    result=dict(case=case,mode=mode,exit=code,screen_timeout=timed_out,wall_seconds=time.perf_counter()-started,
        last_time=float(steps[-1][2]) if steps else None,steps=int(steps[-1][0]) if steps else None,
        max_cells=max(counts) if counts else None,last_logged_cells=counts[-1] if counts else None,
        max_energy_error=max(energies) if energies else None,
        completed=code==0 and bool(steps) and float(steps[-1][2])>=end and '[invariant-checker] enabled' in log
            and not re.search(r'\bnan\b|STATE INVARIANT|job aborted|invalid least-squares',log,re.I))
    (out/'result.json').write_text(json.dumps(result,indent=2))
    print(label,json.dumps(result),flush=True)

if __name__=='__main__':
    p=argparse.ArgumentParser()
    p.add_argument('--cases',nargs='+',choices=['noh','sedov','sod','sod1d'],default=['noh','sedov','sod'])
    p.add_argument('--modes',nargs='+',type=int,choices=[0,4,5,6],default=[0,6])
    p.add_argument('--end',type=float,default=.05)
    p.add_argument('--refine',type=float,default=.2)
    p.add_argument('--coarsen',type=float,default=.19)
    p.add_argument('--prefix',required=True)
    p.add_argument('--limit',type=float,default=180,help='Seconds per early screen; 0 disables timeout for full runs.')
    a=p.parse_args()
    for c in a.cases:
        for m in a.modes: run(c,m,a.end,a.refine,a.coarsen,f'{a.prefix}-{c}-m{m}',a.limit)
