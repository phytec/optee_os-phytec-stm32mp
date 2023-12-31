/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2017-2022, STMicroelectronics
 */

#ifndef PLATFORM_CONFIG_H
#define PLATFORM_CONFIG_H

#include <mm/generic_ram_layout.h>

/* Make stacks aligned to data cache line length */
#define STACK_ALIGNMENT			32

#if defined(CFG_WITH_PAGER)
#if defined(CFG_WITH_LPAE)
/*
 * Optimize unpaged memory size:
 * - one table for the level2 table for overall vmem range
 * - two tables for TEE RAM fine grain mapping [2ffc.0000 301f.ffff]
 * - one table for a 2MByte dynamic shared virtual memory (SHM_VASPACE)
 */
#define MAX_XLAT_TABLES			4
#else
/*
 * Optimize unpaged memory size:
 * - two tables for TEE RAM mapping [2ffc.0000 300f.ffff]
 * - one table for secure internal RAMs (PM: ROMed core TEE RAM)
 * - one table for non-secure internal RAMs (PM: DDR first page)
 * - two tables for a 2MByte dynamiq shared virtual memory (SHM_VASPACE)
 */
#define MAX_XLAT_TABLES			6
#endif /*CFG_WITH_LPAE*/
#else
/* Be generous with this setup that has plenty of secure RAM */
#define MAX_XLAT_TABLES			10
#endif /*CFG_WITH_PAGER*/

/* SoC interface registers base address ranges */
#define APB1_BASE			0x40000000
#define APB1_SIZE			0x0001d000
#define APB2_BASE			0x44000000
#define APB2_SIZE			0x00014000
#define APB3_BASE			0x50020000
#define APB3_SIZE			0x0000b000
#define APB4_BASE			0x5a000000
#define APB4_SIZE			0x00008000
#define APB5_BASE			0x5c000000
#define APB5_SIZE			0x0000b000
#ifdef CFG_STM32MP13
#define APB6_BASE			0x4c000000
#define APB6_SIZE			0x0000d000
#endif

#define AHB2_BASE			0x48000000
#define AHB2_SIZE			0x01040000
#define AHB4_BASE			0x50000000
#define AHB4_SIZE			0x00020000
#ifdef CFG_STM32MP13
#define AHB5_BASE			0x54000000
#define AHB5_SIZE			0x00008000
#else
#define AHB5_BASE			0x54000000
#define AHB5_SIZE			0x00005000
#endif


/* SoC interface registers base address */
#define MCUSRAM_BASE			0x30000000ul
#define RETRAM_BASE			0x38000000ul
#define BKPSRAM_BASE			0x54000000
#define BSEC_BASE			0x5c005000
#define ETZPC_BASE			0x5c007000
#define CRYP1_BASE			0x54001000
#define DBGMCU_BASE			0x50081000
#define DDR_BASE			0xc0000000ul
#define DDRCTRL_BASE			0x5a003000
#define DDRPHYC_BASE			0x5a004000
#define GIC_BASE			0xa0021000ul
#define GPIOA_BASE			0x50002000
#define GPIOB_BASE			0x50003000
#define GPIOC_BASE			0x50004000
#define GPIOD_BASE			0x50005000
#define GPIOE_BASE			0x50006000
#define GPIOF_BASE			0x50007000
#define GPIOG_BASE			0x50008000
#define GPIOH_BASE			0x50009000
#define GPIOI_BASE			0x5000a000

#ifdef CFG_STM32MP13
#define HASH1_BASE			0x54003000
#define RNG1_BASE			0x54004000
#define I2C3_BASE			0x4C004000
#define I2C4_BASE			0x4C005000
#define I2C5_BASE			0x4C006000
#endif

#ifdef CFG_STM32MP15
#define GPIOJ_BASE			0x5000b000
#define GPIOK_BASE			0x5000c000
#define GPIOZ_BASE			0x54004000
#define HASH1_BASE			0x54002000
#define RNG1_BASE			0x54003000
#define I2C4_BASE			0x5c002000
#define I2C6_BASE			0x5c009000
#endif

#define IWDG1_BASE			0x5c003000
#define IWDG2_BASE			0x5a002000
#define PWR_BASE			0x50001000
#define RCC_BASE			0x50000000
#define RTC_BASE			0x5c004000
#ifdef CFG_STM32MP15
#define SPI6_BASE			0x5c001000
#endif
#define STGEN_BASE			0x5c008000
#define SYSCFG_BASE			0x50020000
#ifdef CFG_STM32MP13
#define SYSRAM_BASE			0x2ffe0000
#endif
#ifdef CFG_STM32MP15
#define SYSRAM_BASE			0x2ffc0000
#endif
#define TAMP_BASE			0x5c00a000
#define TIM1_BASE			0x44000000
#define TIM2_BASE			0x40000000
#define TIM3_BASE			0x40001000
#define TIM4_BASE			0x40002000
#define TIM5_BASE			0x40003000
#define TIM6_BASE			0x40004000
#define TIM7_BASE			0x40005000
#define TIM8_BASE			0x44001000
#ifdef CFG_STM32MP13
#define LPTIM2_BASE			0x50021000
#define LPTIM3_BASE			0x50022000
#define TIM12_BASE			0x4C007000
#define TIM13_BASE			0x4C008000
#define TIM14_BASE			0x4C009000
#define TIM15_BASE			0x4C00A000
#define TIM16_BASE			0x4C00B000
#define TIM17_BASE			0x4C00C000
#endif
#ifdef CFG_STM32MP15
#define TIM12_BASE			0x40006000
#define TIM13_BASE			0x40007000
#define TIM14_BASE			0x40008000
#define TIM15_BASE			0x44006000
#define TIM16_BASE			0x44007000
#define TIM17_BASE			0x44008000
#endif
#define TZC_BASE			0x5c006000
#define UART1_BASE			0x5c000000
#ifdef CFG_STM32MP13
#define UART2_BASE			0x4c001000
#endif
#ifdef CFG_STM32MP15
#define UART2_BASE			0x4000e000
#endif
#define UART3_BASE			0x4000f000
#define UART4_BASE			0x40010000
#define UART5_BASE			0x40011000
#define UART6_BASE			0x44003000
#define UART7_BASE			0x40018000
#define UART8_BASE			0x40019000

#define VREFBUF_BASE			0x50025000
#define LTDC_BASE			0x5A001000
#define DCMIPP_BASE			0x5A000000
#define ADC1_BASE			0x48003000
#define ADC2_BASE			0x48004000
#define OTG_BASE			0x49000000
#define TSC_BASE			0x5000B000
#define MDMA_BASE			0x58000000
#define ETH1_BASE			0x5800A000
#define ETH2_BASE			0x5800E000
#define FMC_BASE			0x58002000
#define QSPI_BASE			0x58003000

/* Console configuration */
#define STM32MP1_DEBUG_USART_BASE	UART4_BASE
#define GIC_SPI_UART4			84

#define CONSOLE_UART_BASE		STM32MP1_DEBUG_USART_BASE
#define CONSOLE_UART_SIZE		1024

/* BSEC OTP resources */
#define STM32MP1_OTP_MAX_ID		U(0x5F)
#define STM32MP1_UPPER_OTP_START	U(0x20)

#define OTP_MAX_SIZE			(STM32MP1_OTP_MAX_ID + U(1))

#define CFG0_OTP				"cfg0_otp"
#ifdef CFG_STM32MP13
#define CFG0_OTP_BIT_LENGTH			U(16)
#define CFG0_OTP_MODE_MASK			GENMASK_32(9, 0)
#define CFG0_OTP_MODE_SHIFT			U(0)
#define CFG0_OPEN_DEVICE			U(0x17)
#define CFG0_CLOSED_DEVICE			U(0x3F)
#define CFG0_CLOSED_DEVICE_NO_BOUNDARY_SCAN	U(0x17F)
#define CFG0_CLOSED_DEVICE_NO_JTAG		U(0x3FF)
#define CFG0_CLOSED_MASK			CFG0_CLOSED_DEVICE
#endif
#ifdef CFG_STM32MP15
#define CFG0_OTP_BIT_LENGTH			U(8)
#define CFG0_OTP_SECURED_POS			U(6)
#define CFG0_CLOSED_MASK			BIT(CFG0_OTP_SECURED_POS)
#endif

#define HW2_OTP					"hw2_otp"
#define HW2_OTP_IWDG_HW_ENABLE_SHIFT		U(3)
#define HW2_OTP_IWDG_FZ_STOP_SHIFT		U(5)
#define HW2_OTP_IWDG_FZ_STANDBY_SHIFT		U(7)
#define HW2_OTP_PRODUCT_BELOW_2V5		BIT(13)

#define PART_NUMBER_OTP				"part_number_otp"
#define PART_NUMBER_OTP_PART_SHIFT		U(0)

#ifdef CFG_STM32MP13
#define PART_NUMBER_OTP_BIT_LENGTH		U(16)
#define PART_NUMBER_OTP_PART_MASK		GENMASK_32(14, 0)
#endif

#ifdef CFG_STM32MP15
#define PART_NUMBER_OTP_BIT_LENGTH		U(8)
#define PART_NUMBER_OTP_PART_MASK		GENMASK_32(7, 0)
#endif

#define HW2_OTP_IWDG_HW_ENABLE_SHIFT		U(3)
#define HW2_OTP_IWDG_FZ_STOP_SHIFT		U(5)
#define HW2_OTP_IWDG_FZ_STANDBY_SHIFT		U(7)

/* GIC resources */
#define GIC_SIZE			0x2000
#define GICC_OFFSET			0x1000
#define GICD_OFFSET			0x0000

#define GIC_NON_SEC_SGI_0		0
#define GIC_SEC_SGI_0			8
#define GIC_SEC_SGI_1			9

#define GIC_MPU_WAKEUP_PIN		157

#define TARGET_CPU0_GIC_MASK		BIT(0)
#define TARGET_CPU1_GIC_MASK		BIT(1)
#define TARGET_CPUS_GIC_MASK		GENMASK_32(CFG_TEE_CORE_NB_CORE - 1, 0)

#define STM32MP_GIC_PRIORITY_CSTOP	U(0xC0)

/* IWDG resources */
#define IWDG1_INST			U(0)
#define IWDG2_INST			U(1)

/* RCC platform resources */
#ifdef CFG_STM32MP13
#define RCC_WAKEUP_IT			U(154)
#endif

#ifdef CFG_STM32MP15
#define RCC_WAKEUP_IT			U(177)
#endif

/* TAMP resources */
#define TAMP_BKP_REGISTER_OFF		U(0x100)

/* USART/UART resources */
#define USART1_BASE			UART1_BASE
#define USART2_BASE			UART2_BASE
#define USART3_BASE			UART3_BASE
#define USART6_BASE			UART6_BASE

/* DBGMCU resources */
#define DBGMCU_IDC			U(0x0)
#define DBGMCU_IDC_DEV_ID_MASK		GENMASK_32(11, 0)
#define DBGMCU_IDC_REV_ID_MASK		GENMASK_32(31, 16)
#define DBGMCU_IDC_REV_ID_SHIFT		U(16)

/* BKPSRAM layout */
#ifdef CFG_STM32MP13
#define BKPSRAM_SIZE			U(0x2000)
#else
#define BKPSRAM_SIZE			U(0x1000)
#endif

#define BKPSRAM_PM_OFFSET		U(0x0000)
#define BKPSRAM_PM_SIZE			(BKPSRAM_PM_MAILBOX_SIZE + \
						BKPSRAM_PM_CONTEXT_SIZE)

#define BKPSRAM_PM_MAILBOX_OFFSET	BKPSRAM_PM_OFFSET
#define BKPSRAM_PM_MAILBOX_SIZE		U(0x100)
#define BKPSRAM_PM_CONTEXT_OFFSET	(BKPSRAM_PM_MAILBOX_OFFSET + \
						BKPSRAM_PM_MAILBOX_SIZE)
#define BKPSRAM_PM_CONTEXT_SIZE		U(0xF00)

/* SYSRAM layout */
#define SYSRAM_SIZE			0x40000
#define SYSRAM_NS_SIZE			(SYSRAM_SIZE - SYSRAM_SEC_SIZE)

/* Non-secure SYSRAM must be above (higher addresses) secure SYSRAM */
#if (CFG_STM32MP1_SCMI_SHM_BASE >= SYSRAM_BASE) && \
	((CFG_STM32MP1_SCMI_SHM_BASE + CFG_STM32MP1_SCMI_SHM_SIZE) <= \
	 (SYSRAM_BASE + SYSRAM_SIZE))
#define SYSRAM_SEC_SIZE		(CFG_STM32MP1_SCMI_SHM_BASE - SYSRAM_BASE)
#else
#define SYSRAM_SEC_SIZE		SYSRAM_SIZE
#endif

/* RETRAM layout */
#define RETRAM_SIZE			U(0x10000)

/* MCUSRAM layout */
#define MCUSRAM_SIZE			U(0x60000)

/* SoC part numbers and revisions */
#define STM32MP135C_PART_NB		U(0x05010000)
#define STM32MP135A_PART_NB		U(0x05010001)
#define STM32MP133C_PART_NB		U(0x050100C0)
#define STM32MP133A_PART_NB		U(0x050100C1)
#define STM32MP131C_PART_NB		U(0x050106C8)
#define STM32MP131A_PART_NB		U(0x050106C9)
#define STM32MP135F_PART_NB		U(0x05010800)
#define STM32MP135D_PART_NB		U(0x05010801)
#define STM32MP133F_PART_NB		U(0x050108C0)
#define STM32MP133D_PART_NB		U(0x050108C1)
#define STM32MP131F_PART_NB		U(0x05010EC8)
#define STM32MP131D_PART_NB		U(0x05010EC9)

#define STM32MP157C_PART_NB		U(0x05000000)
#define STM32MP157A_PART_NB		U(0x05000001)
#define STM32MP153C_PART_NB		U(0x05000024)
#define STM32MP153A_PART_NB		U(0x05000025)
#define STM32MP151C_PART_NB		U(0x0500002E)
#define STM32MP151A_PART_NB		U(0x0500002F)
#define STM32MP157F_PART_NB		U(0x05000080)
#define STM32MP157D_PART_NB		U(0x05000081)
#define STM32MP153F_PART_NB		U(0x050000A4)
#define STM32MP153D_PART_NB		U(0x050000A5)
#define STM32MP151F_PART_NB		U(0x050000AE)
#define STM32MP151D_PART_NB		U(0x050000AF)

#ifdef CFG_STM32MP13
#define STM32MP1_CHIP_ID		U(0x501)
#define STM32MP1_REV_A			U(0x00001000)
#define STM32MP1_REV_Y			U(0x00001003)
#define STM32MP1_REV_Z			U(0x00001001)
#endif

#ifdef CFG_STM32MP15
#define STM32MP1_CHIP_ID		U(0x500)
#define STM32MP1_REV_A			U(0x00001000)
#define STM32MP1_REV_B			U(0x00002000)
#define STM32MP1_REV_Z			U(0x00002001)
#endif

#define STM32MP1_CHIP_DEFAULT_VERSION	U(0)

/* OPP */
#define PLAT_OPP_ID1			1U
#define PLAT_OPP_ID2			2U
#define PLAT_MAX_OPP_NB			2U
#define PLAT_MAX_PLLCFG_NB		6U

#endif /*PLATFORM_CONFIG_H*/
