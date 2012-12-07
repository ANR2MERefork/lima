/*
 * Copyright (c) 2011-2012 Luc Verhaegen <libv@skynet.be>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

/*
 * Draws a single smoothed triangle.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <GLES2/gl2.h>

#include "limare.h"
#include "formats.h"

#include "esUtil.h"
#include "companion.h"

#define WIDTH 1280
#define HEIGHT 720

int
main(int argc, char *argv[])
{
	struct limare_state *state;
	int ret;

	const char *vertex_shader_source =
	  "uniform mat4 modelviewMatrix;\n"
	  "uniform mat4 modelviewprojectionMatrix;\n"
	  "uniform mat3 normalMatrix;\n"
	  "\n"
	  "attribute vec4 in_position;    \n"
	  "attribute vec3 in_normal;      \n"
	  "attribute vec2 in_coord;       \n"
	  "\n"
	  "vec4 lightSource = vec4(2.0, 2.0, 20.0, 0.0);\n"
	  "                             \n"
	  "varying vec4 vVaryingColor;  \n"
	  "varying vec2 coord;          \n"
	  "                             \n"
	  "void main()                  \n"
	  "{                            \n"
	  "    gl_Position = modelviewprojectionMatrix * in_position;\n"
	  "    vec3 vEyeNormal = normalMatrix * in_normal;\n"
	  "    vec4 vPosition4 = modelviewMatrix * in_position;\n"
	  "    vec3 vPosition3 = vPosition4.xyz / vPosition4.w;\n"
	  "    vec3 vLightDir = normalize(lightSource.xyz - vPosition3);\n"
	  "    float diff = max(0.0, dot(vEyeNormal, vLightDir));\n"
	  "    vVaryingColor = vec4(diff * vec3(1.0, 1.0, 1.0), 1.0);\n"
	  "    coord = in_coord;        \n"
	  "}                            \n";

	const char *fragment_shader_source =
	  "precision mediump float;     \n"
	  "                             \n"
	  "varying vec4 vVaryingColor;  \n"
	  "varying vec2 coord;          \n"
	  "                             \n"
	  "uniform sampler2D in_texture; \n"
	  "                             \n"
	  "void main()                  \n"
	  "{                            \n"
	  "    gl_FragColor = vVaryingColor * texture2D(in_texture, coord);\n"
	  "}                            \n";

	GLfloat vVertices[] = {
	  // front
	  -1.0f, -1.0f, +1.0f, // point blue
	  +1.0f, -1.0f, +1.0f, // point magenta
	  -1.0f, +1.0f, +1.0f, // point cyan
	  +1.0f, +1.0f, +1.0f, // point white
	  // back
	  +1.0f, -1.0f, -1.0f, // point red
	  -1.0f, -1.0f, -1.0f, // point black
	  +1.0f, +1.0f, -1.0f, // point yellow
	  -1.0f, +1.0f, -1.0f, // point green
	  // right
	  +1.0f, -1.0f, +1.0f, // point magenta
	  +1.0f, -1.0f, -1.0f, // point red
	  +1.0f, +1.0f, +1.0f, // point white
	  +1.0f, +1.0f, -1.0f, // point yellow
	  // left
	  -1.0f, -1.0f, -1.0f, // point black
	  -1.0f, -1.0f, +1.0f, // point blue
	  -1.0f, +1.0f, -1.0f, // point green
	  -1.0f, +1.0f, +1.0f, // point cyan
	  // top
	  -1.0f, +1.0f, +1.0f, // point cyan
	  +1.0f, +1.0f, +1.0f, // point white
	  -1.0f, +1.0f, -1.0f, // point green
	  +1.0f, +1.0f, -1.0f, // point yellow
	  // bottom
	  -1.0f, -1.0f, -1.0f, // point black
	  +1.0f, -1.0f, -1.0f, // point red
	  -1.0f, -1.0f, +1.0f, // point blue
	  +1.0f, -1.0f, +1.0f  // point magenta
	};

	GLfloat vCoords[] = {
	  // front
	  0.0f, 1.0f,
	  1.0f, 1.0f,
	  0.0f, 0.0f,
	  1.0f, 0.0f,
	  // back
	  0.0f, 1.0f,
	  1.0f, 1.0f,
	  0.0f, 0.0f,
	  1.0f, 0.0f,
	  // right
	  0.0f, 1.0f,
	  1.0f, 1.0f,
	  0.0f, 0.0f,
	  1.0f, 0.0f,
	  // left
	  0.0f, 1.0f,
	  1.0f, 1.0f,
	  0.0f, 0.0f,
	  1.0f, 0.0f,
	  // top
	  0.0f, 1.0f,
	  1.0f, 1.0f,
	  0.0f, 0.0f,
	  1.0f, 0.0f,
	  // bottom
	  0.0f, 1.0f,
	  1.0f, 1.0f,
	  0.0f, 0.0f,
	  1.0f, 0.0f,
	};

	GLfloat vNormals[] = {
	  // front
	  +0.0f, +0.0f, +1.0f, // forward
	  +0.0f, +0.0f, +1.0f, // forward
	  +0.0f, +0.0f, +1.0f, // forward
	  +0.0f, +0.0f, +1.0f, // forward
	  // back
	  +0.0f, +0.0f, -1.0f, // backbard
	  +0.0f, +0.0f, -1.0f, // backbard
	  +0.0f, +0.0f, -1.0f, // backbard
	  +0.0f, +0.0f, -1.0f, // backbard
	  // right
	  +1.0f, +0.0f, +0.0f, // right
	  +1.0f, +0.0f, +0.0f, // right
	  +1.0f, +0.0f, +0.0f, // right
	  +1.0f, +0.0f, +0.0f, // right
	  // left
	  -1.0f, +0.0f, +0.0f, // left
	  -1.0f, +0.0f, +0.0f, // left
	  -1.0f, +0.0f, +0.0f, // left
	  -1.0f, +0.0f, +0.0f, // left
	  // top
	  +0.0f, +1.0f, +0.0f, // up
	  +0.0f, +1.0f, +0.0f, // up
	  +0.0f, +1.0f, +0.0f, // up
	  +0.0f, +1.0f, +0.0f, // up
	  // bottom
	  +0.0f, -1.0f, +0.0f, // down
	  +0.0f, -1.0f, +0.0f, // down
	  +0.0f, -1.0f, +0.0f, // down
	  +0.0f, -1.0f, +0.0f  // down
	};

	unsigned short indices_triangle[36] = {
		0,  1,  2,
		3,  2,  1,

		4,  5,  6,
		7,  6,  5,

		8,  9, 10,
		11, 10, 9,

		12, 13, 14,
		15, 14, 13,

		16, 17, 18,
		19, 18, 17,

		20, 21, 22,
		23, 22, 21,
	};

	state = limare_init();
	if (!state)
		return -1;

	limare_buffer_clear(state);

	ret = limare_state_setup(state, WIDTH, HEIGHT, 0xFF505050);
	if (ret)
		return ret;

	vertex_shader_attach(state, vertex_shader_source);
	fragment_shader_attach(state, fragment_shader_source);

	limare_link(state);

	limare_attribute_pointer(state, "in_position", 4, 3, 24, vVertices);
	limare_attribute_pointer(state, "in_coord", 4, 2, 24, vCoords);
	limare_attribute_pointer(state, "in_normal", 4, 3, 24, vNormals);

	ESMatrix modelview;
	esMatrixLoadIdentity(&modelview);
	esTranslate(&modelview, 0.0f, 0.0f, -8.0f);
	esRotate(&modelview, 45.0f, 1.0f, 0.0f, 0.0f);
	esRotate(&modelview, 45.0f, 0.0f, 1.0f, 0.0f);
	esRotate(&modelview, 10.0f, 0.0f, 0.0f, 1.0f);

	GLfloat aspect = (GLfloat)(HEIGHT) / (GLfloat)(WIDTH);
	printf("aspect: %f\n", aspect);

	ESMatrix projection;
	esMatrixLoadIdentity(&projection);
	esFrustum(&projection, -2.8f, +2.8f, -2.8f * aspect, +2.8f * aspect,
		  6.0f, 10.0f);

	ESMatrix modelviewprojection;
	esMatrixLoadIdentity(&modelviewprojection);
	esMatrixMultiply(&modelviewprojection, &modelview, &projection);

	float normal[9];
	normal[0] = modelview.m[0][0];
	normal[1] = modelview.m[0][1];
	normal[2] = modelview.m[0][2];
	normal[3] = modelview.m[1][0];
	normal[4] = modelview.m[1][1];
	normal[5] = modelview.m[1][2];
	normal[6] = modelview.m[2][0];
	normal[7] = modelview.m[2][1];
	normal[8] = modelview.m[2][2];

	limare_uniform_attach(state, "modelviewMatrix", 16, &modelview.m[0][0]);
	limare_uniform_attach(state, "modelviewprojectionMatrix", 16,
			      &modelviewprojection.m[0][0]);
	limare_uniform_attach(state, "normalMatrix", 9, normal);

	limare_texture_attach(state, "in_texture", companion_texture_flat,
                              COMPANION_TEXTURE_WIDTH, COMPANION_TEXTURE_HEIGHT,
			      COMPANION_TEXTURE_FORMAT);

	limare_new(state);

	ret = limare_draw_elements(state, GL_TRIANGLES, 36,
				   &indices_triangle, GL_UNSIGNED_SHORT);
	if (ret)
		return ret;

	limare_flush(state);

	limare_buffer_swap(state);

	limare_finish(state);

	return 0;
}
