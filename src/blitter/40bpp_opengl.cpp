/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file 40bpp_opengl.cpp Implementation of the optimized 40 bpp blitter. */

#include "../stdafx.h"

#ifdef WITH_OPENGL

#if defined(__APPLE__)
#	include <OpenGL/gl3.h>
#else
#	include <GL/gl.h>
#endif
#include "../3rdparty/opengl/glext.h"

#include "../video/video_driver.hpp"
#include "../video/opengl.h"
#include "40bpp_opengl.hpp"

#include "../table/sprites.h"

#include "../safeguards.h"

/** Instantiation of the 40bpp OpenGL blitter factory. */
static FBlitter_40bppOpenGL iFBlitter_40bppOpenGL;

/** Cached black value. */
static const Colour _black_colour(0, 0, 0);


Sprite *Blitter_40bppOpenGL::Encode(const SpriteLoader::Sprite *sprite, AllocatorProc *allocator)
{
	/* Allocate and construct sprite data. */
	Sprite *dest_sprite = (Sprite *)allocator(sizeof(*dest_sprite) + sizeof(OpenGLSprite));

	OpenGLSprite *gl_sprite = (OpenGLSprite *)dest_sprite->data;
	new (gl_sprite) OpenGLSprite(sprite->width, sprite->height, sprite->type == ST_FONT ? 1 : ZOOM_LVL_COUNT, sprite->colours);

	/* Upload texture data. */
	for (int i = 0; i < (sprite->type == ST_FONT ? 1 : ZOOM_LVL_COUNT); i++) {
		gl_sprite->Update(sprite[i].width, sprite[i].height, i, sprite[i].data);
	}

	dest_sprite->height = sprite->height;
	dest_sprite->width = sprite->width;
	dest_sprite->x_offs = sprite->x_offs;
	dest_sprite->y_offs = sprite->y_offs;

	return dest_sprite;
}

#endif
