/*
 * Copyright (c) 2026 Silicon Laboratories Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Standalone CMU CLKOUT driver (MCO) for EFR32 Series 2.
 *
 * Selects an internal clock source, divides it by an EXPORTCLK prescaler,
 * and routes the result to a GPIO pin. 
 */

 #define DT_DRV_COMPAT silabs_efr32_cmu_clkout

 #include <errno.h>
 #include <string.h>
 
 #include <zephyr/device.h>
 #include <zephyr/devicetree.h>
 #include <zephyr/logging/log.h>
 #include <zephyr/drivers/clock_control/clock_silabs_efr32_mco.h>
 
 #include <em_cmu.h>
 #include <em_gpio.h>
 
 LOG_MODULE_REGISTER(silabs_mco, CONFIG_CLOCK_CONTROL_LOG_LEVEL);
 
 #define CMU_CLKOUT0  	0
 #define CMU_CLKOUT1 	1
 #define CMU_CLKOUT2 	2
 
 struct silabs_mco_config {
	 uint8_t  gpio_port;
	 uint8_t  gpio_pin;
	 uint8_t  prescaler;
	 const char *source_name;
 };
 
 static CMU_Select_TypeDef mco_resolve_source(const char *s)
 {
	 if (strcmp(s, "hfrcodpll") == 0) { return cmuSelect_HFRCODPLL; }
	 if (strcmp(s, "hfxo")      == 0) { return cmuSelect_HFXO; }
	 if (strcmp(s, "expclk")    == 0) { return cmuSelect_EXPCLK; }
	 if (strcmp(s, "lfrco")     == 0) { return cmuSelect_LFRCO; }
	 if (strcmp(s, "lfxo")      == 0) { return cmuSelect_LFXO; }
	 if (strcmp(s, "hclk")      == 0) { return cmuSelect_HCLK; }
	 if (strcmp(s, "ulfrco")    == 0) { return cmuSelect_ULFRCO; }
	 if (strcmp(s, "fsrco")     == 0) { return cmuSelect_FSRCO; }
	 return cmuSelect_Disabled;
 }
 
 static int silabs_get_clkout_index(uint8_t gpio_port)
 {
	 switch (gpio_port) {
	 case 0: /* PORT A */
	 case 1: /* PORT B */
		 return CMU_CLKOUT2;
	 case 2: /* PORT C */
		 return CMU_CLKOUT0;
	 case 3: /* PORT D */
		 return CMU_CLKOUT1;
	 default:
		 return -EINVAL;
	 }
 }
 
 int silabs_mco_enable(const struct device *dev)
 {
	 const struct silabs_mco_config *cfg;
	 CMU_Select_TypeDef source;
	 int clkout_no;
 
	 if (dev == NULL) {
		 return -EINVAL;
	 }
 
	 cfg = dev->config;
	 source = mco_resolve_source(cfg->source_name);
 
	 if (source == cmuSelect_Disabled) {
		 return -ENOTSUP;
	 }
 
	 clkout_no = silabs_get_clkout_index(cfg->gpio_port);
	 if (clkout_no < 0) {
		 return clkout_no;
	 }
 
	 CMU_ClkOutPinConfig((uint32_t)clkout_no, source,
				 (uint32_t)cfg->prescaler,
				 (GPIO_Port_TypeDef)cfg->gpio_port,
				 (unsigned int)cfg->gpio_pin);
 
	 return 0;
 }
 
 int silabs_mco_disable(const struct device *dev)
 {
	 const struct silabs_mco_config *cfg;
	 int clkout_no;
 
	 if (dev == NULL) {
		 return -EINVAL;
	 }
 
	 cfg = dev->config;
 
	 clkout_no = silabs_get_clkout_index(cfg->gpio_port);
	 if (clkout_no < 0) {
		 return clkout_no;
	 }
 
	 CMU_ClkOutPinConfig((uint32_t)clkout_no, cmuSelect_Disabled,
				 (uint32_t)cfg->prescaler,
				 (GPIO_Port_TypeDef)cfg->gpio_port,
				 (unsigned int)cfg->gpio_pin);
 
	 return 0;
 }
 
 static int silabs_mco_init(const struct device *dev)
 {
	 const struct silabs_mco_config *cfg = dev->config;
 
	 CMU_Select_TypeDef source = mco_resolve_source(cfg->source_name);
	 if (source == cmuSelect_Disabled) {
		 return 0;
	 }
 
	 int clkout_no = silabs_get_clkout_index(cfg->gpio_port);
	 if (clkout_no < 0) {
		 LOG_ERR("invalid gpio port %u for CLKOUT", cfg->gpio_port);
		 return clkout_no;
	 }
 
	 CMU_ClkOutPinConfig((uint32_t)clkout_no, source,
				 (uint32_t)cfg->prescaler,
				 (GPIO_Port_TypeDef)cfg->gpio_port,
				 (unsigned int)cfg->gpio_pin);
 
	 LOG_DBG("CLKOUT%u -> %s  prescaler=%u  port=%u pin=%u",
		 clkout_no, cfg->source_name,
		 cfg->prescaler, cfg->gpio_port, cfg->gpio_pin);
 
	 return 0;
 }
 
 #define SILABS_MCO_INIT(inst)                                                      \
	 static const struct silabs_mco_config silabs_mco_cfg_##inst = {            \
		 .gpio_port    = (uint8_t)DT_INST_PROP(inst, silabs_clkout_gpio_port),\
		 .gpio_pin     = (uint8_t)DT_INST_PROP(inst, silabs_clkout_gpio_pin),\
		 .prescaler    = (uint8_t)DT_INST_PROP_OR(inst,                     \
					 silabs_clkout_prescaler, 1),               \
		 .source_name  = DT_INST_PROP(inst, silabs_clkout_source),          \
	 };                                                                         \
																					\
	 DEVICE_DT_INST_DEFINE(inst, silabs_mco_init, NULL, NULL,                   \
				   &silabs_mco_cfg_##inst,                              \
				   PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,    \
				   NULL);
 
 DT_INST_FOREACH_STATUS_OKAY(SILABS_MCO_INIT);
 