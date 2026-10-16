# Offline display regression; never opens a serial port or flashes firmware.
param([string]$PreviewPath = '')
$Repository = Split-Path $PSScriptRoot -Parent
if ([string]::IsNullOrWhiteSpace($PreviewPath)) {
    $PreviewPath = Join-Path $Repository 'build/dashboard-offline-preview.png'
}
$previewDirectory = Split-Path -Parent ([System.IO.Path]::GetFullPath($PreviewPath))
New-Item -ItemType Directory -Path $previewDirectory -Force | Out-Null
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms.DataVisualization
[System.Windows.Forms.Application]::EnableVisualStyles()
$PortName='OFFLINE'
$BaudRate=115200
$source=[System.IO.File]::ReadAllText((Join-Path $Repository 'tools/serial_dashboard.ps1')).Replace("`r`n","`n")
$tokens=$null; $errors=$null
[void][System.Management.Automation.Language.Parser]::ParseInput($source,[ref]$tokens,[ref]$errors)
if($errors.Count -gt 0){throw ($errors | Out-String)}
$start=$source.IndexOf('function New-TextLabel')
$end=$source.IndexOf('$script:receiveBuffer =')
. ([scriptblock]::Create($source.Substring($start,$end-$start)))
$marker='$timer.Add_Tick({'
$start=$source.IndexOf($marker)+$marker.Length
$end=$source.IndexOf("`n})`n`n"+'$form.Add_FormClosed',$start)
if($end -lt 0){throw 'Tick handler ending not found'}
$handler=[scriptblock]::Create($source.Substring($start,$end-$start))
$serialPort=[pscustomobject]@{BytesToRead=0;Data=''}
$serialPort | Add-Member -MemberType ScriptMethod -Name ReadExisting -Value {$data=$this.Data; $this.Data=''; $this.BytesToRead=0; return $data}
$timer=New-Object System.Windows.Forms.Timer
$script:receiveBuffer=''
$script:sampleNumber=0
$script:lastBoardMilliseconds=-1L
$script:lastValidSampleAt=[DateTime]::Now
function Send-Sample([string]$line){$serialPort.Data=$line+"`n";$serialPort.BytesToRead=$serialPort.Data.Length; . $handler}
function Assert-True([bool]$condition,[string]$message){if(-not $condition){throw $message}}
Send-Sample '1000,1024,1024,353,1,0,0,0,0,0,0,0,0,0,0,0,0'
Assert-True ($pwmLabel.Text -eq 'PWM: C3=0 C4=0 C5=0 C6=0') 'Zero sample mapping'
Send-Sample '1100,353,1024,1694,1,1,1,-1000,-1000,0,4200,0,0,0,1000,4200,0'
Assert-True ($pwmLabel.Text -eq 'PWM: C3=0 C4=4200 C5=4200 C6=0') 'Reverse mapping'
Assert-True ($lpwmSeries.Points[1].YValues[0] -eq 4200) 'Left C4 plot value'
Assert-True ($rightRpwmSeries.Points[1].YValues[0] -eq 4200) 'Right C5 plot value'
Assert-True ($rpwmSeries.YAxisType.ToString() -eq 'Secondary') 'PWM secondary axis'
Assert-True ($commandArea.AxisY2.Maximum -eq 8400) 'PWM axis range'
Assert-True ([Math]::Abs($lpwmSeries.Points[1].XValue-1.1) -lt 0.001) 'Board seconds axis'
Send-Sample 'invalid,telemetry'
Assert-True ($throttleSeries.Points.Count -eq 2) 'Invalid sample ignored'
Send-Sample '1200,1694,1024,1694,1,1,1,1000,1000,4200,0,0,0,0,-1000,0,4200'
Assert-True ($pwmLabel.Text -eq 'PWM: C3=4200 C4=0 C5=0 C6=4200') 'Forward mapping'
$script:lastValidSampleAt=[DateTime]::Now.AddSeconds(-2)
. $handler
Assert-True ($connectionLabel.Text -like 'WAITING*') 'Stale connection marked'
Assert-True ($armedState.BackColor.ToArgb() -eq [System.Drawing.Color]::LightGray.ToArgb()) 'Stale armed indicator cleared'
Send-Sample '100,1024,1024,353,1,0,0,0,0,0,0,0,0,0,0,0,0'
Assert-True ($throttleSeries.Points.Count -eq 1) 'Reset clears history'
Send-Sample '200,1024,1024,353,1,0,0,0,0,0,0,0,0,0'
Assert-True ($pwmLabel.Text -eq 'PWM: C3=0 C4=0 C5=0 C6=0') 'Legacy sample compatibility'
for($i=3;$i -le 210;$i++){
 $nowMs=$i*100
 $throttle=[int]([Math]::Min(1000,($i-3)*5))
 $pulse=[int]($throttle*4.2)
 Send-Sample "$nowMs,1694,1024,1694,1,1,1,$throttle,$throttle,$pulse,0,0,0,0,-$throttle,0,$pulse"
}
Assert-True ($throttleSeries.Points.Count -eq 200) 'Bounded history'
$form.CreateControl()
$form.PerformLayout()
$form.StartPosition = [System.Windows.Forms.FormStartPosition]::Manual
$form.Location = [System.Drawing.Point]::new(-20000,-20000)
$form.Show()
[System.Windows.Forms.Application]::DoEvents()
$bitmap=New-Object System.Drawing.Bitmap($form.ClientSize.Width,$form.ClientSize.Height)
$form.DrawToBitmap($bitmap,[System.Drawing.Rectangle]::new(0,0,$bitmap.Width,$bitmap.Height))
$bitmap.Save($PreviewPath,[System.Drawing.Imaging.ImageFormat]::Png)
$bitmap.Dispose();$timer.Dispose();$form.Dispose()
Write-Output 'PASS: zero/forward/reverse mapping, PWM axis, real seconds, invalid lines, stale state, board reset, legacy CSV, history bound.'
