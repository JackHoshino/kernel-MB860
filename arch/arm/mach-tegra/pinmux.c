/*
 * linux/arch/arm/mach-tegra/pinmux.c
 *
 * Copyright (C) 2010 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/spinlock.h>
#include <linux/io.h>

#include <mach/iomap.h>
#include <mach/pinmux.h>

static const struct tegra_pingroup_desc *pingroups = NULL;

static char *tegra_mux_names[TEGRA_MAX_MUX] = {
	[TEGRA_MUX_AHB_CLK] = "AHB_CLK",
	[TEGRA_MUX_APB_CLK] = "APB_CLK",
	[TEGRA_MUX_AUDIO_SYNC] = "AUDIO_SYNC",
	[TEGRA_MUX_CRT] = "CRT",
	[TEGRA_MUX_DAP1] = "DAP1",
	[TEGRA_MUX_DAP2] = "DAP2",
	[TEGRA_MUX_DAP3] = "DAP3",
	[TEGRA_MUX_DAP4] = "DAP4",
	[TEGRA_MUX_DAP5] = "DAP5",
	[TEGRA_MUX_DISPLAYA] = "DISPLAYA",
	[TEGRA_MUX_DISPLAYB] = "DISPLAYB",
	[TEGRA_MUX_EMC_TEST0_DLL] = "EMC_TEST0_DLL",
	[TEGRA_MUX_EMC_TEST1_DLL] = "EMC_TEST1_DLL",
	[TEGRA_MUX_GMI] = "GMI",
	[TEGRA_MUX_GMI_INT] = "GMI_INT",
	[TEGRA_MUX_HDMI] = "HDMI",
	[TEGRA_MUX_I2C] = "I2C",
	[TEGRA_MUX_I2C2] = "I2C2",
	[TEGRA_MUX_I2C3] = "I2C3",
	[TEGRA_MUX_IDE] = "IDE",
	[TEGRA_MUX_IRDA] = "IRDA",
	[TEGRA_MUX_KBC] = "KBC",
	[TEGRA_MUX_MIO] = "MIO",
	[TEGRA_MUX_MIPI_HS] = "MIPI_HS",
	[TEGRA_MUX_NAND] = "NAND",
	[TEGRA_MUX_OSC] = "OSC",
	[TEGRA_MUX_OWR] = "OWR",
	[TEGRA_MUX_PCIE] = "PCIE",
	[TEGRA_MUX_PLLA_OUT] = "PLLA_OUT",
	[TEGRA_MUX_PLLC_OUT1] = "PLLC_OUT1",
	[TEGRA_MUX_PLLM_OUT1] = "PLLM_OUT1",
	[TEGRA_MUX_PLLP_OUT2] = "PLLP_OUT2",
	[TEGRA_MUX_PLLP_OUT3] = "PLLP_OUT3",
	[TEGRA_MUX_PLLP_OUT4] = "PLLP_OUT4",
	[TEGRA_MUX_PWM] = "PWM",
	[TEGRA_MUX_PWR_INTR] = "PWR_INTR",
	[TEGRA_MUX_PWR_ON] = "PWR_ON",
	[TEGRA_MUX_RTCK] = "RTCK",
	[TEGRA_MUX_SDIO1] = "SDIO1",
	[TEGRA_MUX_SDIO2] = "SDIO2",
	[TEGRA_MUX_SDIO3] = "SDIO3",
	[TEGRA_MUX_SDIO4] = "SDIO4",
	[TEGRA_MUX_SFLASH] = "SFLASH",
	[TEGRA_MUX_SPDIF] = "SPDIF",
	[TEGRA_MUX_SPI1] = "SPI1",
	[TEGRA_MUX_SPI2] = "SPI2",
	[TEGRA_MUX_SPI2_ALT] = "SPI2_ALT",
	[TEGRA_MUX_SPI3] = "SPI3",
	[TEGRA_MUX_SPI4] = "SPI4",
	[TEGRA_MUX_TRACE] = "TRACE",
	[TEGRA_MUX_TWC] = "TWC",
	[TEGRA_MUX_UARTA] = "UARTA",
	[TEGRA_MUX_UARTB] = "UARTB",
	[TEGRA_MUX_UARTC] = "UARTC",
	[TEGRA_MUX_UARTD] = "UARTD",
	[TEGRA_MUX_UARTE] = "UARTE",
	[TEGRA_MUX_ULPI] = "ULPI",
	[TEGRA_MUX_VI] = "VI",
	[TEGRA_MUX_VI_SENSOR_CLK] = "VI_SENSOR_CLK",
	[TEGRA_MUX_XIO] = "XIO",
};

typedef struct pinmux_conf {
	char pg_name[5];
	char pg_mux_name[24];
	char pg_pupd_name[16];
	char pg_tri_name[17]; 
	int  vddio;
} pinmux_conf;

struct pin_tabl {
	pinmux_conf field[TEGRA_MAX_PINGROUP];
} pin_tabl;


static DEFINE_SPINLOCK(mux_lock);

static int tristate_refcount[TEGRA_MAX_PINGROUP];

static const char *pingroup_name(tegra_pingroup_t pg)
{
	if (pg < 0 || pg >=  TEGRA_MAX_PINGROUP)
		return "<UNKNOWN>";

	return pingroups[pg].name;
}

static const char *func_name(tegra_mux_func_t func)
{
	if (func == TEGRA_MUX_RSVD1)
		return "RSVD1";

	if (func == TEGRA_MUX_RSVD2)
		return "RSVD2";

	if (func == TEGRA_MUX_RSVD3)
		return "RSVD3";

	if (func == TEGRA_MUX_RSVD4)
		return "RSVD4";

	if (func == TEGRA_MUX_NONE)
		return "NONE";

	if (func < 0 || func >=  TEGRA_MAX_MUX)
		return "<UNKNOWN>";

	return tegra_mux_names[func];
}


static const char *tri_name(unsigned long val)
{
	return val ? "TRISTATE" : "NORMAL";
}

static const char *pupd_name(unsigned long val)
{
	switch (val) {
	case 0:
		return "NORMAL";

	case 1:
		return "PULL_DOWN";

	case 2:
		return "PULL_UP";

	default:
		return "RSVD";
	}
}


static inline unsigned long pg_readl(unsigned long offset)
{
	return readl(IO_TO_VIRT(TEGRA_APB_MISC_BASE + offset));
}

static inline void pg_writel(unsigned long value, unsigned long offset)
{
	writel(value, IO_TO_VIRT(TEGRA_APB_MISC_BASE + offset));
}

static int tegra_pinmux_cancel_func(const struct tegra_pingroup_config *config)
{
	int mux = -1;
	int mux_safe = -1;
	int i;
	unsigned long reg;
	unsigned long flags;
	tegra_pingroup_t pg = config->pingroup;
	tegra_mux_func_t func = config->func;

	if (pg < 0 || pg >=  TEGRA_MAX_PINGROUP)
		return -ERANGE;

	if (pingroups[pg].mux_reg < 0)
		return -EINVAL;

	if (func < 0)
		return -ERANGE;

	if (func & TEGRA_MUX_RSVD) {
		return 0;
	}

	for (i = 0; i < 4; i++) {
		if (pingroups[pg].funcs[i] == func) {
			mux = i;
			break;
		}
	}

	if (pingroups[pg].func_safe & TEGRA_MUX_RSVD) {
		mux_safe = pingroups[pg].func_safe & 3;
	} else {
		for (i = 0; i < 4; i++) {
			if (pingroups[pg].funcs[i] == pingroups[pg].func_safe) {
				mux_safe = i;
				break;
			}
		}
	}

	if (mux < 0 || mux_safe < 0)
		return -EINVAL;

	spin_lock_irqsave(&mux_lock, flags);

	reg = pg_readl(pingroups[pg].mux_reg);

	if (((reg >> pingroups[pg].mux_bit) & 0x3) == mux) {
		reg &= ~(0x3 << pingroups[pg].mux_bit);
		reg |= mux_safe << pingroups[pg].mux_bit;
		pg_writel(reg, pingroups[pg].mux_reg);
	}

	spin_unlock_irqrestore(&mux_lock, flags);
	
	snprintf(pin_tabl.field[pg].pg_mux_name, sizeof(pin_tabl.field[pg].pg_mux_name), "SAFE_MUX_%s", func_name(mux_safe));

	return 0;
}

static int tegra_pinmux_set_func(const struct tegra_pingroup_config *config)
{
	int mux = -1;
	int i;
	unsigned long reg;
	unsigned long flags;
	tegra_pingroup_t pg = config->pingroup;
	tegra_mux_func_t func = config->func;

	if (pg < 0 || pg >=  TEGRA_MAX_PINGROUP)
		return -ERANGE;

	if (pingroups[pg].mux_reg < 0)
		return -EINVAL;

	if (func < 0)
		return -ERANGE;

	if (func & TEGRA_MUX_RSVD) {
		mux = func & 0x3;
	} else {
		for (i = 0; i < 4; i++) {
			if (pingroups[pg].funcs[i] == func) {
				mux = i;
				break;
			}
		}
	}

	if (mux < 0)
		return -EINVAL;

	spin_lock_irqsave(&mux_lock, flags);

	reg = pg_readl(pingroups[pg].mux_reg);
	reg &= ~(0x3 << pingroups[pg].mux_bit);
	reg |= mux << pingroups[pg].mux_bit;
	pg_writel(reg, pingroups[pg].mux_reg);

	spin_unlock_irqrestore(&mux_lock, flags);
	
	/*snprintf(pin_tabl.field[pg].pg_mux_name, sizeof(pin_tabl.field[pg].pg_mux_name), "TEGRA_MUX_%s", tegra_mux_names[pingroups[pg].funcs[mux]]);*/

	snprintf(pin_tabl.field[pg].pg_mux_name, sizeof(pin_tabl.field[pg].pg_mux_name), "TEGRA_MUX_%s", func_name(config->func));
	return 0;
}

int tegra_pinmux_get_vddio(tegra_pingroup_t pg)
{
	if (pg < 0 || pg >=  TEGRA_MAX_PINGROUP)
		return -EINVAL;
	return pingroups[pg].vddio;
}

int tegra_pinmux_get_tristate(tegra_pingroup_t pg)
{
	unsigned long reg;

	if (pg < 0 || pg >=  TEGRA_MAX_PINGROUP)
		return 0;

	if (pingroups[pg].tri_reg < 0)
		return 0;

	reg = pg_readl(pingroups[pg].tri_reg);
	if (reg & (1 << pingroups[pg].tri_bit))
		return 0;
	else
		return 1;
}

int tegra_pinmux_set_tristate(tegra_pingroup_t pg, tegra_tristate_t tristate)
{
	unsigned long reg;
	unsigned long flags;
	bool is_update  = false;

	if (pg < 0 || pg >=  TEGRA_MAX_PINGROUP)
		return -ERANGE;

	if (pingroups[pg].tri_reg < 0)
		return -EINVAL;

	spin_lock_irqsave(&mux_lock, flags);

	if (tristate == TEGRA_TRI_NORMAL) {
		is_update = (tristate_refcount[pg] == 0);
		tristate_refcount[pg]++;
	} else {
		is_update = (tristate_refcount[pg] == 1);
		if (tristate_refcount[pg] > 0) {
			tristate_refcount[pg]--;
		}
	}
	if (is_update) {
		reg = pg_readl(pingroups[pg].tri_reg);
		reg &= ~(0x1 << pingroups[pg].tri_bit);
		if (tristate)
			reg |= 1 << pingroups[pg].tri_bit;
		pg_writel(reg, pingroups[pg].tri_reg);
	}

	spin_unlock_irqrestore(&mux_lock, flags);

	if (tristate == TEGRA_TRI_NORMAL) { 
		snprintf(pin_tabl.field[pg].pg_tri_name, sizeof(pin_tabl.field[pg].pg_tri_name), "TEGRA_TRI_NORMAL");
	} else {
		snprintf(pin_tabl.field[pg].pg_tri_name, sizeof(pin_tabl.field[pg].pg_tri_name), "TEGRA_TRI_TRISTATE");
	}
	return 0;
}

int tegra_pinmux_set_pullupdown(tegra_pingroup_t pg, tegra_pullupdown_t pupd)
{
	unsigned long reg;
	unsigned long flags;

	printk(KERN_INFO "in tegra_pinmux_set_pullupdown");
	
	if (pg < 0 || pg >=  TEGRA_MAX_PINGROUP)
		return -ERANGE;

	if (pingroups[pg].pupd_reg < 0)
		return -EINVAL;

	if (pupd != TEGRA_PUPD_NORMAL &&
	    pupd != TEGRA_PUPD_PULL_DOWN &&
	    pupd != TEGRA_PUPD_PULL_UP)
		return -EINVAL;


	spin_lock_irqsave(&mux_lock, flags);

	reg = pg_readl(pingroups[pg].pupd_reg);
	reg &= ~(0x3 << pingroups[pg].pupd_bit);
	reg |= pupd << pingroups[pg].pupd_bit;
	pg_writel(reg, pingroups[pg].pupd_reg);

	spin_unlock_irqrestore(&mux_lock, flags);

	snprintf(pin_tabl.field[pg].pg_pupd_name, sizeof(pin_tabl.field[pg].pg_pupd_name), "TEGRA_PUPD_%s", pupd_name(pupd));

	return 0;
}

static void tegra_pinmux_config_pingroup(const struct tegra_pingroup_config *config)
{
	tegra_pingroup_t pingroup = config->pingroup;
	tegra_mux_func_t func     = config->func;
	tegra_pullupdown_t pupd   = config->pupd;
	tegra_tristate_t tristate = config->tristate;
	int err;

	if (pingroups[pingroup].mux_reg >= 0) {
		err = tegra_pinmux_set_func(config);
		/*snprintf(pin_tabl.field[pingroup].pg_mux_name, sizeof(pin_tabl.field[pingroup].pg_mux_name), "TEGRA_MUX_%s", func_name(func));*/
		if (err < 0) {
			pr_err("pinmux: can't set pingroup %s func to %s: %d\n",
			       pingroup_name(pingroup), func_name(func), err);
		}
	
	}

	if (pingroups[pingroup].pupd_reg >= 0) {
		err = tegra_pinmux_set_pullupdown(pingroup, pupd);
		/*snprintf(pin_tabl.field[pingroup].pg_pupd_name, sizeof(pin_tabl.field[pingroup].pg_pupd_name), "TEGRA_PUPD_%s", pupd_name(pupd));*/
		if (err < 0) {
			pr_err("pinmux: can't set pingroup %s pullupdown to %s: %d\n",
			       pingroup_name(pingroup), pupd_name(pupd), err);
		}
	}

	if (pingroups[pingroup].tri_reg >= 0) {
		err = tegra_pinmux_set_tristate(pingroup, tristate);
		/*snprintf(pin_tabl.field[pingroup].pg_tri_name, sizeof(pin_tabl.field[pingroup].pg_tri_name), "TEGRA_TRI_%s", tri_name(func));*/
		if (err < 0) {
			pr_err("pinmux: can't set pingroup %s tristate to %s: %d\n",
			       pingroup_name(pingroup), tri_name(func), err);
		}
	}
}



void tegra_pinmux_config_table(const struct tegra_pingroup_config *config, int len)
{
	int i;

	for (i = 0; i < len; i++)
		tegra_pinmux_config_pingroup(&config[i]);
}

void tegra_pinmux_config_pinmux_table(const struct tegra_pingroup_config *config,
				      int len, bool is_set)
{
	int i;
	int err;

	for (i = 0; i < len; i++) {
		if (pingroups[config[i].pingroup].mux_reg >= 0) {
			if (is_set) err = tegra_pinmux_set_func(&config[i]);
			else err = tegra_pinmux_cancel_func(&config[i]);
			
			if (err < 0) {
				pr_err("pinmux: can't set pingroup %s func"
					" to %s: %d\n", pingroup_name(config[i].pingroup),
					func_name(config[i].func), err);
			}
		}
	}
}

void pinmux_show(void)
{
	int i;
	unsigned long flags;

	char pg_name[5];
	char pg_mux_name[24];
	char pg_pupd_name[16];
	char pg_tri_name[16];

	printk(KERN_INFO "pICS_PINMUX TABLE (PINGROUP;VDDIO;MUX;PUPD;TRISTATE)");
	
/*	spin_lock_irqsave(&mux_lock, flags);*/

	for (i = 0; i < TEGRA_MAX_PINGROUP; i++) {
#if 0
		unsigned long tri;
		unsigned long mux;
		unsigned long pupd;

		snprintf(pg_name, sizeof(pg_name), "%s",
			 pingroups[i].name);

		if (pingroups[i].mux_reg < 0) {
			snprintf(pg_mux_name, sizeof(pg_mux_name), "NONE");
		} else {
			mux = (pg_readl(pingroups[i].mux_reg) >>
			       pingroups[i].mux_bit) & 0x3;
			if (pingroups[i].funcs[mux] == TEGRA_MUX_RSVD) {
				snprintf(pg_mux_name, sizeof(pg_mux_name), "TEGRA_MUX_RSVD%1lu", mux+1);
			} else {
				snprintf(pg_mux_name, sizeof(pg_mux_name), "TEGRA_MUX_%s", tegra_mux_names[pingroups[i].funcs[mux]]);
			}
		}

		if (pingroups[i].mux_reg < 0) {
			snprintf(pg_pupd_name, sizeof(pg_pupd_name), "TEGRA_PUPD_NORMAL");
		} else {
			
			pupd = (pg_readl(pingroups[i].pupd_reg) >>
				pingroups[i].pupd_bit) & 0x3;
			snprintf(pg_pupd_name, sizeof(pg_pupd_name), "TEGRA_PUPD_%s", pupd_name(pupd));
		} /*else snprintf(pg_pupd_name, sizeof(pg_pupd_name), "PROBLEM");*/

		if (pingroups[i].tri_reg < 0) {
			snprintf(pg_tri_name, sizeof(pg_tri_name), "TEGRA_TRI_NORMAL");
		} /*else  { 
			snprintf(pg_tri_name, sizeof(pg_tri_name), "PROBLEM");
		}*/else {
			tri = (pg_readl(pingroups[i].tri_reg) >>
			       pingroups[i].tri_bit) & 0x1;
			snprintf(pg_tri_name, sizeof(pg_tri_name), "TEGRA_TRI_%s", tri_name(tri));
		}
		printk(KERN_INFO "pICS_: %s; %s; %s; %s", pg_name, pg_mux_name, pg_pupd_name, pg_tri_name);
	}
	spin_unlock_irqrestore(&mux_lock, flags);
#endif
	printk(KERN_INFO "pICS_: %s; %d; %s; %s; %s", pin_tabl.field[i].pg_name, pin_tabl.field[i].vddio, pin_tabl.field[i].pg_mux_name, pin_tabl.field[i].pg_pupd_name, pin_tabl.field[i].pg_tri_name);
	}
}


void tegra_pinmux_config_tristate_table(const struct tegra_pingroup_config *config,
					int len, tegra_tristate_t tristate)
{
	int i;
	int err;
	tegra_pingroup_t pingroup;

	for (i = 0; i < len; i++) {
		pingroup = config[i].pingroup;
		if (pingroups[pingroup].tri_reg >= 0) {
			err = tegra_pinmux_set_tristate(pingroup, tristate);
			printk(KERN_INFO "pICS_%s: pingroup %s on MUX %s setting to tristate %s\n",__func__, pingroup_name(pingroup), func_name(config[i].func), tri_name(tristate));
			snprintf(pin_tabl.field[pingroup].pg_mux_name, sizeof(pin_tabl.field[pingroup].pg_mux_name), "TEGRA_MUX_%s", func_name(config[i].func));
			snprintf(pin_tabl.field[pingroup].pg_tri_name, sizeof(pin_tabl.field[pingroup].pg_tri_name), "TEGRA_TRI_%s", tri_name(tristate));
			if (err < 0) {
				pr_err("pinmux: can't set pingroup %s tristate"
					" to %s: %d\n",	pingroup_name(pingroup),
					tri_name(tristate), err);
			} 
		}
	}
	/*pinmux_show();*/
}

void tegra_pinmux_set_vddio_tristate(tegra_vddio_t vddio,
				     tegra_tristate_t tristate)
{
	int pg;
	for (pg = 0; pg < TEGRA_MAX_PINGROUP; ++pg) {
		if (pingroups[pg].vddio == vddio &&
		    pingroups[pg].tri_reg >= 0) {
			printk(KERN_INFO "pICS_%s: vddio pingroup %s setting to tristate %s\n",__func__, pingroup_name(pg), tri_name(tristate));
			snprintf(pin_tabl.field[pg].pg_tri_name, sizeof(pin_tabl.field[pg].pg_tri_name), "TEGRA_TRI_%s", tri_name(tristate));
			pin_tabl.field[pg].vddio = vddio;
			if (tegra_pinmux_set_tristate(pg, tristate)<0) {
				pr_err("pinmux: can't set pingroup %s tristate"
				       " to %s\n", pingroup_name(pg),
				       tri_name(tristate));
			}
		}
	}
}

void tegra_pinmux_config_pullupdown_table(const struct tegra_pingroup_config *config,
					  int len, tegra_pullupdown_t pupd)
{
	int i;
	int err;
	tegra_pingroup_t pingroup;

	for (i = 0; i < len; i++) {
		pingroup = config[i].pingroup;
		if (pingroups[pingroup].pupd_reg >= 0) {
			printk(KERN_INFO "pICS_%s: pingroup %s on MUX %s setting to TEGRA_PUPD_%s\n",__func__, pingroup_name(pingroup), func_name(config[i].func), pupd_name(pupd));
			snprintf(pin_tabl.field[pingroup].pg_mux_name, sizeof(pin_tabl.field[pingroup].pg_mux_name), "TEGRA_MUX_%s", func_name(config[i].func));
			snprintf(pin_tabl.field[pingroup].pg_pupd_name, sizeof(pin_tabl.field[pingroup].pg_pupd_name), "TEGRA_PUPD_%s", pupd_name(pupd));
			err = tegra_pinmux_set_pullupdown(pingroup, pupd);
			if (err < 0) {
				pr_err("pinmux: can't set pingroup %s pullupdown"
					" to %s: %d\n",	pingroup_name(pingroup),
					pupd_name(pupd), err);
			}
		}
	}
}

void tegra_pinmux_init_pingroups(void)
{
	int i;
	unsigned long mux; 
	unsigned long flags;
	unsigned long tri;
	unsigned long pupd;

	pingroups = tegra_pinmux_get_pingroups();


	for (i = 0; i < TEGRA_MAX_PINGROUP; i++) {

		pinmux_conf *pt = &pin_tabl.field[i];

		snprintf(pt->pg_name, sizeof(pt->pg_name), "%s",
			 pingroups[i].name);
		
		if (pingroups[i].mux_reg < 0) {
			snprintf(pt->pg_mux_name, sizeof(pt->pg_mux_name), "NONE");
		} else {
			mux = (pg_readl(pingroups[i].mux_reg) >>
			       pingroups[i].mux_bit) & 0x3;
			if (pingroups[i].funcs[mux] == TEGRA_MUX_RSVD) {
				snprintf(pt->pg_mux_name, sizeof(pt->pg_mux_name), "TEGRA_MUX_RSVD%1lu", mux+1);
			} else { 
				snprintf(pt->pg_mux_name, sizeof(pt->pg_mux_name), "TEGRA_MUX_%s", tegra_mux_names[pingroups[i].funcs[mux]]);
			}
		}
		
		if (pingroups[i].mux_reg < 0) {
			snprintf(pt->pg_pupd_name, sizeof(pt->pg_pupd_name), "TEGRA_PUPD_NORMAL");
		} else {
			/*spin_lock_irqsave(&mux_lock, flags);
			pupd = (pg_readl(pingroups[i].pupd_reg) >>
				pingroups[i].pupd_bit) & 0x3;
			spin_unlock_irqrestore(&mux_lock, flags);
			snprintf(pt->pg_pupd_name, sizeof(pt->pg_pupd_name), "TEGRA_PUPD_%s", pupd_name(pupd));*/
		}
		if (pingroups[i].tri_reg < 0) {
			snprintf(pt->pg_tri_name, sizeof(pt->pg_tri_name), "TEGRA_TRI_NORMAL");
		} else {
			/*spin_lock_irqsave(&mux_lock, flags);
			tri = (pg_readl(pingroups[i].tri_reg) >>
			       pingroups[i].tri_bit) & 0x1;
			spin_unlock_irqrestore(&mux_lock, flags);
			snprintf(pt->pg_tri_name, sizeof(pt->pg_tri_name), "TEGRA_TRI_%s", tri_name(tri));*/
		}
		pt->vddio=pingroups[i].vddio;
		
	}
	

}

#ifdef	CONFIG_DEBUG_FS

#include <linux/debugfs.h>
#include <linux/seq_file.h>

static void dbg_pad_field(struct seq_file *s, int len)
{
	seq_putc(s, ',');

	while (len-- > -1)
		seq_putc(s, ' ');
}

static int dbg_pinmux_show(struct seq_file *s, void *unused)
{
	int i;
	int len;

	for (i = 0; i < TEGRA_MAX_PINGROUP; i++) {
		unsigned long tri;
		unsigned long mux;
		unsigned long pupd;

		seq_printf(s, "\t{TEGRA_PINGROUP_%s", pingroups[i].name);
		len = strlen(pingroups[i].name);
		dbg_pad_field(s, 5 - len);

		if (pingroups[i].mux_reg < 0) {
			seq_printf(s, "TEGRA_MUX_NONE");
			len = strlen("NONE");
		} else {
			mux = (pg_readl(pingroups[i].mux_reg) >>
			       pingroups[i].mux_bit) & 0x3;
			if (pingroups[i].funcs[mux] == TEGRA_MUX_RSVD) {
				seq_printf(s, "TEGRA_MUX_RSVD%1lu", mux+1);
				len = 5;
			} else {
				seq_printf(s, "TEGRA_MUX_%s",
					   tegra_mux_names[pingroups[i].funcs[mux]]);
				len = strlen(tegra_mux_names[pingroups[i].funcs[mux]]);
			}
		}
		dbg_pad_field(s, 13-len);

		if (pingroups[i].mux_reg < 0) {
			seq_printf(s, "TEGRA_PUPD_NORMAL");
			len = strlen("NORMAL");
		} else {
			pupd = (pg_readl(pingroups[i].pupd_reg) >>
				pingroups[i].pupd_bit) & 0x3;
			seq_printf(s, "TEGRA_PUPD_%s", pupd_name(pupd));
			len = strlen(pupd_name(pupd));
		}
		dbg_pad_field(s, 9 - len);

		if (pingroups[i].tri_reg < 0) {
			seq_printf(s, "TEGRA_TRI_NORMAL");
		} else {
			tri = (pg_readl(pingroups[i].tri_reg) >>
			       pingroups[i].tri_bit) & 0x1;

			seq_printf(s, "TEGRA_TRI_%s", tri_name(tri));
		}
		seq_printf(s, "},\n");
	}
	return 0;
}

static int dbg_pinmux_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_pinmux_show, &inode->i_private);
}

static const struct file_operations debug_fops = {
	.open		= dbg_pinmux_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init tegra_pinmux_debuginit(void)
{
	(void) debugfs_create_file("tegra_pinmux", S_IRUGO,
					NULL, NULL, &debug_fops);
	return 0;
}
late_initcall(tegra_pinmux_debuginit);
#endif
