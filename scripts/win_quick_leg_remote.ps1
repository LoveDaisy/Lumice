# File/lock/process control for the native Windows leg (PowerShell 5.1).
# ASCII source intentionally avoids PowerShell 5.1's BOM-dependent decoding.
param([string]$RequestFile, [string]$TestBuild, [string]$Labels)
$ErrorActionPreference = 'Stop'

if ($TestBuild) {
    try {
        $selection = & ctest.exe --test-dir $TestBuild -L $Labels --show-only=json-v1
        if ($LASTEXITCODE -ne 0) { throw 'ctest enumeration failed' }
        $tests = @((($selection -join "`n") | ConvertFrom-Json).tests)
        # Same nonempty-selection invariant as test.sh guarded_ctest_layer.
        if ($tests.Count -eq 0) {
            Write-Host 'WIN_LEG_RESULT: FAIL stage=ctest reason=no-tests'
            exit 1
        }
        Write-Host "win-leg: selected-tests=$($tests.Count)"
        $executables = @($tests | ForEach-Object { $_.command } | Where-Object { $_ -match '\.exe$' } | Sort-Object -Unique)
        foreach ($exe in $executables) {
            & cmd.exe /d /c "`"$exe`" --help >nul 2>&1"
            if ($LASTEXITCODE -eq 4551) {
                $beforeHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $exe).Hash
                Write-Host "win-leg: SAC blocked $exe hash=$beforeHash; relinking once"
                Remove-Item -LiteralPath $exe
                & cmake.exe --build $TestBuild
                if ($LASTEXITCODE -ne 0) {
                    Write-Host 'WIN_LEG_RESULT: FAIL stage=build reason=sac-relink'
                    exit 1
                }
                $afterHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $exe).Hash
                Write-Host "win-leg: SAC relink hash=$afterHash"
                & cmd.exe /d /c "`"$exe`" --help >nul 2>&1"
            }
            if ($LASTEXITCODE -ne 0) {
                Write-Host "WIN_LEG_RESULT: FAIL stage=sac reason=launch-$LASTEXITCODE"
                exit 1
            }
        }
        & ctest.exe --test-dir $TestBuild -L $Labels --output-on-failure
        if ($LASTEXITCODE -ne 0) {
            Write-Host 'WIN_LEG_RESULT: FAIL stage=ctest reason=test-failed'
            exit 1
        }
        Write-Host 'WIN_LEG_RESULT: PASS'
        exit 0
    } catch {
        Write-Host $_.Exception.Message
        Write-Host 'WIN_LEG_RESULT: FAIL stage=ctest reason=exception'
        exit 1
    }
}

$runDir = Split-Path -Parent $RequestFile
$request = Get-Content -Raw $RequestFile | ConvertFrom-Json
$ws = $request.workspace.Replace('/', '\')
$lockRoot = 'C:\lumice-test\locks'
$lock = Join-Path $lockRoot 'home-box.lock'
$holder = Join-Path $lock 'holder.json'
$token = Split-Path -Leaf $runDir
$owned = $false
$lease = $null
$release = $true
$result = 'FAIL stage=sync reason=exception'
$stage = 'sync'
$clock = [Diagnostics.Stopwatch]::StartNew()

function Native-Mkdir([string]$Path) {
    & cmd.exe /d /c "mkdir `"$Path`"" 2>$null
    return ($LASTEXITCODE -eq 0)
}

try {
    [IO.Directory]::CreateDirectory($lockRoot) | Out-Null
    while (-not $owned) {
        # Serialize acquisition, metadata initialization and stale reclamation.
        # The guard is an OS handle, not another stale-able directory lock.
        $guard = $null
        try {
            $guard = [IO.File]::Open((Join-Path $lockRoot 'home-box.guard'), 'OpenOrCreate', 'ReadWrite', 'None')
            if (Test-Path $lock) {
                $born = (Get-Item $lock).CreationTimeUtc
                if (Test-Path $holder) {
                    $old = Get-Content -Raw $holder | ConvertFrom-Json
                    $born = [DateTime]::Parse($old.started).ToUniversalTime()
                }
                # A controller lost during a build cannot prove its descendants
                # stopped. Keep that slot for explicit recovery, regardless of age.
                $unconfirmedBuild = Test-Path (Join-Path $lock 'build-active')
                if (-not $unconfirmedBuild -and ([DateTime]::UtcNow - $born).TotalMinutes -gt 90) {
                    # A live owner holds this handle throughout sync/build/test;
                    # age alone must never evict a live run.
                    $probe = [IO.File]::Open((Join-Path $lock 'lease'), 'OpenOrCreate', 'ReadWrite', 'None')
                    $probe.Dispose()
                    Remove-Item -Recurse -Force $lock
                    Write-Host 'win-leg: stale-lock-broken'
                }
            }
            if (-not (Test-Path $lock)) {
                if (Native-Mkdir $lock) {
                    $lease = [IO.File]::Open((Join-Path $lock 'lease'), 'OpenOrCreate', 'ReadWrite', 'None')
                    @{token=$token; host=$env:COMPUTERNAME; pid=$PID; started=[DateTime]::UtcNow.ToString('o'); scope=$ws} |
                        ConvertTo-Json | Set-Content -Encoding ASCII $holder
                    $owned = $true
                }
            }
        } catch [IO.IOException] {
            # Another owner/claimant is active; bounded queueing below.
        } finally {
            if ($null -ne $guard) { $guard.Dispose() }
        }
        if (-not $owned) {
            if ($clock.Elapsed.TotalSeconds -ge $request.wait) {
                $result = 'SKIPPED busy'
                break
            }
            Write-Host 'win-leg: waiting for home-box.lock'
            Start-Sleep -Seconds ([Math]::Min(30, [Math]::Max(1, $request.wait - [int]$clock.Elapsed.TotalSeconds)))
        }
    }
    if ($owned) {
        $sentinel = Join-Path $ws '.lumice-winleg-owner'
        if (Test-Path $ws) {
            if (-not (Test-Path $sentinel)) {
                throw 'Refusing an existing workspace without the dedicated ownership sentinel'
            }
        } else {
            [IO.Directory]::CreateDirectory($ws) | Out-Null
            Set-Content -Encoding ASCII $sentinel 'lumice-win-quick-leg-v1'
        }
        if ((Get-Content -Raw $sentinel).Trim() -ne 'lumice-win-quick-leg-v1' -or (Test-Path (Join-Path $ws '.git'))) {
            throw 'Workspace ownership mismatch'
        }
        $stampFile = Join-Path $ws '.lumice-winleg-sync-rev'
        $stamp = ''
        if (Test-Path $stampFile) { $stamp = (Get-Content -Raw $stampFile).Trim() }
        $mode = $request.mode
        # Packaging observed a stamp before acquiring the lock. Another run may
        # have advanced it; full recovery is safe, applying its old diff is not.
        if ($stamp -ne $request.stamp) { $mode = 'full' }
        Write-Host "win-leg: sync=$mode commit=$($request.rev)"
        if ($mode -eq 'full') {
            Get-ChildItem -Force $ws | Where-Object {
                $_.Name -notin @('build', '.lumice-winleg-owner', '.lumice-winleg-sync-rev')
            } | Remove-Item -Recurse -Force
        } else {
            foreach ($name in $request.deletes) {
                $path = Join-Path $ws $name
                if (Test-Path $path) {
                    & cmd.exe /d /c "del /f /q `"$path`""
                    if ($LASTEXITCODE -ne 0) { throw "Could not delete $name" }
                }
            }
        }
        $archive = Join-Path $runDir 'source.tar'
        $tar = Join-Path $env:SystemRoot 'System32\tar.exe'
        if ($mode -eq 'full') {
            & $tar -xf $archive -C $ws
            if ($LASTEXITCODE -ne 0) { throw 'Full archive extraction failed' }
            $changed = @($request.manifest.PSObject.Properties.Name)
        } else {
            $changed = @($request.changes)
            if ($changed.Count -gt 0) {
                $list = Join-Path $runDir 'changed.txt'
                [IO.File]::WriteAllLines($list, [string[]]$changed, (New-Object Text.UTF8Encoding $false))
                & $tar -xf $archive -C $ws -T $list
                if ($LASTEXITCODE -ne 0) { throw 'Diff archive extraction failed' }
            }
        }
        foreach ($name in $changed) { (Get-Item -LiteralPath (Join-Path $ws $name)).LastWriteTimeUtc = [DateTime]::UtcNow }
        # Enumerate outside build/ rather than recursively scanning its cached
        # dependency sources. Check contents too: a stamp alone is not evidence.
        $actual = @{}
        foreach ($entry in Get-ChildItem -Force $ws) {
            if ($entry.Name -in @('build', '.lumice-winleg-owner', '.lumice-winleg-sync-rev')) { continue }
            $files = @($entry)
            if ($entry.PSIsContainer) { $files = @(Get-ChildItem -File -Force -Recurse $entry.FullName) }
            foreach ($file in $files) {
                $name = $file.FullName.Substring($ws.Length + 1).Replace('\', '/')
                $actual[$name] = (Get-FileHash -Algorithm SHA256 -LiteralPath $file.FullName).Hash.ToLowerInvariant()
            }
        }
        $expected = @($request.manifest.PSObject.Properties)
        $mismatch = $actual.Count -ne $expected.Count
        foreach ($item in $expected) {
            if ($actual[$item.Name] -ne $item.Value) { Write-Host "win-leg: tree mismatch: $($item.Name)"; $mismatch = $true }
        }
        if ($mismatch) {
            $result = 'FAIL stage=sync reason=tree-mismatch'
        } else {
            Write-Host "win-leg: tree-check=PASS files=$($actual.Count)"
            Set-Content -Encoding ASCII $stampFile $request.rev
            Remove-Item -Force $archive
            $stage = 'build'
            $outLog = Join-Path $runDir 'run.log'
            $errLog = Join-Path $runDir 'run.err'
            $cmd = Join-Path $runDir 'win_quick_leg_remote.cmd'
            $args = "/d /c `"`"$cmd`" `"$ws`" `"$($request.labels)`"`""
            Set-Content -Encoding ASCII (Join-Path $lock 'build-active') $token
            $release = $false
            $proc = Start-Process -FilePath $env:ComSpec -ArgumentList $args -PassThru -NoNewWindow `
                -RedirectStandardOutput $outLog -RedirectStandardError $errLog
            # PowerShell 5.1 otherwise loses ExitCode when WaitForExit reopens an
            # already-exited process. Retain the handle before waiting.
            $null = $proc.Handle
            if (-not $proc.WaitForExit([int]($request.timeout * 1000))) {
                & "$env:SystemRoot\System32\taskkill.exe" /PID $proc.Id /T /F
                $release = $LASTEXITCODE -eq 0
                $result = 'FAIL stage=timeout reason=deadline'
            } else {
                $proc.WaitForExit()
                $release = $true
                $lines = @(Get-Content $outLog | Where-Object { $_ -match '^WIN_LEG_RESULT: ' })
                if ($lines.Count -eq 1 -and $proc.ExitCode -eq 0 -and $lines[0] -match '^WIN_LEG_RESULT: PASS$') {
                    $result = 'PASS'
                } elseif ($lines.Count -eq 1 -and $lines[0] -match '^WIN_LEG_RESULT: FAIL ') {
                    $result = $lines[0].Substring('WIN_LEG_RESULT: '.Length)
                } else { $result = 'FAIL stage=build reason=missing-result' }
            }
        }
    }
} catch {
    Write-Host "win-leg: $($_.Exception.Message)"
    $result = "FAIL stage=$stage reason=exception"
} finally {
    if ($owned -and $release) {
        $guard = $null
        try {
            # Retain the lease until release is serialized with acquisition.
            for ($attempt = 0; $attempt -lt 100 -and $null -eq $guard; $attempt++) {
                try { $guard = [IO.File]::Open((Join-Path $lockRoot 'home-box.guard'), 'OpenOrCreate', 'ReadWrite', 'None') }
                catch [IO.IOException] { Start-Sleep -Milliseconds 50 }
            }
            if ($null -eq $guard) { throw 'Could not acquire release guard' }
            if ((Get-Content -Raw $holder | ConvertFrom-Json).token -eq $token) {
                $lease.Dispose()
                $lease = $null
                Remove-Item -Force -Recurse $lock
            }
        } catch {
            Write-Host "win-leg: lock release: $($_.Exception.Message)"
            $result = 'FAIL stage=sync reason=lock-release'
        } finally {
            if ($null -ne $guard) { $guard.Dispose() }
        }
    }
    if ($null -ne $lease) { $lease.Dispose() }
    # Archives are transient even on sync/queue failure. Completed diagnostic
    # runs are retained for seven days; never prune an unconfirmed process tree.
    try {
        $archive = Join-Path $runDir 'source.tar'
        if (Test-Path $archive) { Remove-Item -Force $archive }
        if ($release) { Set-Content -Encoding ASCII (Join-Path $runDir '.completed') $token }
        foreach ($oldRun in Get-ChildItem -Directory (Split-Path -Parent $runDir)) {
            if ($oldRun.Name -notmatch '^[0-9a-f]{12}-tmp\.[A-Za-z0-9]+$') { continue }
            $completed = Join-Path $oldRun.FullName '.completed'
            if ((Test-Path $completed) -and (Get-Item $completed).LastWriteTimeUtc -lt [DateTime]::UtcNow.AddDays(-7)) {
                Remove-Item -Recurse -Force $oldRun.FullName
            }
        }
    } catch { Write-Host "win-leg: staging cleanup: $($_.Exception.Message)" }
}
Write-Host "WIN_LEG_RESULT: $result"
exit 0
