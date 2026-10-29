#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <cfloat>
#include <cfenv>
#include <cmath>
#include <limits>

extern "C" {
#include "retarget.h"
unsigned char CT_Crc8(const unsigned char *data, unsigned char size);
int OriginalTinyPrintf(const char *format, ...);
}
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
static std::string output;
static std::string originalOutput;
static unsigned long reads, busy, writes;
static unsigned char port;
static unsigned long checks;
extern "C" void OriginalSendChar(unsigned char value)
{
    originalOutput += static_cast<char>(value);
}

struct StatusRegister {
    operator unsigned long() const {
        ++reads;
        return busy;
    }
};
struct DataRegister {
    DataRegister &operator=(unsigned long value) {
        ++writes;
        output += static_cast<char>(value);
        return *this;
    }
};
struct MockUart {
    StatusRegister LST;
    struct { DataRegister UINT16; } LUTDR;
};
static MockUart RLN30;
#define _UART_TRANSMISSION_OPERATED 1UL
#define APP_RUNTIME_LOG_TAG "V1"
static unsigned char Device_CanPort() { return port; }
#include "actual_console.inc"

static void require(bool value, const char *message) {
    ++checks;
    if (!value) {
        std::cerr << "FAIL " << message << " output=[" << output << "]\n";
        std::exit(1);
    }
}
static void clear() {
    output.clear();
    reads = writes = busy = 0UL;
}
static void expect(const std::string &text) {
    require(output == text, "byte-exact text/CRLF");
    require(writes == text.size(), "one UART write per output character");
}

template <typename... Args>
static void compareOriginal(const char *format, Args... args)
{
    clear();
    const int count = tiny_printf(format, args...);
#if TINY_PRINTF_FULL
    char reference[4096];
    std::snprintf(reference, sizeof(reference), format, args...);
    expect(reference);
#else
    originalOutput.clear();
    OriginalTinyPrintf(format, args...);
    expect(originalOutput);
#endif
    require(count == static_cast<int>(output.size()), "full formatter return count");
}

static void compareFloat(const char *format, double value)
{
    char expected[4096];
    clear();
    const int count = tiny_printf(format, value);
    const int wanted = std::snprintf(expected, sizeof(expected), format, value);
    require(wanted >= 0 && wanted < static_cast<int>(sizeof(expected)), "bounded native reference");
    expect(expected);
    require(count == wanted, "float field/count reference");
}

static void checkFloatEdges()
{
    const double values[] = {0.0, -0.0, 0.125, -1.25, 1.75, 1.0e20, -1.0e20,
                             18446744073709551616.0, DBL_MAX, -DBL_MAX};
    for (double value : values)
    {
        compareFloat("%.0f", value);
        compareFloat("%.256f", value);
    }
    compareFloat("%.2048f", 1.25);
    compareFloat("%.6e", DBL_MAX);
    compareFloat("%.6E", DBL_MIN);
    compareFloat("%.6e", std::numeric_limits<double>::denorm_min());
    compareFloat("%+.256e", 1.25);
    compareFloat("%020.3f", -1.25);
    compareFloat("%-20.3e", 12.5);
    unsigned long long state = 0x123456789ABCDEF0ULL;
    for (unsigned int i = 0U; i < 128U; ++i)
    {
        state = state * 6364136223846793005ULL + 1ULL;
        const unsigned long long exponent = 1023ULL + 64ULL + (state % 960ULL);
        const unsigned long long bits = (state & 0x800FFFFFFFFFFFFFULL) | (exponent << 52);
        double large;
        std::memcpy(&large, &bits, sizeof(large));
        compareFloat("%.0f", large);
        compareFloat("%+340.3f", large);
    }
    const double infinity = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    clear();
    require(tiny_printf("%f/%F/%+010e", infinity, -infinity, infinity) == 19,
            "infinity field count");
    expect("inf/-INF/      +inf");
    clear();
    tiny_printf("%f/%F", nan, -nan);
    expect("nan/-NAN");
    clear();
    std::feclearexcept(FE_ALL_EXCEPT);
    tiny_printf("%.3f", -0.0);
    require((std::fetestexcept(FE_DIVBYZERO) & FE_DIVBYZERO) == 0,
            "negative zero does not use division");
    expect("-0.000");
    clear();
    require(tiny_printf("%2147483648f", 1.0) < 0, "width parser overflow rejected");
    expect("");
    clear();
    require(tiny_printf("%.2147483648f", 1.0) < 0, "precision parser overflow rejected");
    expect("");
    clear();
    require(tiny_printf("%.2147483647f", 1.0) < 0, "float count overflow rejected before output");
    expect("");
    clear();
    require(tiny_printf("%.2147483647d", -1) < 0, "signed precision/count overflow rejected");
    expect("");
}

int main() {
    char expected[1024];
#if CAN_TRANSFER_UART_TRACE
    char byte[8];
#endif
    unsigned char data[64];
    const unsigned long values[] = {0UL, 1UL, 9UL, 10UL, 99UL, 100UL, 999UL,
                                   1000UL, 0x80000000UL, 0xFFFFFFFFUL};
    require(sizeof(long) == 4U && sizeof(unsigned int) == 4U, "RH850 integer widths in host model");
    require(sizeof(void *) == 4U, "RH850 pointer width in host model");
    checkFloatEdges();
    compareOriginal("[%s][%c][%%][%.4s][%-8.3s]", "text", 'Z', "abcdef", "abcdef");
    compareOriginal("[%+08d][% 8i][%-8u][%o]", -123, 42, 35U, 0777U);
    compareOriginal("[%.6d][%.8u][%012.8llX]", -42, 35U, 0x12345ULL);
#if !TINY_PRINTF_FULL
    compareOriginal("literal:%q/%%");
#else
    clear();
    require(tiny_printf("%q") < 0 && output.empty(), "unknown conversion rejected");
#endif
    compareOriginal("[%llu][%llX][%lld]", ULLONG_MAX, ULLONG_MAX, -123456789012345LL);
#if !TINY_PRINTF_FULL
    compareOriginal("[%12p]", reinterpret_cast<void *>(0x1234U));
#else
    clear();
    tiny_printf("[%12p]", reinterpret_cast<void *>(0x1234U));
    expect("[      0x1234]");
#endif
    compareOriginal("[%.2f][%+.3F][%.2e][%.2E][%#.0f]", 1.25, -2.5, 9.75, 0.00125, 2.0);
    compareOriginal("[%08.2f][%-10.2e][%.3f]", -1.25, 12.5, -0.0);
    clear();
    tiny_printf("%lld", LLONG_MIN);
    expect("-9223372036854775808");
    clear();
    const int precise = tiny_printf("%.80u/%.80llu", 7U, ULLONG_MAX);
    std::snprintf(expected, sizeof(expected), "%.80u/%.80llu", 7U, ULLONG_MAX);
    expect(expected);
    require(precise == 161, "precision streams beyond digit buffer");
    clear();
#if !TINY_PRINTF_FULL
    require(tiny_printf("abc%") == 4, "incomplete format returns at terminator");
    expect("abc%");
#else
    require(tiny_printf("abc%") < 0, "incomplete format rejected");
    expect("abc");
#endif
    for (unsigned long value : values) {
        clear();
        std::snprintf(expected, sizeof(expected), "%lu/0x%08lX/%08lx\r\n", value, value, value);
        const int count = tiny_printf("%lu/0x%08lX/%08lx\r\n", value, value, value);
        expect(expected);
        require(count == static_cast<int>(std::strlen(expected)), "formatter character count");
        clear();
        PrintActualReset(value);
        std::snprintf(expected, sizeof(expected), "reset cause: 0x%08lX\r\n", value);
        expect(expected);
        clear();
        const unsigned long next = PrintActualCounter(value);
        std::snprintf(expected, sizeof(expected), "log(timer)-V1: %lu\r\n", value);
        expect(expected);
        require(next == value + 1UL, "actual counter increments once including wrap");
    }
    for (int value : {INT_MIN, -1, 0, 1, INT_MAX}) {
        clear();
        tiny_printf("%d/%012d", value, value);
        std::snprintf(expected, sizeof(expected), "%d/%012d", value, value);
        expect(expected);
    }
    clear();
    tiny_printf("%ld/%u/%c/%%/%s", LONG_MIN, UINT_MAX, 'Z', "literal%08lX");
    expect("-2147483648/4294967295/Z/%/literal%08lX");
    clear();
    tiny_printf("%s", static_cast<const char *>(nullptr));
    expect("(null)");

    for (port = 0U; port < 6U; ++port) {
        require(Device_CanPort() == port, "source-port test stub");
        for (unsigned int seed = 0U; seed < 256U; ++seed) {
            for (unsigned int i = 0U; i < 64U; ++i) {
                data[i] = static_cast<unsigned char>(i + seed);
            }
            data[7] = CT_Crc8(data, 7U);
            for (unsigned char bad = 0U; bad < 2U; ++bad) {
                clear();
                ClassicalTrace("RX", 0x710U, data);
#if CAN_TRANSFER_UART_TRACE
                std::snprintf(expected, sizeof(expected), "TRACE CT CAN%X RX id=0x710 dlc=8 data=[", static_cast<unsigned int>(port));
                std::string line(expected);
                for (unsigned int i = 0U; i < 8U; ++i) {
                    if (i != 0U) { line += ' '; }
                    std::snprintf(byte, sizeof(byte), "%02X", static_cast<unsigned int>(data[i]));
                    line += byte;
                }
                line += bad ? "] crc=BAD\r\n" : "] crc=OK\r\n";
                expect(line);
#else
                expect("");
#endif
                data[7] ^= 1U;
            }
            for (unsigned int length : {8U, 64U}) {
                clear();
                FdTrace("TRACE CT TX", data, static_cast<unsigned char>(length));
#if CAN_TRANSFER_UART_TRACE
                std::snprintf(expected, sizeof(expected), "TRACE CT TX CAN%u FD+BRS data=[", static_cast<unsigned int>(port));
                std::string line(expected);
                for (unsigned int i = 0U; i < length; ++i) {
                    std::snprintf(byte, sizeof(byte), "%02X ", static_cast<unsigned int>(data[i]));
                    line += byte;
                }
                line += "]\r\n";
                expect(line);
#else
                expect("");
#endif
            }
        }
    }
    clear();
    busy = 1UL;
    require(SendChar('X') < 0, "busy UART reports failure");
    require(output.empty() && writes == 0UL, "busy UART returns without a write");
    require(reads >= 100000UL && reads <= 100002UL, "finite original UART wait guard");
    busy = 0UL;
    require(SendChar('Y') == 'Y', "UART hook returns accepted unsigned byte");
    require(output == "Y" && writes == 1UL, "UART output resumes when available");
#ifdef HAS_V4_CONSOLE
    clear();
    busy = 1UL;
    V4SendChar('X');
    require(output.empty() && writes == 0UL, "v4 busy UART does not write");
    require(reads >= 1000000UL && reads <= 1000002UL, "v4 original wait bound preserved");
    busy = 0UL;
    V4SendChar('Z');
    require(output == "Z" && writes == 1UL, "v4 UART resumes");
#endif
#if TINY_PRINTF_FULL
    clear();
    busy = 1UL;
    require(tiny_printf("ABC") < 0 && output.empty(), "full formatter propagates real UART timeout");
    require(reads <= 100002UL, "UART failure stops remaining characters");
#endif
    std::cout << "PASS actual tiny_printf/SendChar/log excerpts; trace=" << CAN_TRANSFER_UART_TRACE
              << ";checks=" << checks << ";bounded UART busy/no-write checked\n";
}
