// Reuse the existing CAN fixtures/fault tests without running unrelated suites.
#define FIRMWARE_CAN_REVIEW_ONLY
#include "host_link_protocol_tests.cpp"
#undef FIRMWARE_CAN_REVIEW_ONLY

int main() {
    TestFirmwareUpdateProfileDispatch();
    TestF13BootImageAndFraming();
    TestF13BootClientTransactions();
    TestF13BootUpdateFaultInjection();
    std::cout << "Existing CAN profile/client/update fault regressions: "
        << g_failures << " failures; no hardware\n";
    return g_failures ? 1 : 0;
}
