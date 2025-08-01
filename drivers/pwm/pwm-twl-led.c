// SPDX-License-Identifier: GPL-2.0-only
/*
 * Driver for TWL4030/6030 Pulse Width Modulator used as LED driver
 *
 * Copyright (C) 2012 Texas Instruments
 * Author: Peter Ujfalusi <peter.ujfalusi@ti.com>
 *
 * This driver is a complete rewrite of the former pwm-twl6030.c authorded by:
 * Hemanth V <hemanthv@ti.com>
 *
 * Reference manual for the twl6030 is available at:
 * https://www.ti.com/lit/ds/symlink/twl6030.pdf
 *
 * Limitations:
 * - The twl6030 hardware only supports two period lengths (128 clock ticks and
 *   64 clock ticks), the driver only uses 128 ticks
 * - The hardware doesn't support ON = 0, so the active part of a period doesn't
 *   start at its beginning.
 * - The hardware could support inverted polarity (with a similar limitation as
 *   for normal: the last clock tick is always inactive).
 * - The hardware emits a constant low output when disabled.
 * - A request for .duty_cycle = 0 results in an output wave with one active
 *   clock tick per period. This should better use the disabled state.
 * - The driver only implements setting the relative duty cycle.
 * - The driver doesn't implement .get_state().
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/mfd/twl.h>
#include <linux/slab.h>

/*
 * This driver handles the PWM driven LED terminals of TWL4030 and TWL6030.
 * To generate the signal on TWL4030:
 *  - LEDA uses PWMA
 *  - LEDB uses PWMB
 * TWL6030 has one LED pin with dedicated LEDPWM
 */

#define TWL4030_LED_MAX		0x7f
#define TWL6030_LED_MAX		0xff

/* Registers, bits and macro for TWL4030 */
#define TWL4030_LEDEN_REG	0x00
#define TWL4030_PWMA_REG	0x01

#define TWL4030_LEDXON		(1 << 0)
#define TWL4030_LEDXPWM		(1 << 4)
#define TWL4030_LED_PINS	(TWL4030_LEDXON | TWL4030_LEDXPWM)
#define TWL4030_LED_TOGGLE(led, x)	((x) << (led))

/* Register, bits and macro for TWL6030 */
#define TWL6030_LED_PWM_CTRL1	0xf4
#define TWL6030_LED_PWM_CTRL2	0xf5

#define TWL6040_LED_MODE_HW	0x00
#define TWL6040_LED_MODE_ON	0x01
#define TWL6040_LED_MODE_OFF	0x02
#define TWL6040_LED_MODE_MASK	0x03

static inline struct twl_pwmled_chip *to_twl(struct pwm_chip *chip)
{
	return pwmchip_get_drvdata(chip);
}

static int twl4030_pwmled_config(struct pwm_chip *chip, struct pwm_device *pwm,
			      int duty_ns, int period_ns)
{
	int duty_cycle = DIV_ROUND_UP(duty_ns * TWL4030_LED_MAX, period_ns) + 1;
	u8 pwm_config[2] = { 1, 0 };
	int base, ret;

	/*
	 * To configure the duty period:
	 * On-cycle is set to 1 (the minimum allowed value)
	 * The off time of 0 is not configurable, so the mapping is:
	 * 0 -> off cycle = 2,
	 * 1 -> off cycle = 2,
	 * 2 -> off cycle = 3,
	 * 126 - > off cycle 127,
	 * 127 - > off cycle 1
	 * When on cycle == off cycle the PWM will be always on
	 */
	if (duty_cycle == 1)
		duty_cycle = 2;
	else if (duty_cycle > TWL4030_LED_MAX)
		duty_cycle = 1;

	base = pwm->hwpwm * 2 + TWL4030_PWMA_REG;

	pwm_config[1] = duty_cycle;

	ret = twl_i2c_write(TWL4030_MODULE_LED, pwm_config, base, 2);
	if (ret < 0)
		dev_err(pwmchip_parent(chip), "%s: Failed to configure PWM\n", pwm->label);

	return ret;
}

static int twl4030_pwmled_enable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	int ret;
	u8 val;

	ret = twl_i2c_read_u8(TWL4030_MODULE_LED, &val, TWL4030_LEDEN_REG);
	if (ret < 0) {
		dev_err(pwmchip_parent(chip), "%s: Failed to read LEDEN\n", pwm->label);
		return ret;
	}

	val |= TWL4030_LED_TOGGLE(pwm->hwpwm, TWL4030_LED_PINS);

	ret = twl_i2c_write_u8(TWL4030_MODULE_LED, val, TWL4030_LEDEN_REG);
	if (ret < 0)
		dev_err(pwmchip_parent(chip), "%s: Failed to enable PWM\n", pwm->label);

	return ret;
}

static void twl4030_pwmled_disable(struct pwm_chip *chip,
				   struct pwm_device *pwm)
{
	int ret;
	u8 val;

	ret = twl_i2c_read_u8(TWL4030_MODULE_LED, &val, TWL4030_LEDEN_REG);
	if (ret < 0) {
		dev_err(pwmchip_parent(chip), "%s: Failed to read LEDEN\n", pwm->label);
		return;
	}

	val &= ~TWL4030_LED_TOGGLE(pwm->hwpwm, TWL4030_LED_PINS);

	ret = twl_i2c_write_u8(TWL4030_MODULE_LED, val, TWL4030_LEDEN_REG);
	if (ret < 0)
		dev_err(pwmchip_parent(chip), "%s: Failed to disable PWM\n", pwm->label);
}

static int twl4030_pwmled_apply(struct pwm_chip *chip, struct pwm_device *pwm,
				const struct pwm_state *state)
{
	int ret;

	if (state->polarity != PWM_POLARITY_NORMAL)
		return -EINVAL;

	if (!state->enabled) {
		if (pwm->state.enabled)
			twl4030_pwmled_disable(chip, pwm);

		return 0;
	}

	/*
	 * We cannot skip calling ->config even if state->period ==
	 * pwm->state.period && state->duty_cycle == pwm->state.duty_cycle
	 * because we might have exited early in the last call to
	 * pwm_apply_might_sleep because of !state->enabled and so the two values in
	 * pwm->state might not be configured in hardware.
	 */
	ret = twl4030_pwmled_config(chip, pwm,
				    state->duty_cycle, state->period);
	if (ret)
		return ret;

	if (!pwm->state.enabled)
		ret = twl4030_pwmled_enable(chip, pwm);

	return ret;
}


static const struct pwm_ops twl4030_pwmled_ops = {
	.apply = twl4030_pwmled_apply,
};

static int twl6030_pwmled_config(struct pwm_chip *chip, struct pwm_device *pwm,
			      int duty_ns, int period_ns)
{
	int duty_cycle = (duty_ns * TWL6030_LED_MAX) / period_ns;
	u8 on_time;
	int ret;

	on_time = duty_cycle & 0xff;

	ret = twl_i2c_write_u8(TWL6030_MODULE_ID1, on_time,
			       TWL6030_LED_PWM_CTRL1);
	if (ret < 0)
		dev_err(pwmchip_parent(chip), "%s: Failed to configure PWM\n", pwm->label);

	return ret;
}

static int twl6030_pwmled_enable(struct pwm_chip *chip, struct pwm_device *pwm)
{
	int ret;
	u8 val;

	ret = twl_i2c_read_u8(TWL6030_MODULE_ID1, &val, TWL6030_LED_PWM_CTRL2);
	if (ret < 0) {
		dev_err(pwmchip_parent(chip), "%s: Failed to read PWM_CTRL2\n",
			pwm->label);
		return ret;
	}

	val &= ~TWL6040_LED_MODE_MASK;
	val |= TWL6040_LED_MODE_ON;

	ret = twl_i2c_write_u8(TWL6030_MODULE_ID1, val, TWL6030_LED_PWM_CTRL2);
	if (ret < 0)
		dev_err(pwmchip_parent(chip), "%s: Failed to enable PWM\n", pwm->label);

	return ret;
}

static void twl6030_pwmled_disable(struct pwm_chip *chip,
				   struct pwm_device *pwm)
{
	int ret;
	u8 val;

	ret = twl_i2c_read_u8(TWL6030_MODULE_ID1, &val, TWL6030_LED_PWM_CTRL2);
	if (ret < 0) {
		dev_err(pwmchip_parent(chip), "%s: Failed to read PWM_CTRL2\n",
			pwm->label);
		return;
	}

	val &= ~TWL6040_LED_MODE_MASK;
	val |= TWL6040_LED_MODE_OFF;

	ret = twl_i2c_write_u8(TWL6030_MODULE_ID1, val, TWL6030_LED_PWM_CTRL2);
	if (ret < 0)
		dev_err(pwmchip_parent(chip), "%s: Failed to disable PWM\n", pwm->label);
}

static int twl6030_pwmled_apply(struct pwm_chip *chip, struct pwm_device *pwm,
				const struct pwm_state *state)
{
	int err;

	if (state->polarity != pwm->state.polarity)
		return -EINVAL;

	if (!state->enabled) {
		if (pwm->state.enabled)
			twl6030_pwmled_disable(chip, pwm);

		return 0;
	}

	err = twl6030_pwmled_config(chip, pwm,
				    state->duty_cycle, state->period);
	if (err)
		return err;

	if (!pwm->state.enabled)
		err = twl6030_pwmled_enable(chip, pwm);

	return err;
}

static int twl6030_pwmled_request(struct pwm_chip *chip, struct pwm_device *pwm)
{
	int ret;
	u8 val;

	ret = twl_i2c_read_u8(TWL6030_MODULE_ID1, &val, TWL6030_LED_PWM_CTRL2);
	if (ret < 0) {
		dev_err(pwmchip_parent(chip), "%s: Failed to read PWM_CTRL2\n",
			pwm->label);
		return ret;
	}

	val &= ~TWL6040_LED_MODE_MASK;
	val |= TWL6040_LED_MODE_OFF;

	ret = twl_i2c_write_u8(TWL6030_MODULE_ID1, val, TWL6030_LED_PWM_CTRL2);
	if (ret < 0)
		dev_err(pwmchip_parent(chip), "%s: Failed to request PWM\n", pwm->label);

	return ret;
}

static void twl6030_pwmled_free(struct pwm_chip *chip, struct pwm_device *pwm)
{
	int ret;
	u8 val;

	ret = twl_i2c_read_u8(TWL6030_MODULE_ID1, &val, TWL6030_LED_PWM_CTRL2);
	if (ret < 0) {
		dev_err(pwmchip_parent(chip), "%s: Failed to read PWM_CTRL2\n",
			pwm->label);
		return;
	}

	val &= ~TWL6040_LED_MODE_MASK;
	val |= TWL6040_LED_MODE_HW;

	ret = twl_i2c_write_u8(TWL6030_MODULE_ID1, val, TWL6030_LED_PWM_CTRL2);
	if (ret < 0)
		dev_err(pwmchip_parent(chip), "%s: Failed to free PWM\n", pwm->label);
}

static const struct pwm_ops twl6030_pwmled_ops = {
	.apply = twl6030_pwmled_apply,
	.request = twl6030_pwmled_request,
	.free = twl6030_pwmled_free,
};

static int twl_pwmled_probe(struct platform_device *pdev)
{
	struct pwm_chip *chip;
	unsigned int npwm;
	const struct pwm_ops *ops;

	if (twl_class_is_4030()) {
		ops = &twl4030_pwmled_ops;
		npwm = 2;
	} else {
		ops = &twl6030_pwmled_ops;
		npwm = 1;
	}

	chip = devm_pwmchip_alloc(&pdev->dev, npwm, 0);
	if (IS_ERR(chip))
		return PTR_ERR(chip);

	chip->ops = ops;

	return devm_pwmchip_add(&pdev->dev, chip);
}

#ifdef CONFIG_OF
static const struct of_device_id twl_pwmled_of_match[] = {
	{ .compatible = "ti,twl4030-pwmled" },
	{ .compatible = "ti,twl6030-pwmled" },
	{ },
};
MODULE_DEVICE_TABLE(of, twl_pwmled_of_match);
#endif

static struct platform_driver twl_pwmled_driver = {
	.driver = {
		.name = "twl-pwmled",
		.of_match_table = of_match_ptr(twl_pwmled_of_match),
	},
	.probe = twl_pwmled_probe,
};
module_platform_driver(twl_pwmled_driver);

MODULE_AUTHOR("Peter Ujfalusi <peter.ujfalusi@ti.com>");
MODULE_DESCRIPTION("PWM driver for TWL4030 and TWL6030 LED outputs");
MODULE_ALIAS("platform:twl-pwmled");
MODULE_LICENSE("GPL");
