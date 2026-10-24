param([Parameter(Mandatory=$true)][string]$DeviceRoot)
$ErrorActionPreference='Stop'
foreach($selectedBus in @('UART','I2C','Xmodem')){
    $boot='Sample_Project_RH850_S1_ISP_Boot_loader_'+$selectedBus
    foreach($role in @('boot','app','transfer')){
        $name=switch($role){boot{$boot}app{$boot+'_app'}transfer{'Sample_Project_RH850_S1_'+$(if($selectedBus -eq 'I2C'){'I2C_Slave_Transfer'}else{$selectedBus+'_Transfer'})}}
        $project=Join-Path $DeviceRoot $name
        $log=[IO.File]::ReadAllText((Join-Path $project ('tmp\build\'+$role+'_rebuild.log')))
        if($log -notmatch 'Build ended\(Error:0, Warning:\d+\)' -or $log -match 'Failed:[1-9]'){throw ('Build did not pass: '+$name)}
        $warnings=@([regex]::Matches($log,'\bW\d{7}:')|ForEach-Object {$_.Value.TrimEnd(':')})
        $unexpected=@($warnings|Where-Object {$_ -notin @('W0561017','W0561322')})
        if($unexpected.Count){throw ('Unreviewed source/build warnings: '+$name+' / '+($unexpected -join ', '))}
        $map=[IO.File]::ReadAllText((Join-Path $project ('DefaultBuild\'+$name+'.map')))
        if($map -notmatch '(?m)^\.text\r?\n\s+[0-9a-fA-F]+\s+[0-9a-fA-F]+\s+[0-9a-fA-F]+\s+4\s*$'){
            throw ('Linked .text is not4-byte aligned: '+$name)
        }
        Write-Host ('PASS '+$name+': source warnings0;license='+@($warnings|Where-Object {$_ -eq 'W0561017'}).Count+
            ';alignment='+@($warnings|Where-Object {$_ -eq 'W0561322'}).Count+';linked .text align4. Not a zero-warning build.')
    }
}
