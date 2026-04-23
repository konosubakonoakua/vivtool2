# build.ps1 - Script to build vivlauncher using Visual Studio environment

$vsPath = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
$projectFile = "vivlauncher.vcxproj"
$configuration = "Release"
$platform = "x64"

if ($args.Count -ge 1) { $configuration = $args[0] }

Write-Host "Building $projectFile [$configuration|$platform]..." -ForegroundColor Cyan

# Use cmd.exe to set up the environment and run msbuild
$cmd = "call `"$vsPath`" && msbuild `"$projectFile`" /p:Configuration=$configuration /p:Platform=$platform"

cmd.exe /c $cmd

if ($LASTEXITCODE -eq 0) {
    Write-Host "`nBuild Successful!" -ForegroundColor Green
} else {
    Write-Host "`nBuild Failed with exit code $LASTEXITCODE" -ForegroundColor Red
}

exit $LASTEXITCODE
