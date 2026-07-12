#include "sense/sense.h"

#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <zephyr/audio/dmic.h>

#include <assert.h>
#include <math.h>
#include <arm_math.h>

LOG_MODULE_REGISTER(imd70d122, LOG_LEVEL_DBG);


#define MICROPHONE_NODE DT_ALIAS(microphone)
static const struct device *microphone = DEVICE_DT_GET(MICROPHONE_NODE);

#define MIC_SAMPLE_RATE CONFIG_SPV_SENSE_MIC_SAMPLE_RATE
#define MIC_SAMPLE_BIT_SIZE 16
#define MIC_SAMPLE_BYTE_SIZE (MIC_SAMPLE_BIT_SIZE / 8)
#define MIC_CHANNEL_COUNT 1
#define MIC_BLOCK_MS	CONFIG_SPV_SENSE_MIC_BLOCK_MS
#define MIC_CAPTURE_BLOCK_COUNT CONFIG_SPV_SENSE_MIC_BLOCKS_PER_CAPTURE

#define MIC_BLOCK_SIZE ((MIC_SAMPLE_RATE / 1000) * MIC_BLOCK_MS * MIC_SAMPLE_BYTE_SIZE * MIC_CHANNEL_COUNT)

#define MIC_TIMEOUT_MS		200
#define MIC_SENSITIVITY		CONFIG_SPV_SENSE_MIC_SENSITIVITY		// From datasheet
#define MIC_SENSITIVITY_REF	CONFIG_SPV_SENSE_MIC_SENSITIVITY_REF	// From datasheet
#define MIC_SETTLE_CYCLES	CONFIG_SPV_SENSE_MIC_WARMUP_CAPTURES

K_MEM_SLAB_DEFINE_STATIC(mem_slab, MIC_BLOCK_SIZE, MIC_CAPTURE_BLOCK_COUNT, 4);

/**
 * A-weighting biquad coefficients for a 16 kHz sampling rate. Calculated using
 * the `weightingFilter` MATLAB function (with `a` parameter sign correction).
 *
 * Each line implements one biquad:
 * { b0, b1, b2, a1, a2 }
 * y[n] = b0*x[n] + b1*x[n-1] + b2*x[n-2] - a1*y[n-1] - a2*y[n-2]
 */
static const float32_t a_weighting_coefficients[3][5] = {
	{ 0.5312,  1.0624,  0.5312, -0.8216, -0.1687 },
	{ 1.0000, -2.0000,  1.0000,  1.7055, -0.7160 },
	{ 1.0000, -2.0000,  1.0000,  1.9839, -0.9840 }
};
static_assert(
	MIC_SAMPLE_RATE == 16000,
	"Microphone and filter sampling frequency mismatch, "
	"recalculate the filter or change sampling frequency."
);

static struct pcm_stream_cfg stream = {
	.pcm_rate = MIC_SAMPLE_RATE,
	.pcm_width = MIC_SAMPLE_BIT_SIZE,
	.block_size = MIC_BLOCK_SIZE,
	.mem_slab = &mem_slab,
};
static struct dmic_cfg cfg = {
	.io = {
		.min_pdm_clk_freq = 350000,
		.max_pdm_clk_freq = 3300000,
	},
	.streams = &stream,
	.channel = {
		.req_num_streams = 1,
		.req_num_chan = MIC_CHANNEL_COUNT
	},
};

static K_MUTEX_DEFINE(mic_mutex);


int sense_noise_read(sense_centibel_t *noise) {
	int err;

	err = k_mutex_lock(&mic_mutex, K_FOREVER);
	if(err) {
		LOG_ERR("Error locking microphone mutex: %d", err);
		return err;
	}

	err = dmic_trigger(microphone, DMIC_TRIGGER_START);
	if(err) {
		LOG_ERR("Error starting microphone stream: %d", err);
		goto err_unlock_mutex;
	}

	void *buffer;
	size_t size;

	for(size_t i = 0; i < MIC_SETTLE_CYCLES; i++) {
		err = dmic_read(microphone, 0, &buffer, &size, MIC_TIMEOUT_MS);
		if(err) {
			LOG_ERR("Error discarding stream: %d", err);
			goto err_stop_mic;
		} else {
			k_mem_slab_free(&mem_slab, buffer);
		}
	}

	err = dmic_read(microphone, 0, &buffer, &size, MIC_TIMEOUT_MS);
	if(err) {
		LOG_ERR("Error reading stream: %d", err);
		goto err_stop_mic;
	}


	int16_t *samples = buffer;
	size_t sample_count = size / sizeof(*samples);
	static float32_t f_samples[MIC_BLOCK_SIZE / MIC_SAMPLE_BYTE_SIZE];
	if(sample_count > ARRAY_SIZE(f_samples)) {
		err = -ENOMEM;
		LOG_ERR("Insufficient memory for sample conversion");
		goto err_process_data;
	}

	// Convert to normalized floats.
	//
	// We convert from int16 to float, reinterpreting them as Q15s, which are
	// fixed point in the range [-1.0, 1.0]. So, by reinterpreting we get
	// normalization "for free".
	arm_q15_to_float((q15_t*)buffer, f_samples, sample_count);

	// Calculate DC offset and remove it from the samples.
	float32_t dc_offset;
	arm_mean_f32(f_samples, sample_count, &dc_offset);
	arm_offset_f32(f_samples, -dc_offset, f_samples, sample_count);

	// Apply A-weighting.
	float32_t filter_state[4 * ARRAY_SIZE(a_weighting_coefficients)] = { 0 };
	arm_biquad_casd_df1_inst_f32 weighting_filter;
	arm_biquad_cascade_df1_init_f32(
		&weighting_filter,
		ARRAY_SIZE(a_weighting_coefficients),
		&a_weighting_coefficients[0][0],
		filter_state
	);
	arm_biquad_cascade_df1_f32(&weighting_filter, f_samples, f_samples, sample_count);

	// Calculate RMS and transform from weighted dBFS to dBA.
	float32_t rms;
	arm_rms_f32(f_samples, sample_count, &rms);

	// Normalization isn't required, since we used pre-normalized Q15s earlier.
	float32_t dbfs = 20.0f * log10f(rms);
	float32_t db = dbfs - MIC_SENSITIVITY + MIC_SENSITIVITY_REF;

	k_mem_slab_free(&mem_slab, buffer);
	dmic_trigger(microphone, DMIC_TRIGGER_STOP);
	k_mutex_unlock(&mic_mutex);

	*noise = db * 10;

	return 0;


	err_process_data:
	k_mem_slab_free(&mem_slab, buffer);

	err_stop_mic:
	dmic_trigger(microphone, DMIC_TRIGGER_STOP);

	err_unlock_mutex:
	k_mutex_unlock(&mic_mutex);

	return err;
}

int sense_noise_init() {
	int err;

	cfg.channel.req_chan_map_lo = dmic_build_channel_map(0, 0, PDM_CHAN_LEFT);

	err = dmic_configure(microphone, &cfg);
	if(err) {
		LOG_ERR("Error configuring IM70D122: %d", err);
		return err;
	}

	return 0;
}
