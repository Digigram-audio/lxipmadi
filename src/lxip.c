/*
 *
 * ALSA driver for the digigram lx audio interface
 *
 * Copyright (c) 2016 Jubier Sylvain <alsa@digigram.com>
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/pci_ids.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/printk.h>
#include <linux/moduleparam.h>
#include <linux/version.h>

#include <sound/initval.h>
#include <sound/control.h>
#include <sound/info.h>

#include "lxcommon.h"

MODULE_AUTHOR("Sylvain Jubier <alsa@digigram.com> ");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("digigram lxip");
MODULE_SUPPORTED_DEVICE("{digigram lxip{}}");

static int index[SNDRV_CARDS] = SNDRV_DEFAULT_IDX;
static char *id[SNDRV_CARDS] = SNDRV_DEFAULT_STR;
static bool enable[SNDRV_CARDS] = SNDRV_DEFAULT_ENABLE_PNP;

module_param_array(index, int, NULL, 0444);
MODULE_PARM_DESC(index, "Index value for Digigram LX IP interface.");
module_param_array(id, charp, NULL, 0444);
MODULE_PARM_DESC(id, "ID string for  Digigram LX IP interface.");
module_param_array(enable, bool, NULL, 0444);
MODULE_PARM_DESC(enable, "Enable/disable specific Digigram LX IP soundcards.");

#define PCI_DEVICE_ID_PLX_LXIP                PCI_DEVICE_ID_PLX_9056

static const struct pci_device_id snd_lxip_ids[] = {
	{
		PCI_DEVICE(PCI_VENDOR_ID_PLX,	PCI_DEVICE_ID_PLX_LXIP),
		.subvendor = PCI_VENDOR_ID_DIGIGRAM,
		.subdevice = PCI_SUBDEVICE_ID_DIGIGRAM_LXIP_SUBSYSTEM
	}, /* LX-IP */
	{
		PCI_DEVICE(PCI_VENDOR_ID_PLX, PCI_DEVICE_ID_PLX_LXIP),
		.subvendor = PCI_VENDOR_ID_DIGIGRAM,
		.subdevice =
			PCI_SUBDEVICE_ID_DIGIGRAM_LXIP_MADI_SUBSYSTEM
	}, /* LX-IP-MADI */
	{0, },
};

MODULE_DEVICE_TABLE(pci, snd_lxip_ids);

/* defaults */
#define LXIP_USE_RATE			(SNDRV_PCM_RATE_44100 | \
					SNDRV_PCM_RATE_48000 | \
					SNDRV_PCM_RATE_88200 | \
					SNDRV_PCM_RATE_96000)
#define USE_RATE_MIN                    44100
#define USE_RATE_MAX                    96000
#define LXIP_USE_CHANNELS_MIN           2
#define LXIP_USE_CHANNELS_MAX           64
#define LXIP_USE_PERIODS_MIN            2
#define LXIP_USE_PERIODS_MAX            8	/* theoretical max : infinity
						*                   (your RAM),
						* set to 8 to reduce
						* buffer_bytes_max
*/
#define LXIP_GRANULARITY_MIN            8
#define LXIP_GRANULARITY_MAX            64
#define LXIP_PERIOD_MULTIPLE_GRAN_MIN   1
#define LXIP_PERIOD_MULTIPLE_GRAN_MAX   32	/* theoretical max : 255,
						* set to 32 to reduce
						* buffer_bytes_max
						*/
#define LXIP_SAMPLE_SIZE_MIN            2 /*16bits/sample*/
#define LXIP_SAMPLE_SIZE_MAX            3 /*24bits/sample*/
/* alsa callbacks */
static struct snd_pcm_hardware lx_ip_caps = {
		.info = (SNDRV_PCM_INFO_MMAP |
			SNDRV_PCM_INFO_INTERLEAVED |
			SNDRV_PCM_INFO_MMAP_VALID |
			SNDRV_PCM_INFO_SYNC_START),
		.formats =	(SNDRV_PCM_FMTBIT_S24_3LE |
				SNDRV_PCM_FMTBIT_S24_3BE),
		.rates = LXIP_USE_RATE,
		.rate_min = 44100,
		.rate_max = 96000,
		.channels_min = LXIP_USE_CHANNELS_MIN,
		.channels_max = LXIP_USE_CHANNELS_MAX, .buffer_bytes_max =
				LXIP_USE_CHANNELS_MAX *
				LXIP_GRANULARITY_MAX *
				LXIP_PERIOD_MULTIPLE_GRAN_MAX *
				LXIP_SAMPLE_SIZE_MAX *
				LXIP_USE_PERIODS_MAX, .period_bytes_min =
				LXIP_USE_CHANNELS_MIN *
				LXIP_GRANULARITY_MIN *
				LXIP_PERIOD_MULTIPLE_GRAN_MIN *
				LXIP_SAMPLE_SIZE_MIN, .period_bytes_max =
				LXIP_USE_CHANNELS_MAX *
				LXIP_GRANULARITY_MAX *
				LXIP_PERIOD_MULTIPLE_GRAN_MAX *
				LXIP_SAMPLE_SIZE_MAX, .periods_min =
				LXIP_USE_PERIODS_MIN, .periods_max =
				LXIP_USE_PERIODS_MAX, };

struct ravenna_clocks_info {
	unsigned char cm; /* freq event on ravennas clock */
	unsigned int ravenna_freq; /* internal clock frequency */

};

static unsigned int ravenna_freq_conversion[] = {
		8000, 11025, 12000, 160000,
		22050, 24000, 32000, 44100,
		48000, 64000, 88200, 96000,
		128000,	176400, 192000, 0
};
#define IP_RAVENNA_FREQ_MASK         0x00000F0
#define IP_GET_RAVENNA_FREQ(val)     ((val & IP_RAVENNA_FREQ_MASK) >> 4)
#define IP_CM_MASK                    0x0000001
#define IP_GET_CM(val)                ((val & IP_CM_MASK) >> 0)

int lx_ip_get_clocks_status(struct lx_chip *chip,
		struct ravenna_clocks_info *clocks_information)
{
	int err = 0;
	unsigned long clocks_status;

	clocks_status = lx_dsp_reg_read(chip, REG_MADI_RAVENNA_CLOCK_CFG);
/*	printk(KERN_DEBUG "%s  lxip %lx\n", __func__, clocks_status);*/
	if (clocks_information != NULL) {
		clocks_information->cm = IP_GET_CM(clocks_status);
		clocks_information->ravenna_freq =
				ravenna_freq_conversion[IP_GET_RAVENNA_FREQ(
						clocks_status)];
	}
	return err;
}

static int snd_clock_rate_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *info)
{
	info->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	info->count = 1;
	info->value.integer.min = 0; /* clock not present */
	info->value.integer.max = 192000; /* max sample rate 192 kHz */
	return 0;
}

static int snd_clock_rate_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *value)
{
	struct lx_chip *chip = snd_kcontrol_chip(kcontrol);
	struct ravenna_clocks_info clocks_information;

	lx_ip_get_clocks_status(chip, &clocks_information);
	value->value.integer.value[0] = clocks_information.ravenna_freq;
	return 0;
}
static int set_internal_clock(struct lx_chip *chip) {
    //we don t have yet a proper method ti change clock.
    //
    return 0;
}

/*Clock Mode "Uggly hack" for Digigram Audio Engine.*/
const char * const sync_names[] = {"Internal"};
static int snd_clock_iobox_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *info)
{
	info->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	return snd_ctl_enum_info(info, 1, 1, sync_names);
}

static int snd_clock_iobox_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *value)
{
	struct lx_chip *chip = snd_kcontrol_chip(kcontrol);

	value->value.enumerated.item[0] = chip->use_clock_sync;
	return 0;
}

static int snd_clock_iobox_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *value)
{
	struct lx_chip *chip = snd_kcontrol_chip(kcontrol);
	int changed;

	if (value->value.enumerated.item[0] > 2)
		return -EINVAL;

	changed = value->value.enumerated.item[0] != chip->use_clock_sync;
	return changed;
}

const char * const lxip_granularity_names[] = {"8", "16", "32", "64", "128",
		"256", "512"};
const int lxip_granularity_value[] = {8, 16, 32, 64, 128, 256, 512};

static int snd_granularity_iobox_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *info)
{
	return snd_ctl_enum_info(info, 1, 4, lxip_granularity_names);
}

static int snd_granularity_iobox_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *value)
{
	struct lx_chip *chip = snd_kcontrol_chip(kcontrol);
	int i = 0;

	for (i = 0; i < 7; i++) {
		if (lxip_granularity_value[i] == chip->pcm_granularity) {
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

	changed = (lxip_granularity_value[value->value.enumerated.item[0]]
			!= chip->pcm_granularity);
	if (changed) {
		/*set_gran if allow*/
/*
*                printk(KERN_DEBUG
*                       "%s set gran to %d\n",
*                       __func__,
*                       chip->pcm_granularity);
*/
		err = lx_set_granularity(chip,
		lxip_granularity_value[value->value.enumerated.item[0]]);
		if (err < 0) {
			dev_err(chip->card->dev,
		"setting granularity to %d failed\n",
		lxip_granularity_value[value->value.enumerated.item[0]]);
			return err;
		}
	}
	return changed;
}

static struct snd_kcontrol_new snd_lxip_controls[] = {
		{
			.access	= SNDRV_CTL_ELEM_ACCESS_READ,
			.iface	= SNDRV_CTL_ELEM_IFACE_CARD,
			.name	= "Clock Rates",
			.info	= snd_clock_rate_info,
			.get	= snd_clock_rate_get,
		},
		{
			.iface	= SNDRV_CTL_ELEM_IFACE_MIXER,
			.name	= "Clock Mode",
			.info	= snd_clock_iobox_info,
			.get	= snd_clock_iobox_get,
			.put	= snd_clock_iobox_put,
		},
		{
			.iface	= SNDRV_CTL_ELEM_IFACE_MIXER,
			.name	= "DMA Granularity",
			.info	= snd_granularity_iobox_info,
			.get	= snd_granularity_iobox_get,
			.put	= snd_granularity_iobox_put,
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

void lx_ip_proc_get_clocks_status(struct snd_info_entry *entry,
		struct snd_info_buffer *buffer)
{
	struct ravenna_clocks_info clocks_information;
	struct lx_chip *chip = entry->private_data;

	lx_ip_get_clocks_status(chip, &clocks_information);

	snd_iprintf(buffer, "Ravenna freq change :        %s\n"
			"Internal frequency :      %d Hz\n",
			clocks_information.cm ? "True" : "False",
			clocks_information.ravenna_freq);
}

int lx_ip_proc_create(struct snd_card *card, struct lx_chip *chip)
{
	struct snd_info_entry *entry;
	/*clocks*/
	int err = snd_card_proc_new(card, "Clocks", &entry);

	if (err < 0) {
		dev_err(chip->card->dev,
			"%s, snd_card_proc_new clocks\n", __func__);
		return err;
	}

	snd_info_set_text_ops(entry, chip, lx_ip_proc_get_clocks_status);

	return 0;
}

static struct snd_pcm_ops lx_ops_playback = {
		.open = lx_pcm_open,
		.close = lx_pcm_close,
		.ioctl = snd_pcm_lib_ioctl,
		.prepare = lx_pcm_prepare,
		.hw_params = lx_pcm_hw_params,
		.hw_free = lx_pcm_hw_free,
		.trigger = lx_pcm_trigger,
		.pointer = lx_pcm_stream_pointer,
};

static struct snd_pcm_ops lx_ops_capture = {
		.open = lx_pcm_open,
		.close = lx_pcm_close,
		.ioctl = snd_pcm_lib_ioctl,
		.prepare = lx_pcm_prepare,
		.hw_params = lx_pcm_hw_params,
		.hw_free = lx_pcm_hw_free,
		.trigger = lx_pcm_trigger,
		.pointer = lx_pcm_stream_pointer,
};

static int snd_ip_create(struct snd_card *card, struct pci_dev *pci,
		struct lx_chip **rchip)
{
	int err;
	unsigned int idx;
	struct snd_kcontrol *kcontrol;
	struct lx_chip *chip;

	unsigned int dma_size =
			LXIP_USE_CHANNELS_MAX * /* channels */
			LXIP_SAMPLE_SIZE_MAX * /* 24 bit samples */
			LXIP_USE_PERIODS_MAX * /* periods */
			LXIP_GRANULARITY_MAX * /* frames per period */
			LXIP_PERIOD_MULTIPLE_GRAN_MAX;/* max period size */

	if (pci->subsystem_device ==
	PCI_SUBDEVICE_ID_DIGIGRAM_LXIP_SUBSYSTEM) {
		err = snd_create_generic(card, pci, rchip,
					LX_IP, dma_size,
					lx_ip_caps,
					&lx_ops_playback,
					&lx_ops_capture);

	} else if (pci->subsystem_device ==
	PCI_SUBDEVICE_ID_DIGIGRAM_LXIP_MADI_SUBSYSTEM) {
		err = snd_create_generic(card, pci, rchip,
					LX_IP_MADI, dma_size, lx_ip_caps,
					&lx_ops_playback, &lx_ops_capture);
	} else
		err = -EINVAL;
	if (err < 0)
		goto exit;

	chip = *rchip;
	chip->set_internal_clock = set_internal_clock;

	err = lx_ip_proc_create(card, *rchip);
	if (err < 0) {
		dev_err(&pci->dev,
			"%s,lx_proc_create failed\n", __func__);
		goto device_new_failed;
	}

	for (idx = 0; idx < ARRAY_SIZE(snd_lxip_controls); idx++) {
		kcontrol = snd_ctl_new1(&snd_lxip_controls[idx], *rchip);
		err = snd_ctl_add(card, kcontrol);
		if (err < 0) {
			dev_err(&pci->dev,
				"%s,snd_ctl_add failed\n", __func__);
			goto device_new_failed;
		}
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

//	err = lx_pipe_open(chip, 0, LXIP_USE_CHANNELS_MAX);
//	if (err < 0) {
//		dev_err(&pci->dev,
//			"setting lx_pipe_open failed\n");
//		goto device_new_failed;
//	}
//
//	err = lx_pipe_open(chip, 1, LXIP_USE_CHANNELS_MAX);
//	if (err < 0) {
//		dev_err(&pci->dev,
//			"setting lx_pipe_open failed\n");
//		goto device_new_failed;
//	}
	*rchip = chip;

	return 0;

device_new_failed:
	if (chip->irq >= 0)
		free_irq(pci->irq, *rchip);

//request_irq_failed:
	pci_release_regions(pci);

//request_regions_failed:
	kfree(*rchip);

//alloc_failed:
	pci_disable_device(pci);

exit:
	return err;
}

static int snd_lxip_probe(struct pci_dev *pci,
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

	err = snd_ip_create(card, pci, &chip);
	if (err < 0) {
		dev_err(&pci->dev,
			"%s, error during snd_lxip_create\n", __func__);
		return -ENOENT;
	}
	card->private_data = chip;

	if (chip->lx_type == LX_IP) {
		strcpy(card->driver, "LX_IP");
		sprintf(card->id, "LX_IP");
		sprintf(card->longname, "%s at 0x%lx, 0x%p, irq %i",
				card->shortname, chip->port_plx,
				chip->port_dsp_bar, chip->irq);
		strcpy(chip->card->shortname, "LX-IP");

	} else if (chip->lx_type == LX_IP_MADI) {
		strcpy(card->driver, "LX_IP_MADI");
		sprintf(card->id, "LX_IP_MADI");
		strcpy(chip->card->shortname, "LX-IP-MADI");

	}

	err = snd_card_register(card);
	if (err < 0)
		goto out_free;

/*	printk(KERN_DEBUG "%s, initialization successful\n", __func__);*/
	pci_set_drvdata(pci, card);

	dev++;
	return 0;

out_free: snd_card_free(card);
	return err;

}

static struct pci_driver lxip_driver = {
	.name = KBUILD_MODNAME,
	/*.name = "LX_IP_MADI",*/
	.id_table = snd_lxip_ids,
	.probe = snd_lxip_probe,
	.remove = snd_lx_generic_remove,
};

module_pci_driver(lxip_driver);

