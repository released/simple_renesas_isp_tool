// Compile the production helpers against deterministic failed CRT operations.
#include <windows.h>
#include <atomic>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <fcntl.h>
#include <io.h>
#include <string>
#include <iostream>
namespace text_fault {
int closes = 0;
std::size_t Write(const void*, std::size_t, std::size_t count, FILE*) { errno = ENOSPC; return count ? count - 1U : 0U; }
int Flush(FILE*) { errno = ENOSPC; return EOF; }
int Close(FILE*) { ++closes; errno = EIO; return EOF; }
int Error(FILE*) { return 1; }
}
#define text_file checked_text_fault
#define fwrite text_fault::Write
#define fflush text_fault::Flush
#define fclose text_fault::Close
#define ferror text_fault::Error
#include "../src/core/text_file.h"
#undef text_file
#undef fwrite
#undef fflush
#undef fclose
#undef ferror
int TestTextFileFaults() {
    std::wstring error;
    auto* file = reinterpret_cast<FILE*>(static_cast<std::uintptr_t>(1U));
    int failures = 0;
    const auto check = [&](bool ok, const char* name) { if (!ok) { ++failures; std::cerr << "FAIL: " << name << '\n'; } };
    check(!firmware_upgrade::checked_text_fault::Write(file, "abcdef", &error) && !error.empty(), "short write reported");
    check(!firmware_upgrade::checked_text_fault::Flush(file, &error) && error.find(L"flush") != std::wstring::npos, "flush failure reported");
    check(!firmware_upgrade::checked_text_fault::Close(&file, &error) && !file && text_fault::closes == 1,
        "failed flush still closes exactly once and releases owner");
    check(firmware_upgrade::checked_text_fault::Close(&file, &error) && text_fault::closes == 1, "closed owner is idempotent");
    return failures;
}
