import os
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CXX = Path("C:/msys64/ucrt64/bin/g++.exe")

# The header's inline lifecycle methods reference the p4est/sc runtime. These
# stubs both satisfy the linker and let the observer fixture exercise real
# GhostSession initialize/exchange/rebuild sequencing without an MPI forest.
# Signatures match the real C-linkage declarations and follow the includes so
# the p4est types are in scope.
STUBS = r'''
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include "diagnostics/memory_probe_observer.h"

static p4est_ghost_t fake_ghost;
static p4est_locidx_t fake_mirror_proc_offsets[3];
static int exchange_call_count = 0;
static int memory_used_call_count = 0;

extern "C" {
    int p4est_package_id = 0;
    void *sc_malloc(int package, size_t size) { (void)package; return size == 0 ? NULL : std::malloc(size); }
    void sc_free(int package, void *ptr) { (void)package; std::free(ptr); }
    p4est_ghost_t *p4est_ghost_new(p4est_t *p4est, p4est_connect_type_t conn) { (void)p4est; (void)conn; return &fake_ghost; }
    void p4est_ghost_destroy(p4est_ghost_t *ghost) { (void)ghost; }
    size_t p4est_ghost_memory_used(p4est_ghost_t *ghost) { (void)ghost; ++memory_used_call_count; return 777; }
    void p4est_ghost_exchange_data(p4est_t *p4est, p4est_ghost_t *ghost, void *user_data) {
        (void)p4est; (void)user_data;
        ++exchange_call_count;
        ghost->ghosts.elem_count = 99;
        ghost->mirror_proc_offsets[2] = 100;
    }
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

TYPE_SOURCE = STUBS + r'''
void observe(void *context, const GhostExchangeObservation &observation)
{
    std::uint64_t *generation = static_cast<std::uint64_t *>(context);
    *generation = observation.generation;
}

int main()
{
    static_assert(sizeof(GhostExchangeObservation) == 7 * sizeof(std::uint64_t),
        "observation must remain seven independent uint64_t scalars");
    static_assert(sizeof(GhostSession) == 64,
        "observer storage layout changed unexpectedly");
    GhostExchangeObservation observation = {3, 11, 5, 4, 6, 5896, 777};
    std::uint64_t generation = 0;
    GhostExchangeObserver observer = observe;
    observer(&generation, observation);
    assert(generation == 3);

    GhostSession session;
    assert(!session.has_exchange_observer());
    assert(session.exchange_observer() == NULL);
    assert(session.exchange_observer_context() == NULL);
    session.set_exchange_observer(observer, &generation);
    assert(session.has_exchange_observer());
    assert(session.exchange_observer() == observer);
    assert(session.exchange_observer_context() == &generation);
    session.destroy();
    assert(session.has_exchange_observer());
    assert(session.exchange_observer() == observer);
    assert(session.exchange_observer_context() == &generation);
    session.set_exchange_observer(NULL, NULL);
    assert(!session.has_exchange_observer());
    assert(session.exchange_observer() == NULL);
    assert(session.exchange_observer_context() == NULL);

    int step = 0;
    _putenv_s("LAGRANGIAN_MEMORY_HIGH_WATER", "");
    Diagnostics::MemoryProbeOwner unset_owner(&step);
    assert(!unset_owner.enabled());
    _putenv_s("LAGRANGIAN_MEMORY_HIGH_WATER", "0");
    Diagnostics::MemoryProbeOwner zero_owner(&step);
    assert(!zero_owner.enabled());
    _putenv_s("LAGRANGIAN_MEMORY_HIGH_WATER", "true");
    Diagnostics::MemoryProbeOwner invalid_owner(&step);
    assert(!invalid_owner.enabled());
    _putenv_s("LAGRANGIAN_MEMORY_HIGH_WATER", "1");
    Diagnostics::MemoryProbeOwner enabled_owner(&step);
    assert(enabled_owner.enabled());
    enabled_owner.bind(session);
    assert(session.exchange_observer() == Diagnostics::observe_memory_exchange);
    assert(session.exchange_observer_context() == enabled_owner.context());
    assert(enabled_owner.context()->tracker->values().completed_exchange_count == 0);
    _putenv_s("LAGRANGIAN_MEMORY_HIGH_WATER", "");
    return 0;
}
'''

OBSERVER_SOURCE = STUBS + r'''
struct ObserverState {
    int calls;
    int exchange_count_seen;
    GhostExchangeObservation observations[3];
};

void record_observation(void *context,
    const GhostExchangeObservation &observation)
{
    ObserverState *state = static_cast<ObserverState *>(context);
    assert(state->calls < 3);
    state->observations[state->calls] = observation;
    ++state->calls;
    state->exchange_count_seen = exchange_call_count;
}

void set_schedule(p4est_t &forest, size_t ghost_count, size_t mirror_count,
    p4est_locidx_t logical_send_count)
{
    std::memset(&fake_ghost, 0, sizeof(fake_ghost));
    forest.mpisize = 2;
    forest.local_num_quadrants = 11;
    forest.data_size = sizeof(quad_data_t);
    fake_ghost.ghosts.elem_count = ghost_count;
    fake_ghost.mirrors.elem_count = mirror_count;
    fake_mirror_proc_offsets[0] = 0;
    fake_mirror_proc_offsets[1] = 2;
    fake_mirror_proc_offsets[2] = logical_send_count;
    fake_ghost.mirror_proc_offsets = fake_mirror_proc_offsets;
}

int main()
{
    p4est_t forest = {};
    set_schedule(forest, 3, 4, 5);

    GhostSession unobserved;
    Diagnostics::initialize_selected(
        unobserved, &forest, P4EST_CONNECT_FULL);
    assert(exchange_call_count == 1);
    assert(memory_used_call_count == 0);
    unobserved.destroy();

    set_schedule(forest, 3, 4, 5);
    ObserverState state = {};
    GhostSession observed;
    observed.set_exchange_observer(record_observation, &state);
    Diagnostics::initialize_selected(
        observed, &forest, P4EST_CONNECT_FULL);
    assert(exchange_call_count == 2);
    assert(memory_used_call_count == 1);
    assert(state.calls == 1);
    assert(state.exchange_count_seen == 2);
    assert(state.observations[0].generation == 1);
    assert(state.observations[0].ghost_leaves == 99);
    assert(state.observations[0].logical_send_entries == 100);

    set_schedule(forest, 3, 4, 5);
    Diagnostics::exchange_selected(observed, &forest);
    assert(exchange_call_count == 3);
    assert(memory_used_call_count == 2);
    assert(state.calls == 2);
    assert(state.exchange_count_seen == 3);
    assert(state.observations[1].generation == 1);
    assert(state.observations[1].local_leaves == 11);
    assert(state.observations[1].ghost_leaves == 3);
    assert(state.observations[1].mirror_leaves == 4);
    assert(state.observations[1].logical_send_entries == 5);
    assert(state.observations[1].payload_bytes == sizeof(quad_data_t));
    assert(state.observations[1].p4est_reported_ghost_bytes == 777);
    assert(fake_ghost.ghosts.elem_count == 99);
    assert(fake_ghost.mirror_proc_offsets[2] == 100);

    set_schedule(forest, 6, 7, 8);
    Diagnostics::rebuild_selected(
        observed, &forest, P4EST_CONNECT_FULL);
    assert(observed.exchange_observer() == record_observation);
    assert(observed.exchange_observer_context() == &state);
    assert(exchange_call_count == 4);
    assert(memory_used_call_count == 3);
    assert(state.calls == 3);
    assert(state.exchange_count_seen == 4);
    assert(state.observations[2].generation == 2);
    assert(state.observations[2].ghost_leaves == 99);
    assert(state.observations[2].mirror_leaves == 7);
    assert(state.observations[2].logical_send_entries == 100);

    observed.set_exchange_observer(NULL, NULL);
    Diagnostics::exchange_selected(observed, &forest);
    assert(exchange_call_count == 5);
    assert(memory_used_call_count == 3);
    assert(state.calls == 3);
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
    temporary_root = ROOT / ".tmp"
    temporary_root.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(
        prefix=f"lagrangian_ghost_{name}_", dir=temporary_root
    ) as directory:
        source = Path(directory) / "test.cpp"
        executable = Path(directory) / "test.exe"
        source.write_text(source_text, encoding="utf-8")
        environment = dict(os.environ)
        environment["PATH"] = os.pathsep.join([
            "C:/msys64/usr/bin",
            "C:/msys64/ucrt64/bin",
            environment.get("PATH", ""),
        ])
        environment["TEMP"] = str(directory)
        environment["TMP"] = str(directory)
        environment["TMPDIR"] = str(directory)
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

    observer_type = compile_and_run(TYPE_SOURCE, "observer_type")
    if observer_type.returncode != 0:
        print("FAIL: GhostSession observer type contract")
        return 1

    observer = compile_and_run(OBSERVER_SOURCE, "observer")
    if observer.returncode != 0:
        print("FAIL: GhostSession exchange observer contract")
        return 1

    abort = compile_and_run(ABORT_SOURCE, "abort")
    if abort.returncode == 0:
        print("FAIL: accessing an invalid GhostSession did not abort in debug build")
        return 1

    print("PASS: GhostSession lifecycle state + stale-access detection")
    return 0


if __name__ == "__main__":
    sys.exit(main())
