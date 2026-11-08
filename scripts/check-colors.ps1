param([string]$Path = "build\icon_dark.png")

Add-Type -AssemblyName System.Drawing

$full = (Resolve-Path $Path).Path
$img = [System.Drawing.Image]::FromFile($full)
$bmp = New-Object System.Drawing.Bitmap $img

function Sample-Region($bmp, [int]$x0, [int]$x1, [int]$y0, [int]$y1, [string]$label) {
    $counts = @{}
    for ($x = $x0; $x -lt $x1; $x++) {
        for ($y = $y0; $y -lt $y1; $y++) {
            $c = $bmp.GetPixel($x, $y)
            $k = "$($c.R),$($c.G),$($c.B)"
            if ($counts.ContainsKey($k)) { $counts[$k]++ } else { $counts[$k] = 1 }
        }
    }
    Write-Host "=== $label ==="
    $counts.GetEnumerator() | Sort-Object Value -Descending | Select-Object -First 6 |
        ForEach-Object { Write-Host("  {0,-16} x{1}" -f $_.Key, $_.Value) }
}

Write-Host "image: $($bmp.Width)x$($bmp.Height)"
Sample-Region $bmp 18 62 70 300 "sidebar icons"
Sample-Region $bmp 560 625 870 930 "play button"
Sample-Region $bmp 810 860 240 285 "heart icon"

$bmp.Dispose()
$img.Dispose()
