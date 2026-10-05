// Regression tests. Run: ctest --test-dir build
// The cgg test shells out to ngspice (on PATH in `nix build` / `nix develop`).
#undef NDEBUG  // asserts are the test, keep them in Release
#include "gmid/lib.hpp"
#include "gmid/model_validator.hpp"
#include "gmid/plots.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <format>
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
            for (auto* v : {&r.vbs, &r.gm, &r.gds, &r.cgs, &r.cgd, &r.vth, &r.cgg})
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

// cgg (FFI plot 6) must match an independent ngspice AC measurement of the
// gate: Im(I(Vgs)) / (2*pi*f) with 1 V AC on the gate and d/s/b AC-grounded.
// The DUT is a BSIM4 wrapped in a .subckt, like sky130, to cover the
// @m.x1.<inner>[cgg] path; nonzero cgso/cgdo cover the overlap term.
static void cgg_matches_ac_measurement() {
    const auto dir = fs::temp_directory_path() / "gmid_regression_cgg";
    fs::remove_all(dir);
    fs::create_directories(dir);
    const auto model = dir / "dut.spice";
    std::ofstream(model) <<
        ".subckt ndut d g s b l=1u w=1u\n"
        "mcore d g s b nbsim l={l} w={w}\n"
        ".model nbsim nmos level=54 version=4.5 cgso=3e-10 cgdo=3e-10 cgbo=1e-12\n"
        ".ends\n";

    GmidCharResult* r = gmid_characterise(model.c_str(), "ndut", "mosfet",
        (dir / "out").c_str(), nullptr, 0.0, 1.8, 181, 0.05, 1.8, 18, 10.0, 0.5, 27.0);
    assert(r && r->error[0] == '\0' && r->plot_count == 7);
    const GmidPlotResult& vgs = r->plots[4];   // x = VGS
    const GmidPlotResult& cgg = r->plots[6];   // x = gm/Id, y = Cgg
    assert(cgg.lut_len == vgs.lut_len && cgg.lut_len > 10);

    const double vds = 0.05 + 9 * (1.75 / 17);   // mid-VDS slice
    for (int k : {cgg.lut_len / 4, cgg.lut_len / 2, 3 * cgg.lut_len / 4}) {
        const double c = cgg.lut[k].y;
        assert(c > 1e-15 && c < 1e-12);   // F, positive, fF..pF for 10u x 0.5u

        const auto deck = dir / "ac.sp", data = dir / "ac_cgg";
        std::ofstream(deck) << std::format(
            "* cgg reference\n.include \"{}\"\n"
            "X1 d g 0 0 ndut W=10u L=0.5u\nVgs g 0 {} ac 1\nVds d 0 {}\n"
            ".control\nac lin 1 1k 1k\nlet c = -imag(i(Vgs))/(2*pi*1e3)\n"
            "wrdata {} c\nquit\n.endc\n.end\n", model.string(), vgs.lut[k].x, vds, data.string());
        assert(std::system(std::format("ngspice -b {} > {}.log 2>&1", deck.string(), deck.string()).c_str()) == 0);
        double f = 0, ref = 0;
        assert(std::ifstream(data) >> f >> ref);
        const double err = c / ref - 1.0;
        std::printf("  VGS=%.3f gm/Id=%.2f cgg=%.4g F  ac=%.4g F  err=%+.4f%%\n",
                    vgs.lut[k].x, cgg.lut[k].x, c, ref, 100 * err);
        assert(std::abs(err) < 0.02);
    }
    gmid_free_result(r);
    fs::remove_all(dir);
    std::puts("ok: cgg vs AC");
}

int main() {
    pmos_negative_id_gives_nonempty_slice();
    relative_model_file_resolves();
    cgg_matches_ac_measurement();
}
