param(
  [string]$SecretsPath = (Join-Path $PSScriptRoot "..\secrets.h"),
  [string]$OutputPath = (Join-Path $PSScriptRoot "..\secrets.js")
)

if (-not (Test-Path -LiteralPath $SecretsPath)) {
  throw "Arquivo secrets.h nao encontrado em: $SecretsPath"
}

$content = Get-Content -Raw -LiteralPath $SecretsPath

function Get-StringDefine {
  param([string]$Name)

  $pattern = '^\s*#define\s+' + [regex]::Escape($Name) + '\s+"([^"]*)"'
  $match = [regex]::Match($content, $pattern, [System.Text.RegularExpressions.RegexOptions]::Multiline)
  if (-not $match.Success) {
    throw "Define obrigatorio ausente em secrets.h: $Name"
  }

  $match.Groups[1].Value
}

function Get-OptionalNumberDefine {
  param([string]$Name)

  $pattern = '^\s*#define\s+' + [regex]::Escape($Name) + '\s+(\d+)'
  $match = [regex]::Match($content, $pattern, [System.Text.RegularExpressions.RegexOptions]::Multiline)
  if ($match.Success) {
    return [int]$match.Groups[1].Value
  }

  $null
}

$server = Get-StringDefine "MQTT_SERVER"
$user = Get-StringDefine "MQTT_USER"
$password = Get-StringDefine "MQTT_PASSWORD"
$mqttPort = Get-OptionalNumberDefine "MQTT_PORT"
$wsPort = Get-OptionalNumberDefine "MQTT_WS_PORT"

if (-not $wsPort) {
  $wsPort = if ($mqttPort -eq 8883) { 8884 } else { $mqttPort }
}

if (-not $wsPort) {
  throw "Defina MQTT_PORT ou MQTT_WS_PORT em secrets.h"
}

$config = [ordered]@{
  MQTT_URL = "wss://${server}:${wsPort}/mqtt"
  MQTT_USERNAME = $user
  MQTT_PASSWORD = $password
}

$json = $config | ConvertTo-Json -Depth 2
Set-Content -LiteralPath $OutputPath -Encoding utf8 -Value "window.SMARTWATER_CONFIG = $json;"

Write-Host "Gerado: $OutputPath"
