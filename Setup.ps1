param([switch]$UninstallMode)

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

[System.Windows.Forms.Application]::EnableVisualStyles()

# Configs
$appName = "NYoesyx"
$version = "1.0.0"
$defaultInstallPath = "$env:LOCALAPPDATA\NYoesyx"
$iconPath = "$PSScriptRoot\bin\NYoesyx.ico"

# UI Colors (Matrix style)
$bgColor = [System.Drawing.Color]::FromArgb(15, 15, 15)
$fgColor = [System.Drawing.Color]::LimeGreen
$btnColor = [System.Drawing.Color]::FromArgb(30, 30, 30)

$form = New-Object System.Windows.Forms.Form
$form.Text = "$appName Setup"
$form.Size = New-Object System.Drawing.Size(500, 350)
$form.StartPosition = "CenterScreen"
$form.BackColor = $bgColor
$form.ForeColor = $fgColor
$form.FormBorderStyle = 'FixedDialog'
$form.MaximizeBox = $false
$form.MinimizeBox = $false
if (Test-Path $iconPath) {
    $form.Icon = [System.Drawing.Icon]::ExtractAssociatedIcon($iconPath)
}

# Title Label
$lblTitle = New-Object System.Windows.Forms.Label
$lblTitle.Text = "Welcome to the $appName Setup Wizard"
$lblTitle.Font = New-Object System.Drawing.Font("Consolas", 14, [System.Drawing.FontStyle]::Bold)
$lblTitle.Location = New-Object System.Drawing.Point(20, 20)
$lblTitle.Size = New-Object System.Drawing.Size(440, 30)
$form.Controls.Add($lblTitle)

# Path Label
$lblPath = New-Object System.Windows.Forms.Label
$lblPath.Text = "Installation Directory:"
$lblPath.Font = New-Object System.Drawing.Font("Consolas", 10)
$lblPath.Location = New-Object System.Drawing.Point(20, 80)
$lblPath.Size = New-Object System.Drawing.Size(200, 20)
$form.Controls.Add($lblPath)

# Path TextBox
$txtPath = New-Object System.Windows.Forms.TextBox
$txtPath.Text = $defaultInstallPath
$txtPath.Font = New-Object System.Drawing.Font("Consolas", 10)
$txtPath.Location = New-Object System.Drawing.Point(20, 105)
$txtPath.Size = New-Object System.Drawing.Size(350, 25)
$txtPath.BackColor = $btnColor
$txtPath.ForeColor = $fgColor
$txtPath.BorderStyle = 'FixedSingle'
$form.Controls.Add($txtPath)

# Browse Button
$btnBrowse = New-Object System.Windows.Forms.Button
$btnBrowse.Text = "Browse..."
$btnBrowse.Font = New-Object System.Drawing.Font("Consolas", 10)
$btnBrowse.Location = New-Object System.Drawing.Point(380, 104)
$btnBrowse.Size = New-Object System.Drawing.Size(80, 27)
$btnBrowse.FlatStyle = 'Flat'
$btnBrowse.BackColor = $btnColor
$btnBrowse.Add_Click({
    $folderBrowser = New-Object System.Windows.Forms.FolderBrowserDialog
    $folderBrowser.Description = "Select installation folder"
    if ($folderBrowser.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) {
        $txtPath.Text = $folderBrowser.SelectedPath + "\NYoesyx"
    }
})
$form.Controls.Add($btnBrowse)

# Progress Bar
$progressBar = New-Object System.Windows.Forms.ProgressBar
$progressBar.Location = New-Object System.Drawing.Point(20, 180)
$progressBar.Size = New-Object System.Drawing.Size(440, 20)
$progressBar.Style = 'Continuous'
$form.Controls.Add($progressBar)

# Status Label
$lblStatus = New-Object System.Windows.Forms.Label
$lblStatus.Text = "Ready to install."
$lblStatus.Font = New-Object System.Drawing.Font("Consolas", 9)
$lblStatus.Location = New-Object System.Drawing.Point(20, 210)
$lblStatus.Size = New-Object System.Drawing.Size(440, 20)
$form.Controls.Add($lblStatus)

# Install Button
$btnInstall = New-Object System.Windows.Forms.Button
$btnInstall.Text = "Install"
$btnInstall.Font = New-Object System.Drawing.Font("Consolas", 11, [System.Drawing.FontStyle]::Bold)
$btnInstall.Location = New-Object System.Drawing.Point(260, 260)
$btnInstall.Size = New-Object System.Drawing.Size(100, 35)
$btnInstall.FlatStyle = 'Flat'
$btnInstall.BackColor = $btnColor
$form.Controls.Add($btnInstall)

# Uninstall Button
$btnUninstall = New-Object System.Windows.Forms.Button
$btnUninstall.Text = "Uninstall"
$btnUninstall.Font = New-Object System.Drawing.Font("Consolas", 10)
$btnUninstall.Location = New-Object System.Drawing.Point(140, 260)
$btnUninstall.Size = New-Object System.Drawing.Size(100, 35)
$btnUninstall.FlatStyle = 'Flat'
$btnUninstall.BackColor = $btnColor
$form.Controls.Add($btnUninstall)

# Cancel Button
$btnCancel = New-Object System.Windows.Forms.Button
$btnCancel.Text = "Cancel"
$btnCancel.Font = New-Object System.Drawing.Font("Consolas", 10)
$btnCancel.Location = New-Object System.Drawing.Point(370, 260)
$btnCancel.Size = New-Object System.Drawing.Size(90, 35)
$btnCancel.FlatStyle = 'Flat'
$btnCancel.BackColor = $btnColor
$btnCancel.Add_Click({ $form.Close() })
$form.Controls.Add($btnCancel)

# Logic Functions
function Update-Status([string]$msg, [int]$val) {
    $lblStatus.Text = $msg
    $progressBar.Value = $val
    $form.Refresh()
    Start-Sleep -Milliseconds 300
}

function Invoke-Install {
    $InstallDir = $txtPath.Text
    $SourceDir = "$PSScriptRoot\bin"
    
    if (-not (Test-Path "$SourceDir\nesxi.exe")) {
        [System.Windows.Forms.MessageBox]::Show("Installation media missing (nesxi.exe not found).", "Error", 0, [System.Windows.Forms.MessageBoxIcon]::Error)
        return
    }

    Update-Status "Creating directories..." 10
    if (-not (Test-Path $InstallDir)) { New-Item -Path $InstallDir -ItemType Directory -Force | Out-Null }
    if (-not (Test-Path "$InstallDir\bin")) { New-Item -Path "$InstallDir\bin" -ItemType Directory -Force | Out-Null }
    
    Update-Status "Copying core executables..." 30
    Copy-Item "$SourceDir\nesxi.exe" "$InstallDir\bin\nesxi.exe" -Force
    Copy-Item "$SourceDir\NYoesyx.ico" "$InstallDir\bin\NYoesyx.ico" -Force

    Update-Status "Registering extensions..." 50
    $RegRoot = "HKCU:\Software\Classes"
    # .nesx
    New-Item "$RegRoot\.nesx" -Force | Out-Null
    Set-ItemProperty "$RegRoot\.nesx" -Name "(default)" -Value "NYoesyx.Script" -Force
    New-Item "$RegRoot\NYoesyx.Script\DefaultIcon" -Force | Out-Null
    Set-ItemProperty "$RegRoot\NYoesyx.Script\DefaultIcon" -Name "(default)" -Value "`"$InstallDir\bin\NYoesyx.ico`",0" -Force
    New-Item "$RegRoot\NYoesyx.Script\shell\open\command" -Force | Out-Null
    Set-ItemProperty "$RegRoot\NYoesyx.Script\shell\open\command" -Name "(default)" -Value "`"$InstallDir\bin\nesxi.exe`" run `"%1`"" -Force

    # .nxbin
    New-Item "$RegRoot\.nxbin" -Force | Out-Null
    Set-ItemProperty "$RegRoot\.nxbin" -Name "(default)" -Value "NYoesyx.Binary" -Force
    New-Item "$RegRoot\NYoesyx.Binary\DefaultIcon" -Force | Out-Null
    Set-ItemProperty "$RegRoot\NYoesyx.Binary\DefaultIcon" -Name "(default)" -Value "`"$InstallDir\bin\NYoesyx.ico`",0" -Force
    New-Item "$RegRoot\NYoesyx.Binary\shell\open\command" -Force | Out-Null
    Set-ItemProperty "$RegRoot\NYoesyx.Binary\shell\open\command" -Name "(default)" -Value "`"$InstallDir\bin\nesxi.exe`" run `"%1`"" -Force

    Update-Status "Updating system PATH..." 80
    $PathKey = "HKCU:\Environment"
    $CurrentPath = (Get-ItemProperty -Path $PathKey -Name Path -ErrorAction SilentlyContinue).Path
    $BinPath = "$InstallDir\bin"
    if ($CurrentPath -notmatch [regex]::Escape($BinPath)) {
        $NewPath = $CurrentPath + ";" + $BinPath
        Set-ItemProperty -Path $PathKey -Name Path -Value $NewPath
        [System.Environment]::SetEnvironmentVariable("Path", $NewPath, [System.EnvironmentVariableTarget]::User)
    }

    Update-Status "Refreshing Explorer..." 90
    ie4uinit.exe -show > $null 2>&1

    Update-Status "Installation Complete!" 100
    [System.Windows.Forms.MessageBox]::Show("NYoesyx has been installed successfully to $InstallDir.`n`nYou can now run .nesx files or type 'nesxi' in your terminal.", "Success", 0, [System.Windows.Forms.MessageBoxIcon]::Information)
    $form.Close()
}

function Invoke-Uninstall {
    $InstallDir = $txtPath.Text
    $res = [System.Windows.Forms.MessageBox]::Show("Are you sure you want to completely remove NYoesyx?", "Confirm Uninstall", [System.Windows.Forms.MessageBoxButtons]::YesNo, [System.Windows.Forms.MessageBoxIcon]::Warning)
    if ($res -ne [System.Windows.Forms.DialogResult]::Yes) { return }

    Update-Status "Removing Registry entries..." 20
    $RegRoot = "HKCU:\Software\Classes"
    Remove-Item "$RegRoot\.nesx" -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item "$RegRoot\.nxbin" -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item "$RegRoot\NYoesyx.Script" -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item "$RegRoot\NYoesyx.Binary" -Recurse -Force -ErrorAction SilentlyContinue

    Update-Status "Cleaning System PATH..." 50
    $PathKey = "HKCU:\Environment"
    $CurrentPath = (Get-ItemProperty -Path $PathKey -Name Path -ErrorAction SilentlyContinue).Path
    $BinPath = "$InstallDir\bin"
    if ($CurrentPath -match [regex]::Escape($BinPath)) {
        $NewPath = ($CurrentPath -replace [regex]::Escape(";" + $BinPath), "") -replace [regex]::Escape($BinPath + ";"), ""
        Set-ItemProperty -Path $PathKey -Name Path -Value $NewPath
        [System.Environment]::SetEnvironmentVariable("Path", $NewPath, [System.EnvironmentVariableTarget]::User)
    }

    Update-Status "Removing files..." 80
    if (Test-Path $InstallDir) {
        Remove-Item $InstallDir -Recurse -Force -ErrorAction SilentlyContinue
    }

    Update-Status "Refreshing Explorer..." 90
    ie4uinit.exe -show > $null 2>&1

    Update-Status "Uninstall Complete!" 100
    [System.Windows.Forms.MessageBox]::Show("NYoesyx has been uninstalled.", "Success", 0, [System.Windows.Forms.MessageBoxIcon]::Information)
    $form.Close()
}

$btnInstall.Add_Click({ Invoke-Install })
$btnUninstall.Add_Click({ Invoke-Uninstall })

$form.ShowDialog() | Out-Null
