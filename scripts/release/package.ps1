<#
.SYNOPSIS
    Stage a clean portable CandyLauncher package from a Release build directory and zip it.

.DESCRIPTION
    Copies only the runtime files (whitelist) from the CMake Release build directory into
    dist\CandyLauncher\, then writes dist\CandyLauncher-v<version>-<arch>.zip with a single
    top-level "CandyLauncher" folder. The updater (3rdparty/github-releases-autoupdater)
    accepts this layout. Build artifacts (*.obj, *.lib, *.exp, *.pdb, CMakeFiles, tests,
    user_settings.json, logs) are never copied.

    Version comes from <BuildDir>\generated\common\AppVersion.h (single source of truth is
    project(CandyLauncher VERSION x.y.z) in CMakeLists.txt). Arch comes from the PE header.

.PARAMETER BuildDir
    CMake Release build directory. Default: cmake-build-release-ninja-vs

.PARAMETER OutDir
    Output directory for the staging folder and the zip. Default: dist

.PARAMETER SkinList
    Text file listing the skins to ship, one name per line ("#" comments). Default: scriptselease\package_skins.txt

.EXAMPLE
    powershell -NoProfile -ExecutionPolicy Bypass -File scripts\release\package.ps1
#>
param(
	[string]$BuildDir = "cmake-build-release-ninja-vs",
	[string]$OutDir = "dist",
	[string]$SkinList = (Join-Path $PSScriptRoot "package_skins.txt")
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version 2

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$buildPath = Join-Path $repoRoot $BuildDir
$outPath = Join-Path $repoRoot $OutDir
$appName = "CandyLauncher"
$stage = Join-Path $outPath $appName

function Fail($msg) {
	Write-Host "[package] ERROR: $msg" -ForegroundColor Red
	exit 1
}

function Copy-Required($src, $dstDir) {
	if (-not (Test-Path -LiteralPath $src)) { Fail "required file missing: $src" }
	New-Item -ItemType Directory -Force -Path $dstDir | Out-Null
	Copy-Item -LiteralPath $src -Destination $dstDir -Force
}

function Copy-Optional($src, $dstDir) {
	if (-not (Test-Path -LiteralPath $src)) {
		Write-Host "[package] skip (not found): $src" -ForegroundColor DarkYellow
		return
	}
	New-Item -ItemType Directory -Force -Path $dstDir | Out-Null
	Copy-Item -LiteralPath $src -Destination $dstDir -Force
}

# Copy a directory tree, keeping only files whose name matches one of $include
# (wildcards) and skipping files matching $exclude.
function Copy-Tree($srcDir, $dstDir, [string[]]$include, [string[]]$exclude = @()) {
	if (-not (Test-Path -LiteralPath $srcDir)) { Fail "required directory missing: $srcDir" }
	$srcFull = (Resolve-Path -LiteralPath $srcDir).Path
	$files = Get-ChildItem -LiteralPath $srcFull -File -Recurse
	foreach ($f in $files) {
		$name = $f.Name
		$keep = $false
		foreach ($p in $include) { if ($name -like $p) { $keep = $true; break } }
		if (-not $keep) { continue }
		foreach ($p in $exclude) { if ($name -like $p) { $keep = $false; break } }
		if (-not $keep) { continue }
		$rel = $f.FullName.Substring($srcFull.Length).TrimStart('\')
		$dst = Join-Path $dstDir $rel
		New-Item -ItemType Directory -Force -Path (Split-Path $dst) | Out-Null
		Copy-Item -LiteralPath $f.FullName -Destination $dst -Force
	}
}

# ---------------------------------------------------------------- preflight
if (-not (Test-Path -LiteralPath $buildPath)) { Fail "build directory not found: $buildPath (run scripts\release\run_cmake_generate.cmd + run_build.cmd first)" }

$exePath = Join-Path $buildPath "$appName.exe"
if (-not (Test-Path -LiteralPath $exePath)) { Fail "$appName.exe not found in $buildPath" }

$cache = Get-Content -LiteralPath (Join-Path $buildPath "CMakeCache.txt") -Encoding UTF8
$buildTypeMatch = $cache | Select-String '^CMAKE_BUILD_TYPE:\w+=(.*)$' | Select-Object -First 1
$buildType = if ($buildTypeMatch) { $buildTypeMatch.Matches[0].Groups[1].Value.Trim() } else { "" }
if ($buildType -and $buildType -ne "Release") { Fail "CMAKE_BUILD_TYPE is '$buildType', expected Release. Refusing to package a non-Release build." }

$versionHeader = Join-Path $buildPath "generated\common\AppVersion.h"
if (-not (Test-Path -LiteralPath $versionHeader)) { Fail "AppVersion.h not found: $versionHeader" }
$versionLine = Get-Content -LiteralPath $versionHeader -Encoding UTF8 | Select-String '#define APP_VERSION_STRING "([^"]+)"' | Select-Object -First 1
if (-not $versionLine) { Fail "APP_VERSION_STRING not found in $versionHeader" }
$version = $versionLine.Matches[0].Groups[1].Value

# Architecture from the PE header (IMAGE_FILE_HEADER.Machine)
$fs = [IO.File]::OpenRead($exePath)
try {
	$br = New-Object IO.BinaryReader($fs)
	$fs.Seek(0x3C, 0) | Out-Null
	$peOffset = $br.ReadInt32()
	$fs.Seek($peOffset + 4, 0) | Out-Null
	$machine = $br.ReadUInt16()
} finally { $fs.Close() }
switch ($machine) {
	0x8664 { $arch = "x64" }
	0x014C { $arch = "x86" }
	0xAA64 { $arch = "arm64" }
	default { Fail ("unknown PE machine type 0x{0:X4}" -f $machine) }
}
$everythingDll = if ($arch -eq "x86") { "Everything32.dll" } else { "Everything64.dll" }

Write-Host "[package] $appName $version ($arch) from $buildPath"

# ---------------------------------------------------------------- stage
if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
New-Item -ItemType Directory -Force -Path $stage | Out-Null

# Executables and shared libraries
Copy-Required $exePath $stage
Copy-Optional (Join-Path $buildPath "SkinEditor.exe") $stage
Copy-Required (Join-Path $buildPath $everythingDll) $stage
Copy-Required (Join-Path $buildPath "cpp-pinyin.dll") $stage
Copy-Required (Join-Path $buildPath "sqlite3.dll") $stage

# Pinyin dictionaries (util/PinyinHelper.hpp loads <exe>\dict)
Copy-Tree (Join-Path $buildPath "dict") (Join-Path $stage "dict") @("*")

# Skins: only the ones listed in $SkinList (scripts\release\package_skins.txt by default) are shipped.
# "default" and "night" are hard-wired in the app (DEFAULT_SKIN_PATH / NIGHT_SKIN_PATH) and always go in.
# For each name: skins\skin_<name>.json, the folder skins\<name>\ when it exists, and every file the json
# references with a skin-relative "\dir\file" path (e.g. the shared \skin_default\*.9.png).
# skins\README.md, THEME_GENERATION.md and the skin_test_*.json files are not shipped.
$skinsSrc = Join-Path $buildPath "skins"
$skinsDst = Join-Path $stage "skins"
if (-not (Test-Path -LiteralPath $SkinList)) { Fail "skin list not found: $SkinList" }
$skinNames = @("default", "night")
foreach ($line in Get-Content -LiteralPath $SkinList -Encoding UTF8) {
	$n = ($line -replace '#.*$', '').Trim()
	if ($n -and $skinNames -notcontains $n) { $skinNames += $n }
}
# Collect every string in a parsed skin json that is a skin-relative path ("\x\y", not UNC "\\x").
function Get-SkinRelativePaths($node) {
	$found = @()
	if ($null -eq $node) { return $found }
	if ($node -is [string]) {
		if ($node.StartsWith('\') -and -not $node.StartsWith('\\')) { $found += $node }
	} elseif ($node -is [array]) {
		foreach ($item in $node) { $found += Get-SkinRelativePaths $item }
	} elseif ($node -is [Management.Automation.PSCustomObject]) {
		foreach ($prop in $node.PSObject.Properties) { $found += Get-SkinRelativePaths $prop.Value }
	}
	return $found
}
foreach ($name in $skinNames) {
	$skinJson = Join-Path $skinsSrc "skin_$name.json"
	Copy-Required $skinJson $skinsDst
	$skinFolder = Join-Path $skinsSrc $name
	if (Test-Path -LiteralPath $skinFolder -PathType Container) {
		Copy-Tree $skinFolder (Join-Path $skinsDst $name) @("*")
	}
	try {
		$parsed = Get-Content -LiteralPath $skinJson -Raw -Encoding UTF8 | ConvertFrom-Json
	} catch { Fail "skin json is not valid JSON: $skinJson ($($_.Exception.Message))" }
	foreach ($rel in Get-SkinRelativePaths $parsed) {
		$relTrim = $rel.TrimStart('\')
		$src = Join-Path $skinsSrc $relTrim
		if (-not (Test-Path -LiteralPath $src -PathType Leaf)) {
			Write-Host "[package] skin '$name' references a missing file: $rel" -ForegroundColor DarkYellow
			continue
		}
		$dst = Join-Path $skinsDst $relTrim
		New-Item -ItemType Directory -Force -Path (Split-Path $dst) | Out-Null
		Copy-Item -LiteralPath $src -Destination $dst -Force
	}
}

# UI language resource DLLs (common/I18n.hpp loads <exe>\lang\<code>.dll)
Copy-Tree (Join-Path $buildPath "lang") (Join-Path $stage "lang") @("*.dll")

# Plugins: DLL + manifest only from the plugins root (the subfolders there are CMake build dirs)
$pluginsSrc = Join-Path $buildPath "plugins"
$pluginsDst = Join-Path $stage "plugins"
New-Item -ItemType Directory -Force -Path $pluginsDst | Out-Null
# Only DLLs with a "<stem>.plugin.json" manifest are shipped. A DLL without one is a stale
# output of a removed or deprecated target (e.g. ScreenToolsPlugin2.dll) and is skipped.
$pluginDlls = @()
foreach ($dll in Get-ChildItem -LiteralPath $pluginsSrc -File -Filter "*.dll") {
	$manifest = Join-Path $pluginsSrc ($dll.BaseName + ".plugin.json")
	if (-not (Test-Path -LiteralPath $manifest)) {
		Write-Host "[package] skip plugin without manifest (stale build output?): $($dll.Name)" -ForegroundColor DarkYellow
		continue
	}
	Copy-Item -LiteralPath $dll.FullName -Destination $pluginsDst -Force
	Copy-Item -LiteralPath $manifest -Destination $pluginsDst -Force
	$pluginDlls += $dll
}
if ($pluginDlls.Count -eq 0) { Fail "no plugin DLLs with manifests found in $pluginsSrc" }

# Plugin runtime resources that live under <exe>\plugins\
Copy-Tree (Join-Path $pluginsSrc "CTIcons") (Join-Path $pluginsDst "CTIcons") @("*.ico")      # CherryTree node icons
Copy-Tree (Join-Path $pluginsSrc "icon")    (Join-Path $pluginsDst "icon")    @("*.ico")      # AirPods list icons
Copy-Required (Join-Path $pluginsSrc "NodeJsPlugin\node-host.js") (Join-Path $pluginsDst "NodeJsPlugin")
New-Item -ItemType Directory -Force -Path (Join-Path $pluginsDst "NodeJsPlugin\scripts") | Out-Null

# Folder plugin default config. The Release build does not copy it (plugins/Folder/CMakeLists.txt),
# so take the checked-in default from the source tree.
Copy-Required (Join-Path $repoRoot "plugins\Folder\config_folder_plugin.json") $pluginsDst

# Docs and licenses
Copy-Required (Join-Path $repoRoot "LICENSE") $stage
Copy-Required (Join-Path $repoRoot "README.md") $stage
Copy-Optional (Join-Path $repoRoot "README_ZH.md") $stage

$licDir = Join-Path $stage "licenses"
New-Item -ItemType Directory -Force -Path $licDir | Out-Null
$thirdParty = Join-Path $repoRoot "3rdparty"
foreach ($lib in Get-ChildItem -LiteralPath $thirdParty -Directory) {
	$lic = Get-ChildItem -LiteralPath $lib.FullName -File | Where-Object { $_.Name -match '^(LICENSE|LICENCE|COPYING)(\..*)?$' } | Select-Object -First 1
	if ($lic) { Copy-Item -LiteralPath $lic.FullName -Destination (Join-Path $licDir ($lib.Name + "-" + $lic.Name)) -Force }
}

# ---------------------------------------------------------------- sanity checks
# A Release package must not import the debug CRT.
$debugCrt = @("MSVCP140D.dll", "VCRUNTIME140D.dll", "ucrtbased.dll")
foreach ($bin in Get-ChildItem -LiteralPath $stage -Recurse -File -Include "*.exe", "*.dll") {
	$bytes = [IO.File]::ReadAllBytes($bin.FullName)
	$text = [Text.Encoding]::ASCII.GetString($bytes)
	foreach ($d in $debugCrt) {
		if ($text.IndexOf($d, [StringComparison]::OrdinalIgnoreCase) -ge 0) { Fail "$($bin.Name) imports debug CRT '$d'. This is not a Release binary." }
	}
}

# Nothing user-specific or build-specific may slip through.
$forbidden = @("*.pdb", "*.lib", "*.exp", "*.obj", "*.ilk", "*.log", "user_settings.json", "usage_history.json", "config_websearch_plugin.json", "crash_dump.dmp")
foreach ($p in $forbidden) {
	$hit = Get-ChildItem -LiteralPath $stage -Recurse -File -Filter $p | Select-Object -First 1
	if ($hit) { Fail "forbidden file in package: $($hit.FullName)" }
}

# ---------------------------------------------------------------- zip
$zipName = "$appName-v$version-$arch.zip"
$zipPath = Join-Path $outPath $zipName
if (Test-Path -LiteralPath $zipPath) { Remove-Item -LiteralPath $zipPath -Force }
Add-Type -AssemblyName System.IO.Compression.FileSystem
# includeBaseDirectory = $true -> the zip contains a single top-level "CandyLauncher" folder
[IO.Compression.ZipFile]::CreateFromDirectory($stage, $zipPath, [IO.Compression.CompressionLevel]::Optimal, $true)

$files = Get-ChildItem -LiteralPath $stage -Recurse -File
$stageBytes = ($files | Measure-Object -Property Length -Sum).Sum
$zipBytes = (Get-Item -LiteralPath $zipPath).Length
$sha = [Security.Cryptography.SHA256]::Create()
$zipStream = [IO.File]::OpenRead($zipPath)
try { $hashBytes = $sha.ComputeHash($zipStream) } finally { $zipStream.Close(); $sha.Dispose() }
$sha256 = ([BitConverter]::ToString($hashBytes) -replace '-', '').ToLower()

Write-Host ""
Write-Host "[package] staged : $stage ($($files.Count) files, $([math]::Round($stageBytes / 1MB, 1)) MB)"
Write-Host "[package] zip    : $zipPath ($([math]::Round($zipBytes / 1MB, 1)) MB)"
Write-Host "[package] sha256 : $sha256"
Write-Host "[package] plugins: $($pluginDlls.Count)"
Write-Host "[package] skins  : $($skinNames.Count) ($SkinList)"
Write-Host ""
Write-Host "Next: create GitHub release with tag v$version and upload $zipName (upload through GitHub so the asset digest exists for the updater)."
