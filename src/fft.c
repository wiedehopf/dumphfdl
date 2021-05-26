/* SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdint.h>
#include <string.h>         // memcpy, memmove
#include <pthread.h>        // pthread_*
#include <liquid/liquid.h>  // cbuffercf_*
#include "block.h"          // block_*
#include "fastddc.h"        // fastddc_t
#include "util.h"           // XCALLOC, NEW

struct fft {
	struct block block;
	fastddc_t *ddc;
	float complex *input;
};

void *fft_thread(void *ctx) {
	struct block *block = ctx;
	struct fft *fft = container_of(block, struct fft, block);
	struct circ_buffer *circ_buffer = &block->consumer.in->circ_buffer;
	struct shared_buffer *output = &block->producer.out->shared_buffer;
	fastddc_t *ddc = fft->ddc;
	float complex *cbuf_read_ptr;
	uint32_t samples_read;
	float complex *fft_input = fft->input;

	// The plan can't be created in fft_create because the output buffer
	// is created by block_connect_one2many() which is called after fft_create().
	FFT_PLAN_T *fwd_plan = csdr_make_fft_c2c(ddc->fft_size, fft_input, output->buf, 1, 0);

	fprintf(stderr, "fft: waiting for consumers to initialize\n");
	pthread_barrier_wait(output->consumers_ready);         // Wait for all consumers to initialize
	while(true) {
		pthread_mutex_lock(circ_buffer->mutex);
		// Check for shutdown signal only when there is no data (or not enough data) in the buffer.
		// This causes all the data to be processed and flushed to consumers before shutdown is done.
		while(cbuffercf_size(circ_buffer->buf) < (uint32_t)ddc->input_size) {
			if(block_connection_is_shutdown_signaled(block->consumer.in)) {
				fprintf(stderr, "fft: Exiting (ordered shutdown)\n");
				pthread_mutex_unlock(circ_buffer->mutex);
				goto shutdown;
			}
			pthread_cond_wait(circ_buffer->cond, circ_buffer->mutex);
		}
		memmove(fft_input, fft_input + ddc->input_size, ddc->overlap_length * sizeof(float complex));
		cbuffercf_read(circ_buffer->buf, ddc->input_size, &cbuf_read_ptr, &samples_read);
		ASSERT(samples_read == (uint32_t)ddc->input_size);
		memcpy(fft_input + ddc->overlap_length, cbuf_read_ptr,
				ddc->input_size * sizeof(float complex));
		cbuffercf_release(circ_buffer->buf, ddc->input_size);
		pthread_mutex_unlock(circ_buffer->mutex);

		csdr_fft_execute(fwd_plan);
		// FIXME: rework fastddc_inv_cc, so that this step is not needed
		fft_swap_sides(output->buf, ddc->fft_size);
		pthread_barrier_wait(output->data_ready);
		pthread_barrier_wait(output->consumers_ready);
	}
shutdown:
	block_connection_one2many_shutdown(block->producer.out);
	block->running = false;
	return NULL;
}

struct block *fft_create(int32_t decimation, float transition_bw) {
	NEW(struct fft, fft);
	NEW(fastddc_t, ddc);
	if(fastddc_init(ddc, transition_bw, decimation, 0)) {
		fprintf(stderr, "error in fastddc_init()");
		return NULL;
	}
	fastddc_print(ddc,"fastddc_fwd_cc");
	fft->ddc = ddc;
	fft->input = XCALLOC(ddc->fft_size, sizeof(float complex));
	struct producer producer = { .type = PRODUCER_MULTI, .max_tu = ddc->fft_size };
	struct consumer consumer = { .type = CONSUMER_SINGLE, .min_ru = ddc->fft_size };
	fft->block.producer = producer;
	fft->block.consumer = consumer;
	fft->block.thread_routine = fft_thread;
	return &fft->block;
}

