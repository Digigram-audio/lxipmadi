/*
 *
 * ALSA driver for the digigram lx audio interface
 *
 * Copyright (c) 2016 Jubier Sylvain <alsa@digigram.com>
 */

#ifndef LXMADI_H
#define LXMADI_H

static unsigned int lx_madi_internal_freq[] = {44100, 48000, 88200, 96000};

static struct snd_pcm_hw_constraint_list
lx_madi_hw_constraints_sample_rates = {
	.count = ARRAY_SIZE(lx_madi_internal_freq),
	.list = lx_madi_internal_freq,
	.mask = 0
};
/*
*		snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
*				&lx_madi_hw_constraints_sample_rates);
*/

#endif /*LXMADI_H*/
