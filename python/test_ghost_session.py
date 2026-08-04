import os
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CXX = Path("C:/msys64/ucrt64/bin/g++.exe")

# The header's inline lifecycle methods emit references to the p4est/sc
# runtime (P4EST_ALLOC/FREE, p4est_ghost_new/destroy/exchange_data). The test
# never builds a session, so none of these stubs are ever invoked -- they only
# satisfy the linker. Signatures match the real C-linkage declarations. They
# are placed AFTER the includes so the p4est types are in scope.
STUBS = r'''
#include <cassert>
#include <cstddef>
#include "mesh/ghost_session.h"

extern "C" {
    int p4est_package_id = 0;
    void *sc_malloc(int package, size_t size) { (void)package; (void)size; return 0; }
    void sc_free(int package, void *ptr) { (void)package; (void)ptr; }
    p4est_ghost_t *p4est_ghost_new(p4est_t *p4est, p4est_connect_type_t conn) { (void)p4est; (void)conn; return 0; }
    void p4est_ghost_destroy(p4est_ghost_t *ghost) { (void)ghost; }
    void p4est_ghost_exchange_data(p4est_t *p4est, p4est_ghost_t *ghost, void *user_data) { (void)p4est; (void)ghost; (void)user_data; }
}

'''

# Part 1: state-machine transitions (no p4est linking needed -- the tested
# methods are header-inline and never touch a real forest).
STATE_SOURCE = STUBS + r'''int main()
{
    // Fresh session is empty, invalid, generation 0.
    GhostSession s;
    assert(s.empty());
    assert(!s.valid());
    assert(s.generation() == 0);
    assert(s.topology_version() == 0);

    // Marking a topology change increments the version and leaves it
    // invalid until the next initialize()/rebuild().
    s.invalidate_after_topology_change();
    assert(s.topology_version() == 1);
    assert(!s.valid());
    assert(s.empty());

    s.invalidate_after_topology_change();
    assert(s.topology_version() == 2);
    assert(!s.valid());

    // destroy() on an empty session is a no-op and stays invalid/empty.
    s.destroy();
    assert(s.empty());
    assert(!s.valid());

    return 0;
}
'''

# Part 2: accessing an invalid (never-built / invalidated) session must abort
# in a debug build (assert active because no NDEBUG). Run as a subprocess and
# expect a non-zero exit.
ABORT_SOURCE = STUBS + r'''int main()
{
    GhostSession s;   // uninitialized: empty + invalid
    (void)s.get();    // assert(valid_) fires -> SIGABRT
    return 0;
}
'''


def compile_and_run(source_text: str, name: str) -> subprocess.CompletedProcess:
    with tempfile.TemporaryDirectory(prefix=f"lagrangian_ghost_{name}_") as directory:
        source = Path(directory) / "test.cpp"
        executable = Path(directory) / "test.exe"
        source.write_text(source_text, encoding="utf-8")
        environment = dict(os.environ)
        environment["PATH"] = os.pathsep.join([
            "C:/msys64/usr/bin",
            "C:/msys64/ucrt64/bin",
            environment.get("PATH", ""),
        ])
        compile_result = subprocess.run(
            [
                str(CXX),
                "-std=c++14",
                "-Wall",
                "-Wextra",
                f"-I{ROOT / 'src'}",
                f"-I{ROOT / 'third_party/p4est/build/local/include'}",
                str(source),
                "-o",
                str(executable),
            ],
            cwd=ROOT,
            env=environment,
            capture_output=True,
            text=True,
        )
        if compile_result.returncode != 0:
            print(f"[{name}] compile failed:")
            print(compile_result.stdout + compile_result.stderr)
            return compile_result
        run_result = subprocess.run([str(executable)], cwd=ROOT, env=environment)
        return run_result


def main() -> int:
    state = compile_and_run(STATE_SOURCE, "state")
    if state.returncode != 0:
        print("FAIL: GhostSession state transitions")
        return 1

    abort = compile_and_run(ABORT_SOURCE, "abort")
    if abort.returncode == 0:
        print("FAIL: accessing an invalid GhostSession did not abort in debug build")
        return 1

    print("PASS: GhostSession lifecycle state + stale-access detection")
    return 0


if __name__ == "__main__":
    sys.exit(main())
