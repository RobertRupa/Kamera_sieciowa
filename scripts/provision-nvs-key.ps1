[CmdletBinding()]
param(
    [ValidateSet('COM8')][string]$Port = 'COM8',
    [switch]$AuthorizeBlockKey0,
    [switch]$ValidateOnly
)

$ErrorActionPreference = 'Stop'
if (-not $AuthorizeBlockKey0 -and -not $ValidateOnly) {
    throw 'Explicit authorization for BLOCK_KEY0/HMAC_UP and its protections is required.'
}
$root = Split-Path $PSScriptRoot -Parent
$python = Join-Path $root 'build/windows-python/python.exe'
if (-not (Test-Path $python)) { throw 'Local Python environment is unavailable.' }

if (-not $ValidateOnly) {
    $identity = & $python -m esptool --chip esp32p4 --port $Port --baud 115200 get-security-info 2>&1
    if ($LASTEXITCODE -ne 0) { throw 'Read-only identity check failed; no eFuse writes attempted.' }
    $identityText = $identity | Out-String
    if ($identityText -notmatch 'MAC:\s+80:f1:b2:d2:bc:c3' -or
        $identityText -notmatch 'BLOCK_KEY0\s+-\s+USER/EMPTY') {
        throw 'Device identity or BLOCK_KEY0 purpose differs; no eFuse writes attempted.'
    }
}

$keyPath = Join-Path ([System.IO.Path]::GetTempPath()) ('camera-hub-nvs-' + [guid]::NewGuid().ToString('N') + '.bin')
$key = New-Object byte[] 32
$random = [System.Security.Cryptography.RandomNumberGenerator]::Create()
try {
    $stream = [System.IO.File]::Open($keyPath, [System.IO.FileMode]::CreateNew,
        [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
    $stream.Dispose()
    $sid = [System.Security.Principal.WindowsIdentity]::GetCurrent().User
    $acl = New-Object System.Security.AccessControl.FileSecurity
    $acl.SetOwner($sid)
    $acl.SetAccessRuleProtection($true, $false)
    $rule = New-Object System.Security.AccessControl.FileSystemAccessRule(
        $sid, [System.Security.AccessControl.FileSystemRights]::FullControl,
        [System.Security.AccessControl.AccessControlType]::Allow)
    $acl.AddAccessRule($rule)
    Set-Acl -LiteralPath $keyPath -AclObject $acl
    $random.GetBytes($key)
    [System.IO.File]::WriteAllBytes($keyPath, $key)
    if ((Get-Item -LiteralPath $keyPath).Length -ne 32 -or
        -not (Get-Acl -LiteralPath $keyPath).AreAccessRulesProtected) {
        throw 'Temporary key length/access protection validation failed.'
    }
    if ($ValidateOnly) {
        Write-Output 'PASS: 256-bit key generated locally, protected temporary file; no eFuse command executed.'
    } else {
        & $python -m espefuse --chip esp32p4 --port $Port --baud 115200 --do-not-confirm burn-key BLOCK_KEY0 $keyPath HMAC_UP
        if ($LASTEXITCODE -ne 0) {
            throw 'eFuse operation failed or is uncertain. Do not retry; inspect device status first.'
        }
        Write-Output 'Official BLOCK_KEY0/HMAC_UP operation completed. Verify purpose/protection metadata next.'
    }
} finally {
    [System.Array]::Clear($key, 0, $key.Length)
    $random.Dispose()
    if (Test-Path -LiteralPath $keyPath) { Remove-Item -LiteralPath $keyPath -Force }
}