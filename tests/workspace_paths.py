"""Portable companion checkout paths; no developer-specific workspace layout."""
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def companion_path(kind, root=ROOT):
    variable, folder = {
        'F13Boot': ('FIRMWARE_F13_BOOT_PROJECT', 'RL78_F13_Boot_loader_CAN'),
        'F13App': ('FIRMWARE_F13_APP_PROJECT', 'RL78_F13_Boot_loader_CAN_app'),
        'Web': ('FIRMWARE_WEB_ROOT', None),
    }[kind]
    configured = os.environ.get(variable)
    if configured:
        return Path(configured).expanduser().resolve()
    parent = Path(root).resolve().parent
    return parent / folder if folder else parent
