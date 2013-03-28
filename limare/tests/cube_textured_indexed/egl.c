/*
 * Copyright (c) 2011-2013 Luc Verhaegen <libv@skynet.be>
 * Copyright (c) 2012 Arvin Schnell <arvin.schnell@gmail.com>
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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include "egl_common.h"

#include "esUtil.h"
#include "companion.h"


int
main(int argc, char *argv[])
{
	EGLDisplay display;
	EGLSurface surface;
	GLuint vertex_shader;
	GLuint fragment_shader;
	GLuint program;
	GLint ret;
	GLint width, height;
	GLuint texture;

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

	unsigned char indices_triangle[] = {
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

#if 0
	unsigned char indices_strip[] = {
		0,  1,  2, 3,

		4,

		4,  5,  6,
		7,  6,  5,

		8,  9, 10, 11,

		12, 13, 14, 15,

		16, 17, 18, 19,

		20, 21, 22, 23,
	};
#endif

	buffer_size(&width, &height);

	printf("Buffer dimensions %dx%d\n", width, height);
	float aspect = (float) height / width;

	display = egl_display_init();
	surface = egl_surface_init(display, width, height);

	vertex_shader = vertex_shader_compile(vertex_shader_source);
	fragment_shader = fragment_shader_compile(fragment_shader_source);

	program = glCreateProgram();
	if (!program) {
		printf("Error: failed to create program!\n");
		return -1;
	}

	glAttachShader(program, vertex_shader);
	glAttachShader(program, fragment_shader);

	glBindAttribLocation(program, 0, "in_position");
	glBindAttribLocation(program, 1, "in_normal");
	glBindAttribLocation(program, 2, "in_coord");

	glLinkProgram(program);

	glGetProgramiv(program, GL_LINK_STATUS, &ret);
	if (!ret) {
		char *log;

		printf("Error: program linking failed!:\n");
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &ret);

		if (ret > 1) {
			log = malloc(ret);
			glGetProgramInfoLog(program, ret, NULL, log);
			printf("%s", log);
		}
		return -1;
	} else
		printf("program linking succeeded!\n");

	glUseProgram(program);

	glViewport(0, 0, width, height);

	/* clear the color buffer */
	glClearColor(0.5, 0.5, 0.5, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, vVertices);
	glEnableVertexAttribArray(0);

	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, vNormals);
	glEnableVertexAttribArray(1);

	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, vCoords);
	glEnableVertexAttribArray(2);

	ESMatrix modelview;
	esMatrixLoadIdentity(&modelview);
	esTranslate(&modelview, 0.0f, 0.0f, -8.0f);
	esRotate(&modelview, 45.0f, 1.0f, 0.0f, 0.0f);
	esRotate(&modelview, 45.0f, 0.0f, 1.0f, 0.0f);
	esRotate(&modelview, 10.0f, 0.0f, 0.0f, 1.0f);

	ESMatrix projection;
	esMatrixLoadIdentity(&projection);
	esFrustum(&projection, -2.8f, +2.8f, -2.8f * aspect, +2.8f * aspect, 6.0f, 10.0f);

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

	glEnable(GL_TEXTURE_2D);

	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
		     COMPANION_TEXTURE_WIDTH, COMPANION_TEXTURE_HEIGHT, 0,
		     GL_RGB, GL_UNSIGNED_BYTE, companion_texture_flat);

	GLint modelviewmatrix_handle =
		glGetUniformLocation(program, "modelviewMatrix");
	GLint modelviewprojectionmatrix_handle =
		glGetUniformLocation(program, "modelviewprojectionMatrix");
	GLint normalmatrix_handle =
		glGetUniformLocation(program, "normalMatrix");

	glUniformMatrix4fv(modelviewmatrix_handle, 1, GL_FALSE,
			   &modelview.m[0][0]);
	glUniformMatrix4fv(modelviewprojectionmatrix_handle, 1, GL_FALSE,
			   &modelviewprojection.m[0][0]);
	glUniformMatrix3fv(normalmatrix_handle, 1, GL_FALSE, normal);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);

	GLint texture_loc = glGetUniformLocation(program, "in_texture");
	glUniform1i(texture_loc, 0); // 0 -> GL_TEXTURE0 in glActiveTexture

	glEnable(GL_CULL_FACE);

#if 0
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
#elif 1
	glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_BYTE, indices_triangle);
#else
	glDrawElements(GL_TRIANGLE_STRIP, 4, GL_UNSIGNED_BYTE, indices_strip);
#endif

	eglSwapBuffers(display, surface);

	usleep(1000000);

	fflush(stdout);

	return 0;
}
