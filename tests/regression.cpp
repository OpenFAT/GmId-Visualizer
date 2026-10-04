// Regression tests — no simulator needed. Run: ctest --test-dir build
#undef NDEBUG  // asserts are the test, keep them in Release
#include "gmid/model_validator.hpp"
#include "gmid/plots.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

// PMOS sweeps VGS/VDS negative, so ngspice reports a negative drain current.
// The extractor used to drop every point with `id <= 1e-12`, returning empty LUTs.
static void pmos_negative_id_gives_nonempty_slice() {
    gmid::SweepConfig cfg;
    cfg.vgs_start = 0.0;  cfg.vgs_stop = -1.8; cfg.vgs_steps = 19;
    cfg.vds_start = -0.05; cfg.vds_stop = -1.8; cfg.vds_steps = 5;

    // Square-law-ish PMOS: id = -k (|vgs| - vt)^2 (1 + lambda |vds|)
    gmid::SweepResult r;
    const double vgs_step = (cfg.vgs_stop - cfg.vgs_start) / (cfg.vgs_steps - 1);
    const double vds_step = (cfg.vds_stop - cfg.vds_start) / (cfg.vds_steps - 1);
    for (int j = 0; j < cfg.vds_steps; ++j)
        for (int i = 0; i < cfg.vgs_steps; ++i) {
            const double vgs = cfg.vgs_start + i * vgs_step;
            const double vds = cfg.vds_start + j * vds_step;
            const double ov  = std::max(std::abs(vgs) - 0.4, 0.0) + 1e-3;
            r.vgs.push_back(vgs);
            r.vds.push_back(vds);
            r.id.push_back(-1e-4 * ov * ov * (1.0 + 0.1 * std::abs(vds)));
            for (auto* v : {&r.vbs, &r.gm, &r.gds, &r.cgs, &r.cgd, &r.vth})
                v->push_back(0.0);
        }

    const auto sl = gmid::extract_mosfet_slice(r, cfg);
    assert(!sl.gmid.empty());
    for (std::size_t k = 0; k < sl.gmid.size(); ++k) {
        assert(sl.id[k] > 0.0 && sl.jd[k] > 0.0 && sl.gmid[k] > 0.0);
        assert(sl.av[k] > 0.0);
    }
    std::puts("ok: pmos negative id");
}

// A bare relative model filename has an empty parent_path(), and iterating ""
// threw `directory iterator cannot open directory: []`.
static void relative_model_file_resolves() {
    const auto dir = fs::temp_directory_path() / "gmid_regression_rel";
    fs::create_directories(dir);
    std::ofstream(dir / "dev.spice") << ".model nch nmos level=1 vto=0.4\n";
    std::ofstream(dir / "dev__mismatch.spice") << "* mismatch\n";

    const auto old = fs::current_path();
    fs::current_path(dir);
    auto r = gmid::resolve_model_with_deps("dev.spice");
    fs::current_path(old);

    assert(r.has_value());
    assert(r->find(".model nch") != std::string::npos);
    assert(r->find("* mismatch") != std::string::npos);
    fs::remove_all(dir);
    std::puts("ok: relative model_file");
}

int main() {
    pmos_negative_id_gives_nonempty_slice();
    relative_model_file_resolves();
}
