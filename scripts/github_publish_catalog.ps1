# Tool firmware is opt-in by ownership; unclassified firmware stays local.
function Test-ToolPublicationPath([string]$Path) {
    $path=$Path.Replace('\','/')
    # These pending checks depend on the withheld multi-family serial runtime.
    # A validated I2C example does not release their whole dependency closure.
    if($path -cin @('scripts/sync_isp_console.ps1','scripts/test_isp_console_guides.ps1',
                    'scripts/test_isp_console_runtime.ps1','scripts/test_i2c_boot_api.ps1',
                    'scripts/test_operation_outcomes.ps1','scripts/test_ui_copy.ps1',
                    'tests/isp_client_tests.cpp','tests/isp_xmodem_client_tests.cpp',
                    'tests/test_tool_ui_field_guide.py','tests/isp_console_runtime_mock.inc',
                    'tests/uart0_console_tests.c')) { return $false }
    # The combined model guide also contains hardware-pending serial roles.
    # F13 I2C acceptance does not release this complete aggregate or its inputs.
    if($path -cin @('docs/DEVICE_APP_UPGRADE_PATHS_zh-TW.md',
                    'docs/Device_App_Upgrade_Guide_zh-TW.pdf',
                    'docs/data/device_upgrade_paths.json',
                    'scripts/device_upgrade_content.mjs',
                    'scripts/verify_device_upgrade_paths.mjs',
                    'docs/ISP_SERIAL_TRANSPORT_GUIDE_zh-TW.md',
                    'docs/ISP_UART_DEVICE_DESIGN_zh-TW.md',
                    'docs/ISP_UART_DEVICE_DESIGN_zh-TW.pdf')) {
        $profilePath=Join-Path (Split-Path -Parent $PSScriptRoot) 'docs/data/device_upgrade_paths.json'
        if(!(Test-Path -LiteralPath $profilePath -PathType Leaf)){return $false}
        try {
            $profiles=@(Get-Content -LiteralPath $profilePath -Raw -Encoding UTF8|ConvertFrom-Json)
            $accepted=@('f13-can','f13-i2c','f24-can','f24-can-fd','f24-i2c','rh850-can','rh850-can-fd')
            return ($profiles.Count -eq $accepted.Count -and
                @(Compare-Object @($profiles.key|Sort-Object) @($accepted|Sort-Object)).Count -eq 0)
        } catch {return $false}
    }
    # Local engineering records are never publication inputs. Architecture notes
    # retain their separate engineering-record status.
    if($path -match '^(?:(?:AGENTS(?:_for_Codex)?|CLAUDE|WORKSPACE_RULES|HANDOFF|CODEX_HANDOFF|SELF_CHECK|MANUAL_CLEANUP|DEVELOPMENT_CHECKLIST)\.md|docs/(?:verification|development|internal)/|docs/FIRST_DELIVERY_20261015_zh-TW\.md$|docs/ARCHITECTURE_V2\.md$)') { return $false }
    if($path -match '^firmware/') {
        if($path -match '^firmware/diagnostics/rh850_reset_vector/(?:entry\.asm|fcl_cfg\.h|platform\.c|recovery\.[ch])$') { return $true }
        if($path -match '^firmware/(?:RL78_F24_Bridge_(?:boot|app)|f24_board|f24_bridge_common)/') { return $true }
        if($path -match '^firmware/common/rh850_can_v4/[^/]+\.[ch]$') { return $true }
        return $path -match '^firmware/common/(?:bridge_[A-Za-z0-9_]+\.[ch]|can_burst\.(?:h|inc)|device_uart_rates\.h|diagnostic_clock\.h|host_link_(?:baud|limits)\.h|image_policy\.h)$'
    }
    # Publication metadata and the reviewed F24 I2C operation projection are
    # candidates only; the exact snapshot and board-acceptance gates still apply.
    if($path -cin @('scripts/verify_isp_publication.ps1','docs/ISP_UART_TOOL_GUIDE_zh-TW.md',
                    'docs/ISP_UART_Tool_Operation_zh-TW.pdf','scripts/sync_isp_uart_guide.ps1')) { return $true }
    # Only the CAN-owned RH850 material is released. Serial runtime, preparation
    # and mixed serial tests retain their existing withheld status.
    if($path -ceq 'docs/RH850_RESET_VECTOR_RECOVERY_zh-TW.md' -or
       $path -match '^docs/RH850_(?:CAN_DEVICE_GUIDE|BOOT_APP_PORTING_GUIDE|IMAGE_REFERENCE|UPDATE_TIMING)_zh-TW\.(?:md|pdf)$' -or
       $path -match '^docs/data/rh850_(?:image_example\.json|update_timing/[^/]+\.json)$' -or
       $path -match '^scripts/(?:build|test)_rh850_reset_vector_recovery\.ps1$' -or
       $path -match '^scripts/(?:build_rh850_can_device|build_rh850_can_transfer|sync_rh850_can_v4|sync_rh850_device)\.ps1$' -or
       $path -ceq 'scripts/sync_rh850_guides.mjs' -or
       $path -ceq 'tests/rh850_reset_vector_recovery_tests.c' -or
       $path -ceq 'tests/test_rh850_guide.py') { return $true }
    # These dedicated MCU support assets share the same withheld status as
    # their firmware. The displayed, hash-bound inventory is still reviewed;
    # this guard does not establish hardware acceptance from a file name.
    if($path -match '^(?:docs|scripts|tests)/(?:[^/]*(?:rh850|isp_(?:serial|uart|i2c|xmodem|device|small|rl78|runtime|bus))|data/rh850)') { return $false }
    if($path -match '^(?:src|res|docs|scripts|tests)/') { return $true }
    if($path -ceq 'build/FirmwareUpgradeTool.exe') { return $true }
    return $path -match '^(?:\.(?:gitignore|gitattributes)|(?:README|AGENTS|HANDOFF|SELF_CHECK|DEVELOPMENT_CHECKLIST)\.md|github_push\.bat|FirmwareUpgradeTool\.(?:sln|vcxproj(?:\.filters)?|rc)|RL78_[A-Za-z0-9_]+\.jpg)$'
}

# One catalog for real publication and isolated Git integration tests.
function Get-GithubPublishTargets {
    param([string]$ToolRoot,[string]$F13BootProject,[string]$F13AppProject,
          [string]$F24TransferProject,[string]$F24BootProject,[string]$F24AppProject,
          [string]$F24FdTransferProject,[string]$F24FdBootProject,[string]$F24FdAppProject,
          [string]$F13TransferProject,
          [switch]$IncludeRh850,
          [string]$Rh850TransferProject,[string]$Rh850BootProject,[string]$Rh850AppProject,
          [string]$Rh850FdTransferProject,[string]$Rh850FdBootProject,[string]$Rh850FdAppProject,
          [switch]$IncludeF24I2c,
          [string]$F24I2cTransferProject,[string]$F24I2cBootProject,[string]$F24I2cAppProject,
          [switch]$IncludeF13I2c,
          [string]$F13I2cTransferProject,[string]$F13I2cBootProject,[string]$F13I2cAppProject)
    # Hardware-pending MCU project trees are distinct from Tool-side RH850 support.
    $pendingMcu='(?i)(?:^|[\\/])(?:Sample_Project_RH850_S1_(?:ISP_)?Boot_loader_|RL78_(?:G16|G22|F13|F24)_ISP_Boot_loader_)[^\\/]*(?:[\\/]|$)'
    $targets=@(
        @{Name='RL78_F13_Boot_loader_CAN';Root=$F13BootProject;Remote='released/RL78_F13_Boot_loader_CAN'
          Groups=@(@{Relative='DefaultBuild';Archive='f13_boot';Keep=@('boot0000_1FFF.hex')})},
        @{Name='RL78_F13_Boot_loader_CAN_app';Root=$F13AppProject;Remote='released/RL78_F13_Boot_loader_CAN_app'
          Groups=@(@{Relative='DefaultBuild';Archive='f13_app';Keep=@('RL78_F13_Boot_loader_CAN_app_update.bin')})},
        @{Name='RL78_F13_CAN_Transfer';Root=$F13TransferProject;Remote='released/RL78_F13_CAN_Transfer'
          Groups=@(@{Relative='DefaultBuild';Archive='f13_transfer';Keep=@('RL78_F13_CAN_Transfer.mot')})},
        @{Name='RL78_F24_CAN_Transfer';Root=$F24TransferProject;Remote='released/RL78_F24_CAN_Transfer'
          Groups=@(@{Relative='DefaultBuild';Archive='f24_device_transfer';Keep=@('RL78_F24_CAN_Transfer.hex')})},
        @{Name='RL78_F24_Boot_loader_CAN';Root=$F24BootProject;Remote='released/RL78_F24_Boot_loader_CAN'
          Groups=@(@{Relative='DefaultBuild';Archive='f24_device_boot';Keep=@('boot0000_1FFF.hex')})},
        @{Name='RL78_F24_Boot_loader_CAN_app';Root=$F24AppProject;Remote='released/RL78_F24_Boot_loader_CAN_app'
          Groups=@(@{Relative='DefaultBuild';Archive='f24_device_app';Keep=@('RL78_F24_Boot_loader_CAN_app_update.bin')})},
        @{Name='RL78_F24_CAN_FD_Transfer';Root=$F24FdTransferProject;Remote='released/RL78_F24_CAN_FD_Transfer'
          Groups=@(@{Relative='DefaultBuild';Archive='f24_fd_transfer';Keep=@('RL78_F24_CAN_FD_Transfer.hex')})},
        @{Name='RL78_F24_Boot_loader_CAN_FD';Root=$F24FdBootProject;Remote='released/RL78_F24_Boot_loader_CAN_FD'
          Groups=@(@{Relative='DefaultBuild';Archive='f24_fd_boot';Keep=@('boot0000_1FFF.hex')})},
        @{Name='RL78_F24_Boot_loader_CAN_FD_app';Root=$F24FdAppProject;Remote='released/RL78_F24_Boot_loader_CAN_FD_app'
          Groups=@(@{Relative='DefaultBuild';Archive='f24_fd_app';Keep=@('RL78_F24_Boot_loader_CAN_FD_app_update.bin')})},
        @{Name='simple_renesas_isp_tool';Root=$ToolRoot;Remote='released/simple_renesas_isp_tool';DeferredPattern=$pendingMcu
          ToolScope=$true;RequiredFiles=@('build/FirmwareUpgradeTool.exe')
          Groups=@(@{Relative='firmware\RL78_F24_Bridge_boot\DefaultBuild';Archive='bridge_boot';Keep=@('RL78_F24_Bridge_boot.mot')},
            @{Relative='firmware\RL78_F24_Bridge_app\DefaultBuild';Archive='bridge_app'
              Keep=@('RL78_F24_Bridge_app_bridge_update.bin','RL78_F24_Bridge_app_flash.mot')})}
    )
    if($IncludeRh850) {
        $targets=@($targets | Where-Object Name -ne 'simple_renesas_isp_tool')+@(
            @{Name='Sample_Project_RH850_S1_CAN_Transfer';Root=$Rh850TransferProject;Remote='released/Sample_Project_RH850_S1_CAN_Transfer'
              Groups=@(@{Relative='DefaultBuild';Archive='rh850_can_transfer';Keep=@('Sample_Project_RH850_S1_CAN_Transfer.mot')})},
            @{Name='Sample_Project_RH850_S1_Boot_loader_CAN';Root=$Rh850BootProject;Remote='released/Sample_Project_RH850_S1_Boot_loader_CAN'
              Groups=@(@{Relative='DefaultBuild';Archive='rh850_can_boot';Keep=@('boot0000_FFFF.hex')})},
            @{Name='Sample_Project_RH850_S1_Boot_loader_CAN_app';Root=$Rh850AppProject;Remote='released/Sample_Project_RH850_S1_Boot_loader_CAN_app'
              Groups=@(@{Relative='DefaultBuild';Archive='rh850_can_app';Keep=@('Sample_Project_RH850_S1_Boot_loader_CAN_app_update.bin')})},
            @{Name='Sample_Project_RH850_S1_CAN_FD_Transfer';Root=$Rh850FdTransferProject;Remote='released/Sample_Project_RH850_S1_CAN_FD_Transfer'
              Groups=@(@{Relative='DefaultBuild';Archive='rh850_fd_transfer';Keep=@('Sample_Project_RH850_S1_CAN_FD_Transfer.mot')})},
            @{Name='Sample_Project_RH850_S1_Boot_loader_CAN_FD';Root=$Rh850FdBootProject;Remote='released/Sample_Project_RH850_S1_Boot_loader_CAN_FD'
              Groups=@(@{Relative='DefaultBuild';Archive='rh850_fd_boot';Keep=@('boot0000_FFFF.hex')})},
            @{Name='Sample_Project_RH850_S1_Boot_loader_CAN_FD_app';Root=$Rh850FdAppProject;Remote='released/Sample_Project_RH850_S1_Boot_loader_CAN_FD_app'
              Groups=@(@{Relative='DefaultBuild';Archive='rh850_fd_app';Keep=@('Sample_Project_RH850_S1_Boot_loader_CAN_FD_app_update.bin')})}
        )+@($targets | Where-Object Name -eq 'simple_renesas_isp_tool')
    }
    if($IncludeF24I2c) {
        $targets=@($targets | Where-Object Name -ne 'simple_renesas_isp_tool')+@(
            @{Name='RL78_F24_I2C_Slave_Transfer';Root=$F24I2cTransferProject;Remote='released/RL78_F24_I2C_Slave_Transfer'
              Groups=@(@{Relative='DefaultBuild';Archive='f24_i2c_transfer';Keep=@('RL78_F24_I2C_Slave_Transfer.hex')})},
            @{Name='RL78_F24_ISP_Boot_loader_I2C';Root=$F24I2cBootProject;Remote='released/RL78_F24_ISP_Boot_loader_I2C'
              Groups=@(@{Relative='DefaultBuild';Archive='f24_i2c_boot';Keep=@('boot0000_1FFF.hex')})},
            @{Name='RL78_F24_ISP_Boot_loader_I2C_app';Root=$F24I2cAppProject;Remote='released/RL78_F24_ISP_Boot_loader_I2C_app'
              Groups=@(@{Relative='DefaultBuild';Archive='f24_i2c_app';Keep=@('RL78_F24_ISP_Boot_loader_I2C_app_update.bin')})}
        )+@($targets | Where-Object Name -eq 'simple_renesas_isp_tool')
    }
    if($IncludeF13I2c) {
        $targets=@($targets | Where-Object Name -ne 'simple_renesas_isp_tool')+@(
            @{Name='RL78_F13_I2C_Slave_Transfer';Root=$F13I2cTransferProject;Remote='released/RL78_F13_I2C_Slave_Transfer'
              Groups=@(@{Relative='DefaultBuild';Archive='f13_i2c_transfer';Keep=@('RL78_F13_I2C_Slave_Transfer.hex')})},
            @{Name='RL78_F13_ISP_Boot_loader_I2C';Root=$F13I2cBootProject;Remote='released/RL78_F13_ISP_Boot_loader_I2C'
              Groups=@(@{Relative='DefaultBuild';Archive='f13_i2c_boot';Keep=@('boot0000_1FFF.hex')})},
            @{Name='RL78_F13_ISP_Boot_loader_I2C_app';Root=$F13I2cAppProject;Remote='released/RL78_F13_ISP_Boot_loader_I2C_app'
              Groups=@(@{Relative='DefaultBuild';Archive='f13_i2c_app';Keep=@('RL78_F13_ISP_Boot_loader_I2C_app_update.bin')})}
        )+@($targets | Where-Object Name -eq 'simple_renesas_isp_tool')
    }
    return $targets
}
