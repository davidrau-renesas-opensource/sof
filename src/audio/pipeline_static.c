/*
 * Copyright (c) 2016, Intel Corporation
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
 *         Keyon Jie <yang.jie@linux.intel.comel.com>
 *
 * Static pipeline definition. This will be the default platform pipeline
 * definition if no pipeline is specified by driver topology.
 */

#include <stdint.h>
#include <stddef.h>
#include <reef/reef.h>
#include <reef/lock.h>
#include <reef/list.h>
#include <reef/stream.h>
#include <reef/dai.h>
#include <reef/ipc.h>
#include <platform/clk.h>
#include <platform/platform.h>
#include <reef/audio/component.h>
#include <reef/audio/pipeline.h>


/* 2 * 32 bit*/
#define PLATFORM_INT_FRAME_SIZE		8
/* 2 * 16 bit*/
#define PLATFORM_HOST_FRAME_SIZE	4
/* 2 * 24 (32) bit*/
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

/* default static pipeline SSP port - not used for dynamic pipes */
#define PLATFORM_SSP_PORT	2

/* default SSP stream format - need aligned with codec setting*/
#define PLATFORM_SSP_STREAM_FORMAT	SOF_IPC_FRAME_S24_4LE

/*
 * Static Buffer Convenience Constructors.
 */
#define SPIPE_BUFFER(bid, bsize) \
	{.comp.id = bid, .size = bsize}
#define SPIPE_COMP_CONNECT(source, sink) \
	{.source_id = source, .sink_id = sink}

/*
 * Static Component Convenience Constructors.
 */
#define SPIPE_COMP(cid, ctype, csize) \
	{.id = cid, .type = ctype, .hdr.size = sizeof(struct csize)}
#define SPIPE_HOST(scomp, hno_irq, hdmac, hchan, hconfig) \
	{.comp = scomp, .no_irq = hno_irq, .dmac_id = hdmac,\
	.dmac_chan = hchan, .dmac_config = hconfig}
#define SPIPE_DAI(scomp, ddai_type, ddai_idx, ddmac, dchan, dconfig) \
	{.comp = scomp, .type = ddai_type, .index = ddai_idx, .dmac_id = ddmac,\
	.dmac_chan = dchan, .dmac_config = dconfig}
#define SPIPE_VOL(scomp, vmin, vmax) \
	{.comp = scomp, .min_value = vmin, .max_value = vmax}
#define SPIPE_MIX(scomp) {.comp = scomp}
#define SPIPE_SRC(scomp) {.comp = scomp}
#define SPIPE_TONE(scomp) {.comp = scomp}

/*
 * Static Pipeline Convenience Constructor
 */
#define SPIPE_PIPE(pid, pcore, pdeadline, ppriority) \
	{.pipeline_id = pid, .core = pcore, .deadline = pdeadline, .priority = ppriority}
#define SPIPE_PIPE_CONNECT(psource, bsource, bid, psink, bsink) \
	{.pipeline_source_id = psource, .comp_source_id = bsource, \
	.buffer_id = bid, .pipeline_sink_id = psink, .comp_sink_id = bsink}

/*
 * Static pipeline container and constructor
 */

struct scomps {
	struct sof_ipc_comp *comps;
	uint32_t num_comps;
};

#define SCOMP(ccomps) \
	{.comps = (struct sof_ipc_comp *)ccomps, .num_comps = ARRAY_SIZE(ccomps)}


struct spipe {
	struct scomps *scomps;
	uint32_t num_scomps;
	struct sof_ipc_buffer *buffer;
	uint32_t num_buffers;
	struct sof_ipc_pipe_comp_connect *connect;
	uint32_t num_connections;
};

#define SPIPE(ncomp, sbuffer, sconnect) \
	{.scomps = ncomp, .num_scomps = ARRAY_SIZE(ncomp), \
	.buffer = sbuffer, .num_buffers = ARRAY_SIZE(sbuffer), \
	.connect = sconnect, .num_connections = ARRAY_SIZE(sconnect)}

/*
 * Components used in static pipeline 0.
 */

static struct sof_ipc_comp_host host_p0[] = {
	SPIPE_HOST(SPIPE_COMP(0, SOF_COMP_HOST, sof_ipc_comp_host), 0, 0, 1, 0),	/* ID = 0 */
	SPIPE_HOST(SPIPE_COMP(2, SOF_COMP_HOST, sof_ipc_comp_host), 0, 0, 2, 0),	/* ID = 2 */
	SPIPE_HOST(SPIPE_COMP(9, SOF_COMP_HOST, sof_ipc_comp_host), 0, 0, 3, 0),	/* ID = 9 */
};

static struct sof_ipc_comp_volume volume_p0[] = {
	SPIPE_VOL(SPIPE_COMP(1, SOF_COMP_VOLUME, sof_ipc_comp_volume), 0, 0xffffffff),/* ID = 1 */
	SPIPE_VOL(SPIPE_COMP(3, SOF_COMP_VOLUME, sof_ipc_comp_volume), 0, 0xffffffff),/* ID = 3 */
	SPIPE_VOL(SPIPE_COMP(5, SOF_COMP_VOLUME, sof_ipc_comp_volume), 0, 0xffffffff),/* ID = 5 */
	SPIPE_VOL(SPIPE_COMP(8, SOF_COMP_VOLUME, sof_ipc_comp_volume), 0, 0xffffffff),/* ID = 8 */
};

static struct sof_ipc_comp_dai dai_p0[] = {
	SPIPE_DAI(SPIPE_COMP(6, SOF_COMP_DAI, sof_ipc_comp_dai),
		SOF_DAI_INTEL_SSP, PLATFORM_SSP_PORT, 1, 0, 0),	/* ID = 6 */
	SPIPE_DAI(SPIPE_COMP(7, SOF_COMP_DAI, sof_ipc_comp_dai),
		SOF_DAI_INTEL_SSP, PLATFORM_SSP_PORT, 1, 1, 0),	/* ID = 7 */
};

static struct sof_ipc_comp_mixer mixer_p0[] = {
	SPIPE_MIX(SPIPE_COMP(4, SOF_COMP_MIXER, sof_ipc_comp_mixer)),				/* ID = 4 */
};

static struct scomps pipe0_scomps[] = {
	SCOMP(host_p0),
	SCOMP(volume_p0),
	SCOMP(dai_p0),
	SCOMP(mixer_p0),
};

/*
 * Components used in static pipeline 1.
 */

static struct sof_ipc_comp_host host_p1[] = {
	SPIPE_HOST(SPIPE_COMP(10, SOF_COMP_HOST, sof_ipc_comp_host), 0, 0, 4, 0),	/* ID = 10 */
};

static struct sof_ipc_comp_volume volume_p1[] = {
	SPIPE_VOL(SPIPE_COMP(12, SOF_COMP_VOLUME, sof_ipc_comp_volume), 0, 0xffffffff),/* ID = 12 */
};

static struct sof_ipc_comp_src src_p1[] = {
	SPIPE_SRC(SPIPE_COMP(11, SOF_COMP_SRC, sof_ipc_comp_src)),	/* ID = 11 */
};

static struct scomps pipe1_scomps[] = {
		SCOMP(host_p1),
		SCOMP(volume_p1),
		SCOMP(src_p1),
};

/*
 * Components used in static pipeline 2.
 */

static struct sof_ipc_comp_tone tone_p2[] = {
	SPIPE_TONE(SPIPE_COMP(13, SOF_COMP_HOST, sof_ipc_comp_tone)),	/* ID = 13 */
};

static struct sof_ipc_comp_volume volume_p2[] = {
	SPIPE_VOL(SPIPE_COMP(15, SOF_COMP_VOLUME, sof_ipc_comp_volume), 0, 0xffffffff),/* ID = 15 */
};

static struct sof_ipc_comp_src src_p2[] = {
	SPIPE_SRC(SPIPE_COMP(14, SOF_COMP_SRC, sof_ipc_comp_src)),	/* ID = 14 */
};

static struct scomps pipe2_scomps[] = {
		SCOMP(tone_p2),
		SCOMP(volume_p2),
		SCOMP(src_p2),
};


/*
 * Components used in static pipeline 3.
 */

static struct sof_ipc_comp_tone tone_p3[] = {
	SPIPE_TONE(SPIPE_COMP(0, SOF_COMP_HOST, sof_ipc_comp_tone)),	/* ID = 0 */
};

static struct sof_ipc_comp_dai dai_p3[] = {
	SPIPE_DAI(SPIPE_COMP(1, SOF_COMP_DAI, sof_ipc_comp_dai),
		SOF_DAI_INTEL_SSP, PLATFORM_SSP_PORT, 1, 0, 0),	/* ID = 1 */
	SPIPE_DAI(SPIPE_COMP(2, SOF_COMP_DAI, sof_ipc_comp_dai),
		SOF_DAI_INTEL_SSP, PLATFORM_SSP_PORT, 1, 1, 0),	/* ID = 2 */
};

static struct scomps pipe3_scomps[] = {
	SCOMP(tone_p3),
	SCOMP(dai_p3),
};

/* Host facing buffer */
#define HOST_PERIOD_SIZE \
	(PLAT_HOST_PERIOD_FRAMES * PLATFORM_HOST_FRAME_SIZE)

/* Device facing buffer */
#define DAI_PERIOD_SIZE \
	(PLAT_DAI_PERIOD_FRAMES * PLATFORM_DAI_FRAME_SIZE)

/* Internal buffer */
#define INT_PERIOD_SIZE \
	(PLAT_INT_PERIOD_FRAMES * PLATFORM_INT_FRAME_SIZE)

/*
 * Buffers used in static pipeline 0.
 */
static struct sof_ipc_buffer buffer0[] = {
	/* B20 - LL Playback - PCM 0 Host0 -> Volume1 */
	SPIPE_BUFFER(20, HOST_PERIOD_SIZE * 2),

	/* B21 - LL Playback - PCM 1 - Host2 -> Volume3 */
	SPIPE_BUFFER(21, HOST_PERIOD_SIZE * 2),

	/* B22 Volume1 -> Mixer4 */
	SPIPE_BUFFER(22, INT_PERIOD_SIZE * 1),

	/* B23  Volume3 -> Mixer4 */
	SPIPE_BUFFER(23, INT_PERIOD_SIZE * 1),

	/* B24 Mixer4 -> Volume 5 */
	SPIPE_BUFFER(24, INT_PERIOD_SIZE * 1),

	/* B25 - DAI Playback - Volume5 -> DAI6 */
	SPIPE_BUFFER(25, DAI_PERIOD_SIZE * 2),

	/* B26 - DAI Capture - DAI7 - > Volume8 */
	SPIPE_BUFFER(26, DAI_PERIOD_SIZE * 2),

	/* B27 - PCM0 - Capture LL - Volume8 -> Host9 */
	SPIPE_BUFFER(27, HOST_PERIOD_SIZE * 1),
};

/*
 * Buffers used in static pipeline 1.
 */
static struct sof_ipc_buffer buffer1[] = {
	/* B28 - Playback - PCM 3 - Host10 -> SRC11 */
	SPIPE_BUFFER(28, HOST_PERIOD_SIZE * 16),

	/* B29  SRC11 -> Volume12 */
	SPIPE_BUFFER(29, INT_PERIOD_SIZE * 2),

	/* B30  Volume12 -> Mixer4 */
	SPIPE_BUFFER(30, INT_PERIOD_SIZE * 2),
};

/*
 * Buffers used in static pipeline 2.
 */
static struct sof_ipc_buffer buffer2[] = {
	/* B31 - tone33 -> SRC14 */
	SPIPE_BUFFER(11, HOST_PERIOD_SIZE * 16),

	/* B32  SRC14 -> Volume15 */
	SPIPE_BUFFER(32, INT_PERIOD_SIZE * 2),

	/* B33  Volume15 -> Mixer4 */
	SPIPE_BUFFER(33, INT_PERIOD_SIZE * 2),
};

/*
 * Buffers used in static pipeline 3.
 */
static struct sof_ipc_buffer buffer3[] = {

	/* B0 - DAI Playback -> DAI0 */
	SPIPE_BUFFER(0, DAI_PERIOD_SIZE * 2),

	/* B1 - DAI Capture - DAI1  */
	SPIPE_BUFFER(1, DAI_PERIOD_SIZE * 2),
};

/*
 * Pipeline 0
 *
 * Two Low Latency PCMs mixed into single SSP output.
 *
 * host PCM0(0) --B20--> volume(1) --B22--+
 *                                      |--mixer(4) --B24--> volume(5) --B25--> SSPx(6)
 * host PCM1(2) --B21--> volume(3) --B23--+
 *                                      |
 *                     pipeline 1 >-----+
 *                                      |
 *                     pipeline 2 >-----+
 *
 * host PCM0(9) <--B27-- volume(8) <--B26-- SSPx(7)
 *
 *
 * Pipeline 1
 *
 * One PCM with SRC that is a Mixer 4 source
 *
 * host PCM2(10) --B28 --> SRC(11) --B29--> volume(12) --B30 --> Pipeline 0
 *
 *
 * Pipeline 2
 *
 * Test Pipeline
 *
 * tone(13) --- B31 ---> SRC(14) --B32---> volume(15) --B33 ---> Pipeline 0
 */

/* pipeline 0 component/buffer connections */
static struct sof_ipc_pipe_comp_connect c_connect0[] = {
	SPIPE_COMP_CONNECT(0, 20), /* Host0 -> B20 */
	SPIPE_COMP_CONNECT(20, 1), /* B20 ->  Volume1 */
	SPIPE_COMP_CONNECT(2, 21), /* Host2 -> B21  */
	SPIPE_COMP_CONNECT(21, 3), /* B21 ->  Volume3 */
	SPIPE_COMP_CONNECT(1, 22), /* Volume1 -> B22 */
	SPIPE_COMP_CONNECT(22, 4), /* B22 -> Mixer4 */
	SPIPE_COMP_CONNECT(3, 23), /* Volume3 -> B23 */
	SPIPE_COMP_CONNECT(23, 4), /* B23 -> Mixer4 */
	SPIPE_COMP_CONNECT(4, 24), /* Mixer4 -> B24  */
	SPIPE_COMP_CONNECT(24, 5), /* B24 -> Volume5 */
	SPIPE_COMP_CONNECT(5, 25), /* Volume5 -> B25 */
	SPIPE_COMP_CONNECT(25, 6), /* B25 -> DAI6 */
	SPIPE_COMP_CONNECT(7, 26), /* DAI7 -> B26 */
	SPIPE_COMP_CONNECT(26, 8), /* B26 -> Volume8 */
	SPIPE_COMP_CONNECT(8, 27), /* Volume8 -> B27 */
	SPIPE_COMP_CONNECT(27, 9), /* B27 -> host9 */
};

/* pipeline 1 component/buffer connections */
static struct sof_ipc_pipe_comp_connect c_connect1[] = {
	SPIPE_COMP_CONNECT(10, 28), /* Host10 -> B28 */
	SPIPE_COMP_CONNECT(28, 11), /* B28 ->  SRC11 */
	SPIPE_COMP_CONNECT(11, 29), /* SRC11 -> B29  */
	SPIPE_COMP_CONNECT(29, 12), /* B29 ->  Volume12 */
	//SPIPE_COMP_CONNECT(12, 30), /* Volume12 -> B30 */
};

/* pipeline 2 component/buffer connections */
static struct sof_ipc_pipe_comp_connect c_connect2[] = {
	SPIPE_COMP_CONNECT(13, 31), /* tone13 -> B31 */
	SPIPE_COMP_CONNECT(31, 14), /* B11 ->  SRC14 */
	SPIPE_COMP_CONNECT(14, 32), /* SRC14 -> B32 */
	SPIPE_COMP_CONNECT(32, 15), /* B32 ->  Volume15 */
	//SPIPE_COMP_CONNECT(15, 33), /* Volume15 -> B33 */
};

/* pipeline 3 component/buffer connections */
static struct sof_ipc_pipe_comp_connect c_connect3[] = {
	SPIPE_COMP_CONNECT(0, 1), /* tone0 -> B0 ->  DAI1 */
	//SPIPE_COMP_CONNECT(14, 15), /* SRC14 -> B12 ->  Volume15 */
};

/* pipeline connections to other pipelines */
//static struct sof_ipc_pipe_pipe_connect p_connect[] = {
//	SPIPE_PIPE_CONNECT(101, 12, 30, 100, 4), /* p101 volume12 -> B30 -> p100 Mixer4 */
//	SPIPE_PIPE_CONNECT(102, 15, 33, 100, 4), /* p102 Volume15 -> B33 -> p10 Mixer4 */
//};

/* the static pipelines */
static struct spipe spipe[] = {
	SPIPE(pipe3_scomps, buffer3, c_connect3),
	SPIPE(pipe0_scomps, buffer0, c_connect0),
	SPIPE(pipe1_scomps, buffer1, c_connect1),
	SPIPE(pipe2_scomps, buffer2, c_connect2),
};

/* pipelines - matches array order of spipe */
struct sof_ipc_pipe_new pipeline[] = {
	SPIPE_PIPE(100, 0, 1000, TASK_PRI_HIGH),	/* high pri - 1ms deadline */
//	SPIPE_PIPE(101, 0, 4000, TASK_PRI_MED),	/* med pri - 4ms deadline */
//	SPIPE_PIPE(102, 0, 5000, TASK_PRI_LOW),	/* low pri - 5ms deadline */
};

int init_static_pipeline(struct ipc *ipc)
{
	struct scomps *sc;
	struct sof_ipc_comp *c;
	int i;
	int j;
	int k;
	int ret;

	/* init system pipeline core */
	ret = pipeline_init();
	if (ret < 0)
		return NULL;

	/* create the pipelines */
	for (i = 0; i < ARRAY_SIZE(pipeline); i++) {

		/* create the pipeline */
		ret = ipc_pipeline_new(ipc, &pipeline[i]);
		if (ret < 0)
			goto error;

		sc = spipe[i].scomps;

		/* register components for this pipeline */
		for (j = 0; j < spipe[i].num_scomps; j++) {

			/* all pipeline components have same header */
			c = sc[j].comps;

			for (k = 0; k < sc[j].num_comps; k++) {

				ret = ipc_comp_new(ipc, c);
				if (ret < 0)
					goto error;

				/* next component - sizes not constant */
				c = (void*)c + c->hdr.size;
			}
		}

		/* register buffers for this pipeline */
		for (j = 0; j < spipe[i].num_buffers; j++) {
			ret = ipc_buffer_new(ipc, &spipe[i].buffer[j]);
			if (ret < 0)
				goto error;
		}

		/* connect components in this pipeline */
		for (j = 0; j < spipe[i].num_connections; j++) {
			ret = ipc_comp_connect(ipc, &spipe[i].connect[j]);
			if (ret < 0)
				goto error;
		}

		/* complete the pipeline */
		ipc_pipeline_complete(ipc, pipeline[i].pipeline_id);
	}

#if 0
	/* connect the pipelines */
	for (i = 0; i < ARRAY_SIZE(p_connect); i++) {
		ret = ipc_pipe_connect(ipc, &p_connect[i]);
		if (ret < 0)
			goto error;
	}
#endif
	/* pipelines now ready for params, prepare and cmds */
	return 0;

error:
	trace_pipe_error("ePS");
	
	for (i = 0; i < ARRAY_SIZE(pipeline); i++) {

		/* free pipeline */
		ipc_pipeline_free(ipc, pipeline[i].pipeline_id);

		/* free components */
		for (j = 0; j < spipe[i].num_scomps; j++) {
			sc = spipe[i].scomps;
			for (k = 0; k < sc->num_comps; k++)
				ipc_comp_free(ipc, spipe[i].scomps[j].comps[k].id);
		}

		/* free buffers */
		for (j = 0; j < spipe[i].num_buffers; j++)
			ipc_buffer_free(ipc, spipe[i].buffer[j].comp.id);
	}

	return ret;
}
