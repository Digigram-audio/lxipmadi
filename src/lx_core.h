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

#ifndef LX_CORE_H
#define LX_CORE_H

#include <linux/interrupt.h>
#include <sound/info.h>

#include "lx_defs.h"


#define PCI_SUBDEVICE_ID_DIGIGRAM_LX6464ESEX_SUBSYSTEM	0xc021
#define PCI_SUBDEVICE_ID_DIGIGRAM_LXMADI_SUBSYSTEM	0xca21
#define PCI_SUBDEVICE_ID_DIGIGRAM_LXIP_SUBSYSTEM	0xc821
#define PCI_SUBDEVICE_ID_DIGIGRAM_LXIP_MADI_SUBSYSTEM	0xcc21

#define REG_CRM_NUMBER                12

struct lx_chip;

/* low-level register access */

/* dsp register access */
enum {
	REG_BASE,
	REG_CSM,
	REG_CRM1,
	REG_CRM2,
	REG_CRM3,
	REG_CRM4,
	REG_CRM5,
	REG_CRM6,
	REG_CRM7,
	REG_CRM8,
	REG_CRM9,
	REG_CRM10,
	REG_CRM11,
	REG_CRM12,

	REG_ICR,
	REG_CVR,
	REG_ISR,
	REG_RXHTXH,
	REG_RXMTXM,
	REG_RHLTXL,
	REG_RESETDSP,

	REG_CSUF,
	REG_CSES,
	REG_CRESMSB,
	REG_CRESLSB,
	REG_ADMACESMSB,
	REG_ADMACESLSB,
	REG_CONFES,
	REG_MADI_RAVENNA_CLOCK_CFG,

	REG_MAX_PORT
};

unsigned int lx_dsp_reg_read(struct lx_chip *chip, int port);

void lx_dsp_reg_write(struct lx_chip *chip, int port, unsigned data);

/* plx register access */
enum {
	PLX_PCICR,

	PLX_MBOX0,
	PLX_MBOX1,
	PLX_MBOX2,
	PLX_MBOX3,
	PLX_MBOX4,
	PLX_MBOX5,
	PLX_MBOX6,
	PLX_MBOX7,

	PLX_L2PCIDB,
	PLX_IRQCS,
	PLX_CHIPSC,

	PLX_MAX_PORT
};

unsigned int lx_plx_reg_read(struct lx_chip *chip, int port);
void lx_plx_reg_write(struct lx_chip *chip, int port, u32 data);

/* rhm */
struct lx_rmh {
	u16 cmd_len; /* length of the command to send (WORDs) */
	u16 stat_len; /* length of the status received (WORDs) */
	u16 dsp_stat; /* status type, RMP_SSIZE_XXX */
	u16 cmd_idx; /* index of the command */
	u32 cmd[REG_CRM_NUMBER];
	u32 stat[REG_CRM_NUMBER];
};

/* low-level dsp access */
int lx_message_send_atomic(struct lx_chip *chip, struct lx_rmh *rmh);
int lx_dsp_get_version(struct lx_chip *chip, u32 *rdsp_version);
int lx_dsp_get_clock_frequency(struct lx_chip *chip, u32 *rfreq);
int lx_dsp_set_granularity(struct lx_chip *chip, u32 gran);
int lx_dsp_read_async_events(struct lx_chip *chip, u32 *data);
int lx_dsp_get_mac(struct lx_chip *chip);

/* low-level pipe handling */
int lx_pipe_allocate(struct lx_chip *chip, u32 pipe, int is_capture,
		int channels);
int lx_pipe_release(struct lx_chip *chip, u32 pipe, int is_capture);
int lx_pipe_sample_count(struct lx_chip *chip, u32 pipe, int is_capture,
		u64 *rsample_count);
int lx_pipe_state(struct lx_chip *chip, u32 pipe, int is_capture, u16 *rstate);
int lx_pipe_stop_single(struct lx_chip *chip, u32 pipe, int is_capture);

/* start and pause could be merge... */
int lx_pipe_start_single(struct lx_chip *chip, u32 pipe, int is_capture);
int lx_pipe_pause_single(struct lx_chip *chip, u32 pipe, int is_capture);

int lx_pipe_start_multiple(struct lx_chip *chip);
int lx_pipe_pause_multiple(struct lx_chip *chip);

int lx_pipe_start_pause_play_and_record_dual(struct lx_chip *master_chip,
		struct lx_chip *slave_chip);

int lx_pipe_wait_for_start(struct lx_chip *chip, u32 pipe, int is_capture);
int lx_pipe_wait_for_idle(struct lx_chip *chip, u32 pipe, int is_capture);
int lx_stream_wait_for_start(struct lx_chip *chip, u32 pipe, int is_capture);
int lx_stream_wait_for_idle(struct lx_chip *chip, u32 pipe, int is_capture);

/* low-level stream handling */
int lx_stream_def(struct lx_chip *chip, struct snd_pcm_runtime *runtime,
		u32 pipe, int is_capture);

int lx_stream_sample_position(struct lx_chip *chip, u32 pipe, int is_capture,
		u64 *r_bytepos);

int lx_stream_set_state(struct lx_chip *chip, u32 pipe, int is_capture,
		enum stream_state_t state);

int lx_madi_set_madi_state(struct lx_chip *chip);

static inline int lx_stream_start(struct lx_chip *chip, u32 pipe,
		int is_capture)
{
	return lx_stream_set_state(chip, pipe, is_capture, SSTATE_RUN);
}

static inline int lx_stream_pause(struct lx_chip *chip, u32 pipe,
		int is_capture)
{
	return lx_stream_set_state(chip, pipe, is_capture, SSTATE_PAUSE);
}

static inline int lx_stream_stop(struct lx_chip *chip, u32 pipe, int is_capture)
{
	snd_printdd("->lx_stream_stop\n");
	return lx_stream_set_state(chip, pipe, is_capture, SSTATE_STOP);
}

/* low-level buffer handling */
int lx_buffer_ask(struct lx_chip *chip, u32 pipe, int is_capture, u32 *r_needed,
		u32 *r_freed, u32 *size_array);
int lx_buffer_give(struct lx_chip *chip, u32 pipe, int is_capture,
		u32 buffer_size, u32 buf_address_lo, u32 buf_address_hi,
		u32 *r_buffer_index, unsigned char period_multiple_gran);

int lx_buffer_cancel(struct lx_chip *chip, u32 pipe, int is_capture,
		u32 buffer_index);

/* low-level gain/peak handling */
int lx_level_unmute(struct lx_chip *chip, int is_capture, int unmute);
int lx_level_peaks(struct lx_chip *chip, int is_capture, int channels,
		u32 *r_levels);

/* interrupt handling */
irqreturn_t lx_interrupt(int irq, void *dev_id);

void lx_irq_enable(struct lx_chip *chip);
void lx_irq_disable(struct lx_chip *chip);

/* debug */
void lx_proc_get_irq_counter(struct snd_info_entry *entry,
		struct snd_info_buffer *buffer);

/* Stream Format Header Defines (for LIN and IEEE754) */
#define HEADER_FMT_BASE         HEADER_FMT_BASE_LIN
#define HEADER_FMT_BASE_LIN     0xFED00000
#define HEADER_FMT_BASE_FLOAT   0xFAD00000
/* bit 23 in header_lo. WARNING: old bit 22 is ignored in float format */
#define HEADER_FMT_MONO         0x00000080
#define HEADER_FMT_INTEL        0x00008000
#define HEADER_FMT_16BITS       0x00002000
#define HEADER_FMT_24BITS       0x00004000
/* frequency is less or equ. to 11k*/
#define HEADER_FMT_UPTO11       0x00000200
/* frequency is over 11k and less then 32k.*/
#define HEADER_FMT_UPTO32       0x00000100

#define BIT_FMP_HEADER          23
#define BIT_FMP_SD              22
#define BIT_FMP_MULTICHANNEL    19

#define START_STATE             1
#define PAUSE_STATE             0

/* from PcxAll_e.h */
/* Start/Pause condition for pipes (PCXStartPipe, PCXPausePipe) */
#define START_PAUSE_IMMEDIATE           0
#define START_PAUSE_ON_SYNCHRO          1
#define START_PAUSE_ON_TIME_CODE        2

/* Pipe / Stream state */
#define START_STATE             1
#define PAUSE_STATE             0

extern struct lx_chip *lx_chips_slave;
extern struct lx_chip *lx_chips_master;

int lx_interrupt_debug_events(struct lx_chip *chip);


struct madi_status {
	unsigned char mute;
	unsigned char channel_mode;
	unsigned char tx_frame_mode;
	unsigned char rx_frame_mode;
	unsigned char carrier_error;
	unsigned char lock_error;
	unsigned char async_error;
	unsigned char madi_freq;
};

#define MADI_MUTE_MASK			0x0000001
#define MADI_GET_MUTE(val)		(val & MADI_MUTE_MASK)
#define MADI_CHANNEL_MODE_MASK		0x0000002
#define MADI_GET_CHANNEL_MODE(val)	((val & MADI_CHANNEL_MODE_MASK) >> 1)
#define MADI_TX_FRAME_MODE_MASK		0x0000004
#define MADI_GET_TX_FRAME_MODE(val)	((val & MADI_TX_FRAME_MODE_MASK) >> 2)
#define MADI_RX_FRAME_MODE_MASK		0x0000008
#define MADI_GET_RX_FRAME_MODE(val)	((val & MADI_RX_FRAME_MODE_MASK) >> 3)

#define MADI_CARRIER_ERROR_MASK		0x0000001
#define MADI_GET_CARRIER_ERROR(val)	(val & MADI_CARRIER_ERROR_MASK)
#define MADI_LOCK_ERROR_MASK		0x0000002
#define MADI_GET_LOCK_ERROR(val)	((val & MADI_LOCK_ERROR_MASK) >> 1)
#define MADI_ASYNC_ERROR_MASK		0x0000004
#define MADI_GET_ASYNC_ERROR(val)	((val & MADI_ASYNC_ERROR_MASK) >> 2)
#define MADI_MADI_FREQ_MASK		0x0000030
#define MADI_GET_MADI_FREQ(val)		((val & MADI_MADI_FREQ_MASK) >> 4)
int lx_madi_get_madi_state(struct lx_chip *chip, struct madi_status *status);


#endif /* LX_CORE_H */
