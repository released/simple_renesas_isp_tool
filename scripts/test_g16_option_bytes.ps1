param([Parameter(Mandatory=$true)][string]$DeviceRoot,
      [string]$HubRoot='D:\SourceCode\python\#READ_ME_FIRST#',
      [string]$EvidenceFile='')
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
. (Join-Path $HubRoot 'scripts/invoke_hub_test.ps1')
$checks=New-Object 'Collections.Generic.List[string]'
$fixture=''
function Save-TestText([string]$Path,[string]$Text){[IO.File]::WriteAllText($Path,$Text,(New-Object Text.UTF8Encoding($false)))}
function Hex-Record([int]$Address,[int]$Type,[int[]]$Data=@()){
    $bytes=@($Data.Count,($Address -shr 8),($Address -band 255),$Type)+$Data
    $sum=($bytes|Measure-Object -Sum).Sum
    ':'+(($bytes+@((- [int]$sum) -band 255)|ForEach-Object {'{0:X2}' -f $_}) -join '')
}
$result=Invoke-HubOwnedTest -ProjectRoot $repo -Purpose 'G16 SC/linker/Intel HEX option-byte regression; no device access' -MaxBytes 12MB -Action {
    param($run)
    foreach($bus in @('UART','I2C','Xmodem')){
        foreach($suffix in @($(if($bus -eq 'I2C'){'I2C_Slave_Transfer'}else{$bus+'_Transfer'}),('ISP_Boot_loader_'+$bus),('ISP_Boot_loader_'+$bus+'_app'))){
            $name='RL78_G16_'+$suffix;$source=Join-Path $DeviceRoot $name
            & (Join-Path $source 'validate_scfg.ps1') -Project $source -CheckOnly
            $checks.Add('actual-project/'+$name)
        }
    }
    $name='RL78_G16_ISP_Boot_loader_I2C'
    $source=Join-Path $DeviceRoot $name;$script:fixture=Join-Path $run.Work $name
    [void][IO.Directory]::CreateDirectory($fixture)
    foreach($file in @(($name+'.scfg'),($name+'.mtpj'),'validate_scfg.ps1','isp_transport_config.h','sc_peripheral_config.generated.h')){
        Copy-Item -LiteralPath (Join-Path $source $file) -Destination (Join-Path $fixture $file)
    }
    [void][IO.Directory]::CreateDirectory((Join-Path $fixture 'src'))
    Copy-Item -LiteralPath (Join-Path $source 'src/smc_gen') -Destination (Join-Path $fixture 'src') -Recurse
    $validator=Join-Path $fixture 'validate_scfg.ps1';$mtpj=Join-Path $fixture ($name+'.mtpj')
    $original=[IO.File]::ReadAllText($mtpj)
    foreach($case in @(
        @('c0-ef','>EEF7F9<','>EFF7F9<','WDTON=0 with WDSTBYON=1'),
        @('c0-reserved','>EEF7F9<','>6EF7F9<','reserved bits'),
        @('c0-wdt-enabled','>EEF7F9<','>FEF7F9<','no-WDT profile'),
        @('user-malformed','>EEF7F9<','>EEF7F<','exactly three bytes'),
        @('ocd84','<LinkOptionOcdbgValue-0>85<','<LinkOptionOcdbgValue-0>84<','OCD must'),
        @('ocd-disabled','<LinkOptionOcdbg-0>True<','<LinkOptionOcdbg-0>False<','OCD must'),
        @('clock-f8','>EEF7F9<','>EEF7F8<','user option bytes'),
        @('user-disabled','<LinkOptionUserOptByte-0>True<','<LinkOptionUserOptByte-0>False<','user option bytes'),
        @('security-value','<LinkOptionSecurityOptByteValue-0></','<LinkOptionSecurityOptByteValue-0>85</','security option byte'),
        @('security-enabled','<LinkOptionSecurityOptByte-0>False<','<LinkOptionSecurityOptByte-0>True<','security option byte')
    )){
        if(!$original.Contains($case[1])){throw 'Fixture anchor changed.'}
        Save-TestText $mtpj ($original.Replace($case[1],$case[2]))
        $rejected=$false
        try{& $validator -Project $fixture -CheckOnly}catch{
            if($_.Exception.Message -notmatch $case[3]){throw}
            $rejected=$true
        }
        if(!$rejected){throw ('Missing project option-byte guard: '+$case[0])}
        $checks.Add('reject-project/'+$case[0])
    }
    Save-TestText $mtpj $original
    $config=Join-Path $fixture 'src/smc_gen/r_config/r_bsp_config.h'
    $configText=[IO.File]::ReadAllText($config)
    foreach($case in @(@('BSP_CFG_OPTBYTE2_VALUE (0xF9U)','BSP_CFG_OPTBYTE2_VALUE (0xF8U)','C2=F9'),
                      @('BSP_CFG_OPTBYTE3_VALUE (0x85U)','BSP_CFG_OPTBYTE3_VALUE (0x05U)','C3=85'))){
        if(!$configText.Contains($case[0])){throw 'Generated BSP fixture anchor changed.'}
        Save-TestText $config ($configText.Replace($case[0],$case[1]))
        $rejected=$false
        try{& $validator -Project $fixture -CheckOnly}catch{
            if($_.Exception.Message -notmatch $case[2]){throw}
            $rejected=$true
        }
        if(!$rejected){throw 'Missing generated BSP option-byte guard.'}
        $checks.Add('reject-generated/'+$case[2])
    }
    Save-TestText $config $configText
    $image=Join-Path $fixture 'linked.hex';$eof=Hex-Record 0 1
    $good=Hex-Record 0xC0 0 @(0xEE,0xF7,0xF9,0x85)
    # Expected byte values are from the G16 manual, independent of the validator.
    foreach($records in @(@($good,$eof),@((Hex-Record 0 4 @(0,0)),$good,$eof),
        @((Hex-Record 0 2 @(0,0)),(Hex-Record 0xC0 0 @(0xEE,0xF7)),(Hex-Record 0xC2 0 @(0xF9,0x85)),$eof))){
        Save-TestText $image (($records -join "`n")+"`n")
        & $validator -Project $fixture -CheckOnly -LinkedImagePath $image
        $checks.Add('valid-image/'+$checks.Count)
    }
    $cases=@(
        @{Name='old-ef';Lines=@((Hex-Record 0xC0 0 @(0xEF,0xF7,0xF9,0x85)),$eof);Error='mismatch/missing'},
        @{Name='old-f8';Lines=@((Hex-Record 0xC0 0 @(0xEE,0xF7,0xF8,0x85)),$eof);Error='mismatch/missing'},
        @{Name='ocd05';Lines=@((Hex-Record 0xC0 0 @(0xEE,0xF7,0xF9,0x05)),$eof);Error='mismatch/missing'},
        @{Name='missing-option';Lines=@((Hex-Record 0xC2 0 @(0xF9,0x85)),$eof);Error='mismatch/missing'},
        @{Name='wrong-extended-address';Lines=@((Hex-Record 0 4 @(0,1)),$good,$eof);Error='mismatch/missing'},
        @{Name='overlap';Lines=@($good,$good,$eof);Error='Overlapping'},
        @{Name='missing-eof';Lines=@($good);Error='Missing Intel HEX EOF'},
        @{Name='after-eof';Lines=@($good,$eof,$good);Error='after EOF'},
        @{Name='checksum';Lines=@(($good.Substring(0,$good.Length-2)+'00'),$eof);Error='checksum'},
        @{Name='bad-length';Lines=@(':0400C000EF00',$eof);Error='length'},
        @{Name='bad-type';Lines=@((Hex-Record 0 6),$good,$eof);Error='Unsupported'},
        @{Name='bad-extension';Lines=@((Hex-Record 0 4 @(0)),$good,$eof);Error='linear address'}
    )
    foreach($case in $cases){
        Save-TestText $image (($case.Lines -join "`n")+"`n")
        $rejected=$false
        try{& $validator -Project $fixture -CheckOnly -LinkedImagePath $image}catch{
            if($_.Exception.Message -notmatch $case.Error){throw}
            $rejected=$true
        }
        if(!$rejected){throw ('Missing linked-image guard: '+$case.Name)}
        $checks.Add('reject-image/'+$case.Name)
    }
    # Exercise the real pre-build hook: it must stop before replacing vectors.
    Save-TestText $mtpj ($original.Replace('>EEF7F9<','>EEF7F8<'))
    $sentinel=Join-Path $fixture 'ftable.inc';Save-TestText $sentinel 'OPTION_GUARD_SENTINEL'
    $rejected=$false
    try{& (Join-Path $source 'generate_irq_contract.ps1') -Mode Boot -OutputProject $fixture}catch{
        if($_.Exception.Message -notmatch 'SC consistency:.*user option bytes'){throw}
        $rejected=$true
    }
    if(!$rejected -or [IO.File]::ReadAllText($sentinel) -cne 'OPTION_GUARD_SENTINEL'){throw 'Pre-build did not fail before generation.'}
    $checks.Add('actual-prebuild/fail-before-vector-generation')
    Save-TestText $mtpj $original
    Copy-Item -LiteralPath (Join-Path $source 'post_build.ps1') -Destination (Join-Path $fixture 'post_build.ps1')
    [void][IO.Directory]::CreateDirectory((Join-Path $fixture 'DefaultBuild'))
    Save-TestText (Join-Path $fixture ('DefaultBuild/'+$name+'.hex')) ((Hex-Record 0xC0 0 @(0xEF,0xF7,0xF8,0x05))+"`n"+$eof+"`n")
    $rejected=$false
    try{& (Join-Path $fixture 'post_build.ps1') -Kind boot -Project $fixture}catch{
        if($_.Exception.Message -notmatch 'Linked G16 option byte mismatch/missing'){throw}
        $rejected=$true
    }
    if(!$rejected -or (Test-Path -LiteralPath (Join-Path $fixture 'DefaultBuild/boot0000_1FFF.hex'))){throw 'Post-build did not stop before packaging.'}
    $checks.Add('actual-postbuild/fail-before-packaging')
} -CollectArtifacts {
    param($run)
    # This fresh tree is produced only by the declared fixture copy/writers above.
    Register-HubArtifactTree -Run $run -Producer 'G16 option-byte fixture copy and deterministic mutations'
}
if($EvidenceFile){
    [void][IO.Directory]::CreateDirectory((Split-Path -Parent ([IO.Path]::GetFullPath($EvidenceFile))))
    [ordered]@{Success=$result.Success;Outcome=$result.Outcome;Checks=$checks.ToArray();CleanupRemoved=$result.CleanupRemoved}|ConvertTo-Json -Depth 6|Set-Content -LiteralPath $EvidenceFile -Encoding UTF8
}
if(!$result.Success){$result|ConvertTo-Json -Depth 7;throw 'G16 option-byte regression or owned cleanup failed.'}
Write-Host ('PASS '+$checks.Count+' G16 option-byte checks; fixtures retired; no hardware access.')
