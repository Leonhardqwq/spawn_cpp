#include <array>
#include <chrono>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

using u32 = std::uint32_t;
using u64 = std::uint64_t;
using Clock = std::chrono::steady_clock;

constexpr u32 MASK16 = 0xffffu;
constexpr std::size_t RADIX = 1u << 16;
constexpr u64 SOURCE_ENTRIES = u64{1} << 32;
constexpr u64 SOURCE_BYTES = SOURCE_ENTRIES * sizeof(u32);
static_assert(sizeof(std::size_t) >= 8, "full data requires a 64-bit build");

struct Result {
    u64 min_length;
    u64 max_lcp;
    u32 left;
    u32 right;
    u64 unique_at;
};

u32 cycle_add(u32 i, u64 offset, u64 n) {
    u64 x = static_cast<u64>(i) + offset;
    return static_cast<u32>(x >= n ? x - n : x);
}

template <class Digit>
void radix_pass(const std::vector<u32>& in, std::vector<u32>& out, Digit digit) {
    std::array<u64, RADIX> next{};
    for (u32 i : in) ++next[digit(i)];

    u64 sum = 0;
    for (u64& x : next) {
        const u64 count = x;
        x = sum;
        sum += count;
    }
    for (u32 i : in) out[static_cast<std::size_t>(next[digit(i)]++)] = i;
}

std::optional<Result> solve(const std::vector<u32>& a, bool verbose) {
    const u64 n = a.size();
    if (n == 0 || n > static_cast<u64>(std::numeric_limits<u32>::max()) + 1)
        throw std::runtime_error("input length must be in [1, 2^32]");

    // ponytail: three reused arrays are enough; no key-pair or LCP array is stored.
    std::vector<u32> rank(static_cast<std::size_t>(n));
    std::vector<u32> order(static_cast<std::size_t>(n));
    std::vector<u32> work(static_cast<std::size_t>(n));
    for (u64 i = 0; i < n; ++i) order[static_cast<std::size_t>(i)] = static_cast<u32>(i);

    auto started = Clock::now();
    radix_pass(order, work, [&](u32 i) { return a[i] & MASK16; });
    radix_pass(work, order, [&](u32 i) { return a[i] >> 16; });

    u64 classes = 1;
    rank[order[0]] = 0;
    for (u64 r = 1; r < n; ++r) {
        if (a[order[static_cast<std::size_t>(r - 1)]] != a[order[static_cast<std::size_t>(r)]]) ++classes;
        rank[order[static_cast<std::size_t>(r)]] = static_cast<u32>(classes - 1);
    }

    u64 length = 1;
    if (verbose)
        std::cout << "length=" << length << " classes=" << classes << " seconds="
                  << std::chrono::duration<double>(Clock::now() - started).count() << '\n' << std::flush;

    while (classes < n) {
        if (length >= n) return std::nullopt;
        started = Clock::now();

        auto second = [&](u32 i) { return rank[cycle_add(i, length, n)]; };
        radix_pass(order, work, [&](u32 i) { return second(i) & MASK16; });
        radix_pass(work, order, [&](u32 i) { return second(i) >> 16; });
        radix_pass(order, work, [&](u32 i) { return rank[i] & MASK16; });
        radix_pass(work, order, [&](u32 i) { return rank[i] >> 16; });

        classes = 1;
        work[order[0]] = 0;
        for (u64 r = 1; r < n; ++r) {
            const u32 prev = order[static_cast<std::size_t>(r - 1)];
            const u32 curr = order[static_cast<std::size_t>(r)];
            if (rank[prev] != rank[curr] || second(prev) != second(curr)) ++classes;
            work[curr] = static_cast<u32>(classes - 1);
        }
        rank.swap(work);
        length *= 2;

        if (verbose)
            std::cout << "length=" << length << " classes=" << classes << " seconds="
                      << std::chrono::duration<double>(Clock::now() - started).count() << '\n' << std::flush;
    }

    started = Clock::now();
    u64 best = 0, known = 0;
    u32 best_i = 0, best_j = 0;
    bool have_pair = false;
    for (u64 raw = 0; raw < n; ++raw) {
        const u32 i = static_cast<u32>(raw);
        const u32 r = rank[i];
        if (r == 0) {
            known = 0;
            continue;
        }
        const u32 j = order[static_cast<std::size_t>(static_cast<u64>(r) - 1)];
        while (known < n && a[cycle_add(i, known, n)] == a[cycle_add(j, known, n)]) ++known;
        if (!have_pair || known > best) {
            best = known;
            best_i = i;
            best_j = j;
            have_pair = true;
        }
        if (known) --known;
    }
    if (verbose)
        std::cout << "lcp_seconds=" << std::chrono::duration<double>(Clock::now() - started).count() << '\n';
    return Result{best + 1, best, best_i, best_j, length};
}

std::vector<u32> load(const std::string& source, std::string& used) {
    const std::array<std::string, 2> paths{"../data/" + source + ".bin", "data/" + source + ".bin"};
    for (const std::string& path : paths) {
        std::error_code error;
        const auto bytes = std::filesystem::file_size(path, error);
        if (error) continue;
        if (bytes != SOURCE_BYTES)
            throw std::runtime_error("source must contain exactly 2^32 uint32 values: " + path);

        std::ifstream file(path, std::ios::binary);
        if (!file) throw std::runtime_error("cannot open data file: " + path);
        std::vector<u32> a(static_cast<std::size_t>(SOURCE_ENTRIES));
        file.read(reinterpret_cast<char*>(a.data()), static_cast<std::streamsize>(bytes));
        if (static_cast<u64>(file.gcount()) != bytes) throw std::runtime_error("short read: " + path);
        used = path;
        return a;
    }
    throw std::runtime_error("data file not found");
}

void self_test() {
    const auto result = solve({1, 0, 1, 0, 2}, false);
    if (!result || result->min_length != 3 || result->max_lcp != 2)
        throw std::runtime_error("unique-window self-test failed");
    if (solve({1, 2, 1, 2}, false)) throw std::runtime_error("periodic self-test failed");
}

int main(int argc, char** argv) try {
    if (argc != 2) {
        std::cerr << "usage: lcp <DE|RE|NE|PE>\n";
        return 1;
    }

    std::string source = argv[1];
    for (char& c : source) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    if (source != "DE" && source != "RE" && source != "NE" && source != "PE")
        throw std::runtime_error("source must be DE, RE, NE, or PE");

    self_test();
    std::string path;
    const auto load_started = Clock::now();
    const std::vector<u32> a = load(source, path);
    std::cout << "source=" << source << " file=" << path << " entries=" << a.size()
              << " load_seconds=" << std::chrono::duration<double>(Clock::now() - load_started).count() << '\n';

    const auto result = solve(a, true);
    if (!result) {
        std::cout << "no finite L: the circular encoding array is periodic\n";
        return 2;
    }
    std::cout << "max_lcp=" << result->max_lcp << " min_length=" << result->min_length
              << " witness_indices=" << result->left << ',' << result->right
              << " unique_power_of_two=" << result->unique_at << '\n';
    return 0;
} catch (const std::bad_alloc&) {
    std::cerr << "not enough memory; full input needs about 64 GiB plus process overhead\n";
    return 3;
} catch (const std::exception& e) {
    std::cerr << "error: " << e.what() << '\n';
    return 1;
}
