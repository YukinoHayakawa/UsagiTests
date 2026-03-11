<#
.SYNOPSIS
    Parses test.cpp in-place, converting bare file paths into #include directives
    while safely preserving existing #includes and C-style comments.

.DESCRIPTION
    Reads test.cpp line by line.
    - If a line is already an #include, it is kept as-is.
    - If a line is a valid file path, it's converted to an #include.
    - If a line is a comment (single or multi-line), it is preserved.
    - Any other line throws an error to catch malformed input.
#>

param(
    [Parameter(Mandatory=$false)]
    [string]$FilePath = "test.cpp"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path -Path $FilePath)) {
    Write-Host "Shio: $FilePath not found. Creating an empty file."
    Set-Content -Path $FilePath -Value "" -Encoding UTF8
    exit
}

$lines = Get-Content -Path $FilePath
$output = @()

$inBlockComment = $false
$inConstevalBlock = $false

foreach ($line in $lines) {
    $trimmed = $line.Trim()

    # Empty lines
    if ([string]::IsNullOrWhiteSpace($trimmed)) {
        $output += $line
        continue
    }

    # Consteval block handling
    if ($inConstevalBlock) {
        $output += $line
        if ($trimmed -match '\}') {
            $inConstevalBlock = $false
        }
        continue
    }

    if ($trimmed.StartsWith('consteval') -and $trimmed.Contains('{')) {
        $output += $line
        if (-not ($trimmed -match '\}')) {
            $inConstevalBlock = $true
        }
        continue
    }

    # Block comment handling
    if ($inBlockComment) {
        $output += $line
        if ($trimmed -match '\*/') {
            $inBlockComment = $false
        }
        continue
    }

    if ($trimmed -match '/\*') {
        $output += $line
        if (-not ($trimmed -match '\*/')) {
            $inBlockComment = $true
        }
        continue
    }

    # Single line comments
    if ($trimmed.StartsWith('//')) {
        $output += $line
        continue
    }

    # Already an include
    if ($trimmed.StartsWith('#include')) {
        $output += $line
        continue
    }

    # Bare path conversion
    # We assume if it has no spaces and ends with typical header extensions, or is an absolute path.
    # Actually, we can check if it looks like a valid path string.
    # But since the user specifically requested "if(line.is_valid_path())", we will just test if the path exists.
    # Wait, the path might be relative to the engine root, or absolute. Let's try Test-Path, but if they are copying from ReSharper it might be an absolute path.
    if (Test-Path -Path $trimmed -PathType Leaf) {
        Write-Host "Shio: Converting path to include -> $trimmed"

        # Escape backslashes in path just in case, or use forward slashes
        $safePath = $trimmed.Replace("\", "/")
        $output += "#include `"$safePath`""
        continue
    }

    # If it's not a known comment, not an include, and not a valid path on disk
    Write-Error "Shio: Malformed line in $FilePath -> '$trimmed'"
}

$tmpPath = "$FilePath.tmp"
Set-Content -Path $tmpPath -Value $output -Encoding UTF8
Move-Item -Path $tmpPath -Destination $FilePath -Force
Write-Host "Shio: Successfully processed $FilePath."
