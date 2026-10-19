#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "app_runtime_log.h"

int main(void)
{
    static const unsigned long values[] = {
        0UL, 1UL, 9UL, 10UL, 99UL, 100UL, 65535UL, 65536UL,
        999999999UL, 1000000000UL, 4294967295UL
    };
    static const char *expected[] = {
        "0", "1", "9", "10", "99", "100", "65535", "65536",
        "999999999", "1000000000", "4294967295"
    };
    char guarded[APP_RUNTIME_COUNTER_BUFFER_SIZE + 2U];
    unsigned int index;
    unsigned long counter;

    for (index = 0U; index < sizeof(values) / sizeof(values[0]); ++index) {
        memset(guarded, 'Z', sizeof(guarded));
        App_FormatRuntimeCounter(&guarded[1], values[index]);
        if (guarded[0] != 'Z' || guarded[sizeof(guarded) - 1U] != 'Z' ||
            strcmp(&guarded[1], expected[index]) != 0) return 1;
    }
    counter = ULONG_MAX;
    App_FormatRuntimeCounter(&guarded[1], counter++);
    if (strcmp(&guarded[1], "4294967295") != 0 || counter != 0UL) return 2;
    App_FormatRuntimeCounter(&guarded[1], counter);
    if (strcmp(&guarded[1], "0") != 0) return 3;
    printf("PASS: %s decimal counter, boundaries and unsigned wrap.\n",
        APP_RUNTIME_LOG_TAG);
    return 0;
}
