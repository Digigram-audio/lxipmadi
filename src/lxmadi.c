/*
  * ALSA driver for the digigram lx audio interface
  *
  * Copyright (c) 2016 by Digigram / Jubier Sylvain <alsa@digigram.com>
  *
  *   This program is free software; you can redistribute it and/or modify
  *   it under the terms of the GNU General Public License as published by
  *   the Free Software Foundation; either version 2 of the License, or
  *   (at your option) any later version.
  *
  *   This program is distributed in the hope that it will be useful,
  *   but WITHOUT ANY WARRANTY; without even the implied warranty of
  *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  *   GNU General Public License for more details.
  *
  *   You should have received a copy of the GNU General Public License
  *   along with this program; if not, write to the Free Software
  *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
  */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/pci_ids.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/printk.h>
#include <linux/moduleparam.h>
#include <linux/version.h>

#include <sound/initval.h>
#include <sound/control.h>
#include <sound/info.h>
#include <sound/core.h>

#include "lxcommon.h"

MODULE_SUPPORTED_DEVICE("{digigram lxmadi{}}");

static int index[SNDRV_CARDS] = SNDRV_DEFAULT_IDX;
static char *id[SNDRV_CARDS] = SNDRV_DEFAULT_STR;
static bool enable[SNDRV_CARDS] = SNDRV_DEFAULT_ENABLE_PNP;

module_param_array(index, int, NULL, 0444);
MODULE_PARM_DESC(index, "Index value for Digigram LXMadi interface.");
module_param_array(id, charp, NULL, 0444);
MODULE_PARM_DESC(id, "ID string for  Digigram LXMadi interface.");
module_param_array(enable, bool, NULL, 0444);
MODULE_PARM_DESC(enable, "Enable/disable specific Digigram LXMadi soundcards.");


#define PCI_DEVICE_ID_PLX_LXMADI                PCI_DEVICE_ID_PLX_9056

static const struct pci_device_id snd_lxmadi_ids[] = {
	{
		PCI_DEVICE(PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_LXMADI),
		.subvendor = PCI_VENDOR_ID_DIGIGRAM,
		.subdevice =
			PCI_SUBDEVICE_ID_DIGIGRAM_LXMADI_SUBSYSTEM
	}, /* LXMADI */
	{0, },
};

MODULE_DEVICE_TABLE(pci, snd_lxmadi_ids);

/* defaults */
#define MADI_USE_RATE                   (SNDRV_PCM_RATE_44100 | \
					SNDRV_PCM_RATE_48000 | \
					SNDRV_PCM_RATE_88200 | \
					SNDRV_PCM_RATE_96000)
#define USE_RATE_MIN                    44100
#define USE_RATE_MAX                    96000
#define MADI_USE_CHANNELS_MIN           2
#define MADI_USE_CHANNELS_MAX           64
#define MADI_USE_PERIODS_MIN            2
#define MADI_USE_PERIODS_MAX            8
#define MADI_GRANULARITY_MIN            8
#define MADI_GRANULARITY_MAX            64
#define MADI_PERIOD_MULTIPLE_GRAN_MIN   1
#define MADI_PERIOD_MULTIPLE_GRAN_MAX   32
#define MADI_SAMPLE_SIZE_MIN            2 /*16bits/sample*/
#define MADI_SAMPLE_SIZE_MAX            3 /*24bits/sample*/
/* alsa callbacks */
static struct snd_pcm_hardware lx_madi_caps = {
	.info = (SNDRV_PCM_INFO_MMAP |
		SNDRV_PCM_INFO_INTERLEAVED |
		SNDRV_PCM_INFO_MMAP_VALID |
		SNDRV_PCM_INFO_SYNC_START),
	.formats =	(SNDRV_PCM_FMTBIT_S24_3LE |
			SNDRV_PCM_FMTBIT_S24_3BE),
	.rates = MADI_USE_RATE,
	.rate_min = 44100,
	.rate_max = 96000,
	.channels_min = MADI_USE_CHANNELS_MIN,
	.channels_max = MADI_USE_CHANNELS_MAX,
	.buffer_bytes_max =	MADI_USE_CHANNELS_MAX *
				MADI_GRANULARITY_MAX *
				MADI_PERIOD_MULTIPLE_GRAN_MAX *
				MADI_SAMPLE_SIZE_MAX *
				MADI_USE_PERIODS_MAX,
	.period_bytes_min =	MADI_USE_CHANNELS_MIN *
				MADI_GRANULARITY_MIN *
				MADI_PERIOD_MULTIPLE_GRAN_MIN *
				MADI_SAMPLE_SIZE_MIN,
	.period_bytes_max =	MADI_USE_CHANNELS_MAX *
				MADI_GRANULARITY_MAX *
				MADI_PERIOD_MULTIPLE_GRAN_MAX *
				MADI_SAMPLE_SIZE_MAX,
	.periods_min =		MADI_USE_PERIODS_MIN,
	.periods_max =		MADI_USE_PERIODS_MAX,
};

struct clocks_info {
	unsigned int madi_freq;
	unsigned int word_clock_freq;
	unsigned char diviseur;
	unsigned char wo; /* word clock direction in/out */
	unsigned char cm; /* freq event on madi clock */
	unsigned char cw; /* freq event word clock */
	unsigned int internal_freq; /* internal clock frequency */
	unsigned char clock_sync; /* synchro source madi, word clock, interne */

};

static unsigned int internal_freq_conversion[] = {44100, 48000, 88200, 96000};
static unsigned int external_freq_conversion[] = {
		8000, 11025, 12000, 160000,
		22050, 24000, 32000, 44100,
		48000, 64000, 88200, 96000,
		128000, 176400, 192000, 0
};

#define MADI_CLOCK_SYNC_MASK		0x0000003
#define MADI_GET_CLOCK_SYNC(val)	(val & MADI_CLOCK_SYNC_MASK)
#define IP_RAVENNA_FREQ_MASK		0x000000C
#define IP_GET_RAVENNA_FREQ(val)	((val & IP_RAVENNA_FREQ_MASK) >> 2)
#define MADI_CW_MASK			0x0000010
#define MADI_GET_CW(val)		((val & MADI_CW_MASK) >> 4)
#define IP_CM_MASK			0x0000020
#define IP_GET_CM(val)			((val & IP_CM_MASK) >> 5)
#define MADI_WO_MASK			0x0000040
#define MADI_GET_WO(val)		((val & MADI_WO_MASK) >> 6)
#define MADI_DIVISEUR_MASK		0x0000080
#define MADI_GET_DIVISEUR(val)		((val & MADI_DIVISEUR_MASK) >> 7)
#define MADI_EXT_WORK_CLOCK_FREQ_MASK	0x0000F00
#define MADI_GET_EXT_WORK_CLOCK_FREQ(val) \
			((val & MADI_EXT_WORK_CLOCK_FREQ_MASK) >> 8)
#define MADI_EXT_MADI_FREQ_MASK		0x000F000
#define MADI_GET_EXT_MADI_FREQ(val)	((val & MADI_EXT_MADI_FREQ_MASK) >> 12)

int lx_madi_get_clocks_status(struct lx_chip *chip,
		struct clocks_info *clocks_information)
{
	int err = 0;
	unsigned long clocks_status;

	clocks_status = lx_dsp_reg_read(chip, REG_MADI_RAVENNA_CLOCK_CFG);
/*        printk(KERN_DEBUG "%s  %lxmadi %lx\n", __func__, clocks_status);*/
	if (clocks_information != NULL) {
		clocks_information->madi_freq =
				external_freq_conversion[MADI_GET_EXT_MADI_FREQ(
						clocks_status)];
		clocks_information->word_clock_freq =
			external_freq_conversion[MADI_GET_EXT_WORK_CLOCK_FREQ(
					clocks_status)];
		clocks_information->diviseur = MADI_GET_DIVISEUR(
				clocks_status);
		clocks_information->wo = MADI_GET_WO(clocks_status);
		clocks_information->cm = IP_GET_CM(clocks_status);
		clocks_information->cw = MADI_GET_CW(clocks_status);
		clocks_information->internal_freq =
				internal_freq_conversion[IP_GET_RAVENNA_FREQ(
						clocks_status)];
		clocks_information->clock_sync = (clocks_status
				& MADI_CLOCK_SYNC_MASK);
	}
	return err;
}

int lx_madi_set_clock_diviseur(struct lx_chip *chip,
		unsigned char clock_diviseur)
{
	int err = 0;
	unsigned long clock_status;

	clock_status = lx_dsp_reg_read(chip, REG_MADI_RAVENNA_CLOCK_CFG);
	clock_status &= ~(MADI_DIVISEUR_MASK);
	clock_status |= (((0x01) & clock_diviseur) << 7);
	lx_dsp_reg_write(chip, REG_MADI_RAVENNA_CLOCK_CFG, clock_status);

	return err;
}

int lx_madi_set_clock_frequency(struct lx_chip *chip, int clock_frequency)
{
	int err = 0;
	unsigned i = 0;
	struct clocks_info clocks_information;
	unsigned char fpga_freq = 0;
	unsigned long clock_status;

	for (i = 0; i < 4; i++) {
		if (internal_freq_conversion[i] == clock_frequency)
			fpga_freq = i;
	}

	clock_status = lx_dsp_reg_read(chip, REG_MADI_RAVENNA_CLOCK_CFG);
	clock_status &= ~(IP_RAVENNA_FREQ_MASK);
	clock_status |= (fpga_freq << 2);
	lx_dsp_reg_write(chip, REG_MADI_RAVENNA_CLOCK_CFG, clock_status);

	if (chip == lx_chips_master) {
		if (lx_chips_slave != NULL) {
			/*should be updated in 50ms*/
			for (i = 0; i < 10; i++) {
				lx_madi_get_clocks_status(lx_chips_slave,
						&clocks_information);
				if (clocks_information.word_clock_freq
						== clock_frequency)
					i = 10;
				else
					mdelay(10);
			}
			if (clocks_information.word_clock_freq
					!= clock_frequency) {
				dev_err(chip->card->dev,
	"%s, be careful Master and Slave looks not synchronize by wordclock\n",
					__func__);
			}
		}
	}

	return err;
}

int lx_madi_set_clock_sync(struct lx_chip *chip, int clock_sync)
{
	int err = 0;
	unsigned long clock_status;

	clock_status = lx_dsp_reg_read(chip, REG_MADI_RAVENNA_CLOCK_CFG);
	clock_status &= ~(0x0000003);
	clock_status |= (clock_sync);
	lx_dsp_reg_write(chip, REG_MADI_RAVENNA_CLOCK_CFG, clock_status);

	return err;
}

static int set_internal_clock(struct lx_chip *chip) {
	chip->use_clock_sync = LXMADI_CLOCK_SYNC_INTERNAL;
	return lx_madi_set_clock_sync(chip, LXMADI_CLOCK_SYNC_INTERNAL);
}


int lx_madi_set_word_clock_direction(struct lx_chip *chip,
		unsigned char clock_dir)
{
	int err = 0;
	unsigned long clock_status;

	clock_status = lx_dsp_reg_read(chip, REG_MADI_RAVENNA_CLOCK_CFG);
	clock_status &= ~(0x0000040);
	clock_status |= (clock_dir << 6);
	lx_dsp_reg_write(chip, REG_MADI_RAVENNA_CLOCK_CFG, clock_status);

	return err;
}
/*Mixer		-> "Internal", "Madi In", "Word Clock In"*/
/*internal value -> "Madi In", "Word Clock In","Internal"*/
const char * const sync_names[] = {"Internal", "Madi In", "Word Clock In"};
const char mixer_to_control[] = {2, 0, 1};
const int control_to_mixer[] = {1, 2, 0};
static int snd_clock_iobox_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *info)
{
	info->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;

	return snd_ctl_enum_info(info, 1, 3, sync_names);
}

static int snd_clock_iobox_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *value)
{
	struct lx_chip *chip = snd_kcontrol_chip(kcontrol);

	value->value.enumerated.item[0] =
			control_to_mixer[chip->use_clock_sync];
	return 0;
}

static int snd_clock_iobox_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *value)
{
	struct lx_chip *chip = snd_kcontrol_chip(kcontrol);
	int changed;

	if (value->value.enumerated.item[0] > 2)
		return -EINVAL;

	changed = (value->value.enumerated.item[0]
			!= control_to_mixer[chip->use_clock_sync]);
	if (changed) {
		chip->use_clock_sync =
			mixer_to_control[value->value.enumerated.item[0]];
		lx_madi_set_clock_sync(chip, chip->use_clock_sync);
	}
	return changed;
}

static int snd_clock_rate_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *info)
{
	info->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	info->count = 3;
	info->value.integer.min = 0; /* clock not present */
	info->value.integer.max = 192000; /* max sample rate 192 kHz */
	return 0;
}

static int snd_clock_rate_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *value)
{
	struct lx_chip *chip = snd_kcontrol_chip(kcontrol);
	struct clocks_info clocks_information;

	lx_madi_get_clocks_status(chip, &clocks_information);
	/*"Internal"*/
	value->value.integer.value[0] = clocks_information.internal_freq;
	/*"Madi In",*/
	value->value.integer.value[1] = clocks_information.madi_freq;
	/* "Word Clock In"*/
	value->value.integer.value[2] = clocks_information.word_clock_freq;

	return 0;
}

const char * const word_clock_names[] = {"In", "Out"};

static int snd_word_clock_direction_iobox_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *info)
{
	return snd_ctl_enum_info(info, 1, 2, word_clock_names);
}

static int snd_word_clock_direction_iobox_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *value)
{
	struct lx_chip *chip = snd_kcontrol_chip(kcontrol);

	value->value.enumerated.item[0] = chip->word_clock_out;
	return 0;
}

static int snd_word_clock_direction_iobox_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *value)
{
	struct clocks_info clocks_information;
	struct lx_chip *chip = snd_kcontrol_chip(kcontrol);
	int changed;

	if (value->value.enumerated.item[0] > 2)
		return -EINVAL;

	changed = value->value.enumerated.item[0] != chip->word_clock_out;
	if (changed) {
		/* test before if a worldclock already present,
		 * it could damaged/destroy cards
		 */
		if (chip->word_clock_out == LXMADI_WORD_CLOCK_IN) {

			lx_madi_get_clocks_status(chip, &clocks_information);
			if (clocks_information.word_clock_freq != 0) {
				dev_warn(chip->card->dev,
	"%s, won t set wordclock out. A wordclock is already present\n",
				__func__);
				return -EACCES;
			}
		}
		chip->word_clock_out = value->value.enumerated.item[0];
		lx_madi_set_word_clock_direction(chip, chip->word_clock_out);
	}
	return changed;
}

const char * const madi_internal_clock_frequency_names[] = {
		"44100", "48000", "88200", "96000"
};
const int madi_internal_clock_frequency_value[] = {
		44100, 48000, 88200, 96000
};

static int snd_internal_clock_frequency_iobox_info(
		struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *info)
{

	return snd_ctl_enum_info(info, 1, 4,
			madi_internal_clock_frequency_names);
}

static int snd_internal_clock_frequency_iobox_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *value)
{
	struct lx_chip *chip = snd_kcontrol_chip(kcontrol);

	value->value.enumerated.item[0] = chip->madi_frequency_selector;
	return 0;
}

static int snd_internal_clock_frequency_iobox_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *value)
{
	struct lx_chip *chip = snd_kcontrol_chip(kcontrol);
	int changed;

	if (value->value.enumerated.item[0] > 4)
		return -EINVAL;

	changed = (value->value.enumerated.item[0]
			!= chip->madi_frequency_selector);
	if (changed) {
		chip->madi_frequency_selector = value->value.enumerated.item[0];
		lx_madi_set_clock_frequency(chip,
	madi_internal_clock_frequency_value[chip->madi_frequency_selector]);
	}

	return changed;
}
#ifdef FULL_MADI_MODE
const char *const madi_rx_tx_mode_names[] = {"SMUX", "LEGACY"};
const int const madi_rx_tx_mode_value[] = {0, 1};

static int snd_rx_tx_mode_iobox_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *info)
{

	return snd_ctl_enum_info(info, 1, 2, madi_rx_tx_mode_names);
}

static int snd_rx_tx_mode_iobox_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *value)
{
	struct lx_chip *chip = snd_kcontrol_chip(kcontrol);

	value->value.enumerated.item[0] = chip->rx_tx_mode;

	return 0;
}

static int snd_rx_tx_mode_iobox_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *value)
{
	struct lx_chip *chip = snd_kcontrol_chip(kcontrol);
	int changed;

	if (value->value.enumerated.item[0] > 2)
	return -EINVAL;

	changed = value->value.enumerated.item[0] != chip->rx_tx_mode;
	if (changed) {
		chip->rx_tx_mode = value->value.enumerated.item[0];
		lx_madi_set_madi_state(chip);
		lx_madi_set_clock_diviseur(chip,
				(unsigned char)chip->diviseur_mode);

	}

	return changed;
}
const char *const madi_diviseur_mode_names[] = {"256", "512"};
const int const madi_diviseur_mode_value[] = {0, 1};

static int snd_diviseur_mode_iobox_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *info)
{
	return snd_ctl_enum_info(info, 1, 2, madi_diviseur_mode_names);
}

static int snd_diviseur_mode_iobox_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *value)
{
	struct lx_chip *chip = snd_kcontrol_chip(kcontrol);

	value->value.enumerated.item[0] = chip->diviseur_mode;

	return 0;
}
static int snd_diviseur_mode_iobox_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *value)
{
	struct lx_chip *chip = snd_kcontrol_chip(kcontrol);
	int changed;

	if (value->value.enumerated.item[0] > 2)
	return -EINVAL;

	changed = value->value.enumerated.item[0] != chip->diviseur_mode;
	if (changed) {
		chip->diviseur_mode = value->value.enumerated.item[0];
		/*DO something*/
		lx_madi_set_clock_diviseur(chip,
				(unsigned char)chip->diviseur_mode);
	}

	return changed;
}
#endif

const char * const madi_channel_mode_names[] = {"56/24", "64/32"};
const int madi_channel_mode_value[] = {0, 1};

static int snd_channel_mode_iobox_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *info)
{

	return snd_ctl_enum_info(info, 1, 2, madi_channel_mode_names);
}

static int snd_channel_mode_iobox_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *value)
{
	struct lx_chip *chip = snd_kcontrol_chip(kcontrol);

	value->value.enumerated.item[0] = chip->channel_mode;

	return 0;
}

static int snd_channel_mode_iobox_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *value)
{
	struct lx_chip *chip = snd_kcontrol_chip(kcontrol);
	int changed;

	if (value->value.enumerated.item[0] > 2)
		return -EINVAL;

	changed = value->value.enumerated.item[0] != chip->channel_mode;
	if (changed) {
		chip->channel_mode = value->value.enumerated.item[0];
		lx_madi_set_madi_state(chip);
	}

	return changed;
}

const char * const madi_sync_mode_names[] = {"Independent", "Master", "Slave"};
const int madi_sync_mode_value[] = {0, 1, 2};

static int snd_sync_mode_iobox_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *info)
{
	return snd_ctl_enum_info(info, 1, 3, madi_sync_mode_names);
}

static int snd_sync_mode_iobox_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *value)
{
	struct lx_chip *chip = snd_kcontrol_chip(kcontrol);

	value->value.enumerated.item[0] = chip->multi_card_sync_mode;

	return 0;
}

static int snd_sync_mode_iobox_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *value)
{
	struct lx_chip *chip = snd_kcontrol_chip(kcontrol);
	struct snd_ctl_elem_value *valueToChange;
	int changed;
	int err = 0;

	if (value->value.enumerated.item[0] > 2)
		return -EINVAL;

	changed = value->value.enumerated.item[0] != chip->multi_card_sync_mode;
	if (changed) {
		valueToChange = vmalloc(sizeof(*valueToChange));
		memset(valueToChange, 0, sizeof(*valueToChange));
		if (!valueToChange)
			return -ENOMEM;

		switch (value->value.enumerated.item[0]) {
		case LXMADI_SYNC_INDEPENDENT:
			/*Nothing special*/
			/*enable wordclock direction mixer*/
			chip->mixer_wordclock_out_ctl->vd[0].access &=
					~SNDRV_CTL_ELEM_ACCESS_INACTIVE;
			break;

		case LXMADI_SYNC_MASTER:
			/*Disable wordclock direction mixer*/
			/*wordclock out possible if not wordclock
			 * already present
			 */
			valueToChange->value.enumerated.item[0] =
					LXMADI_WORD_CLOCK_OUT;
			err = chip->mixer_wordclock_out_ctl->put(
					chip->mixer_wordclock_out_ctl,
					valueToChange);
			if (err == -EACCES)
				return err;

			chip->mixer_wordclock_out_ctl->vd[0].access |=
			SNDRV_CTL_ELEM_ACCESS_INACTIVE;
			lx_chips_master = chip;

			break;

		case LXMADI_SYNC_SLAVE:
			/*Disable wordclock direction mixer*/
			/*wordclock in*/
			valueToChange->value.enumerated.item[0] =
					LXMADI_WORD_CLOCK_IN;
			chip->mixer_wordclock_out_ctl->put(
					chip->mixer_wordclock_out_ctl,
					valueToChange);
			chip->mixer_wordclock_out_ctl->vd[0].access |=
			SNDRV_CTL_ELEM_ACCESS_INACTIVE;
			valueToChange->value.enumerated.item[0] =
				control_to_mixer[LXMADI_CLOCK_SYNC_WORDCLOCK];
			chip->mixer_current_clock_ctl->put(
					chip->mixer_wordclock_out_ctl,
					valueToChange);
			lx_chips_slave = chip;
			break;
		default:
			dev_warn(chip->card->dev, "unknown sync\n");
			break;
		}
		chip->multi_card_sync_mode = value->value.enumerated.item[0];
		snd_ctl_notify(chip->card,
		SNDRV_CTL_EVENT_MASK_VALUE |
		SNDRV_CTL_EVENT_MASK_INFO, &chip->mixer_wordclock_out_ctl->id);
		vfree(valueToChange);
	}
	return changed;
}
const char * const madi_granularity_names[] = {
		"8", "16", "32", "64", "128", "256", "512"
};
const int madi_granularity_value[] = {8, 16, 32, 64, 128, 256, 512};

static int snd_granularity_iobox_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *info)
{
	return snd_ctl_enum_info(info, 1, 4, madi_granularity_names);
}

static int snd_granularity_iobox_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *value)
{
	struct lx_chip *chip = snd_kcontrol_chip(kcontrol);
	int i = 0;

	for (i = 0; i < 7; i++) {
		if (madi_granularity_value[i] == chip->pcm_granularity) {
			value->value.enumerated.item[0] = i;
			break;
		}
	}
	return 0;
}

static int snd_granularity_iobox_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *value)
{
	struct lx_chip *chip = snd_kcontrol_chip(kcontrol);
	int changed;
	int err;

	if (value->value.enumerated.item[0] > 4)
		return -EINVAL;

	changed = (madi_granularity_value[value->value.enumerated.item[0]]
			!= chip->pcm_granularity);
	if (changed) {
		/*set_gran if allow*/
/*
*                printk(KERN_DEBUG "%s set gran to %d\n",
*                        __func__,
*                        chip->pcm_granularity);
*/
		err = lx_set_granularity(chip,
		madi_granularity_value[value->value.enumerated.item[0]]);
		if (err < 0) {
			dev_err(chip->card->dev,
				"setting granularity to %d failed\n",
		madi_granularity_value[value->value.enumerated.item[0]]);
			return err;
		}
	}
	return changed;
}

static struct snd_kcontrol_new snd_lxmadi_controls[] = {
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Clock Mode",
		.info =	snd_clock_iobox_info,
		.get = snd_clock_iobox_get,
		.put = snd_clock_iobox_put,
	},
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Work clock direction",
		.info = snd_word_clock_direction_iobox_info,
		.get = snd_word_clock_direction_iobox_get,
		.put = snd_word_clock_direction_iobox_put,
	},
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name = "Internal Clock Frequency",
		.info = snd_internal_clock_frequency_iobox_info,
		.get = snd_internal_clock_frequency_iobox_get,
		.put = snd_internal_clock_frequency_iobox_put,
	},
/* will be accessible (without bug) in next xilinx fw*/
#ifdef FULL_MADI_MODE
		{
			.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
			.name = "Rx Tx Mode",
			.info = snd_rx_tx_mode_iobox_info,
			.get = snd_rx_tx_mode_iobox_get,
			.put = snd_rx_tx_mode_iobox_put,
			.private_value = 2,
			.index = 0,
		},
		{
			.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
			.name = "Xilinx diviseur",
			.info = snd_diviseur_mode_iobox_info,
			.get = snd_diviseur_mode_iobox_get,
			.put = snd_diviseur_mode_iobox_put,
			.private_value = 1,
			.index = 0,
		},
#endif
		{
			.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
			.name = "Channel Mode",
			.info = snd_channel_mode_iobox_info,
			.get = snd_channel_mode_iobox_get,
			.put = snd_channel_mode_iobox_put,
			.private_value = 1,
			.index = 0,
		},
		{
			.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
			.name = "Clock Sync",
			.info = snd_sync_mode_iobox_info,
			.get = snd_sync_mode_iobox_get,
			.put = snd_sync_mode_iobox_put,
			.private_value = 1,
			.index = 0,
		},
		{
			.access = SNDRV_CTL_ELEM_ACCESS_READ,
			.iface = SNDRV_CTL_ELEM_IFACE_CARD,
			.name = "Clock Rates",
			.info = snd_clock_rate_info,
			.get = snd_clock_rate_get,
		},
		{
			.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
			.name = "DMA Granularity",
			.info = snd_granularity_iobox_info,
			.get = snd_granularity_iobox_get,
			.put = snd_granularity_iobox_put,
			.private_value = 1,
			.index = 0,
		},

		{
			.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
			.name = "First channel",
			.info = snd_first_channel_selector_iobox_info,
			.get = snd_first_channel_selector_iobox_get,
			.put = snd_first_channel_selector_iobox_put,
			.private_value = 1,
			.index = 0,
		},
};

void lx_madi_proc_get_clocks_status(struct snd_info_entry *entry,
		struct snd_info_buffer *buffer)
{
	struct clocks_info clocks_information;
	struct lx_chip *chip = entry->private_data;

	lx_madi_get_clocks_status(chip, &clocks_information);

	snd_iprintf(buffer, "Madi In freq :            %d Hz\n"
			"Word Clock In freq :      %d Hz\n"
			"Clock In diviseur :       %s\n"
			"Word Clock direction :    %s\n"
			"Madi freq change :        %s\n"
			"Word Clock freq change :  %s\n"
			"Internal frequency :      %d Hz\n"
			"Sync :                    %s\n",
			clocks_information.madi_freq,
			clocks_information.word_clock_freq,
			clocks_information.diviseur ? "512" : "256",
			word_clock_names[clocks_information.wo],
			clocks_information.cm ? "True" : "False",
			clocks_information.cw ? "True" : "False",
			clocks_information.internal_freq,
		sync_names[control_to_mixer[clocks_information.clock_sync]]);
}

void lx_madi_proc_get_madi_status(struct snd_info_entry *entry,
		struct snd_info_buffer *buffer)
{
	struct madi_status status;
	struct lx_chip *chip = entry->private_data;

	lx_madi_get_madi_state(chip, &status);

	snd_iprintf(buffer, "Mute : \t%s\n"
			"channel_mode :\t%d\n"
			"tx_frame_mode :\t%d\n"
			"rx_frame_mode :\t%d\n"
			"carrier_error :\t%d\n"
			"lock_error :\t%d\n"
			"async_error :\t%d\n"
			"madi_freq :\t0x%x\n", status.mute ? "On" : "Off",
			status.channel_mode ? 64 : 56, status.tx_frame_mode,
			status.rx_frame_mode, status.carrier_error,
			status.lock_error, status.async_error,
			status.rx_frame_mode);
}

int lx_madi_proc_create(struct snd_card *card, struct lx_chip *chip)
{
	struct snd_info_entry *entry;
	/*clocks*/
	int err = snd_card_proc_new(card, "Clocks", &entry);

	if (err < 0) {
		dev_err(chip->card->dev,
			"%s, snd_card_proc_new clocks\n", __func__);
		return err;
	}

	snd_info_set_text_ops(entry, chip, lx_madi_proc_get_clocks_status);

	/*madi state*/
	err = snd_card_proc_new(card, "States", &entry);

	if (err < 0) {
		dev_err(chip->card->dev,
			"%s, snd_card_proc_new States\n", __func__);
		return err;
	}

	snd_info_set_text_ops(entry, chip, lx_madi_proc_get_madi_status);

	return 0;
}

int lx_madi_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct lx_chip *chip = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct clocks_info clocks_information;
	int err = 0;
	int i = 0;
/*        printk(KERN_DEBUG  "%s %d\n", __func__, runtime->rate);*/
	mutex_lock(&chip->setup_mutex);

	lx_madi_get_clocks_status(chip, &clocks_information);

	switch (chip->use_clock_sync) {
	case LXMADI_CLOCK_SYNC_MADI:
		/*is there a clock ?*/
		if (clocks_information.madi_freq <= 0) {
			/*clock not present*/
			dev_warn(chip->card->dev,
			"%s madi clock not present\n", __func__);
			err = -EINVAL;
			goto exit;
		}
		/*clock rate ?*/
		if (runtime->rate != clocks_information.madi_freq) {
			dev_warn(chip->card->dev,
"%s expected rate and madi clock are different expected %d found %d\n",
				__func__,
				runtime->rate,
				clocks_information.madi_freq);
			err = -EINVAL;
			goto exit;
		}

		if (chip == lx_chips_master) {
			if (lx_chips_slave != NULL) {
				/*should be updated in 50ms*/
				for (i = 0; i < 10; i++) {
					lx_madi_get_clocks_status(
							lx_chips_slave,
							&clocks_information);
					if (clocks_information.word_clock_freq
							== runtime->rate)
						i = 10;
					else
						mdelay(10);
				}
				if (clocks_information.word_clock_freq
						!= runtime->rate)
					dev_err(chip->card->dev,
	"%s, be careful Master and Slave looks not synchronize by wordclock\n",
						__func__);
			}
		}
		break;

	case LXMADI_CLOCK_SYNC_WORDCLOCK:
		/*is there a clock ?*/
		if (clocks_information.word_clock_freq <= 0) {
			/*clock not present*/
			dev_warn(chip->card->dev,
			"%s madi clock not present\n", __func__);
			err = -EINVAL;
			goto exit;
		}
		/*clock rate ?*/
		if (runtime->rate != clocks_information.word_clock_freq) {
			dev_warn(chip->card->dev,
	"%s expected rate and word clock are different expected %d found %d\n",
			__func__,
			runtime->rate,
			clocks_information.word_clock_freq);
			err = -EINVAL;
			goto exit;
		}
		break;
	case LXMADI_CLOCK_SYNC_INTERNAL:
		for (i = 0; i < 4; i++) {
			if (madi_internal_clock_frequency_value[i]
					== runtime->rate)
				chip->madi_frequency_selector = i;
		}
		if (madi_internal_clock_frequency_value[
						chip->madi_frequency_selector]
				!= runtime->rate) {
			dev_warn(chip->card->dev,
			"%s unsupported rate\n", __func__);

			err = -EINVAL;
			goto exit;
		}

		lx_madi_set_clock_frequency(chip,
	madi_internal_clock_frequency_value[chip->madi_frequency_selector]);

		break;
	default:
		/*unknown sync....*/
		dev_warn(chip->card->dev, "unknown clock sync\n");
		break;
	}
	if (runtime->rate > 48000) {
		if (chip->channel_mode == LXMADI_32_64_CHANNELS)
			runtime->hw.channels_max = 32;
		else
			runtime->hw.channels_max = 24;
	} else {
		if (chip->channel_mode == LXMADI_32_64_CHANNELS)
			runtime->hw.channels_max = 64;
		else
			runtime->hw.channels_max = 56;
	}
	if (runtime->channels > runtime->hw.channels_max) {
		dev_err(chip->card->dev,
		"%s For rate %dHz nb channels max %d !!!\n", __func__,
				runtime->rate, runtime->hw.channels_max);
		err = -EINVAL;
		goto exit;
	}

	lx_madi_set_clock_diviseur(chip, (unsigned char)chip->diviseur_mode);
	lx_madi_set_madi_state(chip);
	lx_madi_set_clock_frequency(chip, substream->runtime->rate);

	mutex_unlock(&chip->setup_mutex);
	err = lx_pcm_prepare(substream);
	return err;

exit:
	mutex_unlock(&chip->setup_mutex);
	return err;

}

#define SYNC_START
static int lxmadi_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{

	int err = 0;
#ifdef SYNC_START
	struct lx_chip *chip = snd_pcm_substream_chip(substream);
	struct lx_chip *link_chip = NULL;
	struct snd_pcm_substream *s;
	struct lx_stream *link_lx_stream;

/*	printk(KERN_DEBUG "%s cmd %x chip %p\n", __func__, cmd, chip);*/
#endif
#ifndef SYNC_START
	err = lx_pcm_trigger(substream, cmd);
	return err;
#endif
#ifdef SYNC_START
	if (chip->multi_card_sync_mode == LXMADI_SYNC_INDEPENDENT ||
	cmd == SNDRV_PCM_TRIGGER_STOP) {
/*                printk(KERN_DEBUG  "%s NORMAL TRIG\n", __func__);*/
		err = lx_pcm_trigger(substream, cmd);

	}
	/*Normal startup*/
	else {
	       /*check if there is a slave and a master*/
/*                printk(KERN_DEBUG  "%s SYNC TRIG for START\n", __func__);*/
		switch (cmd) {
		case SNDRV_PCM_TRIGGER_START:
			if (snd_pcm_stream_linked(substream)) {
				snd_pcm_group_for_each_entry(s, substream) {
					link_chip = snd_pcm_substream_chip(s);
					if (s ==
					link_chip->capture_stream.stream)
						link_lx_stream =
						&link_chip->capture_stream;
					else
						link_lx_stream =
						&link_chip->playback_stream;
					/*if command pending*/
					while (link_lx_stream->status ==
						LX_STREAM_STATUS_SCHEDULE_STOP)
						;

					link_lx_stream->status =
					LX_STREAM_STATUS_SCHEDULE_RUN;
					snd_pcm_trigger_done(s, substream);
				}

				lx_trigger_pipes_start(lx_chips_master);
				lx_trigger_pipes_start(lx_chips_slave);
				/*change 1st channel during play is forbidden otherwise
				 * we ll have problem to stop*/
				lx_chips_master->mixer_first_channel_selector_ctl->vd[0].access |=
					SNDRV_CTL_ELEM_ACCESS_INACTIVE;
				lx_chips_slave->mixer_first_channel_selector_ctl->vd[0].access |=
					SNDRV_CTL_ELEM_ACCESS_INACTIVE;
				snd_ctl_notify(lx_chips_master->card,
				SNDRV_CTL_EVENT_MASK_VALUE |
				SNDRV_CTL_EVENT_MASK_INFO, &chip->mixer_first_channel_selector_ctl->id);
				snd_ctl_notify(lx_chips_slave->card,
				SNDRV_CTL_EVENT_MASK_VALUE |
				SNDRV_CTL_EVENT_MASK_INFO, &chip->mixer_first_channel_selector_ctl->id);
			}
			break;
		default:
			/*je ne dois pas passer ici.*/
			err = -1;
			break;
		}

	}
/*        printk(KERN_DEBUG "%s, err %d\n", __func__, err);*/

	return err;
#endif
}

static struct snd_pcm_ops lx_ops_playback = {
	.open = lx_pcm_open,
	.close = lx_pcm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.prepare = lx_madi_pcm_prepare,
	.hw_params = lx_pcm_hw_params,
	.hw_free = lx_pcm_hw_free,
	.trigger = lxmadi_pcm_trigger,
	.pointer = lx_pcm_stream_pointer,
};

static struct snd_pcm_ops lx_ops_capture = {
	.open = lx_pcm_open,
	.close = lx_pcm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.prepare = lx_madi_pcm_prepare,
	.hw_params = lx_pcm_hw_params,
	.hw_free = lx_pcm_hw_free,
	.trigger = lxmadi_pcm_trigger,
	.pointer = lx_pcm_stream_pointer,
};

static int snd_lxmadi_create(struct snd_card *card, struct pci_dev *pci,
		struct lx_chip **rchip)
{
	struct lx_chip *chip;
	int err;
	unsigned int idx;
	struct snd_kcontrol *kcontrol;
	unsigned int dma_size;

	dma_size =	MADI_USE_CHANNELS_MAX * /* channels */
			MADI_SAMPLE_SIZE_MAX * /* 24 bit samples */
			MADI_USE_PERIODS_MAX * /* periods */
			MADI_GRANULARITY_MAX * /* frames per period */
			MADI_PERIOD_MULTIPLE_GRAN_MAX;/* max period size */
/*        printk(KERN_DEBUG  "%s\n", __func__);*/

	err = snd_create_generic(card, pci, rchip,
				LX_MADI, dma_size, lx_madi_caps,
				&lx_ops_playback, &lx_ops_capture);

	if (err < 0)
		return err;
	chip = *rchip;
	chip->set_internal_clock = set_internal_clock;
	err = lx_madi_proc_create(card, chip);
	if (err < 0) {
		dev_err(&pci->dev, "%s,lx_proc_create failed\n", __func__);
		goto device_new_failed;
	}

	for (idx = 0; idx < ARRAY_SIZE(snd_lxmadi_controls); idx++) {
		kcontrol = snd_ctl_new1(&snd_lxmadi_controls[idx], chip);
		err = snd_ctl_add(card, kcontrol);
		if (err < 0) {
			dev_err(&pci->dev, "%s,snd_ctl_add failed\n", __func__);
			goto device_new_failed;
		}
		if (!strcmp(kcontrol->id.name, "Work clock direction"))
			chip->mixer_wordclock_out_ctl = kcontrol;

		if (!strcmp(kcontrol->id.name, "Clock Mode"))
			chip->mixer_current_clock_ctl = kcontrol;

		if (!strcmp(kcontrol->id.name, "First channel"))
			chip->mixer_first_channel_selector_ctl = kcontrol;

	}
#if KERNEL_VERSION(4, 1, 0) <= LINUX_VERSION_CODE
	/*nothing*/
#elif KERNEL_VERSION(3, 19, 0) <= LINUX_VERSION_CODE
	snd_card_set_dev(card, &pci->dev);
#elif (KERNEL_VERSION(3, 16, 7 ) == LINUX_VERSION_CODE)
	snd_card_set_dev(card, &pci->dev);
#elif KERNEL_VERSION(3, 10, 17) <= LINUX_VERSION_CODE
	snd_card_set_dev(card, &pci->dev);
#elif KERNEL_VERSION(3, 10, 0) == LINUX_VERSION_CODE
	snd_card_set_dev(card, &pci->dev);

#endif
	*rchip = chip;

	lx_madi_set_clock_diviseur(chip, (unsigned char)chip->diviseur_mode);
	lx_madi_set_madi_state(chip);

//	err = lx_pipe_open(chip, 0, MADI_USE_CHANNELS_MAX);
//	if (err < 0) {
//		dev_err(&pci->dev, "setting lx_pipe_open failed\n");
//		goto device_new_failed;
//	}
//
//	err = lx_pipe_open(chip, 1, MADI_USE_CHANNELS_MAX);
//	if (err < 0) {
//		dev_err(&pci->dev, "setting lx_pipe_open failed\n");
//		goto device_new_failed;
//	}

	lx_chips[lx_chips_count++] = chip;

	return 0;

device_new_failed:
	if (chip->irq >= 0)
		free_irq(pci->irq, chip);

//request_irq_failed:
	pci_release_regions(pci);

//request_regions_failed:
	kfree(chip);

//alloc_failed:
	pci_disable_device(pci);

	return err;
}

static int snd_lxmadi_probe(struct pci_dev *pci,
		const struct pci_device_id *pci_id)
{
	static int dev;
	struct snd_card *card;
	struct lx_chip *chip;
	int err;
/*        printk(KERN_DEBUG  "%s\n", __func__);*/

	if (dev >= SNDRV_CARDS)
		return -ENODEV;
	if (!enable[dev]) {
		dev++;
		return -ENOENT;
	}

#if HAVE_SND_CARD_NEW
	err = snd_card_new(&pci->dev, index[dev], id[dev], THIS_MODULE,
			0, &card);
#else
        err = snd_card_create(index[dev], id[dev], THIS_MODULE, 0, &card);
#endif
	if (err < 0)
		return err;

	err = snd_lxmadi_create(card, pci, &chip);
	if (err < 0) {
		dev_err(&pci->dev,
			"%s, error during snd_lxmadi_create\n",
			__func__);
		return -ENOENT;
	}

	card->private_data = chip;
	strcpy(card->driver, "LXMADI");
	sprintf(card->id, "LX_MADI");
	sprintf(card->longname, "%s at 0x%lx, 0x%p, irq %i", card->shortname,
			chip->port_plx, chip->port_dsp_bar, chip->irq);
	strcpy(chip->card->shortname, "LXMADI");

	err = snd_card_register(card);
	if (err < 0)
		goto out_free;

	dev_err(&pci->dev,
		"%s, initialization successful dev %d lxcount %d\n",
		__func__, dev, lx_chips_count);
	pci_set_drvdata(pci, card);

	dev++;
	return 0;

out_free:
	snd_card_free(card);
	return err;

}

static struct pci_driver lxmadi_driver = {
	.name =     KBUILD_MODNAME,
/*	.name = "LX-MADI",*/
	.id_table = snd_lxmadi_ids,
	.probe = snd_lxmadi_probe,
	.remove = snd_lx_generic_remove,
};

module_pci_driver(lxmadi_driver);

MODULE_AUTHOR("Sylvain Jubier <alsa@digigram.com> ");
MODULE_DESCRIPTION("digigram lxmadi");
MODULE_LICENSE("GPL v2");
