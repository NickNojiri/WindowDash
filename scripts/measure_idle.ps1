<#
.SYNOPSIS
  Measure WindowDash's own resource cost while idle.

.DESCRIPTION
  Samples one WindowDash process every -IntervalSec for -DurationSec (default
  10 minutes) and records CPU %, working set, private bytes, kernel handle
  count, GDI/USER object counts, and thread count.

  Writes three files to -OutDir, named <timestamp>_<label>:
    *.csv          one row per sample (raw data; commit it)
    *.meta.json    machine, exe hash, git SHA, parameters
    *.summary.json the headline numbers

  The headline CPU figure is (total CPU time used) / (wall time) over the whole
  run. Windows accounts process CPU time in ~15.6 ms ticks, so one 1-second
  sample is only good to about +/-1.6% of a core. Trust the whole-run number,
  not individual rows.

  Leak signal: handles / GDI / USER objects should be flat after warm-up.
  A steady climb over 10 minutes is a leak.

.PARAMETER ExePath
  Launch this exe (working directory = its folder) and measure it. When
  omitted, attach to the single running process named -ProcessName.

.PARAMETER Label
  Free text added to file names and metadata, for example "expanded" or
  "collapsed". Collapsing both panels removes PDH and analytics work from
  the 1 Hz timer tick (docs/ARCHITECTURE.md §2), so running once per state
  isolates their cost.

.EXAMPLE
  .\scripts\measure_idle.ps1 -ExePath .\build\WindowDash.exe -Label expanded -StopWhenDone

.EXAMPLE
  .\scripts\measure_idle.ps1 -Label collapsed -DurationSec 600
#>
[CmdletBinding()]
param(
    [string]$ExePath,
    [string]$ProcessName = "WindowDash",
    [int]$DurationSec = 600,
    [double]$IntervalSec = 1.0,
    [int]$WarmupSec = 10,
    [string]$Label = "idle",
    [string]$OutDir = "measurements",
    [switch]$StopWhenDone
)

$ErrorActionPreference = "Stop"
$onWindows = [Environment]::OSVersion.Platform -eq [PlatformID]::Win32NT

# --- GDI/USER object counts (GetGuiResources) -------------------------------
$guiAvailable = $false
if ($onWindows) {
    Add-Type -Namespace WinDash -Name Native -MemberDefinition @'
[DllImport("user32.dll")]
public static extern uint GetGuiResources(System.IntPtr hProcess, uint uiFlags);
'@
    $guiAvailable = $true
}
function Get-GuiCount([System.Diagnostics.Process]$p, [uint32]$flag) {
    # flag 0 = GR_GDIOBJECTS, 1 = GR_USEROBJECTS; -1 when unavailable
    if (-not $guiAvailable) { return -1 }
    return [int][WinDash.Native]::GetGuiResources($p.Handle, $flag)
}

function Get-Percentile([double[]]$values, [double]$pct) {
    # nearest-rank percentile
    if ($values.Count -eq 0) { return $null }
    $sorted = $values | Sort-Object
    $rank = [math]::Ceiling($pct / 100.0 * $sorted.Count)
    if ($rank -lt 1) { $rank = 1 }
    return [double]$sorted[$rank - 1]
}

function Try-Get([scriptblock]$block) {
    try { return & $block } catch { return $null }
}

# --- Find or launch the process ---------------------------------------------
$launched = $false
if ($ExePath) {
    $exe = (Resolve-Path $ExePath).Path
    $proc = Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) -PassThru
    $launched = $true
    Write-Host "Launched $exe (PID $($proc.Id))"
} else {
    $found = @(Get-Process -Name $ProcessName -ErrorAction SilentlyContinue)
    if ($found.Count -ne 1) {
        throw "Expected exactly 1 process named '$ProcessName', found $($found.Count). Use -ExePath or -ProcessName."
    }
    $proc = $found[0]
    $exe = Try-Get { $proc.MainModule.FileName }
    Write-Host "Attached to $ProcessName (PID $($proc.Id))"
}

if ($WarmupSec -gt 0) {
    Write-Host "Warm-up: ${WarmupSec}s (not recorded)"
    Start-Sleep -Seconds $WarmupSec
}

# --- Metadata ---------------------------------------------------------------
$logicalCpus = [Environment]::ProcessorCount
$gitSha = Try-Get { (git rev-parse HEAD 2>$null) }
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$safeLabel = $Label -replace '[^A-Za-z0-9_.-]', '_'
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$base = Join-Path $OutDir "${stamp}_${safeLabel}"

$meta = [ordered]@{
    label             = $Label
    started_local     = (Get-Date).ToString("o")
    duration_sec      = $DurationSec
    interval_sec      = $IntervalSec
    warmup_sec        = $WarmupSec
    pid               = $proc.Id
    exe_path          = $exe
    exe_sha256        = Try-Get { (Get-FileHash -Algorithm SHA256 $exe).Hash }
    git_sha           = $gitSha
    cpu_model         = Try-Get { (Get-CimInstance Win32_Processor | Select-Object -First 1).Name.Trim() }
    logical_cpus      = $logicalCpus
    os                = Try-Get { $os = Get-CimInstance Win32_OperatingSystem; "$($os.Caption) $($os.Version)" }
    ram_gb            = Try-Get { [math]::Round((Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory / 1GB, 1) }
    powershell        = $PSVersionTable.PSVersion.ToString()
    gui_counts_available = $guiAvailable
}
$meta | ConvertTo-Json | Set-Content -Encoding UTF8 "$base.meta.json"

# --- Sample loop ------------------------------------------------------------
$rows = New-Object System.Collections.Generic.List[object]
$clock = [System.Diagnostics.Stopwatch]::StartNew()
$proc.Refresh()
$cpuStart = $proc.TotalProcessorTime.TotalSeconds
$prevCpu = $cpuStart
$prevWall = 0.0
$exitedEarly = $false
$n = [int][math]::Floor($DurationSec / $IntervalSec)

Write-Host "Sampling every ${IntervalSec}s for ${DurationSec}s -> $base.csv"
for ($i = 1; $i -le $n; $i++) {
    # Sleep until the next scheduled tick so samples don't drift.
    $waitMs = [int](($i * $IntervalSec - $clock.Elapsed.TotalSeconds) * 1000)
    if ($waitMs -gt 0) { Start-Sleep -Milliseconds $waitMs }

    $proc.Refresh()
    if ($proc.HasExited) { $exitedEarly = $true; break }

    $wall = $clock.Elapsed.TotalSeconds
    $cpu = $proc.TotalProcessorTime.TotalSeconds
    $oneCore = 100.0 * ($cpu - $prevCpu) / ($wall - $prevWall)
    $rows.Add([pscustomobject]@{
        elapsed_s        = [math]::Round($wall, 3)
        cpu_s_total      = [math]::Round($cpu - $cpuStart, 4)
        cpu_pct_one_core = [math]::Round($oneCore, 3)
        cpu_pct_machine  = [math]::Round($oneCore / $logicalCpus, 4)
        working_set_mb   = [math]::Round($proc.WorkingSet64 / 1MB, 3)
        private_mb       = [math]::Round($proc.PrivateMemorySize64 / 1MB, 3)
        handles          = $proc.HandleCount
        gdi_objects      = Get-GuiCount $proc 0
        user_objects     = Get-GuiCount $proc 1
        threads          = $proc.Threads.Count
    })
    $prevCpu = $cpu
    $prevWall = $wall

    if ($i % 60 -eq 0) { Write-Host ("  {0,5:N0}s  ws={1:N1} MB  handles={2}" -f $wall, ($proc.WorkingSet64 / 1MB), $proc.HandleCount) }
}

$rows | Export-Csv -NoTypeInformation -Encoding UTF8 "$base.csv"

if ($rows.Count -lt 2) { throw "Only $($rows.Count) samples recorded (process exited?)." }

# --- Summary ----------------------------------------------------------------
$first = $rows[0]
$last = $rows[$rows.Count - 1]
$perSample = [double[]]($rows | ForEach-Object { $_.cpu_pct_one_core })
$ws = [double[]]($rows | ForEach-Object { $_.working_set_mb })
$handles = [double[]]($rows | ForEach-Object { $_.handles })
$wholeRunOneCore = 100.0 * $last.cpu_s_total / $last.elapsed_s

$summary = [ordered]@{
    label                        = $Label
    samples                      = $rows.Count
    elapsed_s                    = $last.elapsed_s
    exited_early                 = $exitedEarly
    cpu_s_total                  = $last.cpu_s_total
    cpu_pct_one_core_whole_run   = [math]::Round($wholeRunOneCore, 3)
    cpu_pct_machine_whole_run    = [math]::Round($wholeRunOneCore / $logicalCpus, 4)
    cpu_pct_one_core_sample_p50  = Get-Percentile $perSample 50
    cpu_pct_one_core_sample_p95  = Get-Percentile $perSample 95
    cpu_pct_one_core_sample_max  = ($perSample | Measure-Object -Maximum).Maximum
    working_set_mb_first         = $first.working_set_mb
    working_set_mb_last          = $last.working_set_mb
    working_set_mb_max           = ($ws | Measure-Object -Maximum).Maximum
    private_mb_first             = $first.private_mb
    private_mb_last              = $last.private_mb
    handles_first                = $first.handles
    handles_last                 = $last.handles
    handles_max                  = ($handles | Measure-Object -Maximum).Maximum
    gdi_objects_first            = $first.gdi_objects
    gdi_objects_last             = $last.gdi_objects
    user_objects_first           = $first.user_objects
    user_objects_last            = $last.user_objects
    threads_last                 = $last.threads
}
$summary | ConvertTo-Json | Set-Content -Encoding UTF8 "$base.summary.json"

Write-Host ""
Write-Host "=== ${Label}: $($rows.Count) samples over $($last.elapsed_s)s ==="
Write-Host ("CPU (whole run): {0:N3}% of one core = {1:N4}% of machine ({2} logical CPUs)" -f $wholeRunOneCore, ($wholeRunOneCore / $logicalCpus), $logicalCpus)
Write-Host ("Working set:     {0:N1} -> {1:N1} MB (max {2:N1})" -f $first.working_set_mb, $last.working_set_mb, $summary.working_set_mb_max)
Write-Host ("Handles:         {0} -> {1} (max {2})" -f $first.handles, $last.handles, $summary.handles_max)
Write-Host ("GDI / USER:      {0}/{1} -> {2}/{3}" -f $first.gdi_objects, $first.user_objects, $last.gdi_objects, $last.user_objects)
Write-Host "Wrote $base.csv, .meta.json, .summary.json"

if ($launched -and $StopWhenDone -and -not $proc.HasExited) {
    # WM_CLOSE first, so WindowDash saves analytics.dat on a clean exit.
    [void]$proc.CloseMainWindow()
    if (-not $proc.WaitForExit(5000)) { $proc.Kill() }
}
