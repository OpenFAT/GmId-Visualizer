#include "gmid/model_validator.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

namespace gmid {

// ---------------------------------------------------------------------------
// Compile-time needle tables
// ---------------------------------------------------------------------------

inline constexpr std::array mos_needles{
    std::string_view{"nmos"},  std::string_view{"pmos"},
    std::string_view{"mosfet"},std::string_view{"level="},
    std::string_view{"nfet"},  std::string_view{"pfet"},
    std::string_view{"vth0"},  std::string_view{"tox"},
};

inline constexpr std::array bjt_needles{
    std::string_view{"npn"},  std::string_view{"pnp"},
    std::string_view{"bjt"},  std::string_view{"is="},
    std::string_view{"bf="},  std::string_view{"br="},
    std::string_view{"vaf="}, std::string_view{"ikf="},
};

static constexpr std::size_t MAX_READ = 2'000'000;

// ---------------------------------------------------------------------------
// Implementation
// ---------------------------------------------------------------------------

std::expected<ModelKind, std::string>
validate_model_file(const std::filesystem::path& path) {

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
        return std::unexpected(std::string{"cannot open file: "} + path.string());

    const auto file_size = static_cast<std::size_t>(file.tellg());
    file.seekg(0);

    const std::size_t read_len = std::min(file_size, MAX_READ);
    std::string buf(read_len, '\0');
    file.read(buf.data(), static_cast<std::streamsize>(read_len));

    // Lower-case in place — single pass, branch-free on modern compilers
    std::ranges::transform(buf, buf.begin(),
        [](unsigned char c) -> char { return static_cast<char>(c | ((c >= 'A' && c <= 'Z') << 5)); });

    auto has_any = [&buf](auto const& needles) {
        for (auto n : needles)
            if (buf.find(n) != std::string::npos) return true;
        return false;
    };

    const bool has_mos = has_any(mos_needles);
    const bool has_bjt = has_any(bjt_needles);

    if (has_mos && !has_bjt) return ModelKind::mosfet;
    if (has_bjt && !has_mos) return ModelKind::bjt;
    if (has_mos && has_bjt)  return ModelKind::mosfet;   // tie-break: MOSFET wins
    return ModelKind::unknown;
}

std::expected<std::string, std::string>
extract_device_name(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file)
        return std::unexpected("cannot open: " + path.string());

    std::string line;
    while (std::getline(file, line)) {
        std::string lower = line;
        std::ranges::transform(lower, lower.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        const auto pos = lower.find(".model");
        if (pos == std::string::npos) continue;

        std::istringstream ss(line.substr(pos + 6));
        std::string name;
        if (ss >> name) return name;
    }
    return std::unexpected("no .model statement found in: " + path.string());
}

} // namespace gmid
