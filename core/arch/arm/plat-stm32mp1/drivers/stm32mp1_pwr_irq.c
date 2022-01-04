// SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0+)
/*
 * Copyright (c) 2020-2021, STMicroelectronics
 */

#include <config.h>
#include <drivers/stm32_exti.h>
#include <drivers/stm32_gpio.h>
#include <drivers/stm32mp1_pwr.h>
#include <initcall.h>
#include <io.h>
#include <kernel/dt.h>
#include <kernel/panic.h>
#include <libfdt.h>
#include <platform_config.h>
#include <stm32_util.h>
#include <sys/queue.h>

#define VERBOSE_PWR FMSG

/* PWR Registers */
#define WKUPCR			0x20
#define WKUPFR			0x24
#define MPUWKUPENR		0x28

/* WKUPCR bits definition */
#define WKUP_EDGE_SHIFT		8
#define WKUP_PULL_SHIFT		16
#define WKUP_PULL_MASK		GENMASK_32(1, 0)

enum wkup_pull_setting {
	WKUP_NO_PULL = 0,
	WKUP_PULL_UP,
	WKUP_PULL_DOWN,
	WKUP_PULL_RESERVED
};

/* EXTI line number for PWR Wakeup pin 1 */
#define PWR_EXTI_WKUP1		55

struct stm32_pwr_data {
	vaddr_t base;
	struct stm32_pinctrl_list *pinctrl_list;
	struct itr_handler *hdl[PWR_NB_WAKEUPPINS];
};

struct stm32_pwr_data *pwr_data;

static enum itr_return pwr_it_handler(struct itr_handler *handler)
{
	struct stm32_pwr_data  *priv = (struct stm32_pwr_data *)handler->data;
	uint32_t wkupfr = 0;
	uint32_t wkupenr = 0;
	uint32_t i = 0;

	VERBOSE_PWR("pwr irq handler");

	wkupfr = io_read32(priv->base + WKUPFR);
	wkupenr = io_read32(priv->base + MPUWKUPENR);

	for (i = 0; i < PWR_NB_WAKEUPPINS; i++) {
		if ((wkupfr & BIT(i)) && (wkupenr & BIT(i))) {
			VERBOSE_PWR("handle wkup irq:%d\n", i);

			if (priv->hdl[i]) {
				struct itr_handler *h = priv->hdl[i];

				if (h->handler(h) != ITRR_HANDLED) {
					EMSG("Disabling unhandled interrupt %u",
					     i);
					stm32mp1_pwr_itr_disable(i);
				}
			}

			/* Ack IRQ */
			io_setbits32(priv->base + WKUPCR, BIT(i));
		}
	}

	return ITRR_HANDLED;
}

static TEE_Result
stm32_pwr_irq_set_pull_config(size_t it, enum wkup_pull_setting config)
{
	struct stm32_pwr_data *priv = pwr_data;

	VERBOSE_PWR("irq:%u pull config:0x%" PRIx32, it, config);

	if (config >= WKUP_PULL_RESERVED) {
		EMSG("bad irq pull config");
		return TEE_ERROR_GENERIC;
	}

	io_mask32(priv->base + WKUPCR,
		  (config & WKUP_PULL_MASK) << (WKUP_PULL_SHIFT + it * 2),
		  (WKUP_PULL_MASK) << (WKUP_PULL_SHIFT + it * 2));

	return TEE_SUCCESS;
}

static TEE_Result
stm32_pwr_irq_set_trig(size_t it, enum pwr_wkup_flags trig)
{
	struct stm32_pwr_data *priv = pwr_data;
	uint32_t wkupcr = 0;
	int en = 0;

	VERBOSE_PWR("irq:%u trig:0x%" PRIx32, it, trig);

	en = io_read32(priv->base + MPUWKUPENR) & BIT(it);
	/*
	 * Reference manual request to disable the wakeup pin while
	 * changing the edge detection setting
	 */
	if (en)
		stm32mp1_pwr_itr_disable(it);

	wkupcr = io_read32(priv->base + WKUPCR);
	switch (trig) {
	case PWR_WKUP_FLAG_FALLING:
		wkupcr |= BIT(WKUP_EDGE_SHIFT + it);
		break;
	case PWR_WKUP_FLAG_RISING:
		wkupcr &= ~BIT(WKUP_EDGE_SHIFT + it);
		break;
	default:
		panic("Bad edge configuration");
	}

	io_write32(priv->base + WKUPCR, wkupcr);

	if (en)
		stm32mp1_pwr_itr_enable(it);

	return TEE_SUCCESS;
}

void stm32mp1_pwr_itr_enable(size_t it)
{
	struct stm32_pwr_data *priv = pwr_data;

	VERBOSE_PWR("Pwr irq enable");

	if (IS_ENABLED(CFG_STM32_EXTI))
		stm32_exti_enable_wake(PWR_EXTI_WKUP1 + it);

	io_setbits32(priv->base + MPUWKUPENR, BIT(it));
}

void stm32mp1_pwr_itr_disable(size_t it)
{
	struct stm32_pwr_data *priv = pwr_data;

	VERBOSE_PWR("Pwr irq disable");

	io_clrbits32(priv->base + MPUWKUPENR, BIT(it));

	if (IS_ENABLED(CFG_STM32_EXTI))
		stm32_exti_disable_wake(PWR_EXTI_WKUP1 + it);
}

static TEE_Result stm32mp1_pwr_irt_add(struct itr_handler *hdl)
{
	struct stm32_pwr_data *priv = pwr_data;
	int it = hdl->it;
	struct stm32_pinctrl_list pinctrl_list;
	struct stm32_pinctrl *pinctrl = NULL;
	unsigned int i = 0;

	VERBOSE_PWR("Pwr IRQ add");

	if (!priv) {
		DMSG("Pwr IRQs not yet initialized");
		return TEE_ERROR_DEFER_DRIVER_INIT;
	}

	assert(it >= PWR_WKUP_PIN1 && it < PWR_NB_WAKEUPPINS);
	/* check IRQ not already in use */
	assert(!priv->hdl[it]);

	priv->hdl[it] = hdl;

	STAILQ_FOREACH(pinctrl, priv->pinctrl_list, link) {
		if ((unsigned int)it == i)
			break;

		i++;
	}
	assert(pinctrl);

	STAILQ_INIT(&pinctrl_list);
	STAILQ_INSERT_HEAD(&pinctrl_list, pinctrl, link);

	stm32_pinctrl_load_config(&pinctrl_list);

	VERBOSE_PWR("Wake-up pin on bank=%u pin=%u",
		    pinctrl->bank, pinctrl->pin);

	/* use the same pull up configuration than for the gpio */
	switch (pinctrl->config.pupd) {
	case GPIO_PUPD_NO_PULL:
		break;
	case GPIO_PUPD_PULL_UP:
		stm32_pwr_irq_set_pull_config(it, WKUP_PULL_UP);
		break;
	case GPIO_PUPD_PULL_DOWN:
		stm32_pwr_irq_set_pull_config(it, WKUP_PULL_DOWN);
		break;
	default:
		panic();
	}

	stm32_pwr_irq_set_trig(it, hdl->flags);

	return TEE_SUCCESS;
}

TEE_Result
stm32mp1_pwr_itr_alloc_add(size_t it, itr_handler_t handler, uint32_t flags,
			   void *data, struct itr_handler **phdl)
{
	TEE_Result res = TEE_SUCCESS;
	struct itr_handler *hdl = NULL;

	assert(!(flags & ITRF_SHARED));

	hdl = calloc(1, sizeof(*hdl));
	if (!hdl)
		return TEE_ERROR_OUT_OF_MEMORY;

	hdl->it = it;
	hdl->handler = handler;
	hdl->flags = flags;
	hdl->data = data;

	res = stm32mp1_pwr_irt_add(hdl);
	if (res) {
		free(hdl);
		return res;
	}

	*phdl = hdl;

	return res;
}

static TEE_Result
stm32mp1_pwr_irq_probe(const void *fdt, int node,
		       const void *compat_data __unused)
{
	TEE_Result res = TEE_ERROR_GENERIC;
	struct itr_handler *hdl = NULL;
	struct stm32_pwr_data *priv = NULL;
	struct stm32_pinctrl *pinctrl = NULL;
	size_t count = 0;

	VERBOSE_PWR("Init PWR IRQ");

	pwr_data = calloc(1, sizeof(*pwr_data));
	if (!pwr_data)
		return TEE_ERROR_OUT_OF_MEMORY;

	priv = pwr_data;
	priv->base = stm32_pwr_base();

	res = stm32_pinctrl_dt_get_by_index(fdt, node, 0, &priv->pinctrl_list);
	if (res)
		goto err;

	STAILQ_FOREACH(pinctrl, priv->pinctrl_list, link)
		count++;

	if (count != PWR_NB_WAKEUPPINS) {
		res = TEE_ERROR_BAD_PARAMETERS;
		EMSG("Missing pinctrl description");
		goto err;
	}

	hdl = itr_alloc_add(GIC_MPU_WAKEUP_PIN, pwr_it_handler, 0, priv);
	if (!hdl)
		panic("Could not get wake-up pin IRQ");

	itr_enable(hdl->it);

	VERBOSE_PWR("Init pwr irq done");

	return TEE_SUCCESS;
err:
	free(pwr_data);

	return res;
}

static const struct dt_device_match pwr_irq_match_table[] = {
	{ .compatible = "st,stm32mp1,pwr-irq" },
	{ }
};

DEFINE_DT_DRIVER(stm32mp1_pwr_irq_dt_driver) = {
	.name = "stm32mp1-pwr-irq",
	.match_table = pwr_irq_match_table,
	.probe = &stm32mp1_pwr_irq_probe,
};

static enum itr_return pwr_it_user_handler(struct itr_handler *handler __unused)
{
	VERBOSE_PWR("pwr irq tester handler");

	return ITRR_HANDLED;
}

static TEE_Result
stm32mp1_pwr_irq_user_dt_probe(const void *fdt, int node,
			       const void *compat_data __unused)
{
	TEE_Result res = TEE_SUCCESS;
	struct itr_handler *hdl = NULL;
	const fdt32_t *cuint = NULL;
	size_t it = 0;

	VERBOSE_PWR("Init pwr irq user");

	cuint = fdt_getprop(fdt, node, "st,wakeup-pin-number", NULL);
	if (!cuint)
		panic("Missing wake-up pin number");

	it = fdt32_to_cpu(*cuint) - 1U;

	res = stm32mp1_pwr_itr_alloc_add(it, pwr_it_user_handler,
					 PWR_WKUP_FLAG_FALLING, NULL, &hdl);
	if (res != TEE_SUCCESS)
		return res;

	stm32mp1_pwr_itr_enable(hdl->it);

	return TEE_SUCCESS;
}

static const struct dt_device_match pwr_irq_test_match_table[] = {
	{ .compatible = "st,stm32mp1,pwr-irq-user" },
	{ }
};

DEFINE_DT_DRIVER(stm32mp1_pwr_irq_dt_tester) = {
	.name = "stm32mp1-pwr-irq-user",
	.match_table = pwr_irq_test_match_table,
	.probe = &stm32mp1_pwr_irq_user_dt_probe,
};
