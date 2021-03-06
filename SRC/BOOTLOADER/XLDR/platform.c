// All rights reserved ADENEO EMBEDDED 2010
// Copyright (c) 2007, 2008 BSQUARE Corporation. All rights reserved.

/*
================================================================================
*             Texas Instruments OMAP(TM) Platform Software
* (c) Copyright Texas Instruments, Incorporated. All Rights Reserved.
*
* Use of this software is controlled by the terms and conditions found
* in the license agreement under which this software has been supplied.
*
================================================================================
*/
//
//  File:  startup.c
//
//  This file contains X-Loader startup code for OMAP35XX
//
#include "bsp.h"
#include "omap_32ksyncnt_regs.h"

#include "sdk_i2c.h"
#include "sdk_padcfg.h"
#include "oal_padcfg.h"
#include "oal_i2c.h"

#include "bsp_padcfg.h"
#include "bsp_tps659xx.h"
#include "omap_cpuver.h"

#if defined(FMD_ONENAND) && defined(FMD_NAND)
    #error FMD_ONENAND and FMD_NAND cannot both be defined.
#endif

extern unsigned int  gCPU_family;

//------------------------------------------------------------------------------
//  OPP mode related table

typedef struct CPU_OPP_SETTINGS
{
    UINT32 MPUSpeed;
    UINT32 IVASpeed;
    UINT32 VDD1Init;
    UINT32 VDD2Init;	
}CPU_OPP_Settings, *pCPU_OPP_Settings;

CPU_OPP_Settings OMAP35x_OPP_Table[OMAP35x_OPP_NUM]=
{
     // MPU[125Mhz @ 0.975V], IVA2[ 90Mhz @ 0.975V]
    {125, 90, 0x1e, 0x2c},
     // MPU[250Mhz @ 1.000V], IVA2[180Mhz @ 1.00V]    
    {250, 180, 0x20, 0x2c},
     // MPU[500Mhz @ 1.2000V], IVA2[360Mhz @ 1.20V]    
    {500, 360, 0x30, 0x2c},
     // MPU[550Mhz @ 1.2750V], IVA2[400Mhz @ 1.27V]   
    {550, 400, 0x36, 0x2c},
     // MPU[600Mhz @ 1.3500V], IVA2[430Mhz @ 1.35V]    
    {600, 430, 0x3c, 0x2c},
     // MPU[720hz @ 1.3500V], IVA2[520Mhz @ 1.35V]    
    {720, 520, 0x3c, 0x2c}
};

CPU_OPP_Settings OMAP37x_OPP_Table[OMAP37x_OPP_NUM]=
{
     // MPU[300Mhz  @ 0.9375V], IVA2[260Mhz @ 0.9375V]
    {300, 260, 0x1b, 0x2b},
     // MPU[600Mhz  @ 1.1000V], IVA2[520Mhz @ 1.1000V]		
    {600, 520, 0x28, 0x2b},
     // MPU[800Mhz  @ 1.2625V], IVA2[660Mhz @ 1.2625V]    
    {800, 660, 0x35, 0x2b},
     // MPU[1000Mhz @ 1.3125V], IVA2[875Mhz @ 1.3125V]    
    {1000, 800, 0x3E, 0x2b}
};


//------------------------------------------------------------------------------
//  DDR related table

typedef struct
{
    UINT32 mcfg0;
    UINT32 mcfg1;
    UINT32 actim_ctrla0;
    UINT32 actim_ctrlb0;
    UINT32 actim_ctrla1;
    UINT32 actim_ctrlb1;
    UINT32 rfr_ctrl0;
    UINT32 rfr_ctrl1;
}DDR_DEVICE_PARAM;

typedef enum
{
    DDR_TYPE_MICRON,
    DDR_TYPE_HYNIX,
    DDR_TYPE_MAX
}DDR_DEVICE_TYPE;

DDR_DEVICE_PARAM BSP_DDR_device_params[2]=
{
    /* MICRON DDR */
   {
       (UINT32)BSP_MICRON_SDRC_MCFG_0,
       (UINT32)BSP_MICRON_SDRC_MCFG_1,
       (UINT32)BSP_MICRON_SDRC_ACTIM_CTRLA_0,       
       (UINT32)BSP_MICRON_SDRC_ACTIM_CTRLB_0,       
       (UINT32)BSP_MICRON_SDRC_ACTIM_CTRLA_1,   
       (UINT32)BSP_MICRON_SDRC_ACTIM_CTRLB_1,       
       (UINT32)BSP_MICRON_SDRC_RFR_CTRL_0,
       (UINT32)BSP_MICRON_SDRC_RFR_CTRL_1,
   },
   
    /* HYNIX DDR */
   {
       (UINT32)BSP_HYNIX_SDRC_MCFG_0,
       (UINT32)BSP_HYNIX_SDRC_MCFG_1,
       (UINT32)BSP_HYNIX_SDRC_ACTIM_CTRLA_0,
       (UINT32)BSP_HYNIX_SDRC_ACTIM_CTRLB_0,       
       (UINT32)BSP_HYNIX_SDRC_ACTIM_CTRLA_1,
       (UINT32)BSP_HYNIX_SDRC_ACTIM_CTRLB_1, 
       (UINT32)BSP_HYNIX_SDRC_RFR_CTRL_0,       
       (UINT32)BSP_HYNIX_SDRC_RFR_CTRL_1,        
   }
};
//------------------------------------------------------------------------------
//  Function Prototypes
//
static UINT32 WaitOnValue(UINT32 readBitMask, UINT32 matchValue, volatile UINT32 *readAddr, UINT32 bound);
static UINT32 GetCpuRev();

static VOID WatchdogSetup();
static VOID PinMuxSetup();
static VOID ClockSetup(pCPU_OPP_Settings opp_setting);
static VOID GpioSetup();
VOID MemorySetup();
static VOID UpdateVoltageLevels(pCPU_OPP_Settings opp_setting);

//------------------------------------------------------------------------------
//
//  Function:  WaitOnValue
//
//  Common routine to allow waiting for changes in volatile regs.
//
UINT32 WaitOnValue(UINT32 readBitMask, UINT32 matchValue, volatile UINT32 *readAddr, UINT32 bound)
{
	UINT32 i = 0, val;
	for(;;)
	{
		i++;
		val = INREG32(readAddr) & readBitMask;
		if (val == matchValue)
			return 1;
		if (i == bound)
			return 0;
	} 
}


//------------------------------------------------------------------------------
//
//  Function:  OALGetTickCount()
//
//  Stub routine.
//
UINT32 OALGetTickCount()
{
    return 1;
}

//------------------------------------------------------------------------------
//
//  Function:  Mpu_dpll_init
//
//  Helper function to initialize OMAP 35xx/37xx/36xx MPU DPLL.
//
void Mpu_dpll_init(pCPU_OPP_Settings opp_setting)
{
    OMAP_PRCM_MPU_CM_REGS* pPrcmMpuCM = OALPAtoUA(OMAP_PRCM_MPU_CM_REGS_PA);
    unsigned int val, mpu_mult;

    // put mpu dpll in bypass
    val = INREG32(&pPrcmMpuCM->CM_CLKEN_PLL_MPU);
    val &= ~DPLL_MODE_MASK;
    val |= DPLL_MODE_LOWPOWER_BYPASS;
    OUTREG32(&pPrcmMpuCM->CM_CLKEN_PLL_MPU, val);
    while ((INREG32(&pPrcmMpuCM->CM_IDLEST_PLL_MPU) & DPLL_STATUS_MASK) != DPLL_STATUS_BYPASSED);

    // setup DPLL1 divider
    OUTREG32(&pPrcmMpuCM->CM_CLKSEL2_PLL_MPU, BSP_CM_CLKSEL2_PLL_MPU);
    
    // configure m:n clock ratios as well as frequency selection for mpu dpll
    mpu_mult = ((opp_setting->MPUSpeed / 2) << 8);
    val = BSP_MPU_CLK_SRC | mpu_mult | BSP_MPU_DPLL_DIV;
    OUTREG32(&pPrcmMpuCM->CM_CLKSEL1_PLL_MPU, val);

    // lock dpll with correct frequency selection
    OUTREG32(&pPrcmMpuCM->CM_CLKEN_PLL_MPU, BSP_CM_CLKEN_PLL_MPU);
    while ((INREG32(&pPrcmMpuCM->CM_IDLEST_PLL_MPU) & DPLL_STATUS_MASK) != DPLL_STATUS_LOCKED);
}    


//------------------------------------------------------------------------------
//
//  Function:  Iva_dpll_init
//
//  Helper function to initialize OMAP 35xx IVA DPLL.
//
void Iva_dpll_init(pCPU_OPP_Settings opp_setting)
{
    OMAP_PRCM_IVA2_CM_REGS* pPrcmIvaCM = OALPAtoUA(OMAP_PRCM_IVA2_CM_REGS_PA);
    unsigned int val, iva_mult;

    //---------------------------------
    // setup dpll timings for iva2 dpll
    //

    // put iva2 dpll in bypass
    val = INREG32(&pPrcmIvaCM->CM_CLKEN_PLL_IVA2);

    val &= ~DPLL_MODE_MASK;
    val |= DPLL_MODE_LOWPOWER_BYPASS;
    OUTREG32(&pPrcmIvaCM->CM_CLKEN_PLL_IVA2, val);
    while ((INREG32(&pPrcmIvaCM->CM_IDLEST_PLL_IVA2) & DPLL_STATUS_MASK) != DPLL_STATUS_BYPASSED);

    // setup DPLL1 divider
    OUTREG32(&pPrcmIvaCM->CM_CLKSEL2_PLL_IVA2, BSP_CM_CLKSEL2_PLL_IVA2);
    
    // configure m:n clock ratios as well as frequency selection for iva dpll
    iva_mult = ((opp_setting->IVASpeed/ 2) << 8);
    val = BSP_3530_IVA2_CLK_SRC | iva_mult | BSP_IVA2_DPLL_DIV;
    OUTREG32(&pPrcmIvaCM->CM_CLKSEL1_PLL_IVA2, val);

    // lock dpll with correct frequency selection
    if(gCPU_family == CPU_FAMILY_DM37XX)
    {
        //val = (BSP_EN_IVA2_DPLL_LPMODE | BSP_EN_IVA2_DPLL_DRIFTGUARD | BSP_EN_IVA2_DPLL);
        val = (BSP_EN_IVA2_DPLL_LPMODE | BSP_EN_IVA2_DPLL);
    }
    else
    {
        val = BSP_CM_CLKEN_PLL_IVA2;
    }
    OUTREG32(&pPrcmIvaCM->CM_CLKEN_PLL_IVA2, val);
    while ((INREG32(&pPrcmIvaCM->CM_IDLEST_PLL_IVA2) & DPLL_STATUS_MASK) != DPLL_STATUS_LOCKED);

}

//------------------------------------------------------------------------------
//
//  Function:  Omap37xx_dpll4_init
//
//  Helper function to initialize OMAP 37xx/36xx DPLL4 .
//
void Omap37xx_dpll4_init(void)
{
    OMAP_PRCM_EMU_CM_REGS* pPrcmEmuCM = OALPAtoUA(OMAP_PRCM_EMU_CM_REGS_PA);
    OMAP_PRCM_CAM_CM_REGS* pPrcmCamCM = OALPAtoUA(OMAP_PRCM_CAM_CM_REGS_PA);
    OMAP_PRCM_CLOCK_CONTROL_CM_REGS* pPrcmClkCM = OALPAtoUA(OMAP_PRCM_CLOCK_CONTROL_CM_REGS_PA);
    //OMAP_PRCM_SGX_CM_REGS* pPrcmSgxCM = OALPAtoUA(OMAP_PRCM_SGX_CM_REGS_PA);	
    unsigned int val;
    

    // configure timings for all related peripherals
    OUTREG32(&pPrcmEmuCM->CM_CLKSEL1_EMU, BSP_CM_CLKSEL1_EMU);
    OUTREG32(&pPrcmCamCM->CM_CLKSEL_CAM, BSP_CM_CLKSEL_CAM);
    OUTREG32(&pPrcmClkCM->CM_CLKSEL3_PLL, BSP_CM_CLKSEL3_PLL);
    //OUTREG32(&pPrcmSgxCM->CM_CLKSEL_SGX, BSP_CM_CLKSEL_SGX);
	
    /* Omap37xx using low jitter DPLL, the output freq is one half of OMAP 35xx DPLL4 */
    val = INREG32(&pPrcmClkCM->CM_CLKSEL2_PLL) & 0xfff00000;

    val |= (BSP_PERIPH_DPLL_MULT_37xx | BSP_PERIPH_DPLL_DIV);
    OUTREG32(&pPrcmClkCM->CM_CLKSEL2_PLL, val);
    
    // lock dpll with correct frequency selection
    val =   BSP_CM_CLKEN_PLL & (~(7 << 4));
 
    OUTREG32(&pPrcmClkCM->CM_CLKEN_PLL, val);
    while ((INREG32(&pPrcmClkCM->CM_IDLEST_CKGEN) & DPLL_STATUS_MASK) != DPLL_STATUS_LOCKED);

}

//------------------------------------------------------------------------------
//
//  Function:  Dpll4_init
//
//  Helper function to initialize OMAP 35xx DPLL4 .
//
void Dpll4_init(void)
{
    OMAP_PRCM_EMU_CM_REGS* pPrcmEmuCM = OALPAtoUA(OMAP_PRCM_EMU_CM_REGS_PA);
    OMAP_PRCM_CAM_CM_REGS* pPrcmCamCM = OALPAtoUA(OMAP_PRCM_CAM_CM_REGS_PA);
    OMAP_PRCM_CLOCK_CONTROL_CM_REGS* pPrcmClkCM = OALPAtoUA(OMAP_PRCM_CLOCK_CONTROL_CM_REGS_PA);

    // configure timings for all related peripherals
    OUTREG32(&pPrcmEmuCM->CM_CLKSEL1_EMU, BSP_CM_CLKSEL1_EMU);
    OUTREG32(&pPrcmCamCM->CM_CLKSEL_CAM, BSP_CM_CLKSEL_CAM);
    OUTREG32(&pPrcmClkCM->CM_CLKSEL3_PLL, BSP_CM_CLKSEL3_PLL);
    OUTREG32(&pPrcmClkCM->CM_CLKSEL2_PLL, (INREG32(&pPrcmClkCM->CM_CLKSEL2_PLL) & 0xfff00000) | BSP_CM_CLKSEL2_PLL);

    // lock dpll with correct frequency selection
    OUTREG32(&pPrcmClkCM->CM_CLKEN_PLL, BSP_CM_CLKEN_PLL);
    while ((INREG32(&pPrcmClkCM->CM_IDLEST_CKGEN) & DPLL_STATUS_MASK) != DPLL_STATUS_LOCKED);

}

//------------------------------------------------------------------------------
//
//  Function:  Omap37xx_per_dpll_init
//
//  Helper function to initialize OMAP 37xx/36xx Core DPLL.
//
void Omap37xx_core_dpll_init(void)
{
    OMAP_PRCM_CORE_CM_REGS* pPrcmCoreCM = OALPAtoUA(OMAP_PRCM_CORE_CM_REGS_PA);
    OMAP_PRCM_CLOCK_CONTROL_CM_REGS* pPrcmClkCM = OALPAtoUA(OMAP_PRCM_CLOCK_CONTROL_CM_REGS_PA);

    //---------------------------------
    // setup dpll timings for core and peripheral dpll
    //
    
    // configure clock ratios for L3, L4, FSHOSTUSB, SSI
    // configure clock selection for gpt10, gpt11
    OUTREG32(&pPrcmCoreCM->CM_CLKSEL_CORE, BSP_CM_CLKSEL_CORE);
    //OUTREG32(&pPrcmCoreCM->CM_FCLKEN1_CORE, 0x1 <<13);

    // configure m:n clock ratios as well as frequency selection for core dpll
    OUTREG32(&pPrcmClkCM->CM_CLKSEL1_PLL, (BSP_CORE_DPLL_CLKOUT_DIV | CORE_DPLL_MULT_400 | \
                                                                        CORE_DPLL_DIV_400 | BSP_SOURCE_96M));

}

//------------------------------------------------------------------------------
//
//  Function:  Core_dpll_init
//
//  Helper function to initialize OMAP 35xx Core DPLL.
//
void Core_dpll_init(void)
{
    OMAP_PRCM_CORE_CM_REGS* pPrcmCoreCM = OALPAtoUA(OMAP_PRCM_CORE_CM_REGS_PA);
    OMAP_PRCM_CLOCK_CONTROL_CM_REGS* pPrcmClkCM = OALPAtoUA(OMAP_PRCM_CLOCK_CONTROL_CM_REGS_PA);

    //---------------------------------
    // setup dpll timings for core and peripheral dpll
    //
    
    // configure clock ratios for L3, L4, FSHOSTUSB, SSI
    // configure clock selection for gpt10, gpt11
    OUTREG32(&pPrcmCoreCM->CM_CLKSEL_CORE, BSP_CM_CLKSEL_CORE);
    
    // configure m:n clock ratios as well as frequency selection for core dpll
    OUTREG32(&pPrcmClkCM->CM_CLKSEL1_PLL, BSP_CM_CLKSEL1_PLL);

}
//------------------------------------------------------------------------------
//
//  Function:  Per_dpll_init
//
//  Helper function to initialize OMAP 35xx Peripheral DPLL.
//
void Per_dpll_init(void)
{

    OMAP_PRCM_CLOCK_CONTROL_CM_REGS* pPrcmClkCM = OALPAtoUA(OMAP_PRCM_CLOCK_CONTROL_CM_REGS_PA);

    //---------------------------------
    // setup dpll timings for core and peripheral dpll
    //
    
    // configure clock ratios for 120m
    OUTREG32(&pPrcmClkCM->CM_CLKSEL5_PLL, BSP_CM_CLKSEL5_PLL);
    
    // configure m:n clock ratios as well as frequency selection for core dpll
    OUTREG32(&pPrcmClkCM->CM_CLKSEL4_PLL, BSP_CM_CLKSEL4_PLL);

    // lock dpll with correct frequency selection
    OUTREG32(&pPrcmClkCM->CM_CLKEN2_PLL, BSP_CM_CLKEN2_PLL);
    while ((INREG32(&pPrcmClkCM->CM_IDLEST2_CKGEN) & DPLL_STATUS_MASK) != DPLL_STATUS_LOCKED);
    

}

//------------------------------------------------------------------------------
//
//  Function:  Omap37xx_per_dpll_init
//
//  Helper function to initialize OMAP 37xx/36xx Peripheral DPLL.
//
void Omap37xx_per_dpll_init(void)
{

    OMAP_PRCM_CLOCK_CONTROL_CM_REGS* pPrcmClkCM = OALPAtoUA(OMAP_PRCM_CLOCK_CONTROL_CM_REGS_PA);
    unsigned int val;

    //---------------------------------
    // setup dpll timings for core and peripheral dpll
    //
    
    // configure clock ratios for 120m
    OUTREG32(&pPrcmClkCM->CM_CLKSEL5_PLL, BSP_CM_CLKSEL5_PLL);
    
    // configure m:n clock ratios as well as frequency selection for core dpll
    OUTREG32(&pPrcmClkCM->CM_CLKSEL4_PLL, BSP_CM_CLKSEL4_PLL);

    // lock dpll with correct frequency selection
/*    val = (BSP_EN_PERIPH2_DPLL_LPMODE |  BSP_PERIPH2_DPLL_RAMPTIME |       \
             BSP_EN_PERIPH2_DPLL_DRIFTGUARD |  BSP_EN_PERIPH2_DPLL);*/
    val = (BSP_EN_PERIPH2_DPLL_LPMODE |  BSP_PERIPH2_DPLL_RAMPTIME |       \
           BSP_EN_PERIPH2_DPLL);
                 
             
    OUTREG32(&pPrcmClkCM->CM_CLKEN2_PLL, val);
    while ((INREG32(&pPrcmClkCM->CM_IDLEST2_CKGEN) & DPLL_STATUS_MASK) != DPLL_STATUS_LOCKED);
}

//------------------------------------------------------------------------------
//
//  Function:  PlatformSetup
//
//  Initializes platform settings.  Stack based initialization only - no 
//  global variables allowed.
//
VOID PlatformSetup()
{
    pCPU_OPP_Settings opp_setting;        
	
    //---------------------------------
    // setup dpll timings for mpu dpll
    //
    //if(gCPU_family == CPU_FAMILY_DM37XX)
    {
        opp_setting = &OMAP37x_OPP_Table[BSP_OPM_SELECT_37XX-1];        
    }
	/*
    else if (gCPU_family == CPU_FAMILY_OMAP35XX)
    {
        opp_setting = &OMAP35x_OPP_Table[BSP_OPM_SELECT_35XX-1];        
    }
    else 
    {
        opp_setting = &OMAP35x_OPP_Table[BSP_OPM_SELECT_35XX-1];        
    }
	*/
	
    // Initialize the platform
    WatchdogSetup();
    PinMuxSetup();    
	
    ClockSetup(opp_setting); 

    GpioSetup();

    // configure i2c devices
    OALI2CInit(OMAP_DEVICE_I2C1);
    //OALI2CInit(OMAP_DEVICE_I2C2);
    //OALI2CInit(OMAP_DEVICE_I2C3);

    OALI2CPostInit();	
	
    InitTwlPower();

    UpdateVoltageLevels(opp_setting);

    MemorySetup();
}

//------------------------------------------------------------------------------
//
//  Function:  WatchdogSetup
//
//  Initializes watchdog timer settings.
//
static VOID WatchdogSetup()
{
	/* There are 3 watch dogs WD1=Secure, WD2=MPU, WD3=IVA. WD1 is
	either taken care of by ROM (HS/EMU) or not accessible (GP).
	We need to take care of WD2-MPU or take a PRCM reset. WD3
	should not be running and does not generate a PRCM reset. */

	OMAP_PRCM_WKUP_CM_REGS *pPrcmWkupCM = OALPAtoUA(OMAP_PRCM_WKUP_CM_REGS_PA);
	OMAP_WDOG_REGS *pWdogTimer = OALPAtoUA(OMAP_WDOG2_REGS_PA);

	SETREG32(&pPrcmWkupCM->CM_FCLKEN_WKUP, CM_CLKEN_WDT2);
	SETREG32(&pPrcmWkupCM->CM_ICLKEN_WKUP, CM_CLKEN_WDT2);

	WaitOnValue(CM_IDLEST_ST_WDT2, CM_IDLEST_ST_WDT2, &pPrcmWkupCM->CM_IDLEST_WKUP, 5); // Some issue here

	OUTREG32(&pWdogTimer->WSPR, WDOG_DISABLE_SEQ1);
	while (INREG32(&pWdogTimer->WWPS));
	OUTREG32(&pWdogTimer->WSPR, WDOG_DISABLE_SEQ2);
}

//------------------------------------------------------------------------------
//
//  Function:  PinMuxSetup
//
//  Initializes pin/pad mux settings.
//
static VOID PinMuxSetup()
{
    static const PAD_INFO initialPinMux[] = {
            SDRC_PADS            
            GPMC_PADS
            UART3_PADS
            MMC1_PADS
            I2C1_PADS
            I2C2_PADS
            I2C3_PADS
			MCSPI2_PADS
            WKUP_PAD_ENTRY(SYS_32K, INPUT_ENABLED | PULL_RESISTOR_DISABLED | MUXMODE(0))
            GPIO_PADS       
            END_OF_PAD_ARRAY
    };

    ConfigurePadArray(initialPinMux);
}

//------------------------------------------------------------------------------
//
//  Function:  GpioSetup
//
//  Initializes GPIO pin direction/state.  
//
VOID GpioSetup()
{
    OMAP_GPIO_REGS* pGpio;
       
    // Initialize state/direction for all pins configured as gpio
    // Bank 1 GPIO 0..31
    pGpio = OALPAtoUA(OMAP_GPIO1_REGS_PA);

    // Set GPIO7 as output
    OUTREG32(&pGpio->OE, ~(1 << 7));
}

//------------------------------------------------------------------------------
//
//  Function:  ClockSetup
//
//  Initializes clocks and power. Stack based initialization only - no 
//  global variables allowed.
//
void ClockSetup(pCPU_OPP_Settings opp_setting)
{    
    
    OMAP_PRCM_CLOCK_CONTROL_PRM_REGS* pPrcmClkPRM = OALPAtoUA(OMAP_PRCM_CLOCK_CONTROL_PRM_REGS_PA);
    OMAP_PRCM_WKUP_CM_REGS* pPrcmWkupCM = OALPAtoUA(OMAP_PRCM_WKUP_CM_REGS_PA);
    OMAP_PRCM_PER_CM_REGS* pPrcmPerCM = OALPAtoUA(OMAP_PRCM_PER_CM_REGS_PA);
    OMAP_PRCM_CORE_CM_REGS* pPrcmCoreCM = OALPAtoUA(OMAP_PRCM_CORE_CM_REGS_PA);
    
    // setup input system clock
    OUTREG32(&pPrcmClkPRM->PRM_CLKSEL, BSP_PRM_CLKSEL);

    //if(gCPU_family == CPU_FAMILY_DM37XX)
    {
        Omap37xx_core_dpll_init();
        Omap37xx_dpll4_init();        
        Mpu_dpll_init(opp_setting);
        Iva_dpll_init(opp_setting);   
        Omap37xx_per_dpll_init();
    }
	/*
    else
    {
        Core_dpll_init();
        Dpll4_init();
        Mpu_dpll_init(opp_setting);
        Iva_dpll_init(opp_setting);    
        Per_dpll_init();
    }
	*/
    
    //--------------------------
    // Enable GPTIMER1, GPIO bank 1 (debug led)
    SETREG32(&pPrcmWkupCM->CM_FCLKEN_WKUP, (CM_CLKEN_GPT1|CM_CLKEN_GPIO1));
    SETREG32(&pPrcmWkupCM->CM_ICLKEN_WKUP, (CM_CLKEN_GPT1|CM_CLKEN_GPIO1));

    // Enable UART3 (debug port) and GPIO banks that are accessed in the bootloader
    SETREG32(&pPrcmPerCM->CM_FCLKEN_PER, (CM_CLKEN_UART3|CM_CLKEN_GPIO6|CM_CLKEN_GPIO5|CM_CLKEN_GPIO3));
    SETREG32(&pPrcmPerCM->CM_ICLKEN_PER, (CM_CLKEN_UART3|CM_CLKEN_GPIO6|CM_CLKEN_GPIO5|CM_CLKEN_GPIO3));

    // Disable HS USB OTG interface clock
    CLRREG32(&pPrcmCoreCM->CM_ICLKEN1_CORE, CM_CLKEN_HSOTGUSB);

    // Disable D2D interface clock
    CLRREG32(&pPrcmCoreCM->CM_ICLKEN1_CORE, CM_CLKEN_D2D);

}

//------------------------------------------------------------------------------
//
//  Function:  MemorySetup
//
//  Initializes memory interfaces.
//
#define SDRC_SYSCONFIG_SOFTWARE_RESET       (1 << 1)
extern DDR_DEVICE_TYPE ddr_type;
VOID MemorySetup()
{
    OMAP_GPMC_REGS* pGpmc = OALPAtoUA(OMAP_GPMC_REGS_PA);
    OMAP_SDRC_REGS* pSdrc = OALPAtoUA(OMAP_SDRC_REGS_PA);
    OMAP_PRCM_GLOBAL_PRM_REGS * pPrmGlobal = OALPAtoUA(OMAP_PRCM_GLOBAL_PRM_REGS_PA);
    OMAP_SYSC_PADCONFS_REGS *pConfig = OALPAtoUA(OMAP_SYSC_PADCONFS_REGS_PA);
    //DDR_DEVICE_TYPE ddr_type = DDR_TYPE_MICRON;	
    DDR_DEVICE_PARAM  *pDDR_param;
    unsigned int val = 0;
    BOOL bColdBoot;

    //  Global GPMC Configuration
    OUTREG32(&pGpmc->GPMC_SYSCONFIG,       0x00000008);   // No idle, L3 clock free running      
    OUTREG32(&pGpmc->GPMC_IRQENABLE,       0x00000000);   // All interrupts disabled    
    OUTREG32(&pGpmc->GPMC_TIMEOUT_CONTROL, 0x00000000);   // Time out disabled    
    OUTREG32(&pGpmc->GPMC_CONFIG,          0x00000011);   // WP high, force posted write for NAND    

#ifdef FMD_ONENAND
    // Configure CS0 for OneNAND,  Base Address 0x0C000000
    OUTREG32(&pGpmc->GPMC_CONFIG1_0, BSP_GPMC_ONENAND_CONFIG1);
    OUTREG32(&pGpmc->GPMC_CONFIG2_0, BSP_GPMC_ONENAND_CONFIG2);
    OUTREG32(&pGpmc->GPMC_CONFIG3_0, BSP_GPMC_ONENAND_CONFIG3);
    OUTREG32(&pGpmc->GPMC_CONFIG4_0, BSP_GPMC_ONENAND_CONFIG4);
    OUTREG32(&pGpmc->GPMC_CONFIG5_0, BSP_GPMC_ONENAND_CONFIG5);
    OUTREG32(&pGpmc->GPMC_CONFIG6_0, BSP_GPMC_ONENAND_CONFIG6);
    OUTREG32(&pGpmc->GPMC_CONFIG7_0, BSP_GPMC_ONENAND_CONFIG7);
#endif
    //if(gCPU_family == CPU_FAMILY_DM37XX)
    {
//#ifdef FMD_NAND
        // Configure CS0 for NAND,  Base Address 0x08000000
        OUTREG32(&pGpmc->GPMC_CONFIG1_0, BSP_GPMC_NAND_CONFIG1_200);
        OUTREG32(&pGpmc->GPMC_CONFIG2_0, BSP_GPMC_NAND_CONFIG2_200);
        OUTREG32(&pGpmc->GPMC_CONFIG3_0, BSP_GPMC_NAND_CONFIG3_200);
        OUTREG32(&pGpmc->GPMC_CONFIG4_0, BSP_GPMC_NAND_CONFIG4_200);
        OUTREG32(&pGpmc->GPMC_CONFIG5_0, BSP_GPMC_NAND_CONFIG5_200);
        OUTREG32(&pGpmc->GPMC_CONFIG6_0, BSP_GPMC_NAND_CONFIG6_200);
        OUTREG32(&pGpmc->GPMC_CONFIG7_0, BSP_GPMC_NAND_CONFIG7);
//#endif
	    //Configure CS4 for MCX314
	    OUTREG32(&pGpmc->GPMC_CONFIG1_4, BSP_GPMC_MCX312_CONFIG1);
	    OUTREG32(&pGpmc->GPMC_CONFIG2_4, BSP_GPMC_MCX312_CONFIG2);
	    OUTREG32(&pGpmc->GPMC_CONFIG3_4, BSP_GPMC_MCX312_CONFIG3);
	    OUTREG32(&pGpmc->GPMC_CONFIG4_4, BSP_GPMC_MCX312_CONFIG4);
	    OUTREG32(&pGpmc->GPMC_CONFIG5_4, BSP_GPMC_MCX312_CONFIG5);
	    OUTREG32(&pGpmc->GPMC_CONFIG6_4, BSP_GPMC_MCX312_CONFIG6);
	    OUTREG32(&pGpmc->GPMC_CONFIG7_4, BSP_GPMC_MCX312_CONFIG7);
        // Configure CS6 for LAN,  Base Address 0x15000000
        OUTREG32(&pGpmc->GPMC_CONFIG1_6, BSP_GPMC_LAN_CONFIG1_200);
        OUTREG32(&pGpmc->GPMC_CONFIG2_6, BSP_GPMC_LAN_CONFIG2_200);
        OUTREG32(&pGpmc->GPMC_CONFIG3_6, BSP_GPMC_LAN_CONFIG3_200);
        OUTREG32(&pGpmc->GPMC_CONFIG4_6, BSP_GPMC_LAN_CONFIG4_200); 
        OUTREG32(&pGpmc->GPMC_CONFIG5_6, BSP_GPMC_LAN_CONFIG5_200); 
        OUTREG32(&pGpmc->GPMC_CONFIG6_6, BSP_GPMC_LAN_CONFIG6_200); 
        OUTREG32(&pGpmc->GPMC_CONFIG7_6, BSP_GPMC_LAN_CONFIG7); 

        //ddr_type = DDR_TYPE_HYNIX;
		//ddr_type = DDR_TYPE_MICRON;	
    }
	/*
    else if (gCPU_family == CPU_FAMILY_OMAP35XX)
    {
#ifdef FMD_NAND
        // Configure CS0 for NAND,  Base Address 0x08000000
        OUTREG32(&pGpmc->GPMC_CONFIG1_0, BSP_GPMC_NAND_CONFIG1_166);
        OUTREG32(&pGpmc->GPMC_CONFIG2_0, BSP_GPMC_NAND_CONFIG2_166);
        OUTREG32(&pGpmc->GPMC_CONFIG3_0, BSP_GPMC_NAND_CONFIG3_166);
        OUTREG32(&pGpmc->GPMC_CONFIG4_0, BSP_GPMC_NAND_CONFIG4_166);
        OUTREG32(&pGpmc->GPMC_CONFIG5_0, BSP_GPMC_NAND_CONFIG5_166);
        OUTREG32(&pGpmc->GPMC_CONFIG6_0, BSP_GPMC_NAND_CONFIG6_166);
        OUTREG32(&pGpmc->GPMC_CONFIG7_0, BSP_GPMC_NAND_CONFIG7);
#endif
    
        // Configure CS5 for LAN,  Base Address 0x15000000
        OUTREG32(&pGpmc->GPMC_CONFIG1_5, BSP_GPMC_LAN_CONFIG1_166);
        OUTREG32(&pGpmc->GPMC_CONFIG2_5, BSP_GPMC_LAN_CONFIG2_166);
        OUTREG32(&pGpmc->GPMC_CONFIG3_5, BSP_GPMC_LAN_CONFIG3_166);
        OUTREG32(&pGpmc->GPMC_CONFIG4_5, BSP_GPMC_LAN_CONFIG4_166); 
        OUTREG32(&pGpmc->GPMC_CONFIG5_5, BSP_GPMC_LAN_CONFIG5_166); 
        OUTREG32(&pGpmc->GPMC_CONFIG6_5, BSP_GPMC_LAN_CONFIG6_166); 
        OUTREG32(&pGpmc->GPMC_CONFIG7_5, BSP_GPMC_LAN_CONFIG7); 

        ddr_type = DDR_TYPE_MICRON;		

    }
	*/
    //else
    //{
        /* Not supported CPU family , use default ddr_type */
    //}
    // check global reset status
    val = INREG32(&pPrmGlobal->PRM_RSTST);

    if (val & (GLOBAL_SW_RST | EXTERNAL_WARM_RST))
        bColdBoot = FALSE;
    else
        bColdBoot = TRUE;
    
    // read config register
    INREG32(&pSdrc->SDRC_SYSCONFIG);
    pDDR_param = &BSP_DDR_device_params[ddr_type];		

    if (bColdBoot == FALSE)
    {
	 //After warm reset, the SDRC will be unreliable and this will cause eboot image can't be copied from NAND flash to SDRAM correctly.
        //Therefore, we have to force SDRC to reset after warm reset to solve this issue.
        
         //SDRC reset by software
        OUTREG32(&pSdrc->SDRC_SYSCONFIG, SDRC_SYSCONFIG_SOFTWARE_RESET);
        // wait for at least 1000us
        OALStall(1000);

        //After SDRC reset, we do below steps to configure SDRC regisger again.
    }
	
    //if (bColdBoot == TRUE)
    {
        // Disable SDRC power saving mode
        CLRREG32(&pSdrc->SDRC_POWER, SDRC_POWER_PWDENA);

        // update memory cofiguration
        OUTREG32(&pSdrc->SDRC_MCFG_0, pDDR_param->mcfg0);
        OUTREG32(&pSdrc->SDRC_MCFG_1, pDDR_param->mcfg1);
        OUTREG32(&pSdrc->SDRC_SHARING, BSP_SDRC_SHARING);

        // wait for at least 200us
        OALStall(2000);

        // set autorefresh
        OUTREG32(&pSdrc->SDRC_RFR_CTRL_0, pDDR_param->rfr_ctrl0);
        OUTREG32(&pSdrc->SDRC_RFR_CTRL_1, pDDR_param->rfr_ctrl1);

        // setup ac timings
        OUTREG32(&pSdrc->SDRC_ACTIM_CTRLA_0, pDDR_param->actim_ctrla0);
        OUTREG32(&pSdrc->SDRC_ACTIM_CTRLA_1, pDDR_param->actim_ctrla1);
        OUTREG32(&pSdrc->SDRC_ACTIM_CTRLB_0, pDDR_param->actim_ctrlb0);
        OUTREG32(&pSdrc->SDRC_ACTIM_CTRLB_1, pDDR_param->actim_ctrlb1);    

        // manual command sequence to start bank 0
        OUTREG32(&pSdrc->SDRC_MANUAL_0, 0);
        // wait for at least 200us
        OALStall(2000);
        OUTREG32(&pSdrc->SDRC_MANUAL_0, 1);
        OUTREG32(&pSdrc->SDRC_MANUAL_0, 2);
        OUTREG32(&pSdrc->SDRC_MANUAL_0, 2);
        OUTREG32(&pSdrc->SDRC_MR_0, BSP_SDRC_MR_0);
        
        #if BSP_MICRON_RAMSIZE_1
            // manual command sequence to start bank 1
            OUTREG32(&pSdrc->SDRC_MANUAL_1, 0);
            // wait for at least 200us
            OALStall(2000);
            OUTREG32(&pSdrc->SDRC_MANUAL_1, 1);
            OUTREG32(&pSdrc->SDRC_MANUAL_1, 2);
            OUTREG32(&pSdrc->SDRC_MANUAL_1, 2);
            OUTREG32(&pSdrc->SDRC_MR_1, BSP_SDRC_MR_1);
        #endif

		if(ddr_type == DDR_TYPE_HYNIX)
			OUTREG32(&pSdrc->SDRC_CS_CFG, 4);
		else
			OUTREG32(&pSdrc->SDRC_CS_CFG, 1); //CS1 start from 0x88000000 for micron

        // re-enable power saving mode
        SETREG32(&pSdrc->SDRC_POWER, SDRC_POWER_PWDENA | SDRC_POWER_SRFRONIDLEREQ);

        // update sdrc dll timings
        OUTREG32(&pSdrc->SDRC_DLLA_CTRL, BSP_SDRC_DLLA_CTRL);
        OUTREG32(&pSdrc->SDRC_DLLB_CTRL, BSP_SDRC_DLLB_CTRL);
        
        // update sdram characteristics
        OUTREG32(&pSdrc->SDRC_EMR2_0, BSP_SDRC_EMR2_0);
        OUTREG32(&pSdrc->SDRC_EMR2_1, BSP_SDRC_EMR2_1);
    }

    SETREG32(&pSdrc->SDRC_POWER, SDRC_POWER_SRFRONRESET);

    // allow SDRC to settle
    OALStall(100);

    // release the force on the clke signals
    OUTREG16(&pConfig->CONTROL_PADCONF_SDRC_CKE0, (INPUT_DISABLE | PULL_INACTIVE | MUX_MODE_0));
    OUTREG16(&pConfig->CONTROL_PADCONF_SDRC_CKE1, (INPUT_DISABLE | PULL_INACTIVE | MUX_MODE_0)); 
}


//------------------------------------------------------------------------------
//
//  Function:  UpdateVoltageLevels
//
//  update voltage levels
//
void UpdateVoltageLevels(pCPU_OPP_Settings opp_setting)
{   
    OMAP_PRCM_GLOBAL_PRM_REGS* pPrcmGblPRM = OALPAtoUA(OMAP_PRCM_GLOBAL_PRM_REGS_PA);

    //---------------------------------
    // setup voltage processors
    //

    // setup i2c for smps communication
    OUTREG32(&pPrcmGblPRM->PRM_VC_SMPS_SA, BSP_VC_SMPS_SA_INIT);
    OUTREG32(&pPrcmGblPRM->PRM_VC_SMPS_VOL_RA, BSP_VC_SMPS_VOL_RA_INIT);
    OUTREG32(&pPrcmGblPRM->PRM_VC_SMPS_CMD_RA, BSP_VC_SMPS_CMD_RA_INIT);
    OUTREG32(&pPrcmGblPRM->PRM_VC_CH_CONF, BSP_VC_CH_CONF_INIT);  
    OUTREG32(&pPrcmGblPRM->PRM_VC_I2C_CFG, BSP_PRM_VC_I2C_CFG_INIT);

    // set intial voltage levels
    OUTREG32(&pPrcmGblPRM->PRM_VC_CMD_VAL_0, (opp_setting->VDD1Init << 24) | BSP_PRM_VC_CMD_VAL_0_INIT );
    OUTREG32(&pPrcmGblPRM->PRM_VC_CMD_VAL_1, (opp_setting->VDD2Init << 24) | BSP_PRM_VC_CMD_VAL_1_INIT);

    // set PowerIC error offset, gains, and initial voltage
    OUTREG32(&pPrcmGblPRM->PRM_VP1_CONFIG, (opp_setting->VDD1Init << 8) | BSP_PRM_VP1_CONFIG_INIT);
    OUTREG32(&pPrcmGblPRM->PRM_VP2_CONFIG, (opp_setting->VDD2Init << 8) | BSP_PRM_VP2_CONFIG_INIT);

    // set PowerIC slew range
    OUTREG32(&pPrcmGblPRM->PRM_VP1_VSTEPMIN, BSP_PRM_VP1_VSTEPMIN_INIT);
    OUTREG32(&pPrcmGblPRM->PRM_VP1_VSTEPMAX, BSP_PRM_VP1_VSTEPMAX_INIT);
    OUTREG32(&pPrcmGblPRM->PRM_VP2_VSTEPMIN, BSP_PRM_VP2_VSTEPMIN_INIT);
    OUTREG32(&pPrcmGblPRM->PRM_VP2_VSTEPMAX, BSP_PRM_VP2_VSTEPMAX_INIT);

    // set PowerIC voltage limits and timeout
    OUTREG32(&pPrcmGblPRM->PRM_VP1_VLIMITTO, BSP_PRM_VP1_VLIMITTO_INIT);
    OUTREG32(&pPrcmGblPRM->PRM_VP2_VLIMITTO, BSP_PRM_VP2_VLIMITTO_INIT);
    
    // enable voltage processor
    SETREG32(&pPrcmGblPRM->PRM_VP1_CONFIG, SMPS_VPENABLE);
    SETREG32(&pPrcmGblPRM->PRM_VP2_CONFIG, SMPS_VPENABLE);

    // enable timeout
    SETREG32(&pPrcmGblPRM->PRM_VP1_CONFIG, SMPS_TIMEOUTEN);
    SETREG32(&pPrcmGblPRM->PRM_VP2_CONFIG, SMPS_TIMEOUTEN);    

    // flush commands to smps
    SETREG32(&pPrcmGblPRM->PRM_VP1_CONFIG, SMPS_FORCEUPDATE | SMPS_INITVDD);
    SETREG32(&pPrcmGblPRM->PRM_VP2_CONFIG, SMPS_FORCEUPDATE | SMPS_INITVDD);

    // allow voltage to settle
    OALStall(100);

    // disable voltage processor
    CLRREG32(&pPrcmGblPRM->PRM_VP1_CONFIG, SMPS_VPENABLE | SMPS_FORCEUPDATE | SMPS_INITVDD | SMPS_TIMEOUTEN);
    CLRREG32(&pPrcmGblPRM->PRM_VP2_CONFIG, SMPS_VPENABLE | SMPS_FORCEUPDATE | SMPS_INITVDD | SMPS_TIMEOUTEN);
}
