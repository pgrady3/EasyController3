<# powershell script for resetting and flashing rp2040/pico autonomously #>
$ErrorActionpreference = "silentlycontinue"

<# reset pico over baud rate change to 1200 baud #>
echo "resetting pico"
mode $args baud=12 parity=n data=8 stop=1

<# check if pico has mounted as USB-device #>
$i = 0
$drive = $(Get-WmiObject Win32_LogicalDisk | Where-Object { $_.VolumeName -match "RPI" }).DeviceID.ToString()
while($drive -eq $null){
    echo "."
    Start-Sleep -m 250 <# sleep for 250 ms #>
    $i += 1

    <# Exit script if no usb device could be found #>
   if($i -ge 15){
        echo "No Pico device found. Check COM/USB-Port and wire connected to Pico - Aborting process."
        Exit 1 
   }
   $drive = $(Get-WmiObject Win32_LogicalDisk | Where-Object { $_.VolumeName -match "RPI" }).DeviceID.ToString()
}

<# copy flash-file to pico #>
echo "initiate copying to drive:" $drive
Copy-Item -Filter *.uf2 -Path '.\' -Recurse -Destination $drive+'\RPI-RP2'

Start-Sleep -m 1000 <# give pico some time to unmount #>

<# check if flash has been successful #>
if((Test-Path -Path $drive) -eq $True){
    echo "pico was reset but could not be flashed - please drag and drop .uf2-file manually!"
}
else{ 
    echo "done"
}



