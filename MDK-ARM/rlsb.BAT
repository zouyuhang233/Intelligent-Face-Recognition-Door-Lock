SET PATH=C:\Users\zyh\AppData\Local\Keil_v5\ARM\ARMCC\Bin;C:\Program Files\Common Files\Oracle\Java\javapath;C:\Windows\system32;C:\Windows;C:\Windows\System32\Wbem;C:\Windows\System32\WindowsPowerShell\v1.0\;C:\Windows\System32\OpenSSH\;C:\Program Files (x86)\NVIDIA Corporation\PhysX\Common;C:\Program Files\NVIDIA Corporation\NVIDIA App\NvDLISR;C:\Program Files\dotnet\;D:\Program Files\nodejs\;C:\Program Files\Git\cmd;D:\mysql-8.4.9-winx64\mysql-8.4.9-winx64;D:\mysql-8.4.9-winx64\mysql-8.4.9-winx64\bin;D:\apache-maven-3.9.16-bin\apache-maven-3.9.16\bin;C:\Users\zyh\AppData\Local\Microsoft\WindowsApps;D:\Users\zyh\AppData\Local\Programs\Microsoft VS Code\bin;C:\Users\zyh\AppData\Roaming\npm;C:\Users\zyh\AppData\Local\Microsoft\WinGet\Links;C:\Users\zyh\AppData\Local\Pandoc\;
SET CPU_TYPE=STM32F103ZE
SET CPU_VENDOR=STMicroelectronics
SET UV2_TARGET=rlsb
SET CPU_CLOCK=0x007A1200
"C:\Users\zyh\AppData\Local\Keil_v5\ARM\ARMCC\Bin\ArmAsm" --Via "rlsb\startup_stm32f103xe._ia"
"C:\Users\zyh\AppData\Local\Keil_v5\ARM\ARMCC\Bin\ArmCC" --Via "rlsb\main.__i"
"C:\Users\zyh\AppData\Local\Keil_v5\ARM\ARMCC\Bin\ArmCC" --Via "rlsb\gpio.__i"
"C:\Users\zyh\AppData\Local\Keil_v5\ARM\ARMCC\Bin\ArmCC" --Via "rlsb\stm32f1xx_it.__i"
"C:\Users\zyh\AppData\Local\Keil_v5\ARM\ARMCC\Bin\ArmCC" --Via "rlsb\stm32f1xx_hal_msp.__i"
"C:\Users\zyh\AppData\Local\Keil_v5\ARM\ARMCC\Bin\ArmCC" --Via "rlsb\stm32f1xx_hal_gpio_ex.__i"
"C:\Users\zyh\AppData\Local\Keil_v5\ARM\ARMCC\Bin\ArmCC" --Via "rlsb\stm32f1xx_hal.__i"
"C:\Users\zyh\AppData\Local\Keil_v5\ARM\ARMCC\Bin\ArmCC" --Via "rlsb\stm32f1xx_hal_rcc.__i"
"C:\Users\zyh\AppData\Local\Keil_v5\ARM\ARMCC\Bin\ArmCC" --Via "rlsb\stm32f1xx_hal_rcc_ex.__i"
"C:\Users\zyh\AppData\Local\Keil_v5\ARM\ARMCC\Bin\ArmCC" --Via "rlsb\stm32f1xx_hal_gpio.__i"
"C:\Users\zyh\AppData\Local\Keil_v5\ARM\ARMCC\Bin\ArmCC" --Via "rlsb\stm32f1xx_hal_dma.__i"
"C:\Users\zyh\AppData\Local\Keil_v5\ARM\ARMCC\Bin\ArmCC" --Via "rlsb\stm32f1xx_hal_cortex.__i"
"C:\Users\zyh\AppData\Local\Keil_v5\ARM\ARMCC\Bin\ArmCC" --Via "rlsb\stm32f1xx_hal_pwr.__i"
"C:\Users\zyh\AppData\Local\Keil_v5\ARM\ARMCC\Bin\ArmCC" --Via "rlsb\stm32f1xx_hal_flash.__i"
"C:\Users\zyh\AppData\Local\Keil_v5\ARM\ARMCC\Bin\ArmCC" --Via "rlsb\stm32f1xx_hal_flash_ex.__i"
"C:\Users\zyh\AppData\Local\Keil_v5\ARM\ARMCC\Bin\ArmCC" --Via "rlsb\stm32f1xx_hal_exti.__i"
"C:\Users\zyh\AppData\Local\Keil_v5\ARM\ARMCC\Bin\ArmCC" --Via "rlsb\system_stm32f1xx.__i"
"C:\Users\zyh\AppData\Local\Keil_v5\ARM\ARMCC\Bin\ArmLink" --Via "rlsb\rlsb.lnp"
"C:\Users\zyh\AppData\Local\Keil_v5\ARM\ARMCC\Bin\fromelf.exe" "rlsb\rlsb.axf" --i32combined --output "rlsb\rlsb.hex"
