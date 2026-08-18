import os
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CXX = Path("C:/msys64/ucrt64/bin/g++.exe")

SOURCE = r'''
#include <limits>
#include "defines.h"

int main()
{
    p4est_data_t legacy;
    legacy.which_case = ProblemNo::SodCartesian;
    legacy.start_time = 0.25;
    legacy.end_time = 1.25;
    legacy.current_time = 0.5;
    legacy.delta_time = 0.01;
    legacy.dt_iter = 0.005;
    legacy.used_dt = 0.01;
    legacy.current_step = 7;
    legacy.max_time_step = 99;
    legacy.minus_level = 3;
    legacy.max_level = 8;
    legacy.write_interval_time = 0.2;
    legacy.write_interval_step = 10;
    legacy.refine_coarsen_enum = RefineCriteria::Distance;
    legacy.distance_shock_radius_scale = 0.83;
    legacy.distance_shock_radius_exponent = 0.5;
    legacy.distance_band_half_width = 0.22;

    const SimulationModel::SimulationConfig config =
        legacy.simulation_config();
    const SimulationModel::SimulationClock clock =
        legacy.simulation_clock();

    if (config.problem != legacy.which_case ||
        config.mesh.minimum_level != legacy.minus_level ||
        config.mesh.maximum_level != legacy.max_level ||
        config.mesh.distance_shock_radius_scale != legacy.distance_shock_radius_scale ||
        config.mesh.distance_shock_radius_exponent != legacy.distance_shock_radius_exponent ||
        config.mesh.distance_band_half_width != legacy.distance_band_half_width ||
        config.output.write_interval_step != legacy.write_interval_step ||
        config.solver.solver_type != legacy.solver_type) {
        return 1;
    }
    if (clock.current_time != legacy.current_time ||
        clock.delta_time != legacy.delta_time ||
        clock.stage_timestep != legacy.dt_iter ||
        clock.current_step != legacy.current_step) {
        return 2;
    }
    if (!SimulationModel::valid(config) ||
        !SimulationModel::valid(clock)) {
        return 3;
    }

    SimulationModel::SimulationConfig invalid = config;
    invalid.mesh.maximum_level = invalid.mesh.minimum_level - 1;
    if (SimulationModel::valid(invalid)) {
        return 4;
    }
    invalid = config;
    invalid.output.write_interval_step = 0;
    if (SimulationModel::valid(invalid)) {
        return 5;
    }
    invalid = config;
    invalid.solver.cfl = std::numeric_limits<double>::quiet_NaN();
    if (SimulationModel::valid(invalid)) {
        return 6;
    }
    invalid = config;
    invalid.mesh.distance_shock_radius_scale = 0.0;
    if (SimulationModel::valid(invalid)) {
        return 66;
    }

    legacy.write_interval_step = 0;
    if (legacy.has_valid_simulation_settings()) {
        return 7;
    }

    legacy.write_interval_step = 10;
    legacy.which_case = 999;
    if (legacy.has_valid_simulation_settings()) {
        return 8;
    }

    p4est_data_t loaded;
    IOAlgorithm::ConfigParser parser("typed_config_test.ini");
    loaded.load_from_config(parser);
    if (loaded.start_time != 0.75 || loaded.current_time != 0.75 ||
        loaded.distance_shock_radius_scale != 0.83 ||
        loaded.distance_shock_radius_exponent != 0.5 ||
        loaded.distance_band_half_width != 0.22 ||
        !loaded.has_valid_simulation_settings()) {
        return 9;
    }

    return 0;
}
'''


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="lagrangian_config_test_") as directory:
        source = Path(directory) / "test.cpp"
        executable = Path(directory) / "test.exe"
        source.write_text(SOURCE, encoding="utf-8")
        config = Path(directory) / "typed_config_test.ini"
        config.write_text(
            "start_time = 0.75\nend_time = 1.25\n"
            "refine_coarsen_enum = 5\n"
            "distance_shock_radius_scale = 0.83\n"
            "distance_shock_radius_exponent = 0.5\n"
            "distance_band_half_width = 0.22\n",
            encoding="utf-8")
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
                f"-I{ROOT / 'src'}",
                f"-I{ROOT / 'third_party/p4est/build/local/include'}",
                str(source),
                str(ROOT / "src/io/config_parser.cpp"),
                "-o",
                str(executable),
            ],
            cwd=ROOT,
            env=environment,
            capture_output=True,
            text=True,
        )
        if compile_result.returncode != 0:
            print(compile_result.stdout + compile_result.stderr)
            return compile_result.returncode
        run_result = subprocess.run(
            [str(executable)], cwd=directory, env=environment,
            capture_output=True, text=True,
        )
        if run_result.returncode != 0:
            print(run_result.stdout + run_result.stderr)
            print(f"typed config test executable returned {run_result.returncode}")
            return 1

    print("PASS: typed config and clock preserve legacy values and validate boundaries")
    return 0


if __name__ == "__main__":
    sys.exit(main())
