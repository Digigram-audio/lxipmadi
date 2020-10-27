/*
 *
 * ALSA driver for the digigram lx audio interface
 *
 * Copyright (c) 2016 Jubier Sylvain <alsa@digigram.com>
 */

#ifndef SOUND_PCI_LXCOMMON_H_
#define SOUND_PCI_LXCOMMON_H_

#include <sound/core.h>
#include <sound/pcm.h>
#include <linux/interrupt.h>
#include "lx_core.h"
//#include <uapi/sound/asound.h>
#include <sound/asound.h>
#include <sound/info.h>
#include <linux/atomic.h>
#include <linux/kthread.h>

#ifdef RHEL_RELEASE_CODE
#  define HAVE_SND_CARD_NEW (RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7,5))
#  define PREALLOCATE_PAGES_FOR_ALL_RETURNS_NO_ERR (RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(7,5))
#else
#  define HAVE_SND_CARD_NEW (LINUX_VERSION_CODE >= KERNEL_VERSION(3,15,0))
#  define PREALLOCATE_PAGES_FOR_ALL_RETURNS_NO_ERR (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0))
#endif




enum {
	ES_cmd_free = 0,	/* no command executing */
	ES_cmd_processing = 1,	/* execution of a read/write command */
	ES_read_pending = 2,	/* a asynchron read command is pending */
	ES_read_finishing = 3,	/* a read command has finished waiting (set by
				* Interrupt or CancelIrp)
				*/
};

enum lx_stream_status {
	LX_STREAM_STATUS_SCHEDULE_RUN,
	LX_STREAM_STATUS_RUNNING,
	LX_STREAM_STATUS_SCHEDULE_STOP,
	LX_STREAM_STATUS_STOPPED,
};

struct lx_stream {
	struct snd_pcm_substream *stream;
	snd_pcm_uframes_t frame_pos;
	/* volatile enum lx_stream_status status; */
	enum lx_stream_status status;
	unsigned int is_capture :1;
};

enum lx_madi_clock_sync {
	LXMADI_CLOCK_SYNC_MADI = 0x00,
	LXMADI_CLOCK_SYNC_WORDCLOCK = 0x01,
	LXMADI_CLOCK_SYNC_INTERNAL = 0x02,
};

enum lx_madi_word_clock_direction {
	LXMADI_WORD_CLOCK_IN = 0x00,
	LXMADI_WORD_CLOCK_OUT = 0x01,
};

enum lx_madi_rx_tx_mode {
	LXMADI_SMUX = 0x00,
	LXMADI_LEGACY = 0x01,
};
enum lx_madi_channel_mode {
	LXMADI_24_56_CHANNELS = 0x00,
	LXMADI_32_64_CHANNELS = 0x01,
};

enum lx_madi_clock_diviseur_mode {
	LXMADI_256 = 0x00,
	LXMADI_512 = 0x01,
};

enum lx_madi_sync_mode {
	LXMADI_SYNC_INDEPENDENT = 0x00,
	LXMADI_SYNC_MASTER, LXMADI_SYNC_SLAVE,
};

enum lx_type {
	LX_ETHERSOUND = 0,
	LX_MADI,
	LX_IP,
	LX_IP_MADI,
};

struct debug_irq_counters {
	/*irq part*/
	unsigned int irq_play_begin;	/* wakeup just for play*/
	unsigned int irq_play;		/* wakeup just for play*/
	/* irq for hw but not handled any more by alsa (stop sequence)*/
	unsigned int irq_play_unhandled;
	unsigned int irq_record;		/* wakeup just for record */
	/* irq for hw but not handled any more by alsa (stop sequence) */
	unsigned int irq_record_unhandled;
	unsigned int irq_play_and_record; /* wakeup for both */
	/* wakeup for someone else. somebody else is on the irq... */
	unsigned int irq_none;
	/* the LX wake up us but for nothing important or for an error... */
	unsigned int irq_handled;
	atomic_t atomic_irq_handled;
	unsigned int irq_urun;
	unsigned int irq_orun;
	unsigned int irq_freq;
	unsigned int irq_esa;
	unsigned int irq_timer;
	unsigned int irq_eot;
	unsigned int irq_xes;
	unsigned int irq_wakeup_thread;

	/*thread part*/
	unsigned int thread_play;	/* wakeup just for play */
	unsigned int thread_record;	/* wakeup just for record */
	unsigned int thread_play_and_record; /* wakeup for both */
	unsigned int wakeup_thread;
	unsigned int irq_all;
	/*atomic_t irq_all;*/
	u64 async_event_eobi;
	u64 async_event_eobo;
	u64 async_urun;
	u64 async_orun;

	unsigned int thread_record_but_stop;
	unsigned int thread_play_but_stop;

	/*cmds*/
	unsigned int cmd_irq_waiting;

};

struct lx_chip {
	enum lx_type lx_type;
	unsigned int lx_chip_index;
	struct snd_card *card;
	struct pci_dev *pci;
	int irq;

	u8 mac_address[6];
	/* mutex used in hw_params, open and close */
	struct mutex setup_mutex;
	struct snd_pcm_hardware pcm_hw;

	/* ports */
	unsigned long port_plx;			/* io port (size=256) */
	void __iomem *port_plx_remapped;	/* remapped plx port */
	void __iomem *port_dsp_bar;		/* memory port (32-bit,
						* non-prefetchable,
						* size=8K)
						*/

	/* messaging */
	struct mutex msg_lock; /* message lock */
	struct lx_rmh rmh;
	u32 irqsrc;

	/* configuration */
	uint freq_ratio :2;
	uint playback_mute :1;
	uint hardware_running[2];
	u32 board_sample_rate;	/* sample rate read from
				* board
				*/
	u16 pcm_granularity; /* board blocksize */

	/* dma */
	struct snd_dma_buffer capture_dma_buf;
	struct snd_dma_buffer playback_dma_buf;

	/* pcm */
	struct snd_pcm *pcm;

	/* streams */
	struct lx_stream capture_stream;
	struct lx_stream playback_stream;
	atomic_t play_xrun_advertise;
	atomic_t capture_xrun_advertise;

	/*mixer for all LX*/
	int first_channel_selector;
	int max_channels;

	struct snd_kcontrol *mixer_first_channel_selector_ctl;


	/* special lx madi */
	enum lx_madi_clock_sync use_clock_sync;
	enum lx_madi_word_clock_direction word_clock_out;
	int madi_frequency_selector;
	enum lx_madi_rx_tx_mode rx_tx_mode;	/* legacy/smux */
	enum lx_madi_channel_mode channel_mode;	/* 56/64 channels */
	enum lx_madi_clock_diviseur_mode diviseur_mode;
	enum lx_madi_sync_mode multi_card_sync_mode;
	struct snd_kcontrol *mixer_wordclock_out_ctl;
	struct snd_kcontrol *mixer_current_clock_ctl;

	atomic_t message_pending;

	/*Kthread*/
	unsigned int thread_wakeup;
	struct task_struct *pThread;
	unsigned char thread_stop;
	unsigned char thread_stream_update;
	/* cpt in order to compare embedded cpt -> detect XRUN/ORUN.*/
	unsigned int irq_audio_cpt_play;
	/* cpt in order to compare embedded cpt -> detect XRUN/ORUN.*/
	unsigned int irq_audio_cpt_record;
	unsigned char play_period_multiple_gran;
	unsigned char capture_period_multiple_gran;

	/*TODO DEBUG*/
	struct debug_irq_counters debug_irq;
	atomic_t irq_pending;

	unsigned char capture_stream_prerared;
	unsigned char playback_stream_prerared;

	unsigned long jiffies_start;
	unsigned long jiffies_1st_irq;

	/*in case of external clock loose*/
	int	(*set_internal_clock)(struct lx_chip *chip);

};

extern int lx_chips_count;
extern struct lx_chip *lx_chips[]; /*when there is several LX card*/

/*find closest granularity between witch ask and one provide by hw*/
int lx_set_granularity(struct lx_chip *chip, u32 gran);

/*allocate audio pipes an set granularity*/
int lx_hardware_open(struct lx_chip *chip, struct snd_pcm_substream *substream);

/*start audio pipes*/
int lx_stream_create_and_start(struct lx_chip *chip,
		struct snd_pcm_substream *substream);

/*stop audio pipes*/
int lx_pipe_stop(struct lx_chip *chip, int is_capture);

int lx_pipe_open(struct lx_chip *chip, int is_capture, int channels);

/*closes audio pipes*/
int lx_pipe_close(struct lx_chip *chip, int is_capture);

int lx_pcm_open(struct snd_pcm_substream *substream);

int lx_pcm_close(struct snd_pcm_substream *substream);

snd_pcm_uframes_t lx_pcm_stream_pointer(struct snd_pcm_substream *substream);

int lx_pcm_prepare(struct snd_pcm_substream *substream);

int lx_pcm_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *hw_params);

int lx_pcm_hw_free(struct snd_pcm_substream *substream);

void lx_trigger_pipe_start(struct lx_chip *chip, unsigned int is_capture);
void lx_trigger_start_linked_stream(struct lx_chip *chip);

void lx_trigger_pipes_start(struct lx_chip *chip);

void lx_trigger_tasklet_dispatch_stream(struct lx_chip *chip,
		struct lx_stream *lx_stream);

int lx_pcm_trigger_dispatch(struct lx_chip *chip, struct lx_stream *lx_stream,
		int cmd);

int lx_pcm_trigger(struct snd_pcm_substream *substream, int cmd);

int snd_lx_free(struct lx_chip *chip);

int snd_lx_dev_free(struct snd_device *device);

int lx_init_xilinx_reset(struct lx_chip *chip);

int lx_init_xilinx_test(struct lx_chip *chip);

int lx_init_ethersound_config(struct lx_chip *chip);

int lx_init_get_version_features(struct lx_chip *chip);

int lx_init_dsp(struct lx_chip *chip);

int lx_pcm_create(struct lx_chip *chip);

/*pic meters*/
void lx_proc_levels_read(struct snd_info_entry *entry,
		struct snd_info_buffer *buffer);

int lx_proc_create(struct snd_card *card, struct lx_chip *chip);


int lx_pcm_create_generic(struct lx_chip *chip, unsigned int dma_max_size,
			struct snd_pcm_ops *lx_ops_playback,
			struct snd_pcm_ops *lx_ops_capture);
int snd_create_generic(struct snd_card *card, struct pci_dev *pci,
		struct lx_chip **rchip, unsigned char lx_type,
		unsigned int dma_size, struct snd_pcm_hardware lx_caps,
		struct snd_pcm_ops *lx_ops_playback,
		struct snd_pcm_ops *lx_ops_capture);
void snd_lx_generic_remove(struct pci_dev *pci);

/*mixer for all LX soundcard*/
int snd_first_channel_selector_iobox_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *info);
int snd_first_channel_selector_iobox_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *value);
int snd_first_channel_selector_iobox_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *value);



#endif /* SOUND_PCI_LXCOMMON_H_ */
