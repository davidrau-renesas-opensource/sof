/*
 * Copyright (c) 2017, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 *         Rander Wang <rander.wang@intel.com>
 */

#ifndef __PLATFORM_PLATFORM_H__
#define __PLATFORM_PLATFORM_H__

#include <platform/shim.h>

struct reef;

/* default SSP stream format - need aligned with codec setting*/
#define PLATFORM_SSP_STREAM_FORMAT	SOF_IPC_FRAME_S24_4LE

#define MAX_SSP_COUNT 3

/* IPC Interrupt */
#define PLATFORM_IPC_INTERUPT	IRQ_EXT_IPC_LVL2(0)

/* pipeline IRQ - TODO confirm IRQs */
#define PLATFORM_SCHEDULE_IRQ	IRQ_NUM_SOFTWARE5

#define PLATFORM_IRQ_TASK_HIGH	IRQ_NUM_SOFTWARE4
#define PLATFORM_IRQ_TASK_MED	IRQ_NUM_SOFTWARE3
#define PLATFORM_IRQ_TASK_LOW	IRQ_NUM_SOFTWARE2

/* maximum preload pipeline depth */
#define MAX_PRELOAD_SIZE	20

/* DMA treats PHY addresses as host address unless within DSP region */
#define PLATFORM_HOST_DMA_MASK	0x00000000

/* Host page size */
#define HOST_PAGE_SIZE		4096

#define PLATFORM_SCHEDULE_COST	200

/* Platform stream capabilities */
#define PLATFORM_MAX_CHANNELS	4
#define PLATFORM_MAX_STREAMS	5

/* TODO: get this from IPC - 2 * 32 bit*/
#define PLATFORM_INT_FRAME_SIZE		8
/* TODO: get this from IPC - 2 * 16 bit*/
#define PLATFORM_HOST_FRAME_SIZE	4
/* TODO: get this from IPC - 2 * 24 (32) bit*/
#define PLATFORM_DAI_FRAME_SIZE		8

/* Platform Host DMA buffer config - these should align with DMA engine */
#define PLAT_HOST_PERIOD_FRAMES	48	/* must be multiple of DMA burst size */
#define PLAT_HOST_PERIODS	2	/* give enough latency for DMA refill */

/* Platform Dev DMA buffer config - these should align with DMA engine */
#define PLAT_DAI_PERIOD_FRAMES	48	/* must be multiple of DMA+DEV burst size */
#define PLAT_DAI_PERIODS	2	/* give enough latency for DMA refill */
#define PLAT_DAI_SCHED		1000 /* scheduling time in usecs */

/* Platform internal buffer config - these should align with DMA engine */
#define PLAT_INT_PERIOD_FRAMES	48	/* must be multiple of DMA+DEV burst size */
#define PLAT_INT_PERIODS	2	/* give enough latency for DMA refill */


#define PLATFORM_SCHED_CLOCK	CLK_SSP

#define PLATFORM_NUM_MMAP_POSN	10
#define PLATFORM_NUM_MMAP_VOL	10

/* DMA channel drain timeout in microseconds */
#define PLATFORM_DMA_TIMEOUT	1333

/* IPC page data copy timeout */
#define PLATFORM_IPC_DMA_TIMEOUT 2000

/* WorkQ window size in microseconds */
#define PLATFORM_WORKQ_WINDOW	2000

/* Host finish work schedule delay in microseconds */
#define PLATFORM_HOST_FINISH_DELAY	100

/* Host finish work(drain from host to dai) timeout in microseconds */
#define PLATFORM_HOST_FINISH_TIMEOUT	50000

// TODO: move to SW reg header
#define SW_REG_STATUS	0x0
#define SW_REG_ERRCODE	0x04

/* local buffer size of DMA tracing */
#define DMA_TRACE_LOCAL_SIZE	HOST_PAGE_SIZE

/* the interval of DMA trace copying */
#define DMA_TRACE_US		500000

#define PLATFORM_HOST_DMA_TIMEOUT	50

/* DMAC used for trace DMA */
#define PLATFORM_TRACE_DMAC	DMA_ID_DMAC0

/* Platform defined panic code */
#define platform_panic(__x) \
	sw_reg_write(SW_REG_STATUS, ((sw_reg_read(SW_REG_STATUS) & 0xc0000000) |\
		((0xdead000 | __x) & 0x3fffffff)))

/* Platform defined trace code */
#if USE_SW_REG_STATUS
#define platform_trace_point(__x) \
	sw_reg_write(SW_REG_STATUS, ((sw_reg_read(SW_REG_STATUS) & 0xc0000000) |\
		((0xace0000 | __x) & 0x3fffffff)));\
	sw_reg_write(SW_REG_ERRCODE, __x)
#else	//using SW_REG_STATUS may influence the ROM status, don't do that atm.
#define platform_trace_point(__x) \
	sw_reg_write(SW_REG_ERRCODE, __x)
#endif

struct timer *platform_timer;

/*
 * APIs declared here are defined for every platform and IPC mechanism.
 */

int platform_boot_complete(uint32_t boot_message);

int platform_init(struct reef *reef);

int platform_ssp_set_mn(uint32_t ssp_port, uint32_t source, uint32_t rate,
	uint32_t bclk_fs);

void platform_ssp_disable_mn(uint32_t ssp_port);

#endif
