param(
    [bool]$withFrankendrift = $true,
    [Parameter(Mandatory=$true)]
    [string]$buildType,
    [switch]$withPdbs,
    [Parameter(Mandatory=$true)]
    [string]$vcpkgPath,
    [Parameter(Mandatory=$true)]
    [string]$qtPath
)

$ErrorActionPreference = "Stop"
$loc = Get-Location

$builddir = "$PSScriptRoot\cmake-build-clang-$($buildType.ToLower())"
if (Test-Path $builddir) {
    Remove-Item $builddir -Recurse
}
mkdir $builddir
cd $builddir

cmake -G Ninja -DCMAKE_BUILD_TYPE="$buildType" -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_LINKER=lld-link -DINTERFACE=QT -DCMAKE_PREFIX_PATH="$qtPath/msvc2019_64" -DCMAKE_TOOLCHAIN_FILE="$vcpkgPath/scripts/buildsystems/vcpkg.cmake" -DSOUND=QT ..
ninja

$stagedir = "$PSScriptRoot\gargoyle-staging"
if (Test-Path "$stagedir") {
    Remove-Item $stagedir -Recurse
}
mkdir $stagedir
Copy-Item "$builddir\*.ttf" "$stagedir"
Copy-Item "$builddir\*.otf" "$stagedir"
Copy-Item "$builddir\*.exe" "$stagedir"
Copy-Item "$builddir\garglk\*.dll" "$stagedir"
Copy-Item "$builddir\terps\*.exe" "$stagedir"
Copy-Item "$builddir\terps\tads\*.exe" "$stagedir"
if ($withPdbs) {
    Copy-Item "$builddir\garglk\*.pdb" "$stagedir"
    Copy-Item "$builddir\terps\*.pdb" "$stagedir"
    Copy-Item "$builddir\terps\tads\*.pdb" "$stagedir"
}

if ($withFrankendrift) {
    cd "$PSScriptRoot\terps\frankendrift\FrankenDrift.GlkRunner\FrankenDrift.GlkRunner.Gargoyle"
    Copy-Item "$builddir\garglk\garglk.lib" .
    if ($buildType -eq "Debug") {
        $fdConfig = "Debug"
    } else {
        $fdConfig = "Release"
    }
    dotnet publish --self-contained -c $fdConfig -f net8.0 -r win-x64 -p:GarglkStatic=true
    Copy-Item bin\x64\Release\net8.0\win-x64\publish\FrankenDrift.GlkRunner.Gargoyle.exe "$stagedir"
    if ($withPdbs) {
        Copy-Item "bin\x64\Release\net8.0\win-x64\publish\*.pdb" "$stagedir"
    }
}

cd "$stagedir"
if ($withPdbs) {
    windeployqt --no-opengl-sw --no-compiler-runtime --no-system-d3d-compiler --no-network --no-pdf --pdb garglk.dll
} else {
    windeployqt --no-opengl-sw --no-compiler-runtime --no-system-d3d-compiler --no-network --no-pdf garglk.dll
}
cd $loc