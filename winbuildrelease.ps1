param(
    [bool]$withFrankendrift = $true,
    [Parameter(Mandatory=$true)]
    [string]$buildType,
    [switch]$withPdbs,
    [Parameter(Mandatory=$true)]
    [string]$vcpkgPath,
    [Parameter(Mandatory=$true)]
    [string]$qtPath,
    [bool]$arm64Cross = $false
)

$ErrorActionPreference = "Stop"
$loc = Get-Location

if ($arm64Cross) {
    $target = "arm64"
} else {
    $target = "x64"
}

$builddir = "$PSScriptRoot\cmake-build-clang-$($buildType.ToLower())-$target"
if (Test-Path $builddir) {
    Remove-Item $builddir -Recurse
}
mkdir $builddir
Set-Location $builddir

if ($withFrankendrift) {
	$cmFd = "ON"
} else {
	$cmFd = "OFF"
}

if ($arm64cross) {
    cmake -G Ninja -DCMAKE_BUILD_TYPE="$buildType" -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_LINKER=lld-link -DINTERFACE=QT -DCMAKE_PREFIX_PATH="$qtPath/msvc2019_arm64" -DCMAKE_TOOLCHAIN_FILE="$vcpkgPath/scripts/buildsystems/vcpkg.cmake" -DSOUND=QT -DWITH_FRANKENDRIFT=ON -DCMAKE_C_COMPILER_TARGET=arm64-pc-windows-msvc -DCMAKE_CXX_COMPILER_TARGET=arm64-pc-windows-msvc -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY -DCMAKE_CROSSCOMPILING=ON -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_SYSTEM_PROCESSOR=arm64 -DCMAKE_STATIC_LINKER_FLAGS="/machine:arm64 " -DCMAKE_SHARED_LINKER_FLAGS="/machine:arm64 " -DCMAKE_EXE_LINKER_FLAGS="/machine:arm64 " -DWITH_FRANKENDRIFT="$cmFd" -DDIST_INSTALL=ON ..
} else {
    cmake -G Ninja -DCMAKE_BUILD_TYPE="$buildType" -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_LINKER=lld-link -DINTERFACE=QT -DCMAKE_PREFIX_PATH="$qtPath/msvc2019_64" -DCMAKE_TOOLCHAIN_FILE="$vcpkgPath/scripts/buildsystems/vcpkg.cmake" -DSOUND=QT -DDIST_INSTALL=ON -DWITH_FRANKENDRIFT="$cmFd" ..
}
ninja

# I do not know why, I do not want to know why, I should not have to wonder why, but sometimes dotnet will simply stop before the NativeAOT step. (When switching from building for one arch to the other.)
# Usually it starts working on the fourth attempt for no apparent reason, so I settled for this nonsense:
if ($withFrankendrift) {
    $i = 1
    # The NativeAOT executable should be around 8.5MB large.
    while ($i -le 10 -and (Get-Item "$builddir\terps\frankendrift\FrankenDrift.GlkRunner\FrankenDrift.GlkRunner.Gargoyle\FrankenDrift.GlkRunner.Gargoyle.exe").Length -lt 8000000) {
        Write-Host "[$i/10] FrankenDrift Exe too small, trying again..." -ForegroundColor Red
        Remove-Item -r "$builddir\terps\frankendrift\FrankenDrift.GlkRunner\FrankenDrift.GlkRunner.Gargoyle\bin"
        Remove-Item -r "$builddir\terps\frankendrift\FrankenDrift.GlkRunner\FrankenDrift.GlkRunner.Gargoyle\obj"
        Remove-Item "$builddir\terps\frankendrift\FrankenDrift.GlkRunner\FrankenDrift.GlkRunner.Gargoyle\*.pdb"
        Remove-Item "$builddir\terps\frankendrift\FrankenDrift.GlkRunner\FrankenDrift.GlkRunner.Gargoyle\*.exe"
        Remove-Item "$builddir\terps\frankendrift\FrankenDrift.GlkRunner\FrankenDrift.GlkRunner.Gargoyle\*.dll"
        ninja frankendrift
        $i += 1
    }
    if ($i -gt 10) {
        Write-Error "Failed to produce a working FrankenDrift executable in a reasonable number of attempts."
    } else {
        Write-Host "Produced a working FrankenDrift executable in $i attempt(s)." -ForegroundColor Green
    }
}

$stagedir = "$PSScriptRoot\gargoyle-staging-$target"
if (Test-Path "$stagedir") {
    Remove-Item $stagedir -Recurse
}
mkdir $stagedir
Copy-Item "$builddir\*.ttf" "$stagedir"
Copy-Item "$builddir\*.otf" "$stagedir"
Copy-Item "$builddir\*.exe" "$stagedir"
Copy-Item "$builddir\garglk\gargoyle.exe" "$stagedir"
Copy-Item "$builddir\garglk\*.dll" "$stagedir"
Copy-Item "$builddir\terps\*.exe" "$stagedir"
Copy-Item "$builddir\terps\tads\*.exe" "$stagedir"
if ($withPdbs) {
    Copy-Item "$builddir\garglk\*.pdb" "$stagedir"
    Copy-Item "$builddir\terps\*.pdb" "$stagedir"
    Copy-Item "$builddir\terps\tads\*.pdb" "$stagedir"
}

if ($withFrankendrift) {
    Copy-Item "$builddir\terps\frankendrift\FrankenDrift.GlkRunner\FrankenDrift.GlkRunner.Gargoyle\FrankenDrift.GlkRunner.Gargoyle.exe" "$stagedir"
    if ($withPdbs) {
        Copy-Item "$builddir\terps\frankendrift\FrankenDrift.GlkRunner\FrankenDrift.GlkRunner.Gargoyle\*.pdb" "$stagedir"
    }
}

Set-Location "$stagedir"
$deployArgs = @("--no-opengl-sw","--no-compiler-runtime","--no-system-d3d-compiler","--no-system-dxc-compiler","--no-pdf","--exclude-plugins","ffmpegmediaplugin,qopensslbackend","--skip-plugin-types","iconengines,imageformats,tls")
if ($withPdbs) {
    $deployArgs += "--pdb"
}
if ($arm64Cross) {
    # windeployqt doesn't understand cross-compilation and will deploy x64 Qt binaries, so we need to do this ourselves:
    $deployInfo = (windeployqt @deployArgs --dry-run --json garglk.dll | ConvertFrom-Json).files
    # (`--json` mode ignores most of the exclusions we made above, so we have to do this terribleness)
    $deployInfo = $deployInfo | Where-Object {
        (-not ((Split-Path $_.target -Leaf) -in @("tls","iconengines","imageformats"))) -and (-not ((Split-Path $_.source -Leaf) -in @("ffmpegmediaplugin.dll","avcodec-60.dll","avformat-60.dll","avutil-58.dll","swresample-4.dll","swscale-7.dll")))
    }
    # create target directories
    New-Item -Type Directory -Force -Path ($deployInfo | Select-Object -ExpandProperty target -Unique)
    $deployInfo | ForEach-Object {
        Write-Host "Deploying:" (Split-Path $_.source -Leaf) "-->" $_.target
        Copy-Item $_.source.Replace('\msvc2019_64\', '\msvc2019_arm64\') $_.target
        # The `--pdb` switch is also ignored.
        # (I'm starting to get the impression that `--json` mode isn't very good...)
        if ($withPdbs -and $_.source.EndsWith(".dll")) {
            Write-Host "Deploying:" (Split-Path $_.source -Leaf).Replace(".dll", ".pdb") "-->" $_.target
            Copy-Item $_.source.Replace('\msvc2019_64\', '\msvc2019_arm64\').Replace(".dll", ".pdb") $_.target
        }
    }
} else {
    windeployqt @deployArgs garglk.dll
}
Set-Location $loc
