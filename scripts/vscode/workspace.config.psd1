@{
    ProjectPath = 'MDK-ARM/CprSensor.uvprojx'
    KeilTarget = 'CprSensor'
    HexFilePath = 'MDK-ARM/CprSensor/CprSensor.hex'

    JLink = @{
        Device = 'STM32F103RE'
        Interface = 'SWD'
        SpeedKHz = 4000
        GdbPort = 3331
        SwoPort = 3332
        TelnetPort = 3333
        RttTelnetPort = 19021
    }

    Serial = @{
        BaudRate = 115200
        DataBits = 8
        Parity = 'None'
        StopBits = 'One'
        DtrEnable = $false
        RtsEnable = $false
    }
}