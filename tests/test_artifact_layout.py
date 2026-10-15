"""Permanent source paths must not depend on disposable diagnostics."""
from pathlib import Path
import re
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
NS = {"m": "http://schemas.microsoft.com/developer/msbuild/2003"}


class ArtifactLayoutTests(unittest.TestCase):
    def test_msbuild_outputs_are_temporary(self):
        for name in ("FirmwareUpgradeTool.vcxproj", "tests/HostLinkProtocolTests.vcxproj"):
            project = ET.parse(ROOT / name)
            for tag in ("OutDir", "IntDir"):
                nodes = project.findall(".//m:" + tag, NS)
                self.assertTrue(nodes)
                for node in nodes:
                    self.assertIn("tmp\\internal\\build\\", node.text)

    def test_scripts_keep_delivery_separate(self):
        build = (ROOT / "scripts/build_mfc.ps1").read_text(encoding="utf-8-sig")
        self.assertIn('"build\\FirmwareUpgradeTool.exe"', build)
        self.assertIn('"tmp\\internal\\build\\debug\\', build)
        self.assertIn('"tmp\\internal\\build\\release\\', build)
        clean = (ROOT / "scripts/clean_mfc.bat").read_text()
        self.assertIn('set "BUILD_DIR=%~dp0..\\tmp\\internal\\build"', clean)
        self.assertNotIn('set "BUILD_DIR=%~dp0..\\build"', clean)
        firmware = (ROOT / "scripts/build_f24_bridge.ps1").read_text()
        self.assertIn("tmp\\internal\\build\\logs", firmware)
        self.assertIn("New-Item -ItemType Directory -Path $logRoot", firmware)
        self.assertIn("tmp/", (ROOT / ".gitignore").read_text())

    def test_active_doc_index_has_no_disposable_links(self):
        docs = ROOT / "docs"
        self.assertFalse(list(docs.glob("*VERIFICATION.md")))
        self.assertFalse(list(docs.glob("*_PLAN.md")))
        for file in (docs / "README.md", ROOT / "README.md"):
            for target in re.findall(r"\]\(([^)]+)\)", file.read_text(encoding="utf-8-sig")):
                if target.startswith(("http:", "https:", "#", "mailto:")):
                    continue
                self.assertNotIn("tmp/", target)
                self.assertTrue((file.parent / target.split("#")[0]).exists(), target)


if __name__ == "__main__":
    unittest.main(verbosity=2)
