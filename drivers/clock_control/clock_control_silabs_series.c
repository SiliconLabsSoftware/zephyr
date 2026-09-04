/*
 * Copyright (c) 2024 Silicon Laboratories Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT silabs_series_clock

#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/clock_control/clock_control_silabs.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/sys/util.h>
#include <soc.h>

#include "sl_clock_manager.h"
#include "sl_status.h"

struct silabs_clock_control_config {
	CMU_TypeDef *cmu;
	const struct pinctrl_dev_config *pcfg;
};

/*
 * CLKOUT (MCO) support: route an internal clock to a GPIO pin via the CMU
 * EXPORTCLKCTRL block. Each "silabs,series-clock-clkout" child selects its
 * source through the "clocks" phandle; the pin is set up by this node's
 * pinctrl-0. No emlib is used.
 */
#define SILABS_CLKOUT_COMPAT silabs_series_clock_clkout

#if DT_INST_NODE_HAS_PROP(0, pinctrl_0)
PINCTRL_DT_INST_DEFINE(0);
#define SILABS_CMU_PINCTRL_GET PINCTRL_DT_INST_DEV_CONFIG_GET(0)
#else
#define SILABS_CMU_PINCTRL_GET NULL
#endif

#if DT_HAS_COMPAT_STATUS_OKAY(SILABS_CLKOUT_COMPAT)

/* Map the "clocks" phandle target to the EXPORTCLKCTRL CLKOUTSELn field value. */
#define SILABS_CLKOUT_SEL(node)                                                                    \
	(DT_NODE_HAS_COMPAT(DT_CLOCKS_CTLR(node), silabs_series2_exportclk)                         \
		 ? _CMU_EXPORTCLKCTRL_CLKOUTSEL0_HFEXPCLK                                           \
	 : DT_NODE_HAS_COMPAT(DT_CLOCKS_CTLR(node), silabs_hfxo)                                    \
		 ? _CMU_EXPORTCLKCTRL_CLKOUTSEL0_HFXO                                               \
	 : DT_NODE_HAS_COMPAT(DT_CLOCKS_CTLR(node), silabs_series2_hfrcodpll)                       \
		 ? _CMU_EXPORTCLKCTRL_CLKOUTSEL0_HFRCODPLL                                          \
	 : DT_NODE_HAS_COMPAT(DT_CLOCKS_CTLR(node), silabs_series2_lfxo)                            \
		 ? _CMU_EXPORTCLKCTRL_CLKOUTSEL0_LFXO                                               \
	 : DT_NODE_HAS_COMPAT(DT_CLOCKS_CTLR(node), silabs_series2_lfrco)                           \
		 ? _CMU_EXPORTCLKCTRL_CLKOUTSEL0_LFRCO                                              \
		 : 0xFFU)

/*
 * Each CLKOUT output is its own clock_control device so consumers (e.g. the
 * I2S driver providing codec MCLK) can gate it at runtime with the standard
 * clock_control_on()/clock_control_off() API.
 *
 * Pin mux and EXPORTCLK enable are deferred to clock_control_on(). Doing either
 * in PRE_KERNEL_1 init (or listing clkout under the USART "clocks" property)
 * hung boot on xg27_rb4194a before the UART console came up (bisect: a1a174a92).
 */
struct silabs_clkout_config {
	CMU_TypeDef *cmu;
	const struct pinctrl_dev_config *pcfg; /* parent cmu pinctrl (CLKOUTx pin) */
	uint8_t output;                        /* CLKOUT index 0/1/2 */
	uint8_t sel;                           /* EXPORTCLKCTRL CLKOUTSELn value */
	uint8_t presc;                         /* EXPORTCLK prescaler (clock-div - 1) */
};

static void silabs_clkout_apply(const struct silabs_clkout_config *cfg, bool enable)
{
	uint32_t shift = (uint32_t)cfg->output * _CMU_EXPORTCLKCTRL_CLKOUTSEL1_SHIFT;
	uint32_t mask = _CMU_EXPORTCLKCTRL_CLKOUTSEL0_MASK << shift;
	uint32_t sel = enable ? cfg->sel : _CMU_EXPORTCLKCTRL_CLKOUTSEL0_DISABLED;
	uint32_t val = sel << shift;

	if (cfg->sel == _CMU_EXPORTCLKCTRL_CLKOUTSEL0_HFEXPCLK) {
		mask |= _CMU_EXPORTCLKCTRL_PRESC_MASK;
		if (enable) {
			val |= (uint32_t)cfg->presc << _CMU_EXPORTCLKCTRL_PRESC_SHIFT;
		}
	}

	cfg->cmu->EXPORTCLKCTRL = (cfg->cmu->EXPORTCLKCTRL & ~mask) | val;
}

static int silabs_clkout_on(const struct device *dev, clock_control_subsys_t sys)
{
	const struct silabs_clkout_config *cfg = dev->config;

	ARG_UNUSED(sys);

	if (cfg->pcfg != NULL) {
		(void)pinctrl_apply_state(cfg->pcfg, PINCTRL_STATE_DEFAULT);
	}
	silabs_clkout_apply(cfg, true);
	return 0;
}

static int silabs_clkout_off(const struct device *dev, clock_control_subsys_t sys)
{
	ARG_UNUSED(sys);
	silabs_clkout_apply(dev->config, false);
	return 0;
}

static DEVICE_API(clock_control, silabs_clkout_api) = {
	.on = silabs_clkout_on,
	.off = silabs_clkout_off,
};

static int silabs_clkout_init(const struct device *dev)
{
	ARG_UNUSED(dev);
	/* Leave CLKOUT gated; consumers enable via clock_control_on(). */
	return 0;
}

#define SILABS_CLKOUT_CHECK(node)                                                                  \
	BUILD_ASSERT(SILABS_CLKOUT_SEL(node) != 0xFFU,                                              \
		     "clkout: unsupported \"clocks\" source for CMU CLKOUT");

#define SILABS_CLKOUT_DEFINE(node)                                                                 \
	static const struct silabs_clkout_config silabs_clkout_cfg_##node = {                      \
		.cmu = (CMU_TypeDef *)DT_REG_ADDR(DT_PARENT(node)),                                \
		.pcfg = SILABS_CMU_PINCTRL_GET,                                                     \
		.output = (uint8_t)DT_REG_ADDR(node),                                              \
		.sel = (uint8_t)SILABS_CLKOUT_SEL(node),                                           \
		.presc = (uint8_t)(DT_PROP_OR(node, clock_div, 1) - 1),                            \
	};                                                                                         \
	DEVICE_DT_DEFINE(node, silabs_clkout_init, NULL, NULL, &silabs_clkout_cfg_##node,          \
			 PRE_KERNEL_1, CONFIG_CLOCK_CONTROL_INIT_PRIORITY, &silabs_clkout_api);

DT_FOREACH_STATUS_OKAY(SILABS_CLKOUT_COMPAT, SILABS_CLKOUT_CHECK)
DT_FOREACH_STATUS_OKAY(SILABS_CLKOUT_COMPAT, SILABS_CLKOUT_DEFINE)

#endif /* clkout */

static enum clock_control_status silabs_clock_control_get_status(const struct device *dev,
								 clock_control_subsys_t sys);

static int silabs_clock_control_on(const struct device *dev, clock_control_subsys_t sys)
{
	const struct silabs_clock_control_cmu_config *cfg = sys;
	sl_status_t status;

	if (silabs_clock_control_get_status(dev, sys) == CLOCK_CONTROL_STATUS_ON) {
		return -EALREADY;
	}

	status = sl_clock_manager_enable_bus_clock(&cfg->bus_clock);
	if (status != SL_STATUS_OK) {
		return -ENOTSUP;
	}

	return 0;
}

static int silabs_clock_control_off(const struct device *dev, clock_control_subsys_t sys)
{
	const struct silabs_clock_control_cmu_config *cfg = sys;
	sl_status_t status;

	status = sl_clock_manager_disable_bus_clock(&cfg->bus_clock);
	if (status != SL_STATUS_OK) {
		return -ENOTSUP;
	}

	return 0;
}

static int silabs_clock_control_get_rate(const struct device *dev, clock_control_subsys_t sys,
					 uint32_t *rate)
{
	const struct silabs_clock_control_cmu_config *cfg = sys;
	sl_status_t status;

	status = sl_clock_manager_get_clock_branch_frequency(cfg->branch, rate);
	if (status != SL_STATUS_OK) {
		return -ENOTSUP;
	}

	return 0;
}

static enum clock_control_status silabs_clock_control_get_status(const struct device *dev,
								 clock_control_subsys_t sys)
{
	const struct silabs_clock_control_cmu_config *cfg = sys;
	__maybe_unused const struct silabs_clock_control_config *reg = dev->config;
	uint32_t clock_status = 0;

	if (cfg->bus_clock == 0xFFFFFFFFUL) {
		return CLOCK_CONTROL_STATUS_UNKNOWN;
	}

	switch (FIELD_GET(CLOCK_REG_MASK, cfg->bus_clock)) {
#if defined(_CMU_CLKEN0_MASK)
	case 0:
		clock_status = reg->cmu->CLKEN0;
		break;
#endif
#if defined(_CMU_CLKEN1_MASK)
	case 1:
		clock_status = reg->cmu->CLKEN1;
		break;
#endif
#if defined(_CMU_CLKEN2_MASK)
	case 2:
		clock_status = reg->cmu->CLKEN2;
		break;
#endif
	default:
		__ASSERT(false, "Invalid bus clock: %x\n", cfg->bus_clock);
		break;
	}

	if (clock_status & BIT(FIELD_GET(CLOCK_BIT_MASK, cfg->bus_clock))) {
		return CLOCK_CONTROL_STATUS_ON;
	} else {
		return CLOCK_CONTROL_STATUS_OFF;
	}
}

static int silabs_clock_control_init(const struct device *dev)
{
	const struct silabs_clock_control_config *cfg = dev->config;

	sl_clock_manager_runtime_init();

#if DT_HAS_COMPAT_STATUS_OKAY(SILABS_CLKOUT_COMPAT)
	/*
	 * CLKOUT pin mux is applied in silabs_clkout_on() when a consumer
	 * enables the output — not here at PRE_KERNEL_1.
	 */
	ARG_UNUSED(cfg);
#elif DT_INST_NODE_HAS_PROP(0, pinctrl_0)
	if (cfg->pcfg != NULL) {
		(void)pinctrl_apply_state(cfg->pcfg, PINCTRL_STATE_DEFAULT);
	}
#else
	ARG_UNUSED(cfg);
#endif

	return 0;
}

static DEVICE_API(clock_control, silabs_clock_control_api) = {
	.on = silabs_clock_control_on,
	.off = silabs_clock_control_off,
	.get_rate = silabs_clock_control_get_rate,
	.get_status = silabs_clock_control_get_status,
};

static const struct silabs_clock_control_config silabs_clock_control_config = {
	.cmu = (CMU_TypeDef *)DT_INST_REG_ADDR(0),
	.pcfg = SILABS_CMU_PINCTRL_GET,
};

DEVICE_DT_INST_DEFINE(0, silabs_clock_control_init, NULL, NULL, &silabs_clock_control_config,
		      PRE_KERNEL_1, CONFIG_CLOCK_CONTROL_INIT_PRIORITY, &silabs_clock_control_api);
