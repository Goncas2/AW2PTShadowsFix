param(
    [string]$Compiler = 'gcc.exe',
    [switch]$RunTests,
    [string]$PristineExe = '',
    [switch]$Package
)
$ErrorActionPreference = 'Stop'
$aw2Compiler = (Get-Command $Compiler -ErrorAction Stop).Source
$aw2SavedPath = $env:Path
if ($RunTests) {
    if (-not $PristineExe) { throw 'Specify -PristineExe with -RunTests.' }
    $PristineExe = (Resolve-Path -LiteralPath $PristineExe).Path
    if ((Get-FileHash -LiteralPath $PristineExe -Algorithm SHA256).Hash.ToLowerInvariant() -ne '0e7a7b5fe05ada30ac043d96eeb658e934db96b4783479ade64495ed428b5904') {
        throw 'The test EXE does not match the supported pristine build.'
    }
}
Push-Location $PSScriptRoot
try {
    $env:Path = (Split-Path -Parent $aw2Compiler) + ';' + $env:Path
    New-Item -ItemType Directory -Path '.\build' -Force | Out-Null
    & $aw2Compiler -std=c17 -Wall -Wextra -Werror -Wno-unused-const-variable -O2 -shared -static-libgcc '-Wl,--image-base,0x39aa10000,--no-insert-timestamp,--dynamicbase,--nxcompat,--high-entropy-va' -o '.\build\AW2PTShadowsFix.asi' '.\src\plugin.c' '.\src\aw2_core.c' -lbcrypt
    if ($LASTEXITCODE -ne 0) { throw 'ASI compilation failed.' }
    if ($RunTests) {
        $aw2Nasm = (Get-Command nasm.exe -ErrorAction Stop).Source
        & $aw2Nasm -f win64 -o '.\build\notification_harness.obj' '.\tests\notification_harness.asm'
        if ($LASTEXITCODE -ne 0) { throw 'Test assembly failed.' }
        & $aw2Compiler -std=c17 -Wall -Wextra -Werror -Wno-unused-const-variable -O2 -static-libgcc -o '.\build\test_runtime.exe' '.\tests\test_runtime.c' '.\src\aw2_core.c' '.\build\notification_harness.obj'
        if ($LASTEXITCODE -ne 0) { throw 'Test compilation failed.' }
        & '.\build\test_runtime.exe' $PristineExe | Tee-Object -FilePath '.\build\test-results.txt'
        if ($LASTEXITCODE -ne 0) { throw 'Runtime tests failed.' }
    }
    if ($Package) {
        $aw2Version = '0.1.1'
        New-Item -ItemType Directory -Path '.\dist\plugins' -Force | Out-Null
        Copy-Item -LiteralPath '.\build\AW2PTShadowsFix.asi' -Destination '.\dist\plugins\AW2PTShadowsFix.asi' -Force
        Copy-Item -LiteralPath '.\README.md' -Destination '.\dist\AW2PTShadowsFix-README.md' -Force
        Copy-Item -LiteralPath '.\LICENSE' -Destination '.\dist\AW2PTShadowsFix-LICENSE.txt' -Force
        Compress-Archive -LiteralPath '.\dist\plugins','.\dist\AW2PTShadowsFix-README.md','.\dist\AW2PTShadowsFix-LICENSE.txt' -DestinationPath ('.\dist\AW2PTShadowsFix-'+$aw2Version+'.zip') -Force
    }
} finally {
    Pop-Location
    $env:Path = $aw2SavedPath
}
