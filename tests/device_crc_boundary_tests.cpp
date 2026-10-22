#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

#define __far
#define F13_CRC_HOST_TEST
#include "sw_crc32.c"
#undef __far

static std::uint32_t ReferenceCrc(const unsigned char* bytes, std::size_t size)
{
    std::uint32_t table[256];
    for (unsigned i = 0; i != 256; ++i) {
        std::uint32_t value = i;
        for (unsigned bit = 0; bit != 8; ++bit)
            value = (value >> 1) ^ ((value & 1U) ? 0xEDB88320U : 0U);
        table[i] = value;
    }
    std::uint32_t crc = 0xFFFFFFFFU;
    for (std::size_t i = 0; i != size; ++i)
        crc = (crc >> 8) ^ table[(crc ^ bytes[i]) & 0xFFU];
    return crc ^ 0xFFFFFFFFU;
}

int main(int argc, char** argv)
{
    static_assert(sizeof(unsigned long) == 4, "RL78 CRC words must be 32-bit");
    unsigned checks = 0;
    const auto check = [&checks](bool passed, const char* label) {
        if (!passed) { std::cerr << "FAIL: " << label << '\n'; return false; }
        ++checks;
        return true;
    };
    const auto crc = [](const unsigned char* bytes, std::size_t size) {
        return F13_Crc32Bytes(bytes, static_cast<unsigned long>(size));
    };
    if (!check(crc(nullptr, 0) == 0, "empty input")) return 1;
    if (!check(crc(reinterpret_cast<const unsigned char*>("123456789"), 9) ==
               0xCBF43926UL, "ISO-HDLC known answer")) return 1;

    // Align the model to real 64-KiB boundaries, not just a vector offset.
    std::vector<unsigned char> storage(0x60000U);
    const auto aligned = (reinterpret_cast<std::uintptr_t>(storage.data()) +
                          0xFFFFU) & ~static_cast<std::uintptr_t>(0xFFFFU);
    auto* memory = reinterpret_cast<unsigned char*>(aligned);
    const std::size_t starts[] = {0x2000U, 0xFFF8U, 0x10000U, 0x1FFF8U};
    const std::size_t sizes[] = {0U, 1U, 4U, 8U, 12U, 64U, 256U,
        0xDFFCU, 0xE000U, 0xE004U, 0x10000U, 0x1DFFCU, 0x3DBFCU};
    for (unsigned pattern = 0; pattern != 2; ++pattern) {
        for (std::size_t i = 0; i != 0x40000U; ++i)
            memory[i] = pattern ? 0xFF : static_cast<unsigned char>(i * 37U + (i >> 8));
        for (const auto start : starts) {
            for (const auto size : sizes) {
                if (start + size > 0x40000U) continue;
                if (!check(crc(memory + start, size) == ReferenceCrc(memory + start, size),
                           "linear CRC across 64-KiB boundaries")) return 1;
            }
        }
        const auto baseline = crc(memory + 0x2000U, 0x3DBFCU);
        for (const auto address : {0x10000U, 0x20000U, 0x30000U}) {
            memory[address] ^= 1U;
            if (!check(crc(memory + 0x2000U, 0x3DBFCU) != baseline,
                       "upper Flash corruption detected")) return 1;
            memory[address] ^= 1U;
        }
    }
    for (int i = 1; i < argc; ++i) {
        std::ifstream stream(argv[i], std::ios::binary);
        if (!stream) { std::cerr << "Cannot open " << argv[i] << '\n'; return 1; }
        const std::vector<unsigned char> image{std::istreambuf_iterator<char>(stream), {}};
        if (image.size() < 516U || image.size() > 0x3DC00U) return 1;
        const auto end = image.size() - 4U;
        const auto stored = static_cast<std::uint32_t>(image[end]) |
            (static_cast<std::uint32_t>(image[end + 1]) << 8) |
            (static_cast<std::uint32_t>(image[end + 2]) << 16) |
            (static_cast<std::uint32_t>(image[end + 3]) << 24);
        std::copy(image.begin(), image.end(), memory + 0x2000U);
        if (!check(crc(memory + 0x2000U, end) == stored &&
                   ReferenceCrc(image.data(), end) == stored, "actual BIN trailer")) return 1;
    }
    std::cout << "PASS: " << checks << " CRC checks; native model, not hardware.\n";
    return 0;
}
