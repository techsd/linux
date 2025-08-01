// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) Rockchip Electronics Co., Ltd.
 * Author:
 *      Mark Yao <mark.yao@rock-chips.com>
 *      Sandy Huang <hjc@rock-chips.com>
 */

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/mfd/syscon.h>
#include <linux/of_graph.h>
#include <linux/phy/phy.h>
#include <linux/pinctrl/devinfo.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/reset.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_bridge.h>
#include <drm/drm_bridge_connector.h>
#include <drm/drm_of.h>
#include <drm/drm_panel.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_simple_kms_helper.h>

#include "rockchip_drm_drv.h"
#include "rockchip_lvds.h"

#define DISPLAY_OUTPUT_RGB		0
#define DISPLAY_OUTPUT_LVDS		1
#define DISPLAY_OUTPUT_DUAL_LVDS	2

struct rockchip_lvds;

/**
 * struct rockchip_lvds_soc_data - rockchip lvds Soc private data
 * @probe: LVDS platform probe function
 * @helper_funcs: LVDS connector helper functions
 */
struct rockchip_lvds_soc_data {
	int (*probe)(struct platform_device *pdev, struct rockchip_lvds *lvds);
	const struct drm_encoder_helper_funcs *helper_funcs;
};

struct rockchip_lvds {
	struct device *dev;
	void __iomem *regs;
	struct regmap *grf;
	struct clk *pclk;
	struct phy *dphy;
	const struct rockchip_lvds_soc_data *soc_data;
	int output; /* rgb lvds or dual lvds output */
	int format; /* vesa or jeida format */
	struct drm_device *drm_dev;
	struct drm_panel *panel;
	struct drm_bridge *bridge;
	struct rockchip_encoder encoder;
	struct dev_pin_info *pins;
};

static inline struct rockchip_lvds *brige_to_lvds(struct drm_bridge *bridge)
{
	return (struct rockchip_lvds *)bridge->driver_private;
}

static inline struct rockchip_lvds *encoder_to_lvds(struct drm_encoder *encoder)
{
	struct rockchip_encoder *rkencoder = to_rockchip_encoder(encoder);

	return container_of(rkencoder, struct rockchip_lvds, encoder);
}

static inline void rk3288_writel(struct rockchip_lvds *lvds, u32 offset,
				 u32 val)
{
	writel_relaxed(val, lvds->regs + offset);
	if (lvds->output == DISPLAY_OUTPUT_LVDS)
		return;
	writel_relaxed(val, lvds->regs + offset + RK3288_LVDS_CH1_OFFSET);
}

static inline int rockchip_lvds_name_to_format(const char *s)
{
	if (strncmp(s, "jeida-18", 8) == 0)
		return LVDS_JEIDA_18;
	else if (strncmp(s, "jeida-24", 8) == 0)
		return LVDS_JEIDA_24;
	else if (strncmp(s, "vesa-24", 7) == 0)
		return LVDS_VESA_24;

	return -EINVAL;
}

static inline int rockchip_lvds_name_to_output(const char *s)
{
	if (strncmp(s, "rgb", 3) == 0)
		return DISPLAY_OUTPUT_RGB;
	else if (strncmp(s, "lvds", 4) == 0)
		return DISPLAY_OUTPUT_LVDS;
	else if (strncmp(s, "duallvds", 8) == 0)
		return DISPLAY_OUTPUT_DUAL_LVDS;

	return -EINVAL;
}

static int
rockchip_lvds_bridge_get_modes(struct drm_bridge *bridge, struct drm_connector *connector)
{
	struct rockchip_lvds *lvds = brige_to_lvds(bridge);
	struct drm_panel *panel = lvds->panel;

	return drm_panel_get_modes(panel, connector);
}

static const
struct drm_bridge_funcs rockchip_lvds_bridge_funcs = {
	.atomic_duplicate_state = drm_atomic_helper_bridge_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_bridge_destroy_state,
	.atomic_reset = drm_atomic_helper_bridge_reset,
	.get_modes = rockchip_lvds_bridge_get_modes,
};

static int
rockchip_lvds_encoder_atomic_check(struct drm_encoder *encoder,
				   struct drm_crtc_state *crtc_state,
				   struct drm_connector_state *conn_state)
{
	struct rockchip_crtc_state *s = to_rockchip_crtc_state(crtc_state);

	s->output_mode = ROCKCHIP_OUT_MODE_P888;
	s->output_type = DRM_MODE_CONNECTOR_LVDS;

	return 0;
}

static int rk3288_lvds_poweron(struct rockchip_lvds *lvds)
{
	int ret;
	u32 val;

	ret = clk_enable(lvds->pclk);
	if (ret < 0) {
		DRM_DEV_ERROR(lvds->dev, "failed to enable lvds pclk %d\n", ret);
		return ret;
	}
	ret = pm_runtime_resume_and_get(lvds->dev);
	if (ret < 0) {
		DRM_DEV_ERROR(lvds->dev, "failed to get pm runtime: %d\n", ret);
		clk_disable(lvds->pclk);
		return ret;
	}
	val = RK3288_LVDS_CH0_REG0_LANE4_EN | RK3288_LVDS_CH0_REG0_LANE3_EN |
		RK3288_LVDS_CH0_REG0_LANE2_EN | RK3288_LVDS_CH0_REG0_LANE1_EN |
		RK3288_LVDS_CH0_REG0_LANE0_EN;
	if (lvds->output == DISPLAY_OUTPUT_RGB) {
		val |= RK3288_LVDS_CH0_REG0_TTL_EN |
			RK3288_LVDS_CH0_REG0_LANECK_EN;
		rk3288_writel(lvds, RK3288_LVDS_CH0_REG0, val);
		rk3288_writel(lvds, RK3288_LVDS_CH0_REG2,
			      RK3288_LVDS_PLL_FBDIV_REG2(0x46));
		rk3288_writel(lvds, RK3288_LVDS_CH0_REG4,
			      RK3288_LVDS_CH0_REG4_LANECK_TTL_MODE |
			      RK3288_LVDS_CH0_REG4_LANE4_TTL_MODE |
			      RK3288_LVDS_CH0_REG4_LANE3_TTL_MODE |
			      RK3288_LVDS_CH0_REG4_LANE2_TTL_MODE |
			      RK3288_LVDS_CH0_REG4_LANE1_TTL_MODE |
			      RK3288_LVDS_CH0_REG4_LANE0_TTL_MODE);
		rk3288_writel(lvds, RK3288_LVDS_CH0_REG5,
			      RK3288_LVDS_CH0_REG5_LANECK_TTL_DATA |
			      RK3288_LVDS_CH0_REG5_LANE4_TTL_DATA |
			      RK3288_LVDS_CH0_REG5_LANE3_TTL_DATA |
			      RK3288_LVDS_CH0_REG5_LANE2_TTL_DATA |
			      RK3288_LVDS_CH0_REG5_LANE1_TTL_DATA |
			      RK3288_LVDS_CH0_REG5_LANE0_TTL_DATA);
	} else {
		val |= RK3288_LVDS_CH0_REG0_LVDS_EN |
			    RK3288_LVDS_CH0_REG0_LANECK_EN;
		rk3288_writel(lvds, RK3288_LVDS_CH0_REG0, val);
		rk3288_writel(lvds, RK3288_LVDS_CH0_REG1,
			      RK3288_LVDS_CH0_REG1_LANECK_BIAS |
			      RK3288_LVDS_CH0_REG1_LANE4_BIAS |
			      RK3288_LVDS_CH0_REG1_LANE3_BIAS |
			      RK3288_LVDS_CH0_REG1_LANE2_BIAS |
			      RK3288_LVDS_CH0_REG1_LANE1_BIAS |
			      RK3288_LVDS_CH0_REG1_LANE0_BIAS);
		rk3288_writel(lvds, RK3288_LVDS_CH0_REG2,
			      RK3288_LVDS_CH0_REG2_RESERVE_ON |
			      RK3288_LVDS_CH0_REG2_LANECK_LVDS_MODE |
			      RK3288_LVDS_CH0_REG2_LANE4_LVDS_MODE |
			      RK3288_LVDS_CH0_REG2_LANE3_LVDS_MODE |
			      RK3288_LVDS_CH0_REG2_LANE2_LVDS_MODE |
			      RK3288_LVDS_CH0_REG2_LANE1_LVDS_MODE |
			      RK3288_LVDS_CH0_REG2_LANE0_LVDS_MODE |
			      RK3288_LVDS_PLL_FBDIV_REG2(0x46));
		rk3288_writel(lvds, RK3288_LVDS_CH0_REG4, 0x00);
		rk3288_writel(lvds, RK3288_LVDS_CH0_REG5, 0x00);
	}
	rk3288_writel(lvds, RK3288_LVDS_CH0_REG3,
		      RK3288_LVDS_PLL_FBDIV_REG3(0x46));
	rk3288_writel(lvds, RK3288_LVDS_CH0_REGD,
		      RK3288_LVDS_PLL_PREDIV_REGD(0x0a));
	rk3288_writel(lvds, RK3288_LVDS_CH0_REG20,
		      RK3288_LVDS_CH0_REG20_LSB);

	rk3288_writel(lvds, RK3288_LVDS_CFG_REGC,
		      RK3288_LVDS_CFG_REGC_PLL_ENABLE);
	rk3288_writel(lvds, RK3288_LVDS_CFG_REG21,
		      RK3288_LVDS_CFG_REG21_TX_ENABLE);

	return 0;
}

static void rk3288_lvds_poweroff(struct rockchip_lvds *lvds)
{
	int ret;
	u32 val;

	rk3288_writel(lvds, RK3288_LVDS_CFG_REG21,
		      RK3288_LVDS_CFG_REG21_TX_ENABLE);
	rk3288_writel(lvds, RK3288_LVDS_CFG_REGC,
		      RK3288_LVDS_CFG_REGC_PLL_ENABLE);
	val = LVDS_DUAL | LVDS_TTL_EN | LVDS_CH0_EN | LVDS_CH1_EN | LVDS_PWRDN;
	val |= val << 16;
	ret = regmap_write(lvds->grf, RK3288_LVDS_GRF_SOC_CON7, val);
	if (ret != 0)
		DRM_DEV_ERROR(lvds->dev, "Could not write to GRF: %d\n", ret);

	pm_runtime_put(lvds->dev);
	clk_disable(lvds->pclk);
}

static int rk3288_lvds_grf_config(struct drm_encoder *encoder,
				  struct drm_display_mode *mode)
{
	struct rockchip_lvds *lvds = encoder_to_lvds(encoder);
	u8 pin_hsync = (mode->flags & DRM_MODE_FLAG_PHSYNC) ? 1 : 0;
	u8 pin_dclk = (mode->flags & DRM_MODE_FLAG_PCSYNC) ? 1 : 0;
	u32 val;
	int ret;

	/* iomux to LCD data/sync mode */
	if (lvds->output == DISPLAY_OUTPUT_RGB)
		if (lvds->pins && !IS_ERR(lvds->pins->default_state))
			pinctrl_select_state(lvds->pins->p,
					     lvds->pins->default_state);
	val = lvds->format | LVDS_CH0_EN;
	if (lvds->output == DISPLAY_OUTPUT_RGB)
		val |= LVDS_TTL_EN | LVDS_CH1_EN;
	else if (lvds->output == DISPLAY_OUTPUT_DUAL_LVDS)
		val |= LVDS_DUAL | LVDS_CH1_EN;

	if ((mode->htotal - mode->hsync_start) & 0x01)
		val |= LVDS_START_PHASE_RST_1;

	val |= (pin_dclk << 8) | (pin_hsync << 9);
	val |= (0xffff << 16);
	ret = regmap_write(lvds->grf, RK3288_LVDS_GRF_SOC_CON7, val);
	if (ret)
		DRM_DEV_ERROR(lvds->dev, "Could not write to GRF: %d\n", ret);

	return ret;
}

static int rk3288_lvds_set_vop_source(struct rockchip_lvds *lvds,
				      struct drm_encoder *encoder)
{
	u32 val;
	int ret;

	ret = drm_of_encoder_active_endpoint_id(lvds->dev->of_node, encoder);
	if (ret < 0)
		return ret;

	val = RK3288_LVDS_SOC_CON6_SEL_VOP_LIT << 16;
	if (ret)
		val |= RK3288_LVDS_SOC_CON6_SEL_VOP_LIT;

	ret = regmap_write(lvds->grf, RK3288_LVDS_GRF_SOC_CON6, val);
	if (ret < 0)
		return ret;

	return 0;
}

static void rk3288_lvds_encoder_enable(struct drm_encoder *encoder)
{
	struct rockchip_lvds *lvds = encoder_to_lvds(encoder);
	struct drm_display_mode *mode = &encoder->crtc->state->adjusted_mode;
	int ret;

	drm_panel_prepare(lvds->panel);

	ret = rk3288_lvds_poweron(lvds);
	if (ret < 0) {
		DRM_DEV_ERROR(lvds->dev, "failed to power on LVDS: %d\n", ret);
		drm_panel_unprepare(lvds->panel);
		return;
	}

	ret = rk3288_lvds_grf_config(encoder, mode);
	if (ret) {
		DRM_DEV_ERROR(lvds->dev, "failed to configure LVDS: %d\n", ret);
		drm_panel_unprepare(lvds->panel);
		return;
	}

	ret = rk3288_lvds_set_vop_source(lvds, encoder);
	if (ret) {
		DRM_DEV_ERROR(lvds->dev, "failed to set VOP source: %d\n", ret);
		drm_panel_unprepare(lvds->panel);
		return;
	}

	drm_panel_enable(lvds->panel);
}

static void rk3288_lvds_encoder_disable(struct drm_encoder *encoder)
{
	struct rockchip_lvds *lvds = encoder_to_lvds(encoder);

	drm_panel_disable(lvds->panel);
	rk3288_lvds_poweroff(lvds);
	drm_panel_unprepare(lvds->panel);
}

static int px30_lvds_poweron(struct rockchip_lvds *lvds)
{
	int ret;

	ret = pm_runtime_resume_and_get(lvds->dev);
	if (ret < 0) {
		DRM_DEV_ERROR(lvds->dev, "failed to get pm runtime: %d\n", ret);
		return ret;
	}

	/* Enable LVDS mode */
	ret = regmap_update_bits(lvds->grf, PX30_LVDS_GRF_PD_VO_CON1,
				  PX30_LVDS_MODE_EN(1) | PX30_LVDS_P2S_EN(1),
				  PX30_LVDS_MODE_EN(1) | PX30_LVDS_P2S_EN(1));
	if (ret)
		pm_runtime_put(lvds->dev);

	return ret;
}

static void px30_lvds_poweroff(struct rockchip_lvds *lvds)
{
	regmap_update_bits(lvds->grf, PX30_LVDS_GRF_PD_VO_CON1,
			   PX30_LVDS_MODE_EN(1) | PX30_LVDS_P2S_EN(1),
			   PX30_LVDS_MODE_EN(0) | PX30_LVDS_P2S_EN(0));

	pm_runtime_put(lvds->dev);
}

static int px30_lvds_grf_config(struct drm_encoder *encoder,
				struct drm_display_mode *mode)
{
	struct rockchip_lvds *lvds = encoder_to_lvds(encoder);

	if (lvds->output != DISPLAY_OUTPUT_LVDS) {
		DRM_DEV_ERROR(lvds->dev, "Unsupported display output %d\n",
			      lvds->output);
		return -EINVAL;
	}

	/* Set format */
	return regmap_update_bits(lvds->grf, PX30_LVDS_GRF_PD_VO_CON1,
				  PX30_LVDS_FORMAT(lvds->format),
				  PX30_LVDS_FORMAT(lvds->format));
}

static int px30_lvds_set_vop_source(struct rockchip_lvds *lvds,
				    struct drm_encoder *encoder)
{
	int vop;

	vop = drm_of_encoder_active_endpoint_id(lvds->dev->of_node, encoder);
	if (vop < 0)
		return vop;

	return regmap_update_bits(lvds->grf, PX30_LVDS_GRF_PD_VO_CON1,
				  PX30_LVDS_VOP_SEL(1),
				  PX30_LVDS_VOP_SEL(vop));
}

static void px30_lvds_encoder_enable(struct drm_encoder *encoder)
{
	struct rockchip_lvds *lvds = encoder_to_lvds(encoder);
	struct drm_display_mode *mode = &encoder->crtc->state->adjusted_mode;
	int ret;

	drm_panel_prepare(lvds->panel);

	ret = px30_lvds_poweron(lvds);
	if (ret) {
		DRM_DEV_ERROR(lvds->dev, "failed to power on LVDS: %d\n", ret);
		drm_panel_unprepare(lvds->panel);
		return;
	}

	ret = px30_lvds_grf_config(encoder, mode);
	if (ret) {
		DRM_DEV_ERROR(lvds->dev, "failed to configure LVDS: %d\n", ret);
		drm_panel_unprepare(lvds->panel);
		return;
	}

	ret = px30_lvds_set_vop_source(lvds, encoder);
	if (ret) {
		DRM_DEV_ERROR(lvds->dev, "failed to set VOP source: %d\n", ret);
		drm_panel_unprepare(lvds->panel);
		return;
	}

	drm_panel_enable(lvds->panel);
}

static void px30_lvds_encoder_disable(struct drm_encoder *encoder)
{
	struct rockchip_lvds *lvds = encoder_to_lvds(encoder);

	drm_panel_disable(lvds->panel);
	px30_lvds_poweroff(lvds);
	drm_panel_unprepare(lvds->panel);
}

static const
struct drm_encoder_helper_funcs rk3288_lvds_encoder_helper_funcs = {
	.enable = rk3288_lvds_encoder_enable,
	.disable = rk3288_lvds_encoder_disable,
	.atomic_check = rockchip_lvds_encoder_atomic_check,
};

static const
struct drm_encoder_helper_funcs px30_lvds_encoder_helper_funcs = {
	.enable = px30_lvds_encoder_enable,
	.disable = px30_lvds_encoder_disable,
	.atomic_check = rockchip_lvds_encoder_atomic_check,
};

static int rk3288_lvds_probe(struct platform_device *pdev,
			     struct rockchip_lvds *lvds)
{
	lvds->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(lvds->regs))
		return PTR_ERR(lvds->regs);

	lvds->pclk = devm_clk_get_prepared(lvds->dev, "pclk_lvds");
	if (IS_ERR(lvds->pclk))
		return dev_err_probe(lvds->dev, PTR_ERR(lvds->pclk),
				     "could not get or prepare pclk_lvds\n");

	lvds->pins = devm_kzalloc(lvds->dev, sizeof(*lvds->pins),
				  GFP_KERNEL);
	if (!lvds->pins)
		return -ENOMEM;

	lvds->pins->p = devm_pinctrl_get(lvds->dev);
	if (IS_ERR(lvds->pins->p)) {
		dev_warn(lvds->dev, "no pinctrl handle\n");
		devm_kfree(lvds->dev, lvds->pins);
		lvds->pins = NULL;
	} else {
		lvds->pins->default_state =
			pinctrl_lookup_state(lvds->pins->p, "lcdc");
		if (IS_ERR(lvds->pins->default_state)) {
			dev_warn(lvds->dev, "no default pinctrl state\n");
			devm_kfree(lvds->dev, lvds->pins);
			lvds->pins = NULL;
		}
	}

	return 0;
}

static int px30_lvds_probe(struct platform_device *pdev,
			   struct rockchip_lvds *lvds)
{
	int ret;

	/* MSB */
	ret =  regmap_update_bits(lvds->grf, PX30_LVDS_GRF_PD_VO_CON1,
				  PX30_LVDS_MSBSEL(1),
				  PX30_LVDS_MSBSEL(1));
	if (ret)
		return ret;

	/* PHY */
	lvds->dphy = devm_phy_get(&pdev->dev, "dphy");
	if (IS_ERR(lvds->dphy))
		return PTR_ERR(lvds->dphy);

	ret = phy_init(lvds->dphy);
	if (ret)
		return ret;

	ret = phy_set_mode(lvds->dphy, PHY_MODE_LVDS);
	if (ret)
		return ret;

	return phy_power_on(lvds->dphy);
}

static const struct rockchip_lvds_soc_data rk3288_lvds_data = {
	.probe = rk3288_lvds_probe,
	.helper_funcs = &rk3288_lvds_encoder_helper_funcs,
};

static const struct rockchip_lvds_soc_data px30_lvds_data = {
	.probe = px30_lvds_probe,
	.helper_funcs = &px30_lvds_encoder_helper_funcs,
};

static const struct of_device_id rockchip_lvds_dt_ids[] = {
	{
		.compatible = "rockchip,rk3288-lvds",
		.data = &rk3288_lvds_data
	},
	{
		.compatible = "rockchip,px30-lvds",
		.data = &px30_lvds_data
	},
	{}
};
MODULE_DEVICE_TABLE(of, rockchip_lvds_dt_ids);

static int rockchip_lvds_bind(struct device *dev, struct device *master,
			      void *data)
{
	struct rockchip_lvds *lvds = dev_get_drvdata(dev);
	struct drm_device *drm_dev = data;
	struct drm_encoder *encoder;
	struct drm_connector *connector;
	struct device_node *remote = NULL;
	struct device_node  *port, *endpoint;
	int ret = 0, child_count = 0;
	const char *name;
	u32 endpoint_id = 0;

	lvds->drm_dev = drm_dev;
	port = of_graph_get_port_by_id(dev->of_node, 1);
	if (!port)
		return dev_err_probe(dev, -EINVAL,
				     "can't found port point, please init lvds panel port!\n");

	for_each_child_of_node(port, endpoint) {
		child_count++;
		of_property_read_u32(endpoint, "reg", &endpoint_id);
		ret = drm_of_find_panel_or_bridge(dev->of_node, 1, endpoint_id,
						  &lvds->panel, &lvds->bridge);
		if (!ret) {
			of_node_put(endpoint);
			break;
		}
	}
	if (!child_count) {
		ret = dev_err_probe(dev, -EINVAL, "lvds port does not have any children\n");
		goto err_put_port;
	} else if (ret) {
		dev_err_probe(dev, ret, "failed to find panel and bridge node\n");
		goto err_put_port;
	}
	if (lvds->panel)
		remote = lvds->panel->dev->of_node;
	else
		remote = lvds->bridge->of_node;
	if (of_property_read_string(dev->of_node, "rockchip,output", &name))
		/* default set it as output rgb */
		lvds->output = DISPLAY_OUTPUT_RGB;
	else
		lvds->output = rockchip_lvds_name_to_output(name);

	if (lvds->output < 0) {
		ret = dev_err_probe(dev, lvds->output, "invalid output type [%s]\n", name);
		goto err_put_remote;
	}

	if (of_property_read_string(remote, "data-mapping", &name))
		/* default set it as format vesa 18 */
		lvds->format = LVDS_VESA_18;
	else
		lvds->format = rockchip_lvds_name_to_format(name);

	if (lvds->format < 0) {
		ret = dev_err_probe(dev, lvds->format,
				    "invalid data-mapping format [%s]\n", name);
		goto err_put_remote;
	}

	encoder = &lvds->encoder.encoder;
	encoder->possible_crtcs = drm_of_find_possible_crtcs(drm_dev,
							     dev->of_node);

	ret = drm_simple_encoder_init(drm_dev, encoder, DRM_MODE_ENCODER_LVDS);
	if (ret < 0) {
		drm_err(drm_dev,
			"failed to initialize encoder: %d\n", ret);
		goto err_put_remote;
	}

	drm_encoder_helper_add(encoder, lvds->soc_data->helper_funcs);

	if (lvds->panel) {
		lvds->bridge = drm_panel_bridge_add_typed(lvds->panel, DRM_MODE_CONNECTOR_LVDS);
		if (IS_ERR(lvds->bridge)) {
			ret = PTR_ERR(lvds->bridge);
			goto err_free_encoder;
		}
	}

	if (lvds->bridge) {
		lvds->bridge->driver_private = lvds;
		lvds->bridge->ops = DRM_BRIDGE_OP_MODES;
		lvds->bridge->funcs = &rockchip_lvds_bridge_funcs;

		ret = drm_bridge_attach(encoder, lvds->bridge, NULL, DRM_BRIDGE_ATTACH_NO_CONNECTOR);
		if (ret)
			goto err_free_bridge;

		connector = drm_bridge_connector_init(lvds->drm_dev, encoder);
		if (IS_ERR(connector)) {
			drm_err(drm_dev,
				"failed to initialize bridge connector: %pe\n",
				connector);
			ret = PTR_ERR(connector);
			goto err_free_bridge;
		}

		ret = drm_connector_attach_encoder(connector, encoder);
		if (ret < 0) {
			drm_err(drm_dev, "failed to attach encoder: %d\n", ret);
			goto err_free_bridge;
		}
	}

	pm_runtime_enable(dev);
	of_node_put(remote);
	of_node_put(port);

	return 0;

err_free_bridge:
	drm_panel_bridge_remove(lvds->bridge);
err_free_encoder:
	drm_encoder_cleanup(encoder);
err_put_remote:
	of_node_put(remote);
err_put_port:
	of_node_put(port);

	return ret;
}

static void rockchip_lvds_unbind(struct device *dev, struct device *master,
				void *data)
{
	struct rockchip_lvds *lvds = dev_get_drvdata(dev);
	const struct drm_encoder_helper_funcs *encoder_funcs;

	encoder_funcs = lvds->soc_data->helper_funcs;
	encoder_funcs->disable(&lvds->encoder.encoder);
	pm_runtime_disable(dev);
}

static const struct component_ops rockchip_lvds_component_ops = {
	.bind = rockchip_lvds_bind,
	.unbind = rockchip_lvds_unbind,
};

static int rockchip_lvds_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct rockchip_lvds *lvds;
	const struct of_device_id *match;
	int ret;

	if (!dev->of_node)
		return -ENODEV;

	lvds = devm_kzalloc(&pdev->dev, sizeof(*lvds), GFP_KERNEL);
	if (!lvds)
		return -ENOMEM;

	lvds->dev = dev;
	match = of_match_node(rockchip_lvds_dt_ids, dev->of_node);
	if (!match)
		return -ENODEV;
	lvds->soc_data = match->data;

	lvds->grf = syscon_regmap_lookup_by_phandle(dev->of_node,
						    "rockchip,grf");
	if (IS_ERR(lvds->grf))
		return dev_err_probe(dev, PTR_ERR(lvds->grf), "missing rockchip,grf property\n");

	ret = lvds->soc_data->probe(pdev, lvds);
	if (ret)
		return dev_err_probe(dev, ret, "Platform initialization failed\n");

	dev_set_drvdata(dev, lvds);

	ret = component_add(&pdev->dev, &rockchip_lvds_component_ops);
	if (ret < 0)
		return dev_err_probe(dev, ret, "failed to add component\n");

	return 0;
}

static void rockchip_lvds_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &rockchip_lvds_component_ops);
}

struct platform_driver rockchip_lvds_driver = {
	.probe = rockchip_lvds_probe,
	.remove = rockchip_lvds_remove,
	.driver = {
		   .name = "rockchip-lvds",
		   .of_match_table = rockchip_lvds_dt_ids,
	},
};
