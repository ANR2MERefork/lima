/*
 * Copyright (c) 2011-2012 Luc Verhaegen <libv@skynet.be>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <asm/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <inttypes.h>
#include <string.h>

#define u32 uint32_t
#include "linux/mali_ioctl.h"

#include "limare.h"
#include "plb.h"
#include "gp.h"
#include "pp.h"
#include "jobs.h"
#include "symbols.h"
#include "compiler.h"

static int
limare_fd_open(struct limare_state *state)
{
	state->fd = open("/dev/mali", O_RDWR);
	if (state->fd == -1) {
		printf("Error: Failed to open /dev/mali: %s\n",
		       strerror(errno));
		return errno;
	}

	return 0;
}

static int
limare_gpu_detect(struct limare_state *state)
{
	_mali_uk_get_system_info_size_s system_info_size;
	_mali_uk_get_system_info_s system_info_ioctl;
	struct _mali_system_info *system_info;
	int ret;

	ret = ioctl(state->fd, MALI_IOC_GET_SYSTEM_INFO_SIZE,
		    &system_info_size);
	if (ret) {
		printf("Error: %s: ioctl(GET_SYSTEM_INFO_SIZE) failed: %s\n",
		       __func__, strerror(ret));
		return ret;
	}

	system_info_ioctl.size = system_info_size.size;
	system_info_ioctl.system_info = calloc(1, system_info_size.size);
	if (!system_info_ioctl.system_info) {
		printf("%s: Error: failed to allocate system info: %s\n",
		       __func__, strerror(errno));
		return errno;
	}

	ret = ioctl(state->fd, MALI_IOC_GET_SYSTEM_INFO, &system_info_ioctl);
	if (ret) {
		printf("%s: Error: ioctl(GET_SYSTEM_INFO) failed: %s\n",
		       __func__, strerror(ret));
		free(system_info_ioctl.system_info);
		return ret;
	}

	system_info = system_info_ioctl.system_info;

	switch (system_info->core_info->type) {
	case _MALI_GP2:
	case _MALI_200:
		printf("Detected Mali-200.\n");
		state->type = 200;
		break;
	case _MALI_400_GP:
	case _MALI_400_PP:
		printf("Detected Mali-400.\n");
		state->type = 400;
		break;
	default:
		break;
	}

	return 0;
}

/*
 * TODO: MEMORY MANAGEMENT!!!!!!!!!
 *
 */
static int
limare_mem_init(struct limare_state *state)
{
	_mali_uk_init_mem_s mem_init = { 0 };
	int ret;

	mem_init.ctx = (void *) state->fd;
	mem_init.mali_address_base = 0;
	mem_init.memory_size = 0;
	ret = ioctl(state->fd, MALI_IOC_MEM_INIT, &mem_init);
	if (ret == -1) {
		printf("Error: ioctl MALI_IOC_MEM_INIT failed: %s\n",
		       strerror(errno));
		return errno;
	}

	state->mem_physical = mem_init.mali_address_base;
	state->mem_size = 0x100000;
	state->mem_address = mmap(NULL, state->mem_size, PROT_READ | PROT_WRITE,
				  MAP_SHARED, state->fd, state->mem_physical);
	if (state->mem_address == MAP_FAILED) {
		printf("Error: failed to mmap offset 0x%x (0x%x): %s\n",
		       state->mem_physical, state->mem_size, strerror(errno));
		return errno;
	}

	return 0;
}

struct limare_state *
limare_init(void)
{
	struct limare_state *state;
	int ret;

	state = calloc(1, sizeof(struct limare_state));
	if (!state) {
		printf("%s: Error: failed to allocate state: %s\n",
		       __func__, strerror(errno));
		goto error;
	}

	ret = limare_fd_open(state);
	if (ret)
		goto error;

	ret = limare_gpu_detect(state);
	if (ret)
		goto error;

	ret = limare_mem_init(state);
	if (ret)
		goto error;

	return state;
 error:
	free(state);
	return NULL;
}

/* here we still hardcode our memory addresses. */
int
limare_state_setup(struct limare_state *state, int width, int height,
		    unsigned int clear_color)
{
	if (!state)
		return -1;

	state->width = width;
	state->height = height;

	state->clear_color = clear_color;

	/* first, set up the plb, this is unchanged between draws. */
	state->plb = plb_create(state, state->mem_physical, state->mem_address,
				0x00000, 0x80000);
	if (!state->plb)
		return -1;

	/* now add the area for the pp, again, unchanged between draws. */
	state->pp = pp_info_create(state, state->mem_address + 0x80000,
				   state->mem_physical + 0x80000,
				   0x1000, state->mem_physical + 0x100000);
	if (!state->pp)
		return -1;

	/* now the two command queues */
	if (vs_command_queue_create(state, 0x81000, 0x4000) ||
	    plbu_command_queue_create(state, 0x85000, 0x4000))
		return -1;

	state->draw_mem_offset = 0x90000;
	state->draw_mem_size = 0x70000;

	return 0;
}

int
limare_uniform_attach(struct limare_state *state, char *name, int size,
		       int count, void *data)
{
	int found = 0, i;

	for (i = 0; i < state->vertex_uniform_count; i++) {
		struct symbol *symbol = state->vertex_uniforms[i];

		if (!strcmp(symbol->name, name)) {
			if ((symbol->component_size == size) &&
			    (symbol->component_count == count)) {
				symbol->data = data;
				found = 1;
				break;
			}

			printf("%s: Error: Uniform %s has wrong dimensions\n",
			       __func__, name);
			return -1;
		}
	}

	for (i = 0; i < state->fragment_uniform_count; i++) {
		struct symbol *symbol = state->fragment_uniforms[i];

		if (!strcmp(symbol->name, name)) {
			if ((symbol->component_size == size) &&
			    (symbol->component_count == count)) {
				symbol->data = data;
				found = 1;
				break;
			}

			printf("%s: Error: Uniform %s has wrong dimensions\n",
			       __func__, name);
			return -1;
		}
	}

	if (!found) {
		printf("%s: Error: Unable to find attribute %s\n",
		       __func__, name);
		return -1;
	}

	return 0;
}

int
limare_attribute_pointer(struct limare_state *state, char *name, int size,
			  int count, void *data)
{
	int i;

	for (i = 0; i < state->vertex_attribute_count; i++) {
		struct symbol *symbol = state->vertex_attributes[i];

		if (!strcmp(symbol->name, name)) {
			if (symbol->component_size == size) {
				symbol->component_count = count;
				symbol->data = data;
				return 0;
			}

			printf("%s: Error: Attribute %s has different dimensions\n",
			       __func__, name);
			return -1;
		}
	}

	printf("%s: Error: Unable to find attribute %s\n",
	       __func__, name);
	return -1;
}

int
limare_gl_mali_ViewPortTransform(struct limare_state *state,
				  struct symbol *symbol)
{
	float x0 = 0, y0 = 0, x1 = state->width, y1 = state->height;
	float depth_near = 0, depth_far = 1.0;
	float *viewport;

	if (symbol->data) {
		if (symbol->data_allocated)
			free(symbol->data);
	}

	symbol->data = calloc(8, sizeof(float));
	if (!symbol->data) {
		printf("%s: Error: Failed to allocate data: %s\n",
		       __func__, strerror(errno));
		return -1;
	}

	viewport = symbol->data;

	viewport[0] = x1 / 2;
	viewport[1] = y1 / 2;
	viewport[2] = (depth_far - depth_near) / 2;
	viewport[3] = depth_far;
	viewport[4] = (x0 + x1) / 2;
	viewport[5] = (y0 + y1) / 2;
	viewport[6] = (depth_near + depth_far) / 2;
	viewport[7] = depth_near;

	return 0;
}

int
limare_draw_arrays(struct limare_state *state, int mode, int start, int count)
{
	struct draw_info *draw;
	int i;

	if (!state->plb) {
		printf("%s: Error: plb member is not set up yet.\n", __func__);
		return -1;
	}


	/* Todo, check whether attributes all have data attached! */

	for (i = 0; i < state->vertex_uniform_count; i++) {
		struct symbol *symbol = state->vertex_uniforms[i];

		if (symbol->data)
			continue;

		if (!strcmp(symbol->name, "gl_mali_ViewportTransform")) {
			if (limare_gl_mali_ViewPortTransform(state, symbol))
				return -1;
		} else {
			printf("%s: Error: vertex uniform %s is empty.\n",
			       __func__, symbol->name);

			return -1;
		}
	}

	if (state->draw_count >= 32) {
		printf("%s: Error: too many draws already!\n", __func__);
		return -1;
	}

	if (state->draw_mem_size < 0x1000) {
		printf("%s: Error: no more space available!\n", __func__);
		return -1;
	}

	draw = draw_create_new(state, state->draw_mem_offset, 0x1000,
			       mode, start, count);
	state->draws[state->draw_count] = draw;

	state->draw_mem_offset += 0x1000;
	state->draw_mem_size -= 0x1000;
	state->draw_count++;

	vs_info_attach_shader(draw, state->vertex_binary->shader,
			      state->vertex_binary->shader_size / 16);

	plbu_info_attach_shader(draw, state->fragment_binary->shader,
				state->fragment_binary->shader_size / 4);

	for (i = 0; i < state->vertex_attribute_count; i++) {
		struct symbol *symbol =
			symbol_copy(state->vertex_attributes[i], start, count);

		if (symbol)
			vs_info_attach_attribute(draw, symbol);

	}

	for (i = 0; i < state->vertex_varying_count; i++) {
		struct symbol *symbol =
			symbol_copy(state->vertex_varyings[i], 0, count);

		if (symbol)
			vs_info_attach_varying(draw, symbol);
	}

	if (vs_info_attach_uniforms(draw, state->vertex_uniforms,
				    state->vertex_uniform_count,
				    state->vertex_uniform_size))
		return -1;

	if (plbu_info_attach_uniforms(draw, state->fragment_uniforms,
				      state->fragment_uniform_count,
				      state->fragment_uniform_size))
		return -1;

	vs_commands_draw_add(state, draw);
	vs_info_finalize(state, draw->vs);

	plbu_info_render_state_create(draw);
	plbu_commands_draw_add(state, draw);

	return 0;
}

int
limare_flush(struct limare_state *state)
{
	int ret;

	plbu_commands_finish(state);

	ret = limare_gp_job_start(state);
	if (ret)
		return ret;

	ret = limare_pp_job_start(state, state->pp);
	if (ret)
		return ret;

	limare_jobs_wait();

	return 0;
}

/*
 * Just run fflush(stdout) to give the wrapper library a chance to finish.
 */
void
limare_finish(void)
{
	fflush(stdout);
}
