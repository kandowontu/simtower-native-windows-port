$ErrorActionPreference = 'Stop'

$repository = (git rev-parse --show-toplevel).Trim()
if (-not $repository) {
    throw 'Run this audit inside the Git repository.'
}

Push-Location $repository
try {
    $tracked = @(git ls-files)
    $forbidden = @(
        $tracked | Where-Object {
            $_ -match '(?i)\.(exe|dll|com|hlp|pack|wav|mid|vhd|img|tdt)$' -or
            $_ -match '(^|/)original/' -or
            $_ -match '(^|/)release-native/' -or
            $_ -match '(^|/)port/generated/'
        }
    )
    if ($forbidden.Count -ne 0) {
        $forbidden | ForEach-Object { Write-Error "Forbidden release file: $_" }
        throw 'The source release contains game or binary payloads.'
    }

    $privateNeedles = @(
        ('192.168.1.' + '176'),
        ('C:' + '\Users\' + 'kando'),
        ('github_' + 'pat_'),
        ('gh' + 'p_'),
        ('gh' + 'o_')
    )
    foreach ($needle in $privateNeedles) {
        $matches = @(git grep -n -F -e $needle -- $tracked 2>$null)
        if ($matches.Count -ne 0) {
            $matches | ForEach-Object { Write-Error $_ }
            throw "The source release contains a private path or credential marker: $needle"
        }
    }

    Write-Output "Source release audit passed for $($tracked.Count) tracked files."
}
finally {
    Pop-Location
}

exit 0
