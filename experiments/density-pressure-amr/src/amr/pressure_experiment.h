#pragma once
#include <cstdlib>
namespace PressureExperiment {
inline double finest_fraction(){static const double v=std::getenv("AMR_FINEST_PRESSURE_FRACTION")?std::atof(std::getenv("AMR_FINEST_PRESSURE_FRACTION")):0.;return v;}
inline bool halo() { static const bool value=std::getenv("AMR_PRESSURE_HALO")!=nullptr;return value; }
inline double halo_weight() { static const double value=std::getenv("AMR_PRESSURE_HALO_WEIGHT")?std::atof(std::getenv("AMR_PRESSURE_HALO_WEIGHT")):1.;return value; }
inline bool face_weight() { static const bool value=std::getenv("AMR_PRESSURE_FACE_WEIGHT")!=nullptr;return value; }
inline bool cell_weight() { static const bool value=std::getenv("AMR_PRESSURE_CELL_WEIGHT")!=nullptr;return value; }
inline bool trace() { static const bool value=std::getenv("AMR_PRESSURE_TRACE")!=nullptr;return value; }
inline bool target_family(int level, long long x, long long y) {
    if(!trace() || level!=5) return false;
    const long long targets[][2]={{838860800,234881024},{771751936,402653184},{704643072,503316480},{738197504,469762048},{805306368,335544320},{134217728,0}};
    for(const auto &v:targets) if((x==v[0]&&y==v[1])||(x==v[1]&&y==v[0])) return true;
    return false;
}
inline const char *&stage() { static const char *value="init";return value; }
inline double floor_factor() { static const double value=std::getenv("AMR_PRESSURE_FLOOR_FACTOR") ? std::atof(std::getenv("AMR_PRESSURE_FLOOR_FACTOR")) : 0.; return value; }
inline double &maximum_pressure() { static double value=0.; return value; }
inline int mode() { static const int value=std::getenv("AMR_PRESSURE_MODE") ? std::atoi(std::getenv("AMR_PRESSURE_MODE")) : 0; return value; }
inline double refine() { static const double value=std::getenv("AMR_PRESSURE_REFINE") ? std::atof(std::getenv("AMR_PRESSURE_REFINE")) : .05; return value; }
inline double coarsen() { static const double value=std::getenv("AMR_PRESSURE_COARSEN") ? std::atof(std::getenv("AMR_PRESSURE_COARSEN")) : .025; return value; }
inline bool keep(bool density, double pressure, double threshold) { return mode()==1 ? (density && pressure>threshold) : mode()==2 ? (density || pressure>threshold) : density; }
}
