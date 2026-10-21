param(
    [string]$Compiler = 'gcc.exe',
    [switch]$RunTests,
    [string[]]$PristineExe = @(),
    [switch]$Package
)
$ErrorActionPreference = 'Stop'
$aw2Compiler = (Get-Command $Compiler -ErrorAction Stop).Source
$aw2SavedPath = $env:Path
if ($RunTests) {
    if (-not $PristineExe) { throw 'Specify -PristineExe with -RunTests.' }
    $aw2Profiles = Get-Content -Raw -LiteralPath (Join-Path $PSScriptRoot 'data/supported-builds.json') | ConvertFrom-Json
    $PristineExe = @($PristineExe | ForEach-Object {
        $aw2Path = (Resolve-Path -LiteralPath $_).Path
        $aw2Hash = (Get-FileHash -LiteralPath $aw2Path -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($aw2Hash -notin $aw2Profiles.sha256) { throw 'The test EXE does not match a supported pristine build.' }
        $aw2Path
    })
}
Push-Location $PSScriptRoot
try {
    $env:Path = (Split-Path -Parent $aw2Compiler) + ';' + $env:Path
    New-Item -ItemType Directory -Path '.\build' -Force | Out-Null
    & $aw2Compiler -std=c17 -Wall -Wextra -Werror -Wno-unused-const-variable -O2 -shared -static-libgcc '-Wl,--image-base,0x39aa10000,--no-insert-timestamp,--dynamicbase,--nxcompat,--high-entropy-va' -o '.\build\AW2PTShadowsFix.asi' '.\src\plugin.c' '.\src\aw2_core.c' '.\src\aw2_builds.c' -lbcrypt
    if ($LASTEXITCODE -ne 0) { throw 'ASI compilation failed.' }
    if ($RunTests) {
        $aw2Nasm = (Get-Command nasm.exe -ErrorAction Stop).Source
        & $aw2Nasm -f win64 -o '.\build\notification_harness.obj' '.\tests\notification_harness.asm'
        if ($LASTEXITCODE -ne 0) { throw 'Test assembly failed.' }
        & $aw2Compiler -std=c17 -Wall -Wextra -Werror -Wno-unused-const-variable -O2 -static-libgcc -o '.\build\test_runtime.exe' '.\tests\test_runtime.c' '.\src\aw2_core.c' '.\src\aw2_builds.c' '.\build\notification_harness.obj' -lbcrypt
        if ($LASTEXITCODE -ne 0) { throw 'Test compilation failed.' }
        Set-Content -LiteralPath '.\build\test-results.txt' -Value ''
        foreach ($aw2TestExe in $PristineExe) {
            & '.\build\test_runtime.exe' $aw2TestExe | Tee-Object -FilePath '.\build\test-results.txt' -Append
            if ($LASTEXITCODE -ne 0) { throw 'Runtime tests failed.' }
        }
    }
    if ($Package) {
        $aw2Version = '0.1.2'
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
