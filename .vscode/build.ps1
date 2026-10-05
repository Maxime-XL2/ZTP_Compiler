param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug'
)

$ErrorActionPreference = 'Stop'

$workspaceRoot = Split-Path $PSScriptRoot -Parent
$compiler = 'C:\Program Files\CodeBlocks\MinGW\bin\g++.exe'
$projectPath = Join-Path $workspaceRoot 'ZTP_Compiler.cbp'
$outputPath = Join-Path $workspaceRoot "bin\$Configuration\ZTP_Compiler.exe"
$objectRoot = Join-Path $workspaceRoot "obj\$Configuration"
$statePath = Join-Path $objectRoot '.build-state.json'

if (-not (Test-Path $compiler)) {
    throw "MinGW compiler not found: $compiler"
}

Push-Location $workspaceRoot
try {
    [xml]$project = Get-Content $projectPath
    $sources = @(
        $project.SelectNodes('/CodeBlocks_project_file/Project/Unit') |
            Where-Object { $_.filename -like '*.cpp' } |
            ForEach-Object { $_.filename }
    )
    if ($sources.Count -eq 0) {
        throw "No C++ source files found in $projectPath"
    }

    New-Item -ItemType Directory -Force -Path (Split-Path $outputPath), $objectRoot | Out-Null

    $configurationFlags = if ($Configuration -eq 'Release') { @('-O2') } else { @('-g', '-O0') }
    $compileFlags = @('-std=gnu++14') + $configurationFlags + @('-Wall')
    $linkFlags = @('-mwindows', '-static-libstdc++', '-static-libgcc', '-static',
                   '-lcomdlg32', '-lcomctl32', '-lopengl32', '-lglu32', '-lgdi32')
    if ($Configuration -eq 'Release') { $linkFlags += '-s' }
    $compilerVersion = (& $compiler --version | Select-Object -First 1)
    $compileSignature = (@($compiler, $compilerVersion) + $compileFlags + $sources) -join "`n"
    $projectTimestamp = (Get-Item $projectPath).LastWriteTimeUtc.ToString('o')
    $linkSignature = (@($projectPath, $projectTimestamp) + $linkFlags) -join "`n"
    $previousState = $null
    if (Test-Path $statePath) {
        try { $previousState = Get-Content $statePath -Raw | ConvertFrom-Json }
        catch { $previousState = $null }
    }
    $forceCompile = $null -eq $previousState -or $previousState.compileSignature -ne $compileSignature
    $forceLink = $forceCompile -or $null -eq $previousState -or $previousState.linkSignature -ne $linkSignature
    $rebuiltObjects = $false
    $objectFiles = @()

    foreach ($source in $sources) {
        $objectName = ($source -replace '[\\/]', '_') -replace '\.cpp$', '.o'
        $objectPath = Join-Path $objectRoot $objectName
        $dependencyPath = [System.IO.Path]::ChangeExtension($objectPath, '.d')
        $needsCompile = $forceCompile -or !(Test-Path $objectPath) -or !(Test-Path $dependencyPath)

        if (-not $needsCompile) {
            $dependencyText = [System.IO.File]::ReadAllText($dependencyPath) -replace '\\\r?\n\s*', ' '
            $separator = $dependencyText.IndexOf(':')
            if ($separator -lt 0) {
                $needsCompile = $true
            }
            else {
                $dependencies = $dependencyText.Substring($separator + 1).Trim() -split '\s+'
                $objectTime = (Get-Item $objectPath).LastWriteTimeUtc
                foreach ($dependency in $dependencies) {
                    if (!(Test-Path $dependency) -or (Get-Item $dependency).LastWriteTimeUtc -gt $objectTime) {
                        $needsCompile = $true
                        break
                    }
                }
            }
        }

        if ($needsCompile) {
            $relativeObject = "obj/$Configuration/$objectName"
            $arguments = $compileFlags + @('-MMD', '-MF', $dependencyPath, '-MT', $relativeObject,
                                           '-c', $source, '-o', $objectPath)
            & $compiler @arguments
            if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
            $rebuiltObjects = $true
            Write-Host "Compiled $source"
        }
        $objectFiles += $objectPath
    }

    if (Test-Path $outputPath) {
        $outputTime = (Get-Item $outputPath).LastWriteTimeUtc
        if ($objectFiles | Where-Object { (Get-Item $_).LastWriteTimeUtc -gt $outputTime }) {
            $forceLink = $true
        }
    }
    else { $forceLink = $true }

    if ($rebuiltObjects) { $forceLink = $true }
    if ($forceLink) {
        & $compiler @objectFiles @linkFlags -o $outputPath
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        Write-Host "Linked $outputPath"
    }
    else {
        Write-Host 'Build is up to date.'
    }

    [pscustomobject]@{
        compileSignature = $compileSignature
        linkSignature = $linkSignature
    } | ConvertTo-Json | Set-Content $statePath
}
finally {
    Pop-Location
}
