param(
    [string]$PortName = "",
    [int]$BaudRate = 115200
)

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms.DataVisualization

[System.Windows.Forms.Application]::EnableVisualStyles()

if ([string]::IsNullOrWhiteSpace($PortName)) {
    $detectedPorts = @([System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object)
    if ($detectedPorts.Count -eq 1) {
        $PortName = $detectedPorts[0]
    }
    elseif ($detectedPorts.Count -eq 0) {
        [System.Windows.Forms.MessageBox]::Show(
            "No COM port was found. Connect the USB-to-TTL adapter and try again.",
            "Serial dashboard"
        ) | Out-Null
        exit 1
    }
    else {
        [System.Windows.Forms.MessageBox]::Show(
            "More than one COM port was found: $($detectedPorts -join ', ').`r`nRun with -PortName COMx.",
            "Serial dashboard"
        ) | Out-Null
        exit 1
    }
}

$serialPort = [System.IO.Ports.SerialPort]::new(
    $PortName,
    $BaudRate,
    [System.IO.Ports.Parity]::None,
    8,
    [System.IO.Ports.StopBits]::One
)
$serialPort.NewLine = "`n"
$serialPort.ReadTimeout = 50

try {
    $serialPort.Open()
}
catch {
    [System.Windows.Forms.MessageBox]::Show(
        "Cannot open ${PortName}: $($_.Exception.Message)",
        "Serial dashboard"
    ) | Out-Null
    $serialPort.Dispose()
    exit 1
}

function New-TextLabel {
    param(
        [string]$Text,
        [int]$X,
        [int]$Y,
        [int]$Width = 180,
        [int]$Height = 28,
        [float]$FontSize = 10,
        [bool]$Bold = $false
    )

    $label = New-Object System.Windows.Forms.Label
    $label.Text = $Text
    $label.Location = New-Object System.Drawing.Point($X, $Y)
    $label.Size = New-Object System.Drawing.Size($Width, $Height)
    $fontStyle = if ($Bold) {
        [System.Drawing.FontStyle]::Bold
    }
    else {
        [System.Drawing.FontStyle]::Regular
    }
    $label.Font = New-Object System.Drawing.Font("Segoe UI", $FontSize, $fontStyle)
    return $label
}

function Add-LineSeries {
    param(
        [System.Windows.Forms.DataVisualization.Charting.Chart]$Chart,
        [string]$Name,
        [System.Drawing.Color]$Color,
        [string]$AreaName
    )

    $series = New-Object System.Windows.Forms.DataVisualization.Charting.Series
    $series.Name = $Name
    $series.ChartType = [System.Windows.Forms.DataVisualization.Charting.SeriesChartType]::Line
    $series.BorderWidth = 2
    $series.Color = $Color
    $series.ChartArea = $AreaName
    $Chart.Series.Add($series)
    return $series
}

$form = New-Object System.Windows.Forms.Form
$form.Text = "Tracked Vehicle Serial Dashboard - $PortName @ $BaudRate"
$form.ClientSize = New-Object System.Drawing.Size(980, 700)
$form.StartPosition = [System.Windows.Forms.FormStartPosition]::CenterScreen
$form.BackColor = [System.Drawing.Color]::FromArgb(245, 247, 250)
$form.MinimumSize = New-Object System.Drawing.Size(996, 739)

$title = New-TextLabel "RF206S / Motor Live Monitor" 20 12 500 38 18 $true
$form.Controls.Add($title)

$connectionLabel = New-TextLabel "CONNECTED: $PortName" 755 18 200 28 10 $true
$connectionLabel.TextAlign = [System.Drawing.ContentAlignment]::MiddleCenter
$connectionLabel.BackColor = [System.Drawing.Color]::FromArgb(198, 239, 206)
$connectionLabel.ForeColor = [System.Drawing.Color]::FromArgb(0, 97, 0)
$form.Controls.Add($connectionLabel)

$linkState = New-TextLabel "LINK" 20 64 100 34 11 $true
$enableState = New-TextLabel "ENABLE" 130 64 100 34 11 $true
$armedState = New-TextLabel "ARMED" 240 64 100 34 11 $true
foreach ($stateLabel in @($linkState, $enableState, $armedState)) {
    $stateLabel.TextAlign = [System.Drawing.ContentAlignment]::MiddleCenter
    $stateLabel.BackColor = [System.Drawing.Color]::LightGray
    $form.Controls.Add($stateLabel)
}

$rawLabel = New-TextLabel "Raw: throttle=--  turn=--  SA=--" 370 66 390 30 11 $false
$form.Controls.Add($rawLabel)

$timeLabel = New-TextLabel "Board time: -- ms" 770 66 190 30 10 $false
$form.Controls.Add($timeLabel)

$commandLabel = New-TextLabel "Command: T=--  L=--  R=--" 20 108 300 30 10 $true
$pwmLabel = New-TextLabel "PWM: C1=-- C2=-- C3=-- C4=--" 320 108 380 30 10 $true
$encoderLabel = New-TextLabel "Left encoder: pos=-- cps=-- invalid=--" 700 108 270 30 9 $true
$form.Controls.Add($commandLabel)
$form.Controls.Add($pwmLabel)
$form.Controls.Add($encoderLabel)

$throttleBarLabel = New-TextLabel "Throttle raw (0..2047)" 20 148 190 24 9 $false
$form.Controls.Add($throttleBarLabel)
$throttleBar = New-Object System.Windows.Forms.ProgressBar
$throttleBar.Location = New-Object System.Drawing.Point(210, 149)
$throttleBar.Size = New-Object System.Drawing.Size(740, 22)
$throttleBar.Minimum = 0
$throttleBar.Maximum = 2047
$form.Controls.Add($throttleBar)

$chart = New-Object System.Windows.Forms.DataVisualization.Charting.Chart
$chart.Location = New-Object System.Drawing.Point(20, 188)
$chart.Size = New-Object System.Drawing.Size(930, 300)
$chart.BackColor = [System.Drawing.Color]::White
$commandArea = New-Object System.Windows.Forms.DataVisualization.Charting.ChartArea
$commandArea.Name = "Commands"
$commandArea.AxisX.Title = "Last 20 seconds"
$commandArea.AxisY.Title = "Command / PWM counts"
$commandArea.AxisY.Minimum = -1100
$commandArea.AxisY.Maximum = 1100
$commandArea.AxisX.MajorGrid.Enabled = $false
$commandArea.AxisY.MajorGrid.LineColor = [System.Drawing.Color]::Gainsboro
$chart.ChartAreas.Add($commandArea)
$chart.Legends.Add((New-Object System.Windows.Forms.DataVisualization.Charting.Legend))
$throttleSeries = Add-LineSeries $chart "Throttle command" ([System.Drawing.Color]::RoyalBlue) "Commands"
$leftSeries = Add-LineSeries $chart "Left command" ([System.Drawing.Color]::DarkViolet) "Commands"
$rightSeries = Add-LineSeries $chart "Right command" ([System.Drawing.Color]::DeepPink) "Commands"
$rpwmSeries = Add-LineSeries $chart "Left C1" ([System.Drawing.Color]::ForestGreen) "Commands"
$lpwmSeries = Add-LineSeries $chart "Left C2" ([System.Drawing.Color]::DarkOrange) "Commands"
$rightRpwmSeries = Add-LineSeries $chart "Right C3" ([System.Drawing.Color]::Teal) "Commands"
$rightLpwmSeries = Add-LineSeries $chart "Right C4" ([System.Drawing.Color]::Sienna) "Commands"
$form.Controls.Add($chart)

$speedChart = New-Object System.Windows.Forms.DataVisualization.Charting.Chart
$speedChart.Location = New-Object System.Drawing.Point(20, 500)
$speedChart.Size = New-Object System.Drawing.Size(930, 165)
$speedChart.BackColor = [System.Drawing.Color]::White
$speedArea = New-Object System.Windows.Forms.DataVisualization.Charting.ChartArea
$speedArea.Name = "Encoder"
$speedArea.AxisX.Title = "Last 20 seconds"
$speedArea.AxisY.Title = "Encoder counts / second"
$speedArea.AxisX.MajorGrid.Enabled = $false
$speedArea.AxisY.MajorGrid.LineColor = [System.Drawing.Color]::Gainsboro
$speedChart.ChartAreas.Add($speedArea)
$speedChart.Legends.Add((New-Object System.Windows.Forms.DataVisualization.Charting.Legend))
$speedSeries = Add-LineSeries $speedChart "Encoder CPS" ([System.Drawing.Color]::Crimson) "Encoder"
$form.Controls.Add($speedChart)

$footer = New-TextLabel "Fields: input -> safety state -> command -> TIM1 PWM -> encoder feedback" 20 669 700 24 9 $false
$form.Controls.Add($footer)

$script:receiveBuffer = ""
$script:sampleNumber = 0
$script:lastValidSampleAt = [DateTime]::Now

$timer = New-Object System.Windows.Forms.Timer
$timer.Interval = 50
$timer.Add_Tick({
    try {
        if ($serialPort.BytesToRead -gt 0) {
            $script:receiveBuffer += $serialPort.ReadExisting()
            $normalized = $script:receiveBuffer.Replace("`r", "")
            $lines = $normalized.Split("`n")
            if ($lines.Length -gt 1) {
                $script:receiveBuffer = $lines[$lines.Length - 1]
                for ($lineIndex = 0; $lineIndex -lt ($lines.Length - 1); $lineIndex++) {
                    $line = $lines[$lineIndex].Trim()
                    if (($line -notmatch '^-?\d+(,-?\d+){13}$') -and
                        ($line -notmatch '^-?\d+(,-?\d+){16}$')) {
                        continue
                    }

                    [long[]]$value = $line.Split(',') | ForEach-Object { [long]$_ }
                    $script:lastValidSampleAt = [DateTime]::Now
                    $script:sampleNumber++

                    $rightCommand = if ($value.Length -ge 17) { $value[14] } else { 0 }
                    $rightRpwm = if ($value.Length -ge 17) { $value[15] } else { 0 }
                    $rightLpwm = if ($value.Length -ge 17) { $value[16] } else { 0 }

                    $timeLabel.Text = "Board time: $($value[0]) ms"
                    $rawLabel.Text = "Raw: throttle=$($value[1])  turn=$($value[2])  SA=$($value[3])"
                    $commandLabel.Text = "Command: T=$($value[7])  L=$($value[8])  R=$rightCommand"
                    $pwmLabel.Text = "PWM: C1=$($value[9]) C2=$($value[10]) C3=$rightRpwm C4=$rightLpwm"
                    $encoderLabel.Text = "Left encoder: pos=$($value[11]) cps=$($value[12]) invalid=$($value[13])"

                    $throttleValue = [Math]::Max(0, [Math]::Min(2047, [int]$value[1]))
                    $throttleBar.Value = $throttleValue

                    $states = @(
                        @{ Label = $linkState; Value = $value[4] },
                        @{ Label = $enableState; Value = $value[5] },
                        @{ Label = $armedState; Value = $value[6] }
                    )
                    foreach ($state in $states) {
                        if ($state.Value -ne 0) {
                            $state.Label.BackColor = [System.Drawing.Color]::FromArgb(198, 239, 206)
                            $state.Label.ForeColor = [System.Drawing.Color]::FromArgb(0, 97, 0)
                        }
                        else {
                            $state.Label.BackColor = [System.Drawing.Color]::LightGray
                            $state.Label.ForeColor = [System.Drawing.Color]::Black
                        }
                    }

                    [void]$throttleSeries.Points.AddXY($script:sampleNumber, $value[7])
                    [void]$leftSeries.Points.AddXY($script:sampleNumber, $value[8])
                    [void]$rightSeries.Points.AddXY($script:sampleNumber, $rightCommand)
                    [void]$rpwmSeries.Points.AddXY($script:sampleNumber, $value[9])
                    [void]$lpwmSeries.Points.AddXY($script:sampleNumber, $value[10])
                    [void]$rightRpwmSeries.Points.AddXY($script:sampleNumber, $rightRpwm)
                    [void]$rightLpwmSeries.Points.AddXY($script:sampleNumber, $rightLpwm)
                    [void]$speedSeries.Points.AddXY($script:sampleNumber, $value[12])

                    foreach ($series in @($throttleSeries, $leftSeries, $rightSeries, $rpwmSeries, $lpwmSeries, $rightRpwmSeries, $rightLpwmSeries, $speedSeries)) {
                        while ($series.Points.Count -gt 200) {
                            $series.Points.RemoveAt(0)
                        }
                    }
                }
            }
        }

        if (([DateTime]::Now - $script:lastValidSampleAt).TotalSeconds -gt 1.0) {
            $connectionLabel.Text = "WAITING FOR DATA: $PortName"
            $connectionLabel.BackColor = [System.Drawing.Color]::FromArgb(255, 235, 156)
            $connectionLabel.ForeColor = [System.Drawing.Color]::FromArgb(156, 101, 0)
        }
        else {
            $connectionLabel.Text = "CONNECTED: $PortName"
            $connectionLabel.BackColor = [System.Drawing.Color]::FromArgb(198, 239, 206)
            $connectionLabel.ForeColor = [System.Drawing.Color]::FromArgb(0, 97, 0)
        }
    }
    catch {
        $timer.Stop()
        $connectionLabel.Text = "SERIAL ERROR"
        $connectionLabel.BackColor = [System.Drawing.Color]::MistyRose
        [System.Windows.Forms.MessageBox]::Show($_.Exception.Message, "Serial dashboard") | Out-Null
    }
})

$form.Add_FormClosed({
    $timer.Stop()
    if ($serialPort.IsOpen) {
        $serialPort.Close()
    }
    $serialPort.Dispose()
})

$timer.Start()
[System.Windows.Forms.Application]::Run($form)
