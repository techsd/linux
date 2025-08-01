// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015-2018 Pengutronix, Uwe Kleine-König <kernel@pengutronix.de>
 */

#include <linux/module.h>
#include <linux/siox.h>
#include <linux/gpio/driver.h>
#include <linux/of.h>

struct gpio_siox_ddata {
	struct gpio_chip gchip;
	struct mutex lock;
	u8 setdata[1];
	u8 getdata[3];

	raw_spinlock_t irqlock;
	u32 irq_enable;
	u32 irq_status;
	u32 irq_type[20];
};

/*
 * Note that this callback only sets the value that is clocked out in the next
 * cycle.
 */
static int gpio_siox_set_data(struct siox_device *sdevice, u8 status, u8 buf[])
{
	struct gpio_siox_ddata *ddata = dev_get_drvdata(&sdevice->dev);

	mutex_lock(&ddata->lock);
	buf[0] = ddata->setdata[0];
	mutex_unlock(&ddata->lock);

	return 0;
}

static int gpio_siox_get_data(struct siox_device *sdevice, const u8 buf[])
{
	struct gpio_siox_ddata *ddata = dev_get_drvdata(&sdevice->dev);
	size_t offset;
	u32 trigger;

	mutex_lock(&ddata->lock);

	raw_spin_lock_irq(&ddata->irqlock);

	for (offset = 0; offset < 12; ++offset) {
		unsigned int bitpos = 11 - offset;
		unsigned int gpiolevel = buf[bitpos / 8] & (1 << bitpos % 8);
		unsigned int prev_level =
			ddata->getdata[bitpos / 8] & (1 << (bitpos % 8));
		u32 irq_type = ddata->irq_type[offset];

		if (gpiolevel) {
			if ((irq_type & IRQ_TYPE_LEVEL_HIGH) ||
			    ((irq_type & IRQ_TYPE_EDGE_RISING) && !prev_level))
				ddata->irq_status |= 1 << offset;
		} else {
			if ((irq_type & IRQ_TYPE_LEVEL_LOW) ||
			    ((irq_type & IRQ_TYPE_EDGE_FALLING) && prev_level))
				ddata->irq_status |= 1 << offset;
		}
	}

	trigger = ddata->irq_status & ddata->irq_enable;

	raw_spin_unlock_irq(&ddata->irqlock);

	ddata->getdata[0] = buf[0];
	ddata->getdata[1] = buf[1];
	ddata->getdata[2] = buf[2];

	mutex_unlock(&ddata->lock);

	for (offset = 0; offset < 12; ++offset) {
		if (trigger & (1 << offset)) {
			struct irq_domain *irqdomain = ddata->gchip.irq.domain;
			unsigned int irq = irq_find_mapping(irqdomain, offset);

			/*
			 * Conceptually handle_nested_irq should call the flow
			 * handler of the irq chip. But it doesn't, so we have
			 * to clean the irq_status here.
			 */
			raw_spin_lock_irq(&ddata->irqlock);
			ddata->irq_status &= ~(1 << offset);
			raw_spin_unlock_irq(&ddata->irqlock);

			handle_nested_irq(irq);
		}
	}

	return 0;
}

static void gpio_siox_irq_ack(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct gpio_siox_ddata *ddata = gpiochip_get_data(gc);

	raw_spin_lock(&ddata->irqlock);
	ddata->irq_status &= ~(1 << d->hwirq);
	raw_spin_unlock(&ddata->irqlock);
}

static void gpio_siox_irq_mask(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct gpio_siox_ddata *ddata = gpiochip_get_data(gc);

	raw_spin_lock(&ddata->irqlock);
	ddata->irq_enable &= ~(1 << d->hwirq);
	raw_spin_unlock(&ddata->irqlock);
	gpiochip_disable_irq(gc, irqd_to_hwirq(d));
}

static void gpio_siox_irq_unmask(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct gpio_siox_ddata *ddata = gpiochip_get_data(gc);

	gpiochip_enable_irq(gc, irqd_to_hwirq(d));
	raw_spin_lock(&ddata->irqlock);
	ddata->irq_enable |= 1 << d->hwirq;
	raw_spin_unlock(&ddata->irqlock);
}

static int gpio_siox_irq_set_type(struct irq_data *d, u32 type)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct gpio_siox_ddata *ddata = gpiochip_get_data(gc);

	raw_spin_lock(&ddata->irqlock);
	ddata->irq_type[d->hwirq] = type;
	raw_spin_unlock(&ddata->irqlock);

	return 0;
}

static int gpio_siox_get(struct gpio_chip *chip, unsigned int offset)
{
	struct gpio_siox_ddata *ddata = gpiochip_get_data(chip);
	int ret;

	mutex_lock(&ddata->lock);

	if (offset >= 12) {
		unsigned int bitpos = 19 - offset;

		ret = ddata->setdata[0] & (1 << bitpos);
	} else {
		unsigned int bitpos = 11 - offset;

		ret = ddata->getdata[bitpos / 8] & (1 << (bitpos % 8));
	}

	mutex_unlock(&ddata->lock);

	return ret;
}

static int gpio_siox_set(struct gpio_chip *chip,
			 unsigned int offset, int value)
{
	struct gpio_siox_ddata *ddata = gpiochip_get_data(chip);
	u8 mask = 1 << (19 - offset);

	mutex_lock(&ddata->lock);

	if (value)
		ddata->setdata[0] |= mask;
	else
		ddata->setdata[0] &= ~mask;

	mutex_unlock(&ddata->lock);

	return 0;
}

static int gpio_siox_direction_input(struct gpio_chip *chip,
				     unsigned int offset)
{
	if (offset >= 12)
		return -EINVAL;

	return 0;
}

static int gpio_siox_direction_output(struct gpio_chip *chip,
				      unsigned int offset, int value)
{
	if (offset < 12)
		return -EINVAL;

	return gpio_siox_set(chip, offset, value);
}

static int gpio_siox_get_direction(struct gpio_chip *chip, unsigned int offset)
{
	if (offset < 12)
		return GPIO_LINE_DIRECTION_IN;
	else
		return GPIO_LINE_DIRECTION_OUT;
}

static const struct irq_chip gpio_siox_irq_chip = {
	.name = "siox-gpio",
	.irq_ack = gpio_siox_irq_ack,
	.irq_mask = gpio_siox_irq_mask,
	.irq_unmask = gpio_siox_irq_unmask,
	.irq_set_type = gpio_siox_irq_set_type,
	.flags = IRQCHIP_IMMUTABLE,
	GPIOCHIP_IRQ_RESOURCE_HELPERS,
};

static int gpio_siox_probe(struct siox_device *sdevice)
{
	struct gpio_siox_ddata *ddata;
	struct gpio_irq_chip *girq;
	struct device *dev = &sdevice->dev;
	struct gpio_chip *gc;
	int ret;

	ddata = devm_kzalloc(dev, sizeof(*ddata), GFP_KERNEL);
	if (!ddata)
		return -ENOMEM;

	dev_set_drvdata(dev, ddata);

	mutex_init(&ddata->lock);
	raw_spin_lock_init(&ddata->irqlock);

	gc = &ddata->gchip;
	gc->base = -1;
	gc->can_sleep = 1;
	gc->parent = dev;
	gc->owner = THIS_MODULE;
	gc->get = gpio_siox_get;
	gc->set_rv = gpio_siox_set;
	gc->direction_input = gpio_siox_direction_input;
	gc->direction_output = gpio_siox_direction_output;
	gc->get_direction = gpio_siox_get_direction;
	gc->ngpio = 20;

	girq = &gc->irq;
	gpio_irq_chip_set_chip(girq, &gpio_siox_irq_chip);
	girq->default_type = IRQ_TYPE_NONE;
	girq->handler = handle_level_irq;
	girq->threaded = true;

	ret = devm_gpiochip_add_data(dev, gc, ddata);
	if (ret)
		dev_err(dev, "Failed to register gpio chip (%d)\n", ret);

	return ret;
}

static struct siox_driver gpio_siox_driver = {
	.probe = gpio_siox_probe,
	.set_data = gpio_siox_set_data,
	.get_data = gpio_siox_get_data,
	.driver = {
		.name = "gpio-siox",
	},
};
module_siox_driver(gpio_siox_driver);

MODULE_AUTHOR("Uwe Kleine-Koenig <u.kleine-koenig@pengutronix.de>");
MODULE_DESCRIPTION("SIOX gpio driver");
MODULE_LICENSE("GPL v2");
