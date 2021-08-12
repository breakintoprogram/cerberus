$use = 1

switch($use) {
    1 {
        $filePath = "H:\My Code\Cerberus\Z80\BBC Basic\build.bin"
        $startAddress = 0x0205
    }
}

$comPort = "COM5"
$baud = 9600
$filebuffer = [System.IO.File]::ReadAllBytes($filePath)
$fileSize = $filebuffer.length
$checkError = $false
$blockSize = 10

$port= new-Object System.IO.Ports.SerialPort $comPort, $baud, None, 8, one
$port.open()
if ($port.IsOpen) {
    Write-Host "Writing file to com port $($comPort)"
    $blockIndex = 0
    $blockBytesRemaining = $fileSize
    while($blockBytesRemaining -gt 0 -and !$checkError) {
        $blockSize = [math]::Min($blockBytesRemaining, $blockSize)
        $header = [string]::Format(“0x{0:X4}”, $startAddress + $blockIndex)
        $port.Write($header)
        Write-Host $header -NoNewline
        $checkA = 1
        $checkB = 0
        For($i=0; $i -lt $blockSize; $i++) {
            [byte]$data = $fileBuffer[$blockIndex++]
            $checkA = ($checkA + $data) % 256
            $checkB = ($checkB + $checkA) % 256
            $block = [string]::Format(" {0:X2}", $data)
            $port.Write($block)
            $blockBytesRemaining--
            Write-Host $block -NoNewline
        }
        $checksum = [string]::Format(“{0:X}", ($checkA -shl 8) -bor $checkB)
        $port.Write([byte[]](13),0,1)
        $response = $port.ReadLine().TrimEnd()
        Write-Host " > $($response) : $($checksum)"
        if (!$response.endsWith($checkSum)) {
            $checkError = $true
        }
    }
    $port.Write([byte[]](13),0,1)
    $port.Close()
    if ($checkError) {
        Write-Host "Upload error - mismatched checksum"
    }
}
else {
    Write-Host "Unable to open com port $($comPort)"
}