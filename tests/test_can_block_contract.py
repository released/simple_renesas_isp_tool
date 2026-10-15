"""Source-boundary guards complement executable C/C++ block/HAL fault tests."""
from pathlib import Path
import re
import unittest
from workspace_paths import companion_path

ROOT = Path(__file__).resolve().parents[1]
F13 = companion_path('F13Boot')


class CanBlockContract(unittest.TestCase):
    def test_host_dispatch_owns_burst_without_target_semantics(self):
        text = (ROOT / "firmware/RL78_F24_Bridge_app/host_link_protocol.c").read_text()
        process = (ROOT / "firmware/RL78_F24_Bridge_app/host_link_can_process.inc").read_text()
        self.assertIn("case CB_COMMAND:", text)
        self.assertIn("HL_BAUD_FEATURE | CB_FEATURE", text)
        self.assertIn("(command <= CB_COMMAND)", text)
        self.assertIn("(command != CB_COMMAND)", text)
        self.assertLess(text.index("cb_decode(&pending_transaction.burst"), text.index(
            "schedule_can_transaction(&pending_transaction)", text.index("case CB_COMMAND:")))
        for forbidden in ("CAN_BOOT_", "BLOCK_COMMIT", "0x2000", "0x720", "g_metadata_words"):
            self.assertNotIn(forbidden, text + process)
        self.assertIn('include "host_link_can_process.inc"', text)

    def test_ui_worker_and_image_protocol_fail_closed(self):
        ui = (ROOT / "src/main_frame_workflow.cpp").read_text()
        worker = (ROOT / "src/worker_can_workflows.cpp").read_text()
        self.assertIn("host_link::kCanBurstFeature", ui)
        body = worker[worker.index("bool WorkerSession::ProcessProgramF13Boot"):]
        self.assertLess(body.index("kCanBurstFeature"), body.index("f13_boot::RunUpdate"))
        for name in ("f13_boot_profile.h", "firmware_update_profile.h"):
            text = (ROOT / "src/core" / name).read_text()
            self.assertIn("kProtocolVersion = 5U", text)
        queue = (ROOT / "src/worker_session.cpp").read_text()
        self.assertIn("++dropped_ui_events_", queue)
        self.assertIn("This log is incomplete", queue)

    def test_target_filters_metadata_crc_and_word_bounds_preserved(self):
        config = (F13 / "can_boot_config.h").read_text()
        transport = (F13 / "can_boot_transport.c").read_text()
        rules = (F13 / "RSCANconf/RSCANlite.c").read_text()
        self.assertRegex(config, r"CAN_BOOT_APPLICATION_START\s+\(0x02000UL\)")
        self.assertRegex(config, r"CAN_BOOT_PROTOCOL_VERSION\s+\(0x05U\)")
        self.assertIn("CAN_BOOT_BLOCK_DATA_ID, 0x0000U, 0x07F8U, 0xC000U", rules)
        self.assertIn("CAN_BOOT_RX_RULE_MASK_LOW", rules)
        self.assertNotIn("case CAN_BOOT_CMD_DATA:", transport)
        body = transport[transport.index("case CAN_BOOT_CMD_BLOCK_COMMIT:"):transport.index("case CAN_BOOT_CMD_END:")]
        self.assertLess(body.index("BlockCheck("), body.index("CAN_BootWrite("))
        self.assertLess(body.index("g_metadata_words["), body.index("CAN_BootWrite("))
        self.assertIn("request_offset != expected_offset", body)
        self.assertIn("F13_Crc32Bytes", body)
        self.assertIn("FLASH_END_ADDRESS", transport)
        self.assertIn("g_metadata_sealed == 0U", transport)
        self.assertIn("reply != *(__far unsigned long *)crc_address", transport)


if __name__ == "__main__":
    unittest.main(verbosity=2)
