/* -*- linux-c -*- *
 *
 * ALSA driver for the digigram lx6464es interface
 *
 * Copyright (c) 2008, 2009 Tim Blechmann <tim@klingt.org>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/pci_ids.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/printk.h>       //TODO SJR à enlever
#include <linux/moduleparam.h>
#include <linux/version.h>

#include <sound/initval.h>
#include <sound/control.h>
#include <sound/info.h>

#include "lxcommon.h"

MODULE_AUTHOR("Tim Blechmann");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("digigram lx6464es");
MODULE_SUPPORTED_DEVICE("{digigram lx6464es{}}");


static int index[SNDRV_CARDS] = SNDRV_DEFAULT_IDX;
static char *id[SNDRV_CARDS] = SNDRV_DEFAULT_STR;
static bool enable[SNDRV_CARDS] = SNDRV_DEFAULT_ENABLE_PNP;

module_param_array(index, int, NULL, 0444);
MODULE_PARM_DESC(index, "Index value for Digigram LX6464ES interface.");
module_param_array(id, charp, NULL, 0444);
MODULE_PARM_DESC(id, "ID string for  Digigram LX6464ES interface.");
//module_param_array(enable, bool, NULL, 0444);
//MODULE_PARM_DESC(enable,
//"Enable/disable specific Digigram LX6464ES soundcards.");



#define PCI_DEVICE_ID_PLX_LX6464ES        PCI_DEVICE_ID_PLX_9056

static const struct pci_device_id snd_lx6464es_ids[] = {
        { PCI_DEVICE_SUB(PCI_VENDOR_ID_PLX,
                         PCI_DEVICE_ID_PLX_LX6464ES,
                         PCI_VENDOR_ID_DIGIGRAM,
                         PCI_SUBDEVICE_ID_DIGIGRAM_LX6464ES_SERIAL_SUBSYSTEM),
        },                        /* LX6464ES */
        { PCI_DEVICE_SUB(PCI_VENDOR_ID_PLX,
                         PCI_DEVICE_ID_PLX_LX6464ES,
                         PCI_VENDOR_ID_DIGIGRAM,
                       PCI_SUBDEVICE_ID_DIGIGRAM_LX6464ES_CAE_SERIAL_SUBSYSTEM),
        },                        /* LX6464ES-CAE */

        { PCI_DEVICE_SUB(PCI_VENDOR_ID_PLX,
                         PCI_DEVICE_ID_PLX_LX6464ES,
                         PCI_VENDOR_ID_DIGIGRAM,
                         PCIEX_SUBDEVICE_ID_DIGIGRAM_LX6464ES_SERIAL_SUBSYSTEM),
        },                        /* LX6464ES-PCIEXPRESS */
        { 0, },
};

MODULE_DEVICE_TABLE(pci, snd_lx6464es_ids);



/* alsa callbacks */
static struct snd_pcm_hardware lx_6464_caps = {
        .info             = (SNDRV_PCM_INFO_MMAP |
                             SNDRV_PCM_INFO_INTERLEAVED |
                             SNDRV_PCM_INFO_MMAP_VALID |
                             SNDRV_PCM_INFO_SYNC_START),
        .formats          = (SNDRV_PCM_FMTBIT_S16_LE |
                             SNDRV_PCM_FMTBIT_S16_BE |
                             SNDRV_PCM_FMTBIT_S24_3LE |
                             SNDRV_PCM_FMTBIT_S24_3BE),
        .rates            = (SNDRV_PCM_RATE_CONTINUOUS |
                             SNDRV_PCM_RATE_8000_192000),
        .rate_min         = 8000,
        .rate_max         = 192000,
        .channels_min     = 2,
        .channels_max     = 64,
        .buffer_bytes_max = 64*2*3*MICROBLAZE_IBL_MAX*MAX_STREAM_BUFFER,
        .period_bytes_min = (2*2*MICROBLAZE_IBL_MIN*2),
        .period_bytes_max = (4*64*MICROBLAZE_IBL_MAX*MAX_STREAM_BUFFER),
        .periods_min      = 2,
        .periods_max      = MAX_STREAM_BUFFER,
};

static int lx_control_playback_info(struct snd_kcontrol *kcontrol,
                                    struct snd_ctl_elem_info *uinfo)
{
        printk(KERN_ERR  "%s\n", __func__);
        uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
        uinfo->count = 1;
        uinfo->value.integer.min = 0;
        uinfo->value.integer.max = 1;
        return 0;
}

static int lx_control_playback_get(struct snd_kcontrol *kcontrol,
                                   struct snd_ctl_elem_value *ucontrol)
{
        struct lx_chip *chip = snd_kcontrol_chip(kcontrol);
        printk(KERN_ERR  "%s\n", __func__);
        ucontrol->value.integer.value[0] = chip->playback_mute;
        return 0;
}

static int lx_control_playback_put(struct snd_kcontrol *kcontrol,
                                   struct snd_ctl_elem_value *ucontrol)
{
        struct lx_chip *chip = snd_kcontrol_chip(kcontrol);
        int changed = 0;
        int current_value = chip->playback_mute;

        printk(KERN_ERR  "%s\n", __func__);

        if (current_value != ucontrol->value.integer.value[0]) {
                lx_level_unmute(chip, 0, !current_value);
                chip->playback_mute = !current_value;
                changed = 1;
        }
        return changed;
}

static struct snd_kcontrol_new lx_control_playback_switch = {
        .iface = SNDRV_CTL_ELEM_IFACE_MIXER,
        .name = "PCM Playback Switch",
        .index = 0,
        .access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
        .private_value = 0,
        .info = lx_control_playback_info,
        .get = lx_control_playback_get,
        .put = lx_control_playback_put
};

static int snd_lx6464es_create(struct snd_card *card,
                               struct pci_dev *pci,
                               struct lx_chip **rchip)
{
        struct lx_chip *chip;
        int err;

        static struct snd_device_ops ops = {
                .dev_free = snd_lx_dev_free,
        };

        printk(KERN_ERR  "%s\n", __func__);

        *rchip = NULL;

        /* enable PCI device */
        err = pci_enable_device(pci);
        if (err < 0)
                return err;

        pci_set_master(pci);

        /* check if we can restrict PCI DMA transfers to 32 bits */
        //SJR 4.1
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,1,0)
        err = dma_set_mask(&pci->dev, DMA_BIT_MASK(32));
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3,19,0)
        err = dma_set_mask(&pci->dev, DMA_BIT_MASK(32));
#elif LINUX_VERSION_CODE == KERNEL_VERSION(3,10,17)
        err = pci_set_dma_mask(pci, DMA_BIT_MASK(32));
#else
#error "kernel not supported"
#endif

        if (err < 0) {
                printk(KERN_ERR "%s, architecture does not support "
                           "32bit PCI busmaster DMA\n", __func__);
                pci_disable_device(pci);
                return -ENXIO;
        }

        chip = kzalloc(sizeof(*chip), GFP_KERNEL);
        if (chip == NULL) {
                err = -ENOMEM;
                goto alloc_failed;
        }

        chip->card = card;
        chip->pci = pci;
        chip->irq = -1;
        chip->lx_type = LX_ETHERSOUND;
        chip->pcm_hw = lx_6464_caps;

        /* initialize synchronization structs */
        mutex_init(&chip->msg_lock);
        mutex_init(&chip->setup_mutex);

        /* request resources */
        err = pci_request_regions(pci, card_name);
        if (err < 0){
                goto request_regions_failed;
        }

        /* plx port */
        chip->port_plx = pci_resource_start(pci, 1);
        chip->port_plx_remapped = pci_iomap(pci, 1, 0);

        /* dsp port */
        chip->port_dsp_bar = pci_ioremap_bar(pci, 2);

        err = request_threaded_irq(pci->irq, lx_interrupt, NULL,
                                   IRQF_SHARED, "LX_ETHERSOUND", chip);
        if (err) {
                printk(KERN_ERR "%s, unable to grab IRQ %d\n",
                        __func__,
                        pci->irq);
                goto request_irq_failed;
        }
        chip->irq = pci->irq;

        err = snd_device_new(card, SNDRV_DEV_LOWLEVEL, chip, &ops);
        if (err < 0) {
                goto device_new_failed;
        }

        err = lx_init_dsp(chip);
        if (err < 0) {
                printk(KERN_ERR "%s, error during DSP initialization\n",
                        __func__);
                goto device_new_failed;
        }

        err = lx_pcm_create(chip);
        if (err < 0)
                goto device_new_failed;

        err = lx_proc_create(card, chip);
        if (err < 0)
                goto device_new_failed;

        err = snd_ctl_add(card, snd_ctl_new1(&lx_control_playback_switch,
                                             chip));
        if (err < 0)
                goto device_new_failed;
        //SJR 3.10
#if LINUX_VERSION_CODE == KERNEL_VERSION(3,10,19)
                snd_card_set_dev(card, &pci->dev);
#endif
        *rchip = chip;
        return 0;

device_new_failed:
        free_irq(pci->irq, chip);

request_irq_failed:
        pci_release_regions(pci);

request_regions_failed:
        kfree(chip);

alloc_failed:
        pci_disable_device(pci);

        return err;
}

static int snd_lx6464es_probe(struct pci_dev *pci,
                              const struct pci_device_id *pci_id)
{
        static int dev;
        struct snd_card *card;
        struct lx_chip *chip;
        int err;
        printk(KERN_ERR  "%s\n", __func__);

        if (dev >= SNDRV_CARDS)
                return -ENODEV;
        if (!enable[dev]) {
                dev++;
                return -ENOENT;
        }
//4.1.9
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,1,0)
        err = snd_card_new(&pci->dev, index[dev], id[dev], THIS_MODULE,
                           0, &card);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3,19,0)
        err = snd_card_new(&pci->dev, index[dev], id[dev], THIS_MODULE,
                           0, &card);
#elif LINUX_VERSION_CODE == KERNEL_VERSION(3,10,17)
        err = snd_card_create(index[dev], id[dev], THIS_MODULE, 0, &card);
#else
#error "kernel not supported"
#endif
        if (err < 0)
                return err;

        err = snd_lx6464es_create(card, pci, &chip);
        if (err < 0) {
                printk(KERN_ERR "%s, error during snd_lx6464es_create\n",
                        __func__);
                goto out_free;
        }

        strcpy(card->driver, "LX6464ES");
        sprintf(card->id,
                "LX6464ES_%02X%02X%02X",
                chip->mac_address[3],
                chip->mac_address[4],
                chip->mac_address[5]);

        sprintf(card->shortname,
                "LX6464ES %02X.%02X.%02X.%02X.%02X.%02X",
                chip->mac_address[0],
                chip->mac_address[1],
                chip->mac_address[2],
                chip->mac_address[3],
                chip->mac_address[4],
                chip->mac_address[5]);

        sprintf(card->longname,
                "%s at 0x%lx, 0x%p, irq %i",
                card->shortname,
                chip->port_plx,
                chip->port_dsp_bar,
                chip->irq);

        err = snd_card_register(card);
        if (err < 0)
                goto out_free;

        printk(KERN_ERR  "%s, initialization successful\n", __func__);
        pci_set_drvdata(pci, card);
        dev++;
        return 0;

out_free:
        snd_card_free(card);
        return err;

}

static void snd_lx6464es_remove(struct pci_dev *pci)
{
        printk(KERN_ERR  "%s\n", __func__);
        snd_card_free(pci_get_drvdata(pci));
#if LINUX_VERSION_CODE == KERNEL_VERSION(3,10,17)
        pci_set_drvdata(pci, NULL);
#endif
}


static struct pci_driver lx6464es_driver = {
//        .name =     KBUILD_MODNAME,
        .name =     "LX_ETHERSOUND",
        .id_table = snd_lx6464es_ids,
        .probe =    snd_lx6464es_probe,
        .remove = snd_lx6464es_remove,
};

module_pci_driver(lx6464es_driver);
