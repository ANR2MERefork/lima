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

/*
 *
 * Code to help deal with setting up command streams for GP vs/plbu.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>

#include "ioctl_registers.h"
#include "limare.h"
#include "compiler.h"
#include "plb.h"
#include "symbols.h"
#include "gp.h"
#include "jobs.h"
#include "vs.h"
#include "plbu.h"
#include "render_state.h"
#include "hfloat.h"
#include "from_float.h"
#include "texture.h"

int
vs_command_queue_create(struct limare_state *state, int offset, int size)
{
	state->vs_commands = state->mem_address + offset;
	state->vs_commands_physical = state->mem_physical + offset;
	state->vs_commands_count = 0;
	state->vs_commands_size = size / 8;

	return 0;
}

int
plbu_command_queue_create(struct limare_state *state, int offset, int size)
{
	struct plb *plb = state->plb;
	struct lima_cmd *cmds;
	int i = 0;

	state->plbu_commands = state->mem_address + offset;
	state->plbu_commands_physical = state->mem_physical + offset;
	state->plbu_commands_count = 0;
	state->plbu_commands_size = size / 8;

	cmds = state->plbu_commands;

	cmds[i].val = plb->shift_w | (plb->shift_h << 16);
	if (state->type == LIMARE_TYPE_M400) {
		int block_max;

		if (plb->shift_h > plb->shift_w)
			block_max = plb->shift_h;
		else
			block_max = plb->shift_w;

		if (block_max > 2)
			block_max = 2;

		cmds[i].val |= block_max << 28;
	}
	cmds[i].cmd = LIMA_PLBU_CMD_BLOCK_STEP;
	i++;

	cmds[i].val = ((plb->tiled_w - 1) << 24) | ((plb->tiled_h - 1) << 8);
	cmds[i].cmd = LIMA_PLBU_CMD_TILED_DIMENSIONS;
	i++;

	cmds[i].val = plb->block_w;
	cmds[i].cmd = LIMA_PLBU_CMD_PLBU_BLOCK_STRIDE;
	i++;

	cmds[i].val = plb->mem_physical + plb->plbu_offset;
	if (state->type == LIMARE_TYPE_M200)
		cmds[i].cmd = LIMA_M200_PLBU_CMD_PLBU_ARRAY_ADDRESS;
	else if (state->type == LIMARE_TYPE_M400) {
		cmds[i].cmd = LIMA_M400_PLBU_CMD_PLBU_ARRAY_ADDRESS;
		cmds[i].cmd |= plb->block_w * plb->block_h - 1;
	}
	i++;

#if 0
	cmds[i].val = 0x40100000;
	cmds[i].cmd = LIMA_PLBU_CMD_TILE_HEAP_START;
	i++;

	cmds[i].val = 0x40150000;
	cmds[i].cmd = LIMA_PLBU_CMD_TILE_HEAP_END;
	i++;
#endif

	cmds[i].val = from_float(0.0);
	cmds[i].cmd = LIMA_PLBU_CMD_VIEWPORT_Y;
	i++;

	cmds[i].val = from_float(state->height);
	cmds[i].cmd = LIMA_PLBU_CMD_VIEWPORT_H;
	i++;

	cmds[i].val = from_float(0.0);
	cmds[i].cmd = LIMA_PLBU_CMD_VIEWPORT_X;
	i++;

	cmds[i].val = from_float(state->width);
	cmds[i].cmd = LIMA_PLBU_CMD_VIEWPORT_W;
	i++;

	cmds[i].val = 0x00000000;
	cmds[i].cmd = 0x1000010a;
	i++;

	cmds[i].val = from_float(0.0);
	cmds[i].cmd = LIMA_PLBU_CMD_DEPTH_RANGE_NEAR;
	i++;

	cmds[i].val = from_float(1.0);
	cmds[i].cmd = LIMA_PLBU_CMD_DEPTH_RANGE_FAR;
	i++;

	state->plbu_commands_count = i;

	return 0;
}

void
vs_info_setup(struct limare_state *state, struct draw_info *draw)
{
	struct vs_info *info = draw->vs;
	int i;

	if (state->type == LIMARE_TYPE_M200) {
		/* lima200 has a common area for attributes and varyings. */
		info->common = draw->mem_address + draw->mem_used;
		info->common_offset = draw->mem_used;
		info->common_size = sizeof(struct gp_common);
		draw->mem_used += ALIGN(info->common_size, 0x40);

		/* initialize common */
		for (i = 0; i < 0x10; i++) {
			info->common->attributes[i].physical = 0;
			info->common->attributes[i].size = 0x3F;
		}
		for (i = 0; i < 0x10; i++) {
			info->common->varyings[i].physical = 0;
			info->common->varyings[i].size = 0x3F;
		}
	} else if (state->type == LIMARE_TYPE_M400) {
		info->attribute_area = draw->mem_address + draw->mem_used;
		info->attribute_area_size = 0x10 * sizeof(struct gp_common_entry);
		info->attribute_area_offset = draw->mem_used;
		draw->mem_used += ALIGN(info->attribute_area_size, 0x40);

		info->varying_area = draw->mem_address + draw->mem_used;
		info->varying_area_size = 0x10 * sizeof(struct gp_common_entry);
		info->varying_area_offset = draw->mem_used;
		draw->mem_used += ALIGN(info->varying_area_size, 0x40);
	}
}

int
vs_info_attach_uniforms(struct draw_info *draw, struct symbol **uniforms,
			int count, int size)
{
	struct vs_info *info = draw->vs;
	void *address;
	int i;

	info->uniform_offset = draw->mem_used;
	info->uniform_size = size;
	draw->mem_used += ALIGN(4 * size, 0x40);

	address = draw->mem_address + info->uniform_offset;

	for (i = 0; i < count; i++) {
		struct symbol *symbol = uniforms[i];

		if (symbol->src_stride == symbol->dst_stride)
			memcpy(address + symbol->component_size * symbol->offset,
			       symbol->data, symbol->size);
		else {
			void *symbol_address = address +
				symbol->component_size * symbol->offset;
			int j;

			for (j = 0; (j * symbol->src_stride) < symbol->size; j++)
				memcpy(symbol_address + (j * symbol->dst_stride),
				       symbol->data + (j * symbol->src_stride),
				       symbol->src_stride);
		}
	}

	return 0;
}

int
vs_info_attach_attribute(struct draw_info *draw, struct symbol *attribute)
{
	struct vs_info *info = draw->vs;
	int size;

	if (info->attribute_count == 0x10) {
		printf("%s: No more attributes\n", __func__);
		return -1;
	}

	size = ALIGN(attribute->size, 0x40);
	if (size > (draw->mem_size - draw->mem_used)) {
		printf("%s: No more space\n", __func__);
		return -2;
	}

	attribute->address = draw->mem_address + draw->mem_used;
	attribute->physical = draw->mem_physical + draw->mem_used;
	draw->mem_used += size;

	info->attributes[attribute->offset / 4] = attribute;
	info->attribute_count++;

	memcpy(attribute->address, attribute->data, attribute->size);

	return 0;
}

int
vs_info_attach_varyings(struct limare_state *state, struct draw_info *draw)
{
	struct vs_info *info = draw->vs;

	info->varying_size = ALIGN(state->varying_map_size, 0x40);
	if (info->varying_size > (draw->mem_size - draw->mem_used)) {
		printf("%s: No more space\n", __func__);
		return -2;
	}

	info->varying_offset = draw->mem_used;
	draw->mem_used += info->varying_size;

	info->gl_Position_size = ALIGN(16, 0x40);
	if (info->gl_Position_size > (draw->mem_size - draw->mem_used)) {
		printf("%s: No more space\n", __func__);
		return -2;
	}

	info->gl_Position_offset = draw->mem_used;
	draw->mem_used += info->gl_Position_size;

	return 0;
}

int
vs_info_attach_shader(struct draw_info *draw, unsigned int *shader, int size)
{
	struct vs_info *info = draw->vs;
	int mem_size;

	if (info->shader != NULL) {
		printf("%s: shader already assigned\n", __func__);
		return -1;
	}

	mem_size = ALIGN(size * 16, 0x40);
	if (mem_size > (draw->mem_size - draw->mem_used)) {
		printf("%s: no more space\n", __func__);
		return -2;
	}

	info->shader = draw->mem_address + draw->mem_used;
	info->shader_offset = draw->mem_used;
	info->shader_size = size;
	draw->mem_used += mem_size;

	memcpy(info->shader, shader, 16 * size);

	return 0;
}

void
vs_commands_draw_add(struct limare_state *state, struct draw_info *draw)
{
	struct vs_info *vs = draw->vs;
	struct lima_cmd *cmds = state->vs_commands;
	int i = state->vs_commands_count;

	cmds[i].val = LIMA_VS_CMD_ARRAYS_SEMAPHORE_BEGIN_1;
	cmds[i].cmd = LIMA_VS_CMD_ARRAYS_SEMAPHORE;
	i++;

	cmds[i].val = LIMA_VS_CMD_ARRAYS_SEMAPHORE_BEGIN_2;
	cmds[i].cmd = LIMA_VS_CMD_ARRAYS_SEMAPHORE;
	i++;

	cmds[i].val = draw->mem_physical + vs->shader_offset;
	cmds[i].cmd = LIMA_VS_CMD_SHADER_ADDRESS | (vs->shader_size << 16);
	i++;

	cmds[i].val =
		(state->vertex_binary->parameters.vertex.varying_something - 1)
		<< 20;
	cmds[i].val |= (vs->shader_size - 1) << 10;
	cmds[i].cmd = LIMA_VS_CMD_SHADER_INFO;
	i++;

	cmds[i].val = (state->varying_map_count << 8) | ((vs->attribute_count - 1) << 24);
	cmds[i].cmd = LIMA_VS_CMD_VARYING_ATTRIBUTE_COUNT;
	i++;

	cmds[i].val = draw->mem_physical + vs->uniform_offset;
	cmds[i].cmd = LIMA_VS_CMD_UNIFORMS_ADDRESS |
		(ALIGN(vs->uniform_size, 4) << 14);
	i++;

	if (state->type == LIMARE_TYPE_M200) {
		cmds[i].val = draw->mem_physical + vs->common_offset;
		cmds[i].cmd = LIMA_VS_CMD_COMMON_ADDRESS |
			(vs->common_size << 14);
		i++;
	} else if (state->type == LIMARE_TYPE_M400) {
		cmds[i].val = draw->mem_physical + vs->attribute_area_offset;
		cmds[i].cmd = LIMA_VS_CMD_ATTRIBUTES_ADDRESS |
			(vs->attribute_count << 17);
		i++;

		cmds[i].val = draw->mem_physical + vs->varying_area_offset;
		cmds[i].cmd = LIMA_VS_CMD_VARYINGS_ADDRESS |
			((state->varying_map_count + 1) << 17);
		i++;
	}

	cmds[i].val = 0x00000003; /* always 3 */
	cmds[i].cmd = 0x10000041;
	i++;

	cmds[i].val = draw->vertex_count << 24;
	cmds[i].cmd = LIMA_VS_CMD_VERTEX_COUNT | draw->vertex_count >> 8;
	i++;

	cmds[i].val = 0x00000000;
	cmds[i].cmd = 0x60000000;
	i++;

	cmds[i].val = LIMA_VS_CMD_ARRAYS_SEMAPHORE_END;
	cmds[i].cmd = LIMA_VS_CMD_ARRAYS_SEMAPHORE;
	i++;

	/* update our size so we can set the gp job properly */
	state->vs_commands_count = i;
}

void
vs_info_finalize(struct limare_state *state, struct draw_info *draw, struct vs_info *info)
{
	int i;

	if (state->type == LIMARE_TYPE_M200) {
		for (i = 0; i < info->attribute_count; i++) {
			info->common->attributes[i].physical = info->attributes[i]->physical;
			info->common->attributes[i].size =
				((info->attributes[i]->component_size *
				  info->attributes[i]->component_count) << 11) |
				(info->attributes[i]->component_count - 1);
		}

		for (i = 0; i < state->varying_map_count; i++) {
			info->common->varyings[i].physical = draw->mem_physical +
				info->varying_offset + state->varying_map[i].offset;
			info->common->varyings[i].size =
				(state->varying_map_size << 11) |
				(state->varying_map[i].entries - 1);

			if (state->varying_map[i].entry_size == 2)
				info->common->varyings[i].size |= 0x0C;
		}

		if (state->gl_Position) {
			info->common->varyings[i].physical =
				draw->mem_physical + info->gl_Position_offset;
			info->common->varyings[i].size = 0x8020;
		}

	} else if (state->type == LIMARE_TYPE_M400) {
		for (i = 0; i < info->attribute_count; i++) {
			info->attribute_area[i].physical = info->attributes[i]->physical;
			info->attribute_area[i].size =
				((info->attributes[i]->component_size *
				  info->attributes[i]->component_count) << 11) |
				(info->attributes[i]->component_count - 1);
		}

		for (i = 0; i < state->varying_map_count; i++) {
			info->varying_area[i].physical = draw->mem_physical +
				info->varying_offset + state->varying_map[i].offset;
			info->varying_area[i].size =
				(state->varying_map_size << 11) |
				(state->varying_map[i].entries - 1);

			if (state->varying_map[i].entry_size == 2)
				info->varying_area[i].size |= 0x0C;
		}

		if (state->gl_Position) {
			info->varying_area[i].physical =
				draw->mem_physical + info->gl_Position_offset;
			info->varying_area[i].size = 0x8020;
		}
	}
}

void
plbu_commands_draw_add(struct limare_state *state, struct draw_info *draw)
{
	struct plbu_info *info = draw->plbu;
	struct vs_info *vs = draw->vs;
	struct lima_cmd *cmds = state->plbu_commands;
	int i = state->plbu_commands_count;

	/*
	 *
	 */
	cmds[i].val = LIMA_PLBU_CMD_ARRAYS_SEMAPHORE_BEGIN;
	cmds[i].cmd = LIMA_PLBU_CMD_ARRAYS_SEMAPHORE;
	i++;

	cmds[i].val = 0x00002200 | LIMA_PLBU_CMD_PRIMITIVE_CULL_CCW;
	cmds[i].cmd = LIMA_PLBU_CMD_PRIMITIVE_SETUP;
	i++;

	cmds[i].val = draw->mem_physical + info->render_state_offset;
	cmds[i].cmd = LIMA_PLBU_CMD_RSW_VERTEX_ARRAY;
	cmds[i].cmd |= (draw->mem_physical + vs->gl_Position_offset) >> 4;
	i++;

	cmds[i].val = (draw->vertex_count << 24); /* | draw->vertex_start; */
	cmds[i].cmd = LIMA_PLBU_CMD_VERTEX_COUNT |
		((draw->draw_mode & 0x1F) << 16) | (draw->vertex_count >> 8);
	i++;

	cmds[i].val = LIMA_PLBU_CMD_ARRAYS_SEMAPHORE_END;
	cmds[i].cmd = LIMA_PLBU_CMD_ARRAYS_SEMAPHORE;
	i++;

	/* update our size so we can set the gp job properly */
	state->plbu_commands_count = i;
}

void
plbu_commands_finish(struct limare_state *state)
{
	struct lima_cmd *cmds = state->plbu_commands;
	int i = state->plbu_commands_count;

	/*
	 * Some inter-frame communication apparently.
	 */
#if 0
	cmds[i].val = 0x400e41c0;
	cmds[i].cmd = 0xa0000103;
	i++;

	cmds[i].val = 0x400e41c4;
	cmds[i].cmd = 0xa0000104;
	i++;

	cmds[i].val = 0x400e41c8;
	cmds[i].cmd = 0xa0000107;
	i++;

	cmds[i].val = 0x400e41cc;
	cmds[i].cmd = 0xa0000108;
	i++;

	cmds[i].val = 0x400e41d0;
	cmds[i].cmd = 0xa0000105;
	i++;

	cmds[i].val = 0x400e41d4;
	cmds[i].cmd = 0xa0000106;
	i++;
#endif

	cmds[i].val = 0x00000000;
	cmds[i].cmd = 0xd0000000;
	i++;

	cmds[i].val = 0x00000000;
	cmds[i].cmd = 0xd0000000;
	i++;

	cmds[i].val = 0;
	cmds[i].cmd = LIMA_PLBU_CMD_END;
	i++;

	/* update our size so we can set the gp job properly */
	state->plbu_commands_count = i;
}

int
plbu_info_attach_shader(struct draw_info *draw, unsigned int *shader, int size)
{
	struct plbu_info *info = draw->plbu;
	int mem_size;

	if (info->shader != NULL) {
		printf("%s: shader already assigned\n", __func__);
		return -1;
	}

	mem_size = ALIGN(size * 4, 0x40);
	if (mem_size > (draw->mem_size - draw->mem_used)) {
		printf("%s: no more space\n", __func__);
		return -2;
	}

	info->shader = draw->mem_address + draw->mem_used;
	info->shader_offset = draw->mem_used;
	info->shader_size = size;
	draw->mem_used += mem_size;

	memcpy(info->shader, shader, 4 * size);

	return 0;
}

int
plbu_info_attach_uniforms(struct draw_info *draw, struct symbol **uniforms,
			  int count, int size)
{
	struct plbu_info *info = draw->plbu;
	void *address;
	unsigned int *array;
	int i;

	if (!count)
		return 0;

	info->uniform_array_offset = draw->mem_used;
	info->uniform_array_size = 4;
	draw->mem_used += 0x40;

	array = draw->mem_address + info->uniform_array_offset;

	info->uniform_offset = draw->mem_used;
	info->uniform_size = size;
	draw->mem_used += ALIGN(4 * size, 0x40);

	address = draw->mem_address + info->uniform_offset;
	array[0] = draw->mem_physical + info->uniform_offset;

	for (i = 0; i < count; i++) {
		struct symbol *symbol = uniforms[i];

		memcpy(address + symbol->component_size * symbol->offset,
		       symbol->data, symbol->size);
	}

	return 0;
}

int
plbu_info_attach_textures(struct draw_info *draw, struct texture **textures,
			  int count)
{
	unsigned int *list;
	int i;

	if (!count || !textures[0])
		return 0;

	if (count > 8) {
		printf("%s: Cannot attach more than 8 texture to a draw\n",
		       __func__);
		return -1;
	}

	draw->texture_descriptor_count = count;
	draw->texture_descriptor_list_offset = draw->mem_used;

	draw->mem_used += ALIGN(4 * count, 0x40);

	list = draw->mem_address + draw->texture_descriptor_list_offset;

	for (i = 0; i < count; i++) {
		struct texture *texture = textures[i];

		texture->descriptor_offset = draw->mem_used;
		draw->mem_used += ALIGN(4 * count, 0x40);

		memcpy(draw->mem_address + texture->descriptor_offset,
		       texture->descriptor, 0x40);

		list[i] = draw->mem_physical + textures[i]->descriptor_offset;
	}

	return 0;
}

int
plbu_info_render_state_create(struct limare_state *state, struct draw_info *draw)
{
	struct plbu_info *info = draw->plbu;
	struct vs_info *vs = draw->vs;
	struct render_state *render;
	int size, i;

	if (info->render_state) {
		printf("%s: render_state already assigned\n", __func__);
		return -1;
	}

	size = ALIGN(sizeof(struct render_state), 0x40);
	if (size > (draw->mem_size - draw->mem_used)) {
		printf("%s: no more space\n", __func__);
		return -2;
	}

	if (!info->shader) {
		printf("%s: no shader attached yet!\n", __func__);
		return -3;
	}

	info->render_state = draw->mem_address + draw->mem_used;
	info->render_state_offset = draw->mem_used;
	info->render_state_size = size;
	draw->mem_used += size;

	/* this bit still needs some figuring out :) */
	render = info->render_state;

	render->unknown00 = 0;
	render->unknown04 = 0;
	render->unknown08 = 0xfc3b1ad2;
	render->unknown0C = 0x33;
	render->depth_range = 0xFFFF0000;
	render->unknown14 = 7;
	render->unknown18 = 7;
	render->unknown1C = 0;
	render->unknown20 = 0xF807;
	/* enable 4x MSAA */
	render->unknown20 |= 0x68;
	render->shader_address =
		(draw->mem_physical + info->shader_offset) | info->shader_size;

	render->uniforms_address = 0;

	render->textures_address = 0;
	render->unknown34 = 0x300;
	render->unknown38 = 0x2000;

	if (vs->varying_size) {
		render->varyings_address = draw->mem_physical + vs->varying_offset;
		render->unknown34 |= state->varying_map_size >> 3;
		render->varying_types = 0;

		for (i = 0; i < state->varying_map_count; i++) {
			int val;

			if (state->varying_map[i].entry_size == 4) {
				if (state->varying_map[i].entries == 4)
					val = 0;
				else
					val = 1;
			} else {
				if (state->varying_map[i].entries == 4)
					val = 2;
				else
					val = 3;
			}

			if (i < 10)
				render->varying_types |= val << (3 * i);
			else if (i == 10) {
				render->varying_types |= val << 30;
				render->varyings_address |= val >> 2;
			} else if (i == 11)
				render->varyings_address |= val << 1;

		}
	}

	if (info->uniform_size) {
		render->uniforms_address =
			draw->mem_physical + info->uniform_array_offset;

		render->uniforms_address |=
			(ALIGN(info->uniform_size, 4) / 4) - 1;

		render->unknown34 |= 0x80;
		render->unknown38 |= 0x10000;
	}

	if (draw->texture_descriptor_count) {

		render->textures_address =
			draw->mem_physical + draw->texture_descriptor_list_offset;
		render->unknown34 |= draw->texture_descriptor_count << 14;

		render->unknown34 |= 0x20;
	}

	return 0;
}

int
limare_gp_job_start(struct limare_state *state)
{
	struct lima_gp_frame_registers frame = { 0 };

	frame.vs_commands_start = state->vs_commands_physical;
	frame.vs_commands_end =
		state->vs_commands_physical + 8 * state->vs_commands_count;
	frame.plbu_commands_start = state->plbu_commands_physical;
	frame.plbu_commands_end =
		state->plbu_commands_physical + 8 * state->plbu_commands_count;
	frame.tile_heap_start = 0;
	frame.tile_heap_end = 0;

	return limare_gp_job_start_direct(state, &frame);
}

struct draw_info *
draw_create_new(struct limare_state *state, int offset, int size,
		int draw_mode, int vertex_start, int vertex_count)
{
	struct draw_info *draw = calloc(1, sizeof(struct draw_info));

	if (!draw)
		return NULL;

	draw->mem_address = state->mem_address + offset;
	draw->mem_physical = state->mem_physical + offset;
	draw->mem_used = 0;
	draw->mem_size = size;

	draw->draw_mode = draw_mode;
	draw->vertex_start = vertex_start;
	draw->vertex_count = vertex_count;

	vs_info_setup(state, draw);

	return draw;
}
