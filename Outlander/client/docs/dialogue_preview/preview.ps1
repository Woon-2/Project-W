$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..\..")
$port = 8123
$url = "http://127.0.0.1:$port/client/docs/dialogue_preview/"

Write-Host "Dialogue preview: $url"
Write-Host "Stop with Ctrl+C."

Start-Process $url
python -m http.server $port --directory $repoRoot
