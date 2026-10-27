#requires -Version 5.0
[CmdletBinding()]
param(
    [Parameter(Mandatory = $false)]
    [string]$Owner
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

$script:ProjectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..')).TrimEnd('\', '/')
$script:RepoName = 'quantlab-cpp'
$script:Utf8NoBom = [System.Text.UTF8Encoding]::new($false)

function Invoke-NativeResult {
    param(
        [Parameter(Mandatory = $true)][string]$Executable,
        [Parameter(Mandatory = $true)][string[]]$Arguments
    )
    # Native stderr must be captured without PowerShell 5 treating it as a
    # terminating ErrorRecord. Credentials are never requested or printed here.
    $ErrorActionPreference = 'Continue'
    $lines = @(& $Executable @Arguments 2>&1)
    $code = $LASTEXITCODE
    $text = ($lines | ForEach-Object { $_.ToString() }) -join [Environment]::NewLine
    return [pscustomobject]@{ ExitCode = $code; Text = $text }
}

function Invoke-GitResult {
    param([Parameter(Mandatory = $true)][string[]]$Arguments)
    return Invoke-NativeResult -Executable $script:GitPath -Arguments (@('-C', $script:ProjectRoot) + $Arguments)
}

function Invoke-Git {
    param([Parameter(Mandatory = $true)][string[]]$Arguments)
    $result = Invoke-GitResult -Arguments $Arguments
    if ($result.ExitCode -ne 0) {
        throw ('Git command failed: git {0}{1}{2}' -f ($Arguments -join ' '), [Environment]::NewLine, $result.Text)
    }
    return $result.Text.Trim()
}

function Invoke-GhResult {
    param([Parameter(Mandatory = $true)][string[]]$Arguments)
    return Invoke-NativeResult -Executable $script:GhPath -Arguments $Arguments
}

function Invoke-Gh {
    param([Parameter(Mandatory = $true)][string[]]$Arguments)
    $result = Invoke-GhResult -Arguments $Arguments
    if ($result.ExitCode -ne 0) {
        throw ('GitHub CLI command failed: gh {0}{1}{2}' -f ($Arguments -join ' '), [Environment]::NewLine, $result.Text)
    }
    return $result.Text.Trim()
}

function Get-RepositoryIdentity {
    param([Parameter(Mandatory = $true)][string]$RemoteUrl)
    $url = $RemoteUrl.Trim().TrimEnd('/') -replace '\.git$', ''
    if ($url -match '^(?:https://github\.com/|git@github\.com:|ssh://git@github\.com/)([A-Za-z0-9-]+)/([A-Za-z0-9_.-]+)$') {
        return ('{0}/{1}' -f $Matches[1], $Matches[2])
    }
    return ''
}

function Test-AllowedPath {
    param([Parameter(Mandatory = $true)][string]$Path)
    return ($Path -match '^(include|src|tests|benchmarks|docs|scripts|\.github)/' -or
        $Path -in @('.editorconfig', '.gitattributes', '.gitignore', 'CMakeLists.txt',
                    'README.md', 'README.zh-CN.md', 'AGENTS.md', 'CLAUDE.md', 'LICENSE'))
}

function Assert-AllowedPaths {
    param([string]$Paths)
    foreach ($path in @($Paths -split '\r?\n')) {
        if ($path -and -not (Test-AllowedPath -Path $path)) {
            throw "Refusing to publish an out-of-scope tracked or staged path: $path"
        }
    }
}

function Enable-WorkflowPages {
    param(
        [Parameter(Mandatory = $true)][string]$FullName,
        [switch]$AllowEmptyRepository
    )
    $probe = Invoke-GhResult -Arguments @('api', '--hostname', 'github.com', "repos/$FullName/pages")
    if ($probe.ExitCode -eq 0) {
        $method = 'PUT'
    } elseif ($probe.Text -match '\b404\b') {
        $method = 'POST'
    } else {
        throw "Could not inspect GitHub Pages settings: $($probe.Text)"
    }
    $configured = Invoke-GhResult -Arguments @('api', '--hostname', 'github.com',
        '--method', $method, "repos/$FullName/pages", '-f', 'build_type=workflow')
    if ($configured.ExitCode -eq 0) { return $true }
    if ($AllowEmptyRepository -and $method -eq 'POST' -and
        $configured.Text -match '\b(409|422)\b' -and $configured.Text -match '(?i)empty') {
        Write-Host 'GitHub requires a first commit before enabling Pages; setup will retry immediately after push.'
        return $false
    }
    throw "Could not enable GitHub Pages: $($configured.Text)"
}

Write-Host 'Checking the QuantLab project and GitHub login...'
$requiredPaths = @('CMakeLists.txt', 'README.md', 'README.zh-CN.md', 'AGENTS.md',
    'CLAUDE.md', 'include/quantlab/analytics.hpp', 'src/analytics.cpp',
    'tests/analytics_tests.cpp', 'benchmarks/analytics_benchmark.cpp',
    'docs/mainpage.md', 'docs/Doxyfile.in', '.github/workflows/ci.yml', '.gitignore')
foreach ($relativePath in $requiredPaths) {
    if (-not (Test-Path -LiteralPath (Join-Path $script:ProjectRoot $relativePath) -PathType Leaf)) {
        throw "Required project file is missing: $relativePath"
    }
}

$gitCommand = Get-Command git -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
if ($null -eq $gitCommand) {
    throw 'Git was not found on PATH. Install Git and run this script again.'
}
$script:GitPath = $gitCommand.Source
$ghCommand = Get-Command gh -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
if ($null -ne $ghCommand) {
    $script:GhPath = $ghCommand.Source
} else {
    $script:GhPath = Join-Path $script:ProjectRoot 'local-tools/gh/bin/gh.exe'
    if (-not (Test-Path -LiteralPath $script:GhPath -PathType Leaf)) {
        throw 'GitHub CLI was not found. Install gh, add it to PATH, then run this script again.'
    }
}

# The optional portable configuration stays in the ignored local-tools folder.
$portableGhConfig = Join-Path $script:ProjectRoot 'local-tools/gh-config'
if ([string]::IsNullOrWhiteSpace($env:GH_CONFIG_DIR) -and
    (Test-Path -LiteralPath $portableGhConfig -PathType Container)) {
    $env:GH_CONFIG_DIR = $portableGhConfig
}

# Discard all auth-status output. Do not call gh auth token or inspect credential files.
$oldPreference = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
& $script:GhPath auth status --hostname github.com *> $null
$authExitCode = $LASTEXITCODE
$ErrorActionPreference = $oldPreference
if ($authExitCode -ne 0) {
    $quotedGhPath = $script:GhPath.Replace("'", "''")
    Write-Host ("& '{0}' auth login --hostname github.com --git-protocol https --web --scopes workflow" -f $quotedGhPath)
    throw 'Sign in using the command above, then run this script again.'
}

$user = (Invoke-Gh -Arguments @('api', '--hostname', 'github.com', 'user')) | ConvertFrom-Json
$login = [string]$user.login
if ([string]::IsNullOrWhiteSpace($login) -or $login -notmatch '^[A-Za-z0-9-]+$' -or
    [string]::IsNullOrWhiteSpace([string]$user.id)) {
    throw 'GitHub did not return a valid authenticated user identity.'
}
if (-not [string]::IsNullOrWhiteSpace($Owner) -and
    -not [string]::Equals($Owner, $login, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Owner must match the authenticated personal account ($login). No repository was created."
}
$Owner = $login
$fullName = '{0}/{1}' -f $Owner, $script:RepoName
$hostedRepository = 'github.com/{0}' -f $fullName
$repositoryUrl = 'https://github.com/{0}' -f $fullName
$documentationUrl = 'https://{0}.github.io/{1}/' -f $Owner, $script:RepoName

$hasLocalGit = Test-Path -LiteralPath (Join-Path $script:ProjectRoot '.git')
$originMatches = $false
$hasOrigin = $false
$rootProbe = Invoke-GitResult -Arguments @('rev-parse', '--show-toplevel')
if ($hasLocalGit) {
    if ($rootProbe.ExitCode -ne 0) {
        throw 'The local .git entry exists, but Git cannot read this repository.'
    }
    $actualRoot = [System.IO.Path]::GetFullPath($rootProbe.Text.Trim()).TrimEnd('\', '/')
    if (-not [string]::Equals($actualRoot, $script:ProjectRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw 'The Git repository root is not the exact QuantLab project root.'
    }
    $branch = Invoke-Git -Arguments @('symbolic-ref', '--quiet', '--short', 'HEAD')
    if ($branch -ne 'main') {
        throw 'The local branch must be main. This script does not rename branches or move commits.'
    }
    $originProbe = Invoke-GitResult -Arguments @('remote', 'get-url', '--all', 'origin')
    if ($originProbe.ExitCode -eq 0) {
        $hasOrigin = $true
        $originUrls = @($originProbe.Text.Trim() -split '\r?\n')
        $pushUrls = @((Invoke-Git -Arguments @('remote', 'get-url', '--push', '--all', 'origin')) -split '\r?\n')
        $originMatches = $originUrls.Count -eq 1 -and $pushUrls.Count -eq 1
        if ($originMatches) {
            $originIdentity = Get-RepositoryIdentity -RemoteUrl $originUrls[0]
            $pushIdentity = Get-RepositoryIdentity -RemoteUrl $pushUrls[0]
            $originMatches = ([string]::Equals($originIdentity, $fullName, [System.StringComparison]::OrdinalIgnoreCase) -and
                [string]::Equals($pushIdentity, $fullName, [System.StringComparison]::OrdinalIgnoreCase))
        }
        if (-not $originMatches) {
            throw 'Existing origin fetch/push URLs do not match the target repository. They were not changed.'
        }
    }
    Assert-AllowedPaths -Paths (Invoke-Git -Arguments @('diff', '--cached', '--name-only'))
    Assert-AllowedPaths -Paths (Invoke-Git -Arguments @('ls-files'))
} elseif ($rootProbe.ExitCode -eq 0) {
    throw 'This directory is inside another Git repository. Refusing to initialize a nested publication.'
}

$repositoryProbe = Invoke-GhResult -Arguments @('api', '--hostname', 'github.com', "repos/$fullName")
$repositoryExists = $repositoryProbe.ExitCode -eq 0
if ($repositoryExists) {
    $repository = $repositoryProbe.Text | ConvertFrom-Json
    if ($repository.private) {
        throw 'The existing target repository is private. Its visibility was not changed.'
    }
    $commitsProbe = Invoke-GhResult -Arguments @('api', '--hostname', 'github.com', "repos/$fullName/commits?per_page=1")
    if ($commitsProbe.ExitCode -eq 0) {
        $hasRemoteCommits = $commitsProbe.Text -notmatch '^\s*\[\s*\]\s*$'
    } elseif ($commitsProbe.Text -match '\b409\b' -and $commitsProbe.Text -match '(?i)empty') {
        $hasRemoteCommits = $false
    } else {
        throw "Could not safely determine whether the target repository is empty: $($commitsProbe.Text)"
    }
    if ($hasRemoteCommits -and -not ($hasLocalGit -and $originMatches)) {
        throw 'The existing remote has commits and is not this local project origin. Refusing to publish into it.'
    }
} elseif ($repositoryProbe.Text -notmatch '\b404\b') {
    throw "Could not inspect the target repository: $($repositoryProbe.Text)"
}

Write-Host "Preparing $fullName..."
foreach ($relativePath in @('README.md', 'README.zh-CN.md', 'docs/mainpage.md')) {
    $filePath = Join-Path $script:ProjectRoot $relativePath
    $contents = [System.IO.File]::ReadAllText($filePath)
    $contents = $contents.Replace('REPO_OWNER', $Owner)
    if (-not $contents.Contains($repositoryUrl) -or -not $contents.Contains($documentationUrl)) {
        throw "Repository and documentation links do not match this account in $relativePath."
    }
    [System.IO.File]::WriteAllText($filePath, $contents, $script:Utf8NoBom)
}

if (-not $hasLocalGit) {
    $null = Invoke-Git -Arguments @('init', '-b', 'main')
}
$ignoreProbe = Invoke-GitResult -Arguments @('check-ignore', '--quiet', '--no-index', 'output/publication.json')
if ($ignoreProbe.ExitCode -ne 0) {
    throw 'output/publication.json must be ignored by Git before publishing.'
}
$null = Invoke-Git -Arguments @('config', '--local', 'user.name', $login)
$noreplyEmail = '{0}+{1}@users.noreply.github.com' -f $user.id, $login
$null = Invoke-Git -Arguments @('config', '--local', 'user.email', $noreplyEmail)

$allowedPaths = @('.editorconfig', '.gitattributes', '.gitignore', 'CMakeLists.txt',
    'README.md', 'README.zh-CN.md', 'AGENTS.md', 'CLAUDE.md', 'LICENSE',
    'include', 'src', 'tests', 'benchmarks', 'docs', '.github', 'scripts')
$stagePaths = @($allowedPaths | Where-Object { Test-Path -LiteralPath (Join-Path $script:ProjectRoot $_) })
$null = Invoke-Git -Arguments (@('add', '--') + $stagePaths)
Assert-AllowedPaths -Paths (Invoke-Git -Arguments @('diff', '--cached', '--name-only'))
Assert-AllowedPaths -Paths (Invoke-Git -Arguments @('ls-files'))
$diff = Invoke-GitResult -Arguments @('diff', '--cached', '--quiet')
if ($diff.ExitCode -eq 1) {
    $null = Invoke-Git -Arguments @('-c', 'commit.gpgsign=false', 'commit', '-m',
        'Set up QuantLab C++ project and automated quality checks')
} elseif ($diff.ExitCode -ne 0) {
    throw "Could not inspect staged changes: $($diff.Text)"
}
$commit = Invoke-Git -Arguments @('rev-parse', '--verify', 'HEAD')

if (-not $repositoryExists) {
    Write-Host "Creating the public repository $fullName..."
    $null = Invoke-Gh -Arguments @('repo', 'create', $hostedRepository, '--public', '--description',
        'C++17 time-series analytics with CMake, doctest, Google Benchmark, and Doxygen CI/CD')
}
if (-not $hasOrigin) {
    $null = Invoke-Git -Arguments @('remote', 'add', 'origin', "$repositoryUrl.git")
}

Write-Host 'Enabling GitHub Pages with GitHub Actions...'
$pagesReady = Enable-WorkflowPages -FullName $fullName -AllowEmptyRepository
$null = Invoke-Gh -Arguments @('repo', 'edit', $hostedRepository, '--homepage', $documentationUrl)

Write-Host 'Pushing main without force...'
# Configure gh's supported credential helper for this invocation only. Git and
# gh exchange credentials directly; this script never obtains the token.
$credentialHelper = 'credential.helper=!gh auth git-credential'
$oldExecutablePath = $env:PATH
$env:PATH = '{0}{1}{2}' -f (Split-Path -Parent $script:GhPath), [System.IO.Path]::PathSeparator, $env:PATH
$oldTerminalPrompt = $env:GIT_TERMINAL_PROMPT
$env:GIT_TERMINAL_PROMPT = '0'
try {
    $null = Invoke-Git -Arguments @('-c', 'credential.helper=', '-c', $credentialHelper,
        '-c', 'core.sshCommand=ssh -oBatchMode=yes', 'push', '--set-upstream', 'origin', 'main')
} finally {
    $env:PATH = $oldExecutablePath
    if ($null -eq $oldTerminalPrompt) {
        Remove-Item -LiteralPath Env:GIT_TERMINAL_PROMPT -ErrorAction SilentlyContinue
    } else {
        $env:GIT_TERMINAL_PROMPT = $oldTerminalPrompt
    }
}
$null = Invoke-Gh -Arguments @('repo', 'edit', $hostedRepository, '--default-branch', 'main')
if (-not $pagesReady) {
    $null = Enable-WorkflowPages -FullName $fullName
}

Write-Host "Waiting for the CI run for commit $commit..."
$run = $null
for ($attempt = 0; $attempt -lt 30; ++$attempt) {
    $runsProbe = Invoke-GhResult -Arguments @('run', 'list', '--repo', $hostedRepository,
        '--commit', $commit, '--workflow', 'ci.yml', '--event', 'push', '--limit', '10',
        '--json', 'databaseId,headSha,url,status,conclusion')
    if ($runsProbe.ExitCode -eq 0) {
        $runs = @($runsProbe.Text | ConvertFrom-Json)
        $run = $runs | Where-Object { $_.headSha -eq $commit } |
            Sort-Object -Property databaseId -Descending | Select-Object -First 1
        if ($null -ne $run) { break }
    }
    if ($attempt -lt 29) { Start-Sleep -Seconds 4 }
}
if ($null -eq $run) {
    throw "No push-triggered ci.yml run appeared for $commit within two minutes. Inspect $repositoryUrl/actions."
}
Write-Host ([string]$run.url)
$null = Invoke-Gh -Arguments @('run', 'watch', [string]$run.databaseId, '--repo', $hostedRepository,
    '--exit-status', '--interval', '10')

$verifiedRepository = (Invoke-Gh -Arguments @('api', '--hostname', 'github.com', "repos/$fullName")) | ConvertFrom-Json
if ($verifiedRepository.private) {
    throw 'The repository is not publicly accessible after publication.'
}

Write-Host 'Checking the published Doxygen site...'
[System.Net.ServicePointManager]::SecurityProtocol =
    [System.Net.ServicePointManager]::SecurityProtocol -bor [System.Net.SecurityProtocolType]::Tls12
$siteVerified = $false
for ($attempt = 0; $attempt -lt 12; ++$attempt) {
    try {
        $response = Invoke-WebRequest -Uri $documentationUrl -UseBasicParsing -TimeoutSec 20
        if ([int]$response.StatusCode -eq 200 -and [string]$response.Content -match 'QuantLab') {
            $siteVerified = $true
            break
        }
    } catch {
        # Pages may need a short propagation delay after a successful deployment.
    }
    if ($attempt -lt 11) { Start-Sleep -Seconds 5 }
}
if (-not $siteVerified) {
    throw "CI passed, but HTTP 200 with QuantLab content was not verified at $documentationUrl."
}

$evidence = [ordered]@{
    checked_at_utc = [DateTime]::UtcNow.ToString('o')
    commit = $commit
    repository_url = $repositoryUrl
    workflow_url = [string]$run.url
    documentation_url = $documentationUrl
    status = 'success'
}
$outputDirectory = Join-Path $script:ProjectRoot 'output'
$null = New-Item -ItemType Directory -Path $outputDirectory -Force
$evidencePath = Join-Path $outputDirectory 'publication.json'
[System.IO.File]::WriteAllText($evidencePath, ($evidence | ConvertTo-Json -Depth 4), $script:Utf8NoBom)
Write-Host 'Publication and remote checks succeeded.'
Write-Host "Repository: $repositoryUrl"
Write-Host "CI run: $($run.url)"
Write-Host "Documentation: $documentationUrl"
Write-Host "Evidence: $evidencePath"
