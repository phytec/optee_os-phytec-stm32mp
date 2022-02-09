// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) 2017-2022, STMicroelectronics - All Rights Reserved
 */

#include <arm.h>
#include <assert.h>
#include <boot_api.h>
#include <console.h>
#include <config.h>
#include <drivers/clk.h>
#include <drivers/gic.h>
#include <drivers/regulator.h>
#include <drivers/stm32_etzpc.h>
#include <drivers/stm32_iwdg.h>
#include <drivers/stm32mp_dt_bindings.h>
#include <drivers/stm32mp1_ddrc.h>
#include <drivers/stm32mp1_pmic.h>
#include <drivers/stm32mp1_pwr.h>
#ifdef CFG_STM32MP13
#include <drivers/stm32mp13_rcc.h>
#else
#include <drivers/stm32mp1_rcc.h>
#endif
#include <drivers/stpmic1.h>
#include <initcall.h>
#include <io.h>
#include <keep.h>
#include <kernel/cache_helpers.h>
#include <kernel/delay.h>
#include <kernel/misc.h>
#include <kernel/panic.h>
#include <mm/core_memprot.h>
#include <platform_config.h>
#include <sm/psci.h>
#include <stdbool.h>
#include <stm32mp_pm.h>
#include <stm32_util.h>
#include <trace.h>

#include "context.h"
#include "power.h"

#define TIMEOUT_US_1MS		1000

#define PWRLP_TEMPO_5_HSI	5

static uint8_t gicd_rcc_wakeup;
static uint8_t gicc_pmr;

struct pwr_lp_config {
	uint32_t pwr_cr1;
	uint32_t pwr_mpucr;
	const char *regul_suspend_node_name;
};

#ifdef CFG_STM32MP13
#define PWR_CR1_MASK	(PWR_CR1_LPDS | PWR_CR1_LPCFG | PWR_CR1_LVDS |\
			 PWR_CR1_STOP2)
#define PWR_MPUCR_MASK	(PWR_MPUCR_CSSF | PWR_MPUCR_PDDS)

static const struct pwr_lp_config config_pwr[STM32_PM_MAX_SOC_MODE] = {
	[STM32_PM_CSLEEP_RUN] = {
		.pwr_cr1 = 0U,
		.pwr_mpucr = 0U,
		.regul_suspend_node_name = NULL,
	},
	[STM32_PM_CSTOP_ALLOW_STOP] = {
		.pwr_cr1 = 0U,
		.pwr_mpucr = PWR_MPUCR_CSSF,
		.regul_suspend_node_name = NULL,
	},
	[STM32_PM_CSTOP_ALLOW_LP_STOP] = {
		.pwr_cr1 = PWR_CR1_LPDS,
		.pwr_mpucr = PWR_MPUCR_CSSF,
		.regul_suspend_node_name = "lp-stop",
	},
	[STM32_PM_CSTOP_ALLOW_LPLV_STOP] = {
		.pwr_cr1 = PWR_CR1_LVDS | PWR_CR1_LPDS,
		.pwr_mpucr = PWR_MPUCR_CSSF,
		.regul_suspend_node_name = "lplv-stop",
	},
	[STM32_PM_CSTOP_ALLOW_LPLV_STOP2] = {
		.pwr_cr1 = PWR_CR1_LVDS | PWR_CR1_LPDS | PWR_CR1_STOP2,
		.pwr_mpucr = PWR_MPUCR_CSSF,
		.regul_suspend_node_name = "lplv-stop2",
	},
	[STM32_PM_CSTOP_ALLOW_STANDBY_DDR_SR] = {
		.pwr_cr1 = 0U,
		.pwr_mpucr = PWR_MPUCR_CSSF |
			PWR_MPUCR_PDDS,
		.regul_suspend_node_name = "standby-ddr-sr",
	},
	[STM32_PM_CSTOP_ALLOW_STANDBY_DDR_OFF] = {
		.pwr_cr1 = 0U,
		.pwr_mpucr = PWR_MPUCR_CSSF |
			PWR_MPUCR_PDDS,
		.regul_suspend_node_name = "standby-ddr-off",
	},
	[STM32_PM_SHUTDOWN] = {
		.pwr_cr1 = 0U,
		.pwr_mpucr = 0U,
		.regul_suspend_node_name = "standby-ddr-off",
	},
};
#else
#define PWR_CR1_MASK	(PWR_CR1_LPDS | PWR_CR1_LPCFG | PWR_CR1_LVDS)
#define PWR_MPUCR_MASK	(PWR_MPUCR_CSTDBYDIS | PWR_MPUCR_CSSF | PWR_MPUCR_PDDS)

static const struct pwr_lp_config config_pwr[STM32_PM_MAX_SOC_MODE] = {
	[STM32_PM_CSLEEP_RUN] = {
		.pwr_cr1 = 0U,
		.pwr_mpucr = 0U,
		.regul_suspend_node_name = NULL,
	},
	[STM32_PM_CSTOP_ALLOW_STOP] = {
		.pwr_cr1 = 0U,
		.pwr_mpucr = PWR_MPUCR_CSTDBYDIS | PWR_MPUCR_CSSF,
		.regul_suspend_node_name = NULL,
	},
	[STM32_PM_CSTOP_ALLOW_LP_STOP] = {
		.pwr_cr1 = PWR_CR1_LPDS,
		.pwr_mpucr = PWR_MPUCR_CSTDBYDIS | PWR_MPUCR_CSSF,
		.regul_suspend_node_name = "lp-stop",
	},
	[STM32_PM_CSTOP_ALLOW_LPLV_STOP] = {
		.pwr_cr1 = PWR_CR1_LVDS | PWR_CR1_LPDS,
		.pwr_mpucr = PWR_MPUCR_CSTDBYDIS | PWR_MPUCR_CSSF,
		.regul_suspend_node_name = "lplv-stop",
	},
	[STM32_PM_CSTOP_ALLOW_STANDBY_DDR_SR] = {
		.pwr_cr1 = 0U,
		.pwr_mpucr = PWR_MPUCR_CSTDBYDIS | PWR_MPUCR_CSSF |
			PWR_MPUCR_PDDS,
		.regul_suspend_node_name = "standby-ddr-sr",
	},
	[STM32_PM_CSTOP_ALLOW_STANDBY_DDR_OFF] = {
		.pwr_cr1 = 0U,
		.pwr_mpucr = PWR_MPUCR_CSTDBYDIS | PWR_MPUCR_CSSF |
			PWR_MPUCR_PDDS,
		.regul_suspend_node_name = "standby-ddr-off",
	},
	[STM32_PM_SHUTDOWN] = {
		.pwr_cr1 = 0U,
		.pwr_mpucr = 0U,
		.regul_suspend_node_name = "standby-ddr-off",
	},
};
#endif

static void set_rcc_it_priority(uint8_t *it_prio, uint8_t *pmr)
{
	*it_prio = itr_set_ipriority(RCC_WAKEUP_IT, GIC_HIGHEST_SEC_PRIORITY);
	*pmr = itr_set_pmr(STM32MP_GIC_PRIORITY_CSTOP);
}

static void restore_rcc_it_priority(uint8_t it_prio, uint8_t pmr)
{
	(void)itr_set_ipriority(RCC_WAKEUP_IT, it_prio);
	(void)itr_set_pmr(pmr);
}

const char *stm32mp_pm_hint2mode_name(uint32_t pm_hint)
{
	assert(pm_hint < ARRAY_SIZE(config_pwr));

	return config_pwr[pm_hint].regul_suspend_node_name;
}

const char *plat_get_lp_mode_name(int mode)
{
	return stm32mp_pm_hint2mode_name(mode);
}

#define CONSOLE_FLUSH_DELAY_MS		10

#if TRACE_LEVEL >= TRACE_DEBUG
static void wait_console_flushed(void)
{
	console_flush();
	mdelay(CONSOLE_FLUSH_DELAY_MS);
}
#else
static void wait_console_flushed(void)
{
}
#endif

static void cpu_wfi(void)
{
	dsb();
	isb();
	wfi();
}

void stm32_pm_cpu_wfi(void)
{
	wait_console_flushed();
	cpu_wfi();
}

/* If IWDG is not supported, provide a stubbed weak watchdog kicker */
void __weak stm32_iwdg_refresh(void)
{
}

#define ARM_CNTXCTL_IMASK	BIT(1)

static void stm32mp_mask_timer(void)
{
	/* Mask timer interrupts */
	write_cntp_ctl(read_cntp_ctl() | ARM_CNTXCTL_IMASK);
	write_cntv_ctl(read_cntv_ctl() | ARM_CNTXCTL_IMASK);
}

/*
 * stm32_enter_cstop - Prepare CSTOP mode
 *
 * @mode - Target low power mode
 */
void stm32_enter_cstop(uint32_t mode)
{
	uint32_t pwr_cr1 = config_pwr[mode].pwr_cr1;
	vaddr_t pwr_base = stm32_pwr_base();
	vaddr_t rcc_base = stm32_rcc_base();

#ifdef CFG_STM32MP15
	if (mode == STM32_PM_CSTOP_ALLOW_LPLV_STOP2)
		panic("LPLV-Stop2 mode not supported");

	/* Save Self-Refresh (SR) mode and switch to Software SR mode */
	ddr_save_sr_mode(DDR_SSR_MODE);
#endif

	if (stm32mp_with_pmic() && (mode == STM32_PM_CSTOP_ALLOW_LP_STOP ||
				    mode == STM32_PM_CSTOP_ALLOW_LPLV_STOP ||
				    mode == STM32_PM_CSTOP_ALLOW_LPLV_STOP2))
		pwr_cr1 |= PWR_CR1_LPCFG;

	if (mode == STM32_PM_CSTOP_ALLOW_LPLV_STOP2) {
		struct clk *hsi_clk = stm32mp_rcc_clock_id_to_clk(CK_HSI);
		unsigned long freq = clk_get_rate(hsi_clk);
		/* Wait for 2ms before re-enabling PLLs after LPLV-Stop2 mode */
		uint32_t dly = 2U * freq / 1000U;

		io_clrsetbits32(rcc_base + RCC_PWRLPDLYCR,
				RCC_PWRLPDLYCR_PWRLP_DLY_MASK,
				dly);
	} else {
		/* Wait 6 HSI periods after other Stop modes */
		io_clrsetbits32(rcc_base + RCC_PWRLPDLYCR,
				RCC_PWRLPDLYCR_PWRLP_DLY_MASK,
				PWRLP_TEMPO_5_HSI);
	}

	/* Workaround for non secure cache issue: this should not be needed */
	dcache_op_all(DCACHE_OP_CLEAN_INV);

	/* Clear RCC interrupt before enabling it */
	io_setbits32(rcc_base + RCC_MP_CIFR, RCC_MP_CIFR_WKUPF);

	/* Enable RCC Wake-up */
	io_setbits32(rcc_base + RCC_MP_CIER, RCC_MP_CIFR_WKUPF);

	/* Configure low power mode */
	io_clrsetbits32(pwr_base + PWR_MPUCR_OFF, PWR_MPUCR_MASK,
			config_pwr[mode].pwr_mpucr);
	io_clrsetbits32(pwr_base + PWR_CR1_OFF, PWR_CR1_MASK, pwr_cr1);

	/* Clear RCC pending interrupt flags */
	io_write32(rcc_base + RCC_MP_CIFR, RCC_MP_CIFR_MASK);

#ifdef CFG_STM32MP13
	/* Request CSTOP mode to RCC */
	io_setbits32(rcc_base + RCC_MP_SREQSETR, RCC_MP_SREQSETR_STPREQ_P0);
#else
	io_setbits32(rcc_base + RCC_MP_SREQSETR,
		     RCC_MP_SREQSETR_STPREQ_P0 | RCC_MP_SREQSETR_STPREQ_P1);

	stm32_iwdg_refresh();
#endif

	set_rcc_it_priority(&gicd_rcc_wakeup, &gicc_pmr);

#ifndef CFG_STM32MP13
	if (ddr_standby_sr_entry() != 0)
		panic();
#endif

	if (mode == STM32_PM_CSTOP_ALLOW_STANDBY_DDR_SR) {
		/* set POPL to 20ms */
		io_clrsetbits32(pwr_base + PWR_CR3_OFF, PWR_CR3_POPL_MASK,
				20U << PWR_CR3_POPL_SHIFT);

		/* Keep backup RAM content in standby */
		io_setbits32(pwr_base + PWR_CR2_OFF, PWR_CR2_BREN);

		// TODO add a timeout?
		while (!(io_read32(pwr_base + PWR_CR2_OFF) & PWR_CR2_BRRDY))
			;

#ifndef CFG_STM32MP13
		if (stm32mp1_is_retram_during_standby()) {
			/* Keep retention in standby */
			io_setbits32(pwr_base + PWR_CR2_OFF, PWR_CR2_RREN);

			while ((io_read32(pwr_base + PWR_CR2_OFF) &
				(PWR_CR2_RRRDY)) == 0U)
				;
		}
#endif
	}
}

/*
 * stm32_exit_cstop - Exit from CSTOP mode
 */
void stm32_exit_cstop(void)
{
	vaddr_t rcc_base = stm32_rcc_base();

#ifdef CFG_STM32MP15
	if (ddr_standby_sr_exit())
		panic();

	/* Restore Self-Refresh mode saved in stm32_enter_cstop() */
	ddr_restore_sr_mode();
#endif

	restore_rcc_it_priority(gicd_rcc_wakeup, gicc_pmr);

#ifdef CFG_STM32MP13
	/* Disable STOP request */
	io_setbits32(rcc_base + RCC_MP_SREQCLRR, RCC_MP_SREQSETR_STPREQ_P0);
#else
	/* Disable STOP request */
	io_setbits32(rcc_base + RCC_MP_SREQCLRR,
			RCC_MP_SREQSETR_STPREQ_P0 | RCC_MP_SREQSETR_STPREQ_P1);
#endif

	/* Disable RCC Wake-up */
	io_clrbits32(rcc_base + RCC_MP_CIER, RCC_MP_CIFR_WKUPF);

	dsb();
	isb();

	/* Disable retention and backup RAM content after stop */
	io_clrbits32(stm32_pwr_base() + PWR_CR2_OFF, PWR_CR2_BREN);

#ifndef CFG_STM32MP13
	/* Disable retention and backup RAM content after stop */
	io_clrbits32(stm32_pwr_base() + PWR_CR2_OFF, PWR_CR2_RREN);
#endif
}

/*
 * GIC support required in low power sequences and reset sequences
 */
#define GICC_IAR			0x00C
#define GICC_IT_ID_MASK			0x3ff
#define GICC_EOIR			0x010
#define GICC_HPPIR			0x018
#define GICC_AHPPIR			0x028
#define GIC_PENDING_G1_INTID		1022U
#define GIC_SPURIOUS_INTERRUPT		1023U
#define GIC_NUM_INTS_PER_REG		32
#define GIC_MAX_SPI_ID			1020
#define GICD_ICENABLER(n)		(0x180 + (n) * 4)

static void clear_pending_interrupts(void)
{
	uint32_t id = 0;
	vaddr_t gicc_base = get_gicc_base();
	vaddr_t gicd_base = get_gicd_base();

	do {
		id = io_read32(gicc_base + GICC_HPPIR) & GICC_IT_ID_MASK;

		/*
		 * Find out which interrupt it is under the
		 * assumption that the GICC_CTLR.AckCtl bit is 0.
		 */
		if (id == GIC_PENDING_G1_INTID)
			id = io_read32(gicc_base + GICC_AHPPIR) & GICC_IT_ID_MASK;

		if (id < GIC_MAX_SPI_ID) {
			size_t idx = id / GIC_NUM_INTS_PER_REG;
			uint32_t mask = 1 << (id % GIC_NUM_INTS_PER_REG);

			io_write32(gicc_base + GICC_EOIR, id);

			io_write32(gicd_base + GICD_ICENABLER(idx), mask);

			dsb_ishst();
		}
	} while (id < GIC_MAX_SPI_ID);
}

void stm32mp_gic_set_end_of_interrupt(uint32_t it)
{
	vaddr_t gicc_base = get_gicc_base();

	io_write32(gicc_base + GICC_EOIR, it);
}

static void __noreturn wait_cpu_reset(void)
{
#ifdef STM32MP1_USE_MPU0_RESET
	dcache_op_all(DCACHE_OP_CLEAN_INV);
	write_sctlr(read_sctlr() & ~SCTLR_C);
	dcache_op_all(DCACHE_OP_CLEAN_INV);
	__asm__("clrex");

	dsb();
	isb();
#else
	psci_armv7_cpu_off();
#endif

	for ( ; ; ) {
		clear_pending_interrupts();
		wfi();
	}
}

#ifdef STM32MP1_USE_MPU0_RESET
/*
 * tzc_source_ip contains the TZC transaction source IPs that need to be reset
 * before a C-A7 subsystem is reset (i.e. independent reset):
 * - C-A7 subsystem is reset separately later in the sequence,
 * - C-M4 subsystem is not concerned here,
 * - DAP is excluded for debug purpose,
 * - IPs are stored with their ETZPC IDs (STM32MP1_ETZPC_MAX_ID if not
 *   applicable) because some of them need to be reset only if they are not
 *   configured in MCU isolation mode inside ETZPC device tree.
 */
struct tzc_source_ip {
	uint16_t reset_id;
	uint16_t clock_id;
	uint32_t decprot_id;
};

#define _TZC_FIXED(res, clk)				\
	{						\
		.reset_id = (res),			\
		.clock_id = (clk),			\
		.decprot_id = STM32MP1_ETZPC_MAX_ID,	\
	}

#define _TZC_COND(res, clk, decprot)			\
	{						\
		.reset_id = (res),			\
		.clock_id = (clk),			\
		.decprot_id = (decprot),		\
	}

static const struct tzc_source_ip __maybe_unused tzc_source_ip[] = {
	_TZC_FIXED(LTDC_R, LTDC_PX),
	_TZC_FIXED(GPU_R, GPU),
	_TZC_FIXED(USBH_R, USBH),
	_TZC_FIXED(SDMMC1_R, SDMMC1_K),
	_TZC_FIXED(SDMMC2_R, SDMMC2_K),
	_TZC_FIXED(MDMA_R, MDMA),
	_TZC_COND(USBO_R, USBO_K, STM32MP1_ETZPC_OTG_ID),
	_TZC_COND(SDMMC3_R, SDMMC3_K, STM32MP1_ETZPC_SDMMC3_ID),
	_TZC_COND(ETHMAC_R, ETHMAC, STM32MP1_ETZPC_ETH_ID),
	_TZC_COND(DMA1_R, DMA1, STM32MP1_ETZPC_DMA1_ID),
	_TZC_COND(DMA2_R, DMA2, STM32MP1_ETZPC_DMA2_ID),
};

static void reset_peripherals(void)
{
	vaddr_t rcc_base = stm32_rcc_base();
	size_t __maybe_unused id = 0;

	for (id = 0U; id < ARRAY_SIZE(tzc_source_ip); id++) {
		const struct tzc_source_ip *tzc = &tzc_source_ip[id];

		if (!stm32_clock_is_enabled(tzc->clock_id) ||
		    ((tzc->decprot_id != STM32MP1_ETZPC_MAX_ID) &&
		     (etzpc_get_decprot(tzc->decprot_id) ==
		      ETZPC_DECPROT_MCU_ISOLATION)))
			continue;

		if (tzc->reset_id != GPU_R) {
			stm32_reset_assert(tzc->reset_id, TIMEOUT_US_1MS);
			stm32_reset_deassert(tzc->reset_id, TIMEOUT_US_1MS);
		} else {
			/* GPU reset automatically cleared by hardware */
			io_setbits32(rcc_base + RCC_AHB6RSTSETR,
				     RCC_AHB6RSTSETR_GPURST);
		}
	}
}
#endif /* STM32MP1_USE_MPU0_RESET */

static void __noreturn reset_cores(void)
{
	vaddr_t rcc_base = stm32_rcc_base();
#ifdef CFG_STM32MP13
	uint32_t reset_mask = RCC_MP_GRSTCSETR_MPUP0RST;
#else
	uint32_t reset_mask = RCC_MP_GRSTCSETR_MPUP0RST |
			      RCC_MP_GRSTCSETR_MPUP1RST;
#endif
	uint32_t target_mask = 0;

	/* Mask timer interrupts */
	stm32mp_mask_timer();

#ifdef STM32MP1_USE_MPU0_RESET
	reset_peripherals();
#endif

	if (get_core_pos() == 0)
		target_mask = TARGET_CPU1_GIC_MASK;
	else
		target_mask = TARGET_CPU0_GIC_MASK;

	itr_raise_sgi(GIC_SEC_SGI_1, target_mask);

	clear_pending_interrupts();

	io_write32(rcc_base + RCC_MP_GRSTCSETR, reset_mask);

	wait_cpu_reset();
}

/*
 * stm32_pm_cpus_reset - Reset only cpus
 */
void __noreturn stm32_cores_reset(void)
{
	reset_cores();
}
DECLARE_KEEP_PAGER(stm32_cores_reset);

static __maybe_unused void reset_other_core(void)
{
	vaddr_t __maybe_unused rcc_base = stm32_rcc_base();
	uint32_t __maybe_unused reset_mask = 0;
	uint32_t __maybe_unused target_mask = 0;

	if (!stm32mp_supports_second_core())
		return;

#ifdef CFG_STM32MP15
	if (get_core_pos() == 0) {
		reset_mask = RCC_MP_GRSTCSETR_MPUP1RST;
		target_mask = TARGET_CPU1_GIC_MASK;
	} else {
		reset_mask = RCC_MP_GRSTCSETR_MPUP0RST;
		target_mask = TARGET_CPU0_GIC_MASK;
	}
#endif

	itr_raise_sgi(GIC_SEC_SGI_1, target_mask);

#ifdef STM32MP1_USE_MPU0_RESET
	io_write32(rcc_base + RCC_MP_GRSTCSETR, reset_mask);
#endif
}

/*
 * Override default plat_panic() function to trap all cores
 * and let hardware watchdog fire.
 */
void __noreturn plat_panic(void)
{
	DMSG("Panic on CPU %u", get_core_pos());

	reset_other_core();

	while (true)
		cpu_idle();
}

/*
 * stm32_enter_cstop_shutdown - Shutdown CPUs to target low power mode
 * @mode - Target low power mode
 */
void __noreturn stm32_enter_cstop_shutdown(uint32_t mode)
{
	switch (mode) {
	case STM32_PM_SHUTDOWN:
		if (stm32mp_with_pmic()) {
			wait_console_flushed();
			stm32mp_pm_get_pmic();
			stpmic1_switch_off();
			udelay(100);
		}
		break;
	case STM32_PM_CSTOP_ALLOW_STANDBY_DDR_OFF:
		stm32mp_pm_shutdown_context();
#ifdef STM32MP1_USE_MPU0_RESET
		reset_other_core();
#endif
		stm32_enter_cstop(mode);
		dsb();
		isb();
		for ( ; ; )
			wfi();
		break;
	default:
		break;
	}

	panic();
}

/*
 * stm32_enter_cstop_reset - Reset CPUs to target low power mode
 * @mode - Target low power mode
 */
void __noreturn stm32_enter_cstop_reset(uint32_t mode)
{
	switch (mode) {
	case STM32_PM_SHUTDOWN:
		stm32_reset_system();
		udelay(100);
		break;
	default:
		IMSG("Forced system reset");
		wait_console_flushed();
		stm32_reset_system();
		udelay(100);
		break;
	}

	panic();
}

/*
 * stm32_enter_csleep - enter CSLEEP state while WFI and exit in CRUN
 *
 * Configure PWR for CSLEEP state. CPU shall execute a WFI and return
 * once a interrupt is pending.
 */
void stm32_enter_csleep(void)
{
	vaddr_t pwr_base = stm32_pwr_base();

	io_clrsetbits32(pwr_base + PWR_MPUCR_OFF, PWR_MPUCR_MASK,
			config_pwr[STM32_PM_CSLEEP_RUN].pwr_mpucr);
	io_clrsetbits32(pwr_base + PWR_CR1_OFF, PWR_CR1_MASK,
			config_pwr[STM32_PM_CSLEEP_RUN].pwr_cr1);

	stm32_pm_cpu_wfi();
}

/* RCC Wakeup interrupt is used to wake from suspeneded mode */
static enum itr_return rcc_wakeup_it_handler(struct itr_handler *hdl __unused)
{
	/* This interrupt is not expected to be handled */
	panic("RCC  wakeup interrupt");
	return ITRR_HANDLED;
}

static struct itr_handler rcc_wakeup_handler = {
	.it = RCC_WAKEUP_IT,
	.handler = rcc_wakeup_it_handler,
};
DECLARE_KEEP_PAGER(rcc_wakeup_handler);

/* SGI9 (secure SGI 1) informs targeted CPU it shall reset */
static enum itr_return sgi9_it_handler(struct itr_handler *handler)
{
	DMSG("Halting CPU %u", get_core_pos());

	stm32mp_mask_timer();

	stm32mp_dump_core_registers(false);

	stm32mp_gic_set_end_of_interrupt(handler->it);

	clear_pending_interrupts();

	wait_cpu_reset();

	panic("Core reset");

	return ITRR_HANDLED;
}

static struct itr_handler sgi9_reset_handler = {
	.it = GIC_SEC_SGI_1,
	.handler = sgi9_it_handler,
};
DECLARE_KEEP_PAGER(sgi9_reset_handler);

static TEE_Result init_low_power(void)
{
	vaddr_t pwr_base __maybe_unused = stm32_pwr_base();
	vaddr_t rcc_base = stm32_rcc_base();

	itr_add(&rcc_wakeup_handler);
	itr_enable(rcc_wakeup_handler.it);

	itr_add(&sgi9_reset_handler);
	itr_enable(sgi9_reset_handler.it);

#ifdef CFG_STM32MP13
	/* Disable STOP request */
	io_setbits32(rcc_base + RCC_MP_SREQCLRR, RCC_MP_SREQSETR_STPREQ_P0);
#else
	/*
	 * Configure Standby mode available for MCU by default
	 * and allow to switch in standby SoC in all case
	 */
	io_setbits32(pwr_base + PWR_MCUCR_OFF, PWR_MCUCR_PDDS);

	/* Disable STOP request */
	io_setbits32(rcc_base + RCC_MP_SREQCLRR,
		     RCC_MP_SREQSETR_STPREQ_P0 | RCC_MP_SREQSETR_STPREQ_P1);
#endif

	return TEE_SUCCESS;
}
driver_init_late(init_low_power);

/*
 * CPU low power sequences
 */
void __noreturn stm32_pm_cpu_power_down_wfi(void)
{
	vaddr_t __maybe_unused rcc_base = stm32_rcc_base();

	if (get_core_pos() == 0) {
		void (*reset_ep)(void) = stm32mp_sysram_resume;

		stm32_pm_cpu_wfi();

		/* STANDBY not reached: resume from retained SYSRAM */
		stm32_exit_cstop();
		stm32mp_cpu_reset_state();
		reset_ep();
		panic();
	}

	dcache_op_level1(DCACHE_OP_CLEAN);
#ifdef CFG_STM32MP13
	panic();
#else
	io_write32(rcc_base + RCC_MP_GRSTCSETR, RCC_MP_GRSTCSETR_MPUP1RST);
#endif
	cpu_wfi();
	panic();
}