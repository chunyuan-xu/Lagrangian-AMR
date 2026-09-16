#pragma once

namespace IOAlgorithm {

enum class OutputPhase {
    PreStep = 0,
    PostAccept = 1
};

struct OutputStamp {
    int file_step;
    int state_step;
    double time;
    OutputPhase phase;
};

inline OutputStamp make_pre_step_stamp(int current_step, double current_time)
{
    return OutputStamp{
        current_step,
        current_step > 0 ? current_step - 1 : 0,
        current_time,
        OutputPhase::PreStep
    };
}

inline int phase_code(OutputPhase phase)
{
    return static_cast<int>(phase);
}

} // namespace IOAlgorithm
