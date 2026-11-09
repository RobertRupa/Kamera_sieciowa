$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
docker build -t camera-hub-host-tests -f "$root/tests/host/Dockerfile" "$root/tests/host"
if ($LASTEXITCODE -ne 0) { throw 'Host test image build failed.' }
docker run --rm --mount "type=bind,source=$root,target=/workspace" camera-hub-host-tests
if ($LASTEXITCODE -ne 0) { throw 'Host tests failed.' }