/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file 40bpp_opengl.hpp OpenGL 40 bpp blitter. */

#ifndef BLITTER_40BPP_OPENGL_HPP
#define BLITTER_40BPP_OPENGL_HPP

#ifdef WITH_OPENGL

#include "base.hpp"
#include "../video/video_driver.hpp"

/** The optimized 40 bpp blitter (for OpenGL video driver). */
class Blitter_40bppOpenGL : public Blitter {
public:
	uint8 GetScreenDepth() override { return 32; }
	void Draw(Blitter::BlitterParams *bp, BlitterMode mode, ZoomLevel zoom) override {};
	void DrawColourMappingRect(void *dst, int width, int height, PaletteID pal) override {};
	Sprite *Encode(const SpriteLoader::Sprite *sprite, AllocatorProc *allocator) override;
	void *MoveTo(void *video, int x, int y) override { return nullptr; };
	void SetPixel(void *video, int x, int y, uint8 colour) override {};
	void DrawRect(void *video, int width, int height, uint8 colour) override {};
	void DrawLine(void *video, int x, int y, int x2, int y2, int screen_width, int screen_height, uint8 colour, int width, int dash) override {};
	void CopyFromBuffer(void *video, const void *src, int width, int height) override {};
	void CopyToBuffer(const void *video, void *dst, int width, int height) override {};
	void CopyImageToBuffer(const void *video, void *dst, int width, int height, int dst_pitch) override {};
	void ScrollBuffer(void *video, int &left, int &top, int &width, int &height, int scroll_x, int scroll_y) override {};
	int BufferSize(int width, int height) override { return 0; };
	void PaletteAnimate(const Palette &palette) override { };
	Blitter::PaletteAnimation UsePaletteAnimation() override { return Blitter::PALETTE_ANIMATION_NONE; };
	bool NeedsAnimationBuffer() override { return true; }

	void SpriteEvicted(Sprite *data) override { };
	bool HasSpriteEviction() const override { return true; }

	const char *GetName() override { return "40bpp-opengl"; }
};

/** Factory for the 40 bpp OpenGL blitter. */
class FBlitter_40bppOpenGL : public BlitterFactory {
protected:
	bool IsUsable() const override
	{
		return VideoDriver::GetInstance() == nullptr || VideoDriver::GetInstance()->HasAnimBuffer();
	}

public:
	FBlitter_40bppOpenGL() : BlitterFactory("40bpp-opengl", "40bpp OpenGL Blitter") {}
	Blitter *CreateInstance() override { return new Blitter_40bppOpenGL(); }
};

#endif /* WITH_OPENGL */

#endif /* BLITTER_40BPP_OPTIMIZED_HPP */
