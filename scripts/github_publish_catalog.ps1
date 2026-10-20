# One catalog for real publication and isolated Git integration tests.
function Get-GithubPublishTargets {
    param([string]$ToolRoot,[string]$F13BootProject,[string]$F13AppProject,
          [string]$F24TransferProject,[string]$F24BootProject,[string]$F24AppProject,
          [string]$F24FdTransferProject,[string]$F24FdBootProject,[string]$F24FdAppProject)
    @(
        @{Name='RL78_F13_Boot_loader_CAN';Root=$F13BootProject;Remote='released/RL78_F13_Boot_loader_CAN'
          Groups=@(@{Relative='DefaultBuild';Archive='f13_boot';Keep=@('boot0000_1FFF.hex')})},
        @{Name='RL78_F13_Boot_loader_CAN_app';Root=$F13AppProject;Remote='released/RL78_F13_Boot_loader_CAN_app'
          Groups=@(@{Relative='DefaultBuild';Archive='f13_app';Keep=@('RL78_F13_Boot_loader_CAN_app_update.bin')})},
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
        @{Name='simple_renesas_isp_tool';Root=$ToolRoot;Remote='released/simple_renesas_isp_tool'
          Groups=@(@{Relative='firmware\RL78_F24_Bridge_boot\DefaultBuild';Archive='bridge_boot';Keep=@('RL78_F24_Bridge_boot.mot')},
            @{Relative='firmware\RL78_F24_Bridge_app\DefaultBuild';Archive='bridge_app'
              Keep=@('RL78_F24_Bridge_app_bridge_update.bin','RL78_F24_Bridge_app_flash.mot')})}
    )
}
