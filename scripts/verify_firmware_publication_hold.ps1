# Compatibility entry: validate the scoped CAN release gate using owned fixtures.
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'verify_can_publication.ps1') -GateOnly
if(!$?) { throw 'Scoped firmware publication gate verification failed.' }