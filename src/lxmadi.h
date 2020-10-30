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
