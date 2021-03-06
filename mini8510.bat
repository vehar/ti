REM
REM              Texas Instruments OMAP(TM) Platform Software
REM  (c) Copyright Texas Instruments, Incorporated. All Rights Reserved.
REM 
REM  Use of this software is controlled by the terms and conditions found
REM  in the license agreement under which this software has been supplied.
REM

REM --------------------------------------------------------------------------
REM Build Environment
REM --------------------------------------------------------------------------

REM Always copy binaries to flat release directory
set WINCEREL=1
REM Generate .cod, .lst files
set WINCECOD=1

REM ----OS SPECIFIC VERSION SETTINGS----------

REM if "%SG_OUTPUT_ROOT%" == "" (set SG_OUTPUT_ROOT=%_PROJECTROOT%\cesysgen) 

REM set _PLATCOMMONLIB=%_PLATFORMROOT%\common\lib
REM set _TARGETPLATLIB=%_TARGETPLATROOT%\lib
REM set _EBOOTLIBS=%SG_OUTPUT_ROOT%\oak\lib
REM set _KITLLIBS=%SG_OUTPUT_ROOT%\oak\lib

set _RAWFILETGT=%_TARGETPLATROOT%\target\%_TGTCPU%

set BSP_WCE=1


REM --------------------------------------------------------------------------
REM Initial Operating Point - VDD1 voltage, MPU (CPU) and IVA speeds
REM --------------------------------------------------------------------------

REM Select initial operating point (CPU and IVA speed, VDD1 voltage).
REM Note that this controls the operating point selected by the bootloader.
REM If the power management subsystem is enabled, the initial operating point 
REM it uses is controlled by registry entries.
REM The following are choices for 37xx family
REM Use 4 for MPU[1000Mhz @ 1.375V], IVA2[800Mhz @ 1.375V], CORE[400Mhz @ 1.1375V] (OPMTM)
REM Use 3 for MPU[800Mhz  @ 1.2625V], IVA2[660Mhz @ 1.2625V], CORE[400Mhz @ 1.1375V] (OPM120)
REM Use 2 for MPU[600Mhz  @ 1.1000V], IVA2[520Mhz @ 1.1000V], CORE[400Mhz @ 1.1375V] (OPM100)
REM Use 1 for MPU[300Mhz  @ 0.9375V], IVA2[260Mhz @ 0.9375V], CORE[400Mhz @ 1.1375V] (OPM50)
set BSP_OPM_SELECT_37XX=4

REM The following are choices for 35xx family
REM Use 6 for MPU[720Mhz @ 1.350V], IVA2[520Mhz @ 1.350V], CORE[332Mhz @ 1.15V]
REM Use 5 for MPU[600Mhz @ 1.350V], IVA2[430Mhz @ 1.350V], CORE[332Mhz @ 1.15V]
REM Use 4 for MPU[550Mhz @ 1.275V], IVA2[400Mhz @ 1.275V], CORE[332Mhz @ 1.15V]
REM Use 3 for MPU[500Mhz @ 1.200V], IVA2[360Mhz @ 1.200V], CORE[332Mhz @ 1.15V]
REM Use 2 for MPU[250Mhz @ 1.000V], IVA2[180Mhz @ 1.000V], CORE[332Mhz @ 1.15V]
REM Use 1 for MPU[125Mhz @ 0.975V], IVA2[ 90Mhz @ 0.975V], CORE[332Mhz @ 1.05V]
set BSP_OPM_SELECT_35XX=5


REM --------------------------------------------------------------------------
REM Misc. settings
REM --------------------------------------------------------------------------


REM TI BSP builds its own ceddk.dll. Setting this IMG var excludes default CEDDK from the OS image.
set IMGNODFLTDDK=1

REM This BSP targets by default the EVM2 board
set BSP_EVM2=1

REM Use this to enable 4 bit mmc cards
set BSP_EMMCFEATURE=1

REM --------------------------------------------------------------------------
REM Memory Settings
REM --------------------------------------------------------------------------

REM ES3.0 silicon (CPU marked JY192, 128MB SDRAM) does not support SDRAM bank 1
REM ES3.1 silicon (CPU marked JW256, 256MB SDRAM) supports SDRAM bank 1

REM Set to support 128MB of memory in SDRAM bank 1
REM Note that EVM SDRAM bank 0 is always assumed to be 128MB.
REM Note that if this variable is changed, a clean build and XLDR/EBOOT update is required.
set BSP_SDRAM_BANK1_ENABLE=1

REM Set/Unset this variable to add/remove persistent regisrty support.
REM Note that if this variable is changed, a resysgen is required.
REM Mount NAND flash as root file system instead of the Object Store.
REM Enable persistent storage (registry, file cache, etc)
REM Default registry flush period is every 2 minutes, or on suspend
REM See: [HKEY_LOCAL_MACHINE\System\ObjectStore\RegFlush\FlushPeriod]
set IMGREGHIVE=
echo "SET PRJ variables because IMGREGHIVE=%IMGREGHIVE%"
if /i "%IMGREGHIVE%"=="1" set PRJ_ENABLE_FSREGHIVE=1
if /i "%IMGREGHIVE%"=="1" set PRJ_ENABLE_REGFLUSH_THREAD=1
if /i "%IMGREGHIVE%"=="1" set PRJ_ENABLE_FSMOUNTASROOT=1
if /i "%IMGREGHIVE%"=="1" set PRJ_BOOTDEVICE_MSFLASH=1

set BSP_NODM9000=
set BSP_NODIGITAL_CAMERA=1
set BSP_NOSMSC9514=1
set BSP_NOWIFI=1
set BSP_NOBT=1

:EXIT
