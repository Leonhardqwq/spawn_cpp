#define NOMINMAX
#define PVZ_QUICK_EXTERNAL_ARRAY

#include "../inc/quick_seed_finder.h"
#include "../inc/sun_model.hpp"

#include <cmath>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace {

fs::path executable_path;

uint64_t parse_uint(const std::string& text) {
    int base = 10;
    if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
        base = 16;
    else if (text.find_first_of("abcdefABCDEF") != std::string::npos)
        base = 16;
    size_t end = 0;
    auto value = std::stoull(text, &end, base);
    if (end != text.size()) throw std::runtime_error("invalid integer: " + text);
    return value;
}

std::string hex_seed(uint32_t seed) {
    std::ostringstream out;
    out << "0x" << std::uppercase << std::hex << std::setw(8)
        << std::setfill('0') << seed;
    return out.str();
}

json hex_seeds(const std::vector<uint32_t>& seeds) {
    json result = json::array();
    for (uint32_t seed : seeds) result.push_back(hex_seed(seed));
    return result;
}

uint64_t uint_value(const json& value) {
    return value.is_string() ? parse_uint(value.get<std::string>()) : value.get<uint64_t>();
}

uint64_t uint_value(const json& object, const char* key, uint64_t fallback) {
    auto it = object.find(key);
    return it == object.end() ? fallback : uint_value(*it);
}

Scene parse_scene(std::string name) {
    std::transform(name.begin(), name.end(), name.begin(), ::toupper);
    if (name == "DE") return DAY;
    if (name == "NE") return NIGHT;
    if (name == "PE") return POOL;
    if (name == "RE") return ROOF;
    throw std::runtime_error("scene must be DE, NE, PE, or RE");
}

std::string scene_name(Scene scene) {
    static const char* names[] = {"DE", "NE", "PE", "RE"};
    return names[scene];
}

json read_json(const fs::path& path) {
    std::ifstream input(path);
    if (!input) throw std::runtime_error("cannot open " + path.string());
    json result;
    input >> result;
    return result;
}

fs::path data_path(const std::string& scene) {
    const auto file = scene + ".bin";
    std::vector<fs::path> roots = {
        fs::current_path(),
        fs::current_path().parent_path(),
        executable_path.parent_path(),
        executable_path.parent_path().parent_path(),
        executable_path.parent_path().parent_path().parent_path()
    };
    for (const auto& root : roots) {
        auto path = root / "data" / file;
        if (fs::exists(path)) return fs::canonical(path);
    }
    throw std::runtime_error("data/" + file + " not found");
}

class MappedBin {
#ifdef _WIN32
    HANDLE file_ = INVALID_HANDLE_VALUE;
    HANDLE mapping_ = nullptr;
#else
    int file_ = -1;
#endif
    uint32_t* data_ = nullptr;

public:
    explicit MappedBin(const fs::path& path) {
        constexpr uint64_t expected = array_size * sizeof(uint32_t);
#ifdef _WIN32
        file_ = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file_ == INVALID_HANDLE_VALUE)
            throw std::runtime_error("cannot open " + path.string());
        LARGE_INTEGER size{};
        if (!GetFileSizeEx(file_, &size) || uint64_t(size.QuadPart) != expected)
            throw std::runtime_error("source must contain exactly 2^32 uint32 values: " + path.string());
        mapping_ = CreateFileMappingW(file_, nullptr, PAGE_READONLY, 0, 0, nullptr);
        if (!mapping_) throw std::runtime_error("cannot map " + path.string());
        data_ = static_cast<uint32_t*>(MapViewOfFile(mapping_, FILE_MAP_READ, 0, 0, 0));
        if (!data_) throw std::runtime_error("cannot map " + path.string());
#else
        file_ = open(path.c_str(), O_RDONLY);
        if (file_ < 0) throw std::runtime_error("cannot open " + path.string());
        struct stat info{};
        if (fstat(file_, &info) || uint64_t(info.st_size) != expected)
            throw std::runtime_error("source must contain exactly 2^32 uint32 values: " + path.string());
        data_ = static_cast<uint32_t*>(mmap(nullptr, expected, PROT_READ, MAP_SHARED, file_, 0));
        if (data_ == MAP_FAILED) {
            data_ = nullptr;
            throw std::runtime_error("cannot map " + path.string());
        }
#endif
        array = data_;
    }

    ~MappedBin() {
        if (array == data_) array = nullptr;
#ifdef _WIN32
        if (data_) UnmapViewOfFile(data_);
        if (mapping_) CloseHandle(mapping_);
        if (file_ != INVALID_HANDLE_VALUE) CloseHandle(file_);
#else
        if (data_) munmap(data_, array_size * sizeof(uint32_t));
        if (file_ >= 0) close(file_);
#endif
    }

    MappedBin(const MappedBin&) = delete;
    MappedBin& operator=(const MappedBin&) = delete;
};

uint32_t project(uint32_t has, uint32_t factor_mask, uint32_t density_threshold) {
    BasicSeedFinder finder;
    uint32_t result = has & factor_mask & 0x000fffff;
    if ((factor_mask & (1u << 20)) && finder.weight_has(has) < density_threshold)
        result |= 1u << 20;
    return result;
}

struct Candidate {
    uint32_t rng_seed = 0;
    std::vector<uint32_t> codes;
    bool found = false;
};

struct SearchSpec {
    std::string name;
    Scene scene;
    uint32_t start_rng_seed;
    uint64_t find_len;
};

SearchSpec search_spec(const json& request) {
    std::string name = request.at("scene").get<std::string>();
    Scene scene = parse_scene(name);
    uint64_t find_len = uint_value(request, "find_len", array_size);
    if (!find_len || find_len > array_size)
        throw std::runtime_error("find_len must be in [1, 2^32]");
    uint64_t threads = uint_value(request, "threads", MAX_THREAD);
    if (!threads || threads > 1024) throw std::runtime_error("threads must be in [1, 1024]");
    MAX_THREAD = unsigned(threads);
    return {scene_name(scene), scene,
            uint32_t(uint_value(request, "start_rng_seed", 0)), find_len};
}

std::vector<uint32_t> uint_list(const json& values) {
    std::vector<uint32_t> result;
    result.reserve(values.size());
    for (const auto& value : values) result.push_back(uint32_t(uint_value(value)));
    return result;
}

json take_results(uint32_t start_rng_seed, uint64_t find_len) {
    auto raw = std::move(final_results);
    final_results.clear();
    std::sort(raw.begin(), raw.end());
    json results = json::array();
    for (const auto& info : raw) {
        std::vector<uint32_t> seeds;
        for (uint32_t seed : info.seeds) {
            uint64_t offset = uint32_t(uint64_t(seed - start_rng_seed) * inv);
            if (offset < find_len) seeds.push_back(seed);
        }
        std::sort(seeds.begin(), seeds.end(), [&](uint32_t a, uint32_t b) {
            return uint32_t(uint64_t(a - start_rng_seed) * inv) <
                   uint32_t(uint64_t(b - start_rng_seed) * inv);
        });
        seeds.erase(std::unique(seeds.begin(), seeds.end()), seeds.end());
        if (!seeds.empty())
            results.push_back({
                {"metric", info.metric},
                {"seed_count", info.seed_count},
                {"stored_seed_count", seeds.size()},
                {"seeds", hex_seeds(seeds)}
            });
    }
    return results;
}

json search_exact(const json& request) {
    auto spec = search_spec(request);
    auto masks = uint_list(request.at("masks"));
    if (masks.empty()) throw std::runtime_error("masks must not be empty");
    if (std::any_of(masks.begin(), masks.end(), [](uint32_t mask) { return mask >> 19; }))
        throw std::runtime_error("exact masks may only use bits 0..18");

    MappedBin data(data_path(spec.name));
    final_results.clear();
    MultiQuickKMPFinder finder(spec.start_rng_seed, spec.find_len, spec.scene,
                               std::move(masks), false);
    finder.multi_thread_find_kmp();
    return {
        {"search", "exact"}, {"scene", spec.name},
        {"start_rng_seed", hex_seed(spec.start_rng_seed)}, {"find_len", spec.find_len},
        {"results", take_results(spec.start_rng_seed, spec.find_len)}
    };
}

json search_score(const json& request) {
    auto spec = search_spec(request);
    auto constraints = uint_list(request.at("constraints"));
    auto weights = request.at("weights").get<std::vector<int>>();
    uint64_t window = uint_value(request, "window", 50);
    if (constraints.empty() || constraints.size() != weights.size())
        throw std::runtime_error("constraints and weights must be non-empty and equally sized");
    if (std::any_of(constraints.begin(), constraints.end(), [](uint32_t bit) { return bit >= 26; }))
        throw std::runtime_error("score constraints must be zombie bits 0..25");
    if (std::set<uint32_t>(constraints.begin(), constraints.end()).size() != constraints.size())
        throw std::runtime_error("score constraints must be unique");
    if (!window || window > std::numeric_limits<uint32_t>::max())
        throw std::runtime_error("window must be in [1, 2^32-1]");
    uint64_t score_bound = 0;
    for (int weight : weights)
        score_bound += weight < 0 ? uint64_t(-int64_t(weight)) : uint64_t(weight);
    if (score_bound && window > uint64_t(std::numeric_limits<int>::max()) / score_bound)
        throw std::runtime_error("window and weights can overflow the legacy int score metric");

    MappedBin data(data_path(spec.name));
    final_results.clear();
    MultiQuickScoreSeedFinder finder(spec.start_rng_seed, spec.find_len, window,
        spec.scene, std::move(constraints), std::move(weights), false);
    finder.multi_thread_find_score();
    return {
        {"search", "score"}, {"scene", spec.name},
        {"start_rng_seed", hex_seed(spec.start_rng_seed)}, {"find_len", spec.find_len},
        {"window", window}, {"results", take_results(spec.start_rng_seed, spec.find_len)}
    };
}

json search_run(const json& request) {
    auto spec = search_spec(request);
    auto masks = uint_list(request.at("masks"));
    auto fmasks = uint_list(request.at("fmasks"));
    if (std::any_of(masks.begin(), masks.end(), [](uint32_t mask) { return mask >> 26; }) ||
        std::any_of(fmasks.begin(), fmasks.end(), [](uint32_t mask) { return mask >> 26; }))
        throw std::runtime_error("run masks may only use bits 0..25");

    MappedBin data(data_path(spec.name));
    const uint32_t index = uint32_t(uint64_t(spec.start_rng_seed) * inv);
    const auto matches = [&](uint32_t has) {
        const bool required = masks.empty() || std::any_of(masks.begin(), masks.end(),
            [&](uint32_t mask) { return (has & mask) == mask; });
        const bool excluded = fmasks.empty() || std::any_of(fmasks.begin(), fmasks.end(),
            [&](uint32_t mask) { return (has & mask) == 0; });
        return required && excluded;
    };

    struct RunInfo { uint64_t count = 0; std::vector<uint32_t> seeds; };
    std::map<uint64_t, RunInfo> found;
    const auto add = [&](uint64_t length, uint64_t offset) {
        auto& info = found[length];
        ++info.count;
        if (info.seeds.size() < 500)
            info.seeds.push_back(uint32_t(spec.start_rng_seed + offset * uint64_t(SEED_STEP)));
    };
    if (spec.find_len < array_size) {
        uint64_t run_start = 0, length = 0;
        for (uint64_t offset = 0; offset < spec.find_len; ++offset) {
            if (matches(array[uint32_t(index + offset)])) {
                ++length;
            } else {
                add(length, run_start);
                length = 0;
                run_start = offset + 1;
            }
        }
        if (length) add(length, run_start);
    } else {
        // ponytail: one exact O(n) pass keeps runs crossing thread and ring boundaries intact.
        uint64_t first_false = 0;
        while (first_false < spec.find_len &&
               matches(array[uint32_t(index + first_false)])) ++first_false;
        if (first_false == spec.find_len) {
            add(spec.find_len, 0);
        } else {
            uint64_t offset = (first_false + 1) % spec.find_len;
            uint64_t run_start = offset, length = 0;
            for (uint64_t processed = 0; processed < spec.find_len; ++processed) {
                if (matches(array[uint32_t(index + offset)])) {
                    ++length;
                } else {
                    add(length, run_start);
                    length = 0;
                    run_start = offset + 1 == spec.find_len ? 0 : offset + 1;
                }
                if (++offset == spec.find_len) offset = 0;
            }
        }
    }
    json results = json::array();
    for (auto& [metric, info] : found) {
        std::sort(info.seeds.begin(), info.seeds.end(), [&](uint32_t a, uint32_t b) {
            return uint32_t(uint64_t(a - spec.start_rng_seed) * inv) <
                   uint32_t(uint64_t(b - spec.start_rng_seed) * inv);
        });
        results.push_back({
            {"metric", metric}, {"seed_count", info.count},
            {"stored_seed_count", info.seeds.size()}, {"seeds", hex_seeds(info.seeds)}
        });
    }
    return {
        {"search", "run"}, {"scene", spec.name},
        {"start_rng_seed", hex_seed(spec.start_rng_seed)}, {"find_len", spec.find_len},
        {"results", results}
    };
}

Candidate best_candidate(uint32_t start_rng_seed, uint64_t find_len,
                         uint32_t factor_mask, uint32_t density_threshold,
                         const std::vector<uint32_t>& remaining) {
    std::unordered_set<uint32_t> allowed(remaining.begin(), remaining.end());
    Candidate best;
    for (const auto& info : final_results) {
        for (uint32_t rng_seed : info.seeds) {
            uint64_t offset = uint32_t(uint64_t(rng_seed - start_rng_seed) * inv);
            if (offset >= find_len) continue;
            std::unordered_set<uint32_t> seen;
            std::vector<uint32_t> codes;
            uint32_t index = uint32_t(uint64_t(rng_seed) * inv);
            for (size_t i = 0; i < remaining.size(); ++i) {
                uint32_t code = project(array[index++], factor_mask, density_threshold);
                if (!allowed.count(code) || !seen.insert(code).second) break;
                codes.push_back(code);
            }
            const uint32_t best_offset = uint32_t(
                uint64_t(best.rng_seed - start_rng_seed) * inv
            );
            if (!best.found || codes.size() > best.codes.size() ||
                (codes.size() == best.codes.size() && offset < best_offset)) {
                best = {rng_seed, std::move(codes), true};
            }
        }
    }
    return best;
}

json search_set(const json& request) {
    std::string name = request.at("scene").get<std::string>();
    Scene scene = parse_scene(name);
    name = scene_name(scene);
    uint32_t factor_mask = uint32_t(uint_value(request.at("factor_mask")));
    uint32_t density_threshold = uint32_t(uint_value(request, "density_threshold", 12000));
    uint32_t start_rng_seed = uint32_t(uint_value(request, "start_rng_seed", 0));
    uint64_t find_len = uint_value(request, "find_len", array_size);
    bool greedy_all = request.value("greedy_all", true);
    uint64_t threads = uint_value(request, "threads", MAX_THREAD);
    if (!threads || threads > 1024) throw std::runtime_error("threads must be in [1, 1024]");
    MAX_THREAD = unsigned(threads);
    constexpr uint32_t allowed_factors = 0x0007ffffu | (1u << 20);
    if (!factor_mask || (factor_mask & ~allowed_factors))
        throw std::runtime_error("factor_mask may only use zombie bits 0..18 and density bit 20");
    if (!find_len || find_len > array_size) throw std::runtime_error("find_len must be in [1, 2^32]");

    std::vector<uint32_t> remaining;
    for (const auto& value : request.at("targets")) {
        uint32_t target = uint32_t(uint_value(value));
        if (target & ~factor_mask) throw std::runtime_error("target contains a bit outside factor_mask");
        remaining.push_back(target);
    }
    std::sort(remaining.begin(), remaining.end());
    if (remaining.empty() || std::adjacent_find(remaining.begin(), remaining.end()) != remaining.end())
        throw std::runtime_error("targets must be non-empty and unique");

    MappedBin data(data_path(name));
    json segments = json::array();
    const size_t target_count = remaining.size();
    while (!remaining.empty()) {
        final_results.clear();
        MultiQuickSetFinder finder(start_rng_seed, find_len, scene, factor_mask,
                                   remaining, density_threshold, false);
        finder.multi_thread_find_score();
        Candidate best = best_candidate(start_rng_seed, find_len, factor_mask,
                                        density_threshold, remaining);
        if (!best.found || best.codes.empty())
            throw std::runtime_error("no remaining DOE target occurs in the search range");

        segments.push_back({
            {"rng_seed", hex_seed(best.rng_seed)},
            {"length", best.codes.size()},
            {"codes", best.codes}
        });
        std::unordered_set<uint32_t> covered(best.codes.begin(), best.codes.end());
        remaining.erase(std::remove_if(remaining.begin(), remaining.end(),
            [&](uint32_t code) { return covered.count(code); }), remaining.end());
        if (!greedy_all) break;
    }
    final_results.clear();
    return {
        {"segments", segments},
        {"covered", target_count - remaining.size()},
        {"target_count", target_count},
        {"remaining", remaining}
    };
}

json expand(const json& request) {
    Scene scene = parse_scene(request.at("scene").get<std::string>());
    uint32_t user_id = uint32_t(uint_value(request, "user_id", 1));
    uint32_t mode_id = uint32_t(uint_value(request, "mode_id", 13));
    uint32_t level = uint32_t(uint_value(request, "level_start", 1000));
    uint32_t factor_mask = uint32_t(uint_value(request, "factor_mask", 0));
    uint32_t density_threshold = uint32_t(uint_value(request, "density_threshold", 12000));
    BasicSeedFinder finder;
    ZombieTypeList zombie_list(scene);
    json samples = json::array();
    constexpr uint32_t allowed_factors = 0x0007ffffu | (1u << 20);
    if (factor_mask & ~allowed_factors)
        throw std::runtime_error("factor_mask may only use zombie bits 0..18 and density bit 20");

    size_t execution_order = 0;
    size_t segment_index = 0;
    for (const auto& segment : request.at("segments")) {
        uint32_t first_rng_seed = uint32_t(uint_value(segment.at("rng_seed")));
        size_t length = size_t(uint_value(segment.at("length")));
        uint32_t real_seed = finder.get_real_seed(first_rng_seed, user_id, mode_id, level);
        for (size_t offset = 0; offset < length; ++offset, ++execution_order) {
            uint32_t sample_level = uint32_t(level + offset);
            uint32_t rng_seed = uint32_t(first_rng_seed + offset * uint64_t(SEED_STEP));
            if (finder.get_rng_seed(real_seed, user_id, mode_id, sample_level) != rng_seed)
                throw std::runtime_error("seed expansion invariant failed");
            uint32_t spawn_code = zombie_list.get_list(rng_seed);
            uint32_t projected_code = project(spawn_code, factor_mask, density_threshold);
            if (segment.contains("codes") && offset < segment["codes"].size() &&
                projected_code != uint32_t(uint_value(segment["codes"][offset])))
                throw std::runtime_error("expanded code does not match the selected segment");
            samples.push_back({
                {"execution_order", execution_order},
                {"segment", segment_index},
                {"offset", offset},
                {"level", sample_level},
                {"rng_seed", hex_seed(rng_seed)},
                {"real_seed", hex_seed(real_seed)},
                {"bin_index", uint32_t(uint64_t(rng_seed) * inv)},
                {"spawn_code", spawn_code},
                {"projected_code", projected_code},
                {"weight", finder.weight_has(spawn_code)}
            });
        }
        level = uint32_t(level + length);
        ++segment_index;
    }
    return {{"samples", samples}, {"level_end", level}};
}

json inspect(Scene scene, uint32_t real_seed, uint32_t user_id,
             uint32_t mode_id, uint32_t level, uint32_t count) {
    BasicSeedFinder finder;
    ZombieTypeList zombie_list(scene);
    json samples = json::array();
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t sample_level = level + i;
        uint32_t rng_seed = finder.get_rng_seed(real_seed, user_id, mode_id, sample_level);
        uint32_t spawn_code = zombie_list.get_list(rng_seed);
        samples.push_back({
            {"level", sample_level},
            {"rng_seed", hex_seed(rng_seed)},
            {"real_seed", hex_seed(real_seed)},
            {"bin_index", uint32_t(uint64_t(rng_seed) * inv)},
            {"spawn_code", spawn_code},
            {"weight", finder.weight_has(spawn_code)}
        });
    }
    return {{"scene", scene_name(scene)}, {"samples", samples}};
}

json selftest() {
    if (uint32_t(uint64_t(SEED_STEP) * inv) != 1)
        throw std::runtime_error("SEED_STEP inverse is wrong");
    BasicSeedFinder finder;
    for (uint32_t real_seed : {0u, 1u, 0x12345678u, 0xffffffffu}) {
        uint32_t rng_seed = finder.get_rng_seed(real_seed, 1, 13, 1000);
        if (finder.get_real_seed(rng_seed, 1, 13, 1000) != real_seed)
            throw std::runtime_error("rng_seed/real_seed round trip failed");
        if (finder.get_rng_seed(real_seed, 1, 13, 1001) != uint32_t(rng_seed + SEED_STEP))
            throw std::runtime_error("consecutive level seed step failed");
        if (uint32_t(uint64_t(uint32_t(uint64_t(rng_seed) * inv)) * SEED_STEP) != rng_seed)
            throw std::runtime_error("bin index round trip failed");
    }
    ZombieTypeList first(NIGHT), second(NIGHT);
    if (first.get_list(0x12345678) != second.get_list(0x12345678))
        throw std::runtime_error("spawn generation is not deterministic");
    return {{"ok", true}, {"tests", 4}};
}

json simulate_worst(const json& raw, SunConfig config) {
    uint64_t find_len = uint_value(raw, "find_len", array_size);
    uint32_t start_rng_seed = uint32_t(uint_value(raw, "start_rng_seed", 0));
    bool circular = raw.value("circular", find_len == array_size);
    if (!find_len || find_len > array_size)
        throw std::runtime_error("find_len must be in [1, 2^32]");
    if (config.scene < DAY || config.scene > ROOF)
        throw std::runtime_error("invalid scene in sun config");

    MappedBin data(data_path(scene_name(config.scene)));
    SunModel model(config);
    BasicSeedFinder finder;
    uint32_t index = uint32_t(uint64_t(start_rng_seed) * inv);
    double total = 0;
    double max_sum = -std::numeric_limits<double>::infinity(), max_current = 0;
    double min_sum = std::numeric_limits<double>::infinity(), min_current = 0;
    uint64_t max_begin = 0, max_current_begin = 0, max_end = 0;
    uint64_t min_begin = 0, min_current_begin = 0, min_end = 0;
    for (uint64_t offset = 0; offset < find_len; ++offset) {
        double loss = -model.sun_model(array[uint32_t(index + offset)]);
        total += loss;
        if (!offset || max_current < 0) {
            max_current = loss;
            max_current_begin = offset;
        } else {
            max_current += loss;
        }
        if (max_current > max_sum) {
            max_sum = max_current;
            max_begin = max_current_begin;
            max_end = offset;
        }
        if (!offset || min_current > 0) {
            min_current = loss;
            min_current_begin = offset;
        } else {
            min_current += loss;
        }
        if (min_current < min_sum) {
            min_sum = min_current;
            min_begin = min_current_begin;
            min_end = offset;
        }
    }
    uint64_t best_begin = max_begin, best_end = max_end;
    uint64_t best_length = max_end - max_begin + 1;
    double best = max_sum;
    const uint64_t min_length = min_end - min_begin + 1;
    if (circular && min_length < find_len && total - min_sum > best) {
        best = total - min_sum;
        best_begin = (min_end + 1) % find_len;
        best_length = find_len - min_length;
        best_end = (best_begin + best_length - 1) % find_len;
    }
    uint32_t rng_seed = uint32_t(start_rng_seed + best_begin * uint64_t(SEED_STEP));
    uint32_t end_rng_seed = uint32_t(start_rng_seed + best_end * uint64_t(SEED_STEP));
    return {
        {"mode", 3}, {"find_len", find_len}, {"circular", circular},
        {"start_offset", best_begin}, {"end_offset", best_end},
        {"length", best_length}, {"wraps", best_begin + best_length > find_len},
        {"total_loss", best},
        {"bin_index", uint32_t(uint64_t(rng_seed) * inv)},
        {"rng_seed", hex_seed(rng_seed)}, {"end_rng_seed", hex_seed(end_rng_seed)},
        {"real_seed", hex_seed(finder.get_real_seed(
            rng_seed, config.user_id, config.mode_id, config.level_start))}
    };
}

json simulate(const fs::path& path, uint32_t forced_mode = 0) {
    json raw = read_json(path);
    SunConfig config(path.string().c_str());
    if (forced_mode) config.mode = forced_mode;
    if (config.factor1.size() != config.factor2.size() || config.factor1.size() != config.coef.size())
        throw std::runtime_error("factor1, factor2 and coef must have equal lengths");
    if (std::any_of(config.factor1.begin(), config.factor1.end(), [](int bit) { return bit < 0 || bit > 20; }) ||
        std::any_of(config.factor2.begin(), config.factor2.end(), [](int bit) { return bit < 0 || bit > 20; }) ||
        std::any_of(config.coef.begin(), config.coef.end(), [](double value) { return !std::isfinite(value); }) ||
        !std::isfinite(config.bias))
        throw std::runtime_error("sun model factors must be bits 0..20 and coefficients must be finite");
    if (config.mode == 3) return simulate_worst(raw, config);
    SunModel model(config);
    BasicSeedFinder finder;

    if (config.mode == 1) {
        uint32_t sampler_seed = uint32_t(uint_value(raw, "sampler_seed", 20260723));
        std::mt19937 generator(sampler_seed);
        std::uniform_int_distribution<uint32_t> distribution;
        uint32_t failures = 0;
        long double survival_total = 0;
        struct Trial {
            double lowest;
            uint32_t rng_seed;
            uint32_t failure_step;
            bool failed;
        };
        std::vector<Trial> worst;
        worst.reserve(config.mode1_sim_num);
        for (uint32_t i = 0; i < config.mode1_sim_num; ++i) {
            uint32_t rng_seed = distribution(generator);
            double sun = config.sun_start;
            if (config.sun_cap && sun > 9990) sun = 9990;
            double lowest = sun;
            uint32_t failure_step = 0;
            bool failed = sun < config.sun_end;
            for (uint32_t step = 0, seed = rng_seed; step < config.level_num;
                 ++step, seed += SEED_STEP) {
                sun += model.sun_model(model.ztl.get_list(seed));
                if (config.sun_cap && sun > 9990) sun = 9990;
                lowest = std::min(lowest, sun);
                if (!failed && sun < config.sun_end) {
                    failed = true;
                    failure_step = step + 1;
                }
            }
            failures += failed;
            survival_total += failed ? failure_step : config.level_num;
            worst.push_back({lowest, rng_seed, failure_step, failed});
        }
        size_t keep = std::min<size_t>(config.mode1_output_num, worst.size());
        std::sort(worst.begin(), worst.end(), [](const Trial& a, const Trial& b) {
            return a.lowest < b.lowest || (a.lowest == b.lowest && a.rng_seed < b.rng_seed);
        });
        json worst_json = json::array();
        for (size_t i = 0; i < keep; ++i) {
            json item = {
                {"lowest_sun", worst[i].lowest},
                {"rng_seed", hex_seed(worst[i].rng_seed)},
                {"real_seed", hex_seed(finder.get_real_seed(
                    worst[i].rng_seed, config.user_id, config.mode_id, config.level_start))}
            };
            item["failure_step"] = worst[i].failed ? json(worst[i].failure_step) : json(nullptr);
            worst_json.push_back(std::move(item));
        }
        return {
            {"mode", 1}, {"sampler_seed", hex_seed(sampler_seed)},
            {"trials", config.mode1_sim_num}, {"failure_count", failures},
            {"failure_rate", config.mode1_sim_num ? double(failures) / config.mode1_sim_num : 0.0},
            {"censored_count", config.mode1_sim_num - failures},
            {"mean_survival_levels", config.mode1_sim_num ?
                double(survival_total / config.mode1_sim_num) : 0.0},
            {"worst", worst_json}
        };
    }
    if (config.mode == 2) {
        uint32_t rng_seed = finder.get_rng_seed(config.mode2_seed, config.user_id,
                                                config.mode_id, config.level_start);
        double sun = config.sun_start;
        if (config.sun_cap && sun > 9990) sun = 9990;
        bool failed = sun < config.sun_end;
        json samples = json::array();
        for (uint32_t i = 0; i < config.level_num; ++i, rng_seed += SEED_STEP) {
            uint32_t has = model.ztl.get_list(rng_seed);
            double response = model.sun_model(has);
            sun += response;
            if (config.sun_cap && sun > 9990) sun = 9990;
            failed = failed || sun < config.sun_end;
            samples.push_back({
                {"step", i}, {"level", config.level_start + i},
                {"rng_seed", hex_seed(rng_seed)}, {"spawn_code", has},
                {"predicted_response", response}, {"sun", sun},
                {"failed", failed}
            });
        }
        return {{"mode", 2}, {"real_seed", hex_seed(config.mode2_seed)}, {"samples", samples}};
    }
    throw std::runtime_error("simulate supports SunConfig mode 1, 2, or 3");
}

void usage() {
    std::cerr
        << "usage:\n"
        << "  pvz_doe selftest\n"
        << "  pvz_doe inspect <DE|NE|PE|RE> <real_seed> <user_id> <mode_id> <level> [count]\n"
        << "  pvz_doe search <exact|score|run|set> <request.json>\n"
        << "  pvz_doe expand <request.json>\n"
        << "  pvz_doe simulate <config.json>\n"
        << "  pvz_doe simulate worst <config.json>\n";
}

} // namespace

int main(int argc, char** argv) {
    std::ios::sync_with_stdio(false);
    try {
        executable_path = fs::absolute(argv[0]);
        if (argc == 2 && std::string(argv[1]) == "selftest")
            std::cout << selftest().dump() << '\n';
        else if (argc >= 7 && argc <= 8 && std::string(argv[1]) == "inspect")
            std::cout << inspect(parse_scene(argv[2]), uint32_t(parse_uint(argv[3])),
                uint32_t(parse_uint(argv[4])), uint32_t(parse_uint(argv[5])),
                uint32_t(parse_uint(argv[6])), argc == 8 ? uint32_t(parse_uint(argv[7])) : 1).dump() << '\n';
        else if (argc == 4 && std::string(argv[1]) == "search") {
            const std::string mode = argv[2];
            const json request = read_json(argv[3]);
            if (mode == "exact") std::cout << search_exact(request).dump() << '\n';
            else if (mode == "score") std::cout << search_score(request).dump() << '\n';
            else if (mode == "run") std::cout << search_run(request).dump() << '\n';
            else if (mode == "set") std::cout << search_set(request).dump() << '\n';
            else throw std::runtime_error("search mode must be exact, score, run, or set");
        }
        else if (argc == 3 && std::string(argv[1]) == "expand")
            std::cout << expand(read_json(argv[2])).dump() << '\n';
        else if (argc == 3 && std::string(argv[1]) == "simulate")
            std::cout << simulate(argv[2]).dump() << '\n';
        else if (argc == 4 && std::string(argv[1]) == "simulate" && std::string(argv[2]) == "worst")
            std::cout << simulate(argv[3], 3).dump() << '\n';
        else {
            usage();
            return 2;
        }
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
