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

$builddir = "$PSScriptRoot\cmake-build-clang-$($buildType.ToLower())"
if (Test-Path $builddir) {
    Remove-Item $builddir -Recurse
}
mkdir $builddir
cd $builddir

if ($withFrankendrift) {
	$cmFd = "ON"
} else {
	$cmFd = "OFF"
}

if ($arm64cross) {
    cmake -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_LINKER=lld-link -DINTERFACE=QT -DCMAKE_PREFIX_PATH="$qtPath/msvc2019_arm64" -DCMAKE_TOOLCHAIN_FILE="$vcpkgPath/scripts/buildsystems/vcpkg.cmake" -DSOUND=QT -DWITH_FRANKENDRIFT=ON -DCMAKE_C_COMPILER_TARGET=arm64-pc-windows-msvc -DCMAKE_CXX_COMPILER_TARGET=arm64-pc-windows-msvc -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY -DCMAKE_CROSSCOMPILING=ON -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_SYSTEM_PROCESSOR=arm64 -DCMAKE_STATIC_LINKER_FLAGS="/machine:arm64 " -DCMAKE_SHARED_LINKER_FLAGS="/machine:arm64 " -DCMAKE_EXE_LINKER_FLAGS="/machine:arm64 " -DWITH_FRANKENDRIFT=ON -DDIST_INSTALL=ON ..
} else {
    cmake -G Ninja -DCMAKE_BUILD_TYPE="$buildType" -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_LINKER=lld-link -DINTERFACE=QT -DCMAKE_PREFIX_PATH="$qtPath/msvc2019_64" -DCMAKE_TOOLCHAIN_FILE="$vcpkgPath/scripts/buildsystems/vcpkg.cmake" -DSOUND=QT -DDIST_INSTALL=ON -DWITH_FRANKENDRIFT="$cmFd" ..
}
ninja

$stagedir = "$PSScriptRoot\gargoyle-staging"
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

cd "$stagedir"
if ($withPdbs) {
    windeployqt --no-opengl-sw --no-compiler-runtime --no-system-d3d-compiler --no-pdf --pdb garglk.dll
} else {
    windeployqt --no-opengl-sw --no-compiler-runtime --no-system-d3d-compiler --no-pdf garglk.dll
}
cd $loc
