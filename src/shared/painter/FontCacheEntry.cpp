/*
 * Copyright 2006-2009, Stephan Aßmus <superstippi@gmx.de>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

//----------------------------------------------------------------------------
// Anti-Grain Geometry - Version 2.4
// Copyright (C) 2002-2005 Maxim Shemanarev (http://www.antigrain.com)
//
// Permission to copy, use, modify, sell and distribute this software 
// is granted provided this copyright notice appears in all copies. 
// This software is provided "as is" without express or implied
// warranty, and with no claim as to its suitability for any purpose.
//
//----------------------------------------------------------------------------
// Contact: mcseem@antigrain.com
//			mcseemagg@yahoo.com
//			http://www.antigrain.com
//----------------------------------------------------------------------------


#include "FontCacheEntry.h"

#include <string.h>
#include <agg_array.h>

#include <Autolock.h>

#include "FontManager.h"

BLocker
FontCacheEntry::sUsageUpdateLock("FontCacheEntry usage lock");


class FontCacheEntry::GlyphCachePool {
 public:
	enum block_size_e { block_size = 16384-16 };

	GlyphCachePool()
		: fAllocator(block_size)
	{
		memset(fGlyphs, 0, sizeof(fGlyphs));
	}

	const GlyphCache* FindGlyph(uint16 glyphCode) const
	{
		unsigned msb = (glyphCode >> 8) & 0xFF;
		if (fGlyphs[msb]) 
			return fGlyphs[msb][glyphCode & 0xFF];
		return 0;
	}

	GlyphCache* CacheGlyph(uint16 glyphCode, unsigned glyphIndex,
		unsigned dataSize, glyph_data_type dataType, const agg::rect_i& bounds,
		double advanceX, double advanceY)
	{
		unsigned msb = (glyphCode >> 8) & 0xFF;
		if (fGlyphs[msb] == 0) {
			fGlyphs[msb]
				= (GlyphCache**)fAllocator.allocate(sizeof(GlyphCache*) * 256,
						sizeof(GlyphCache*));
			memset(fGlyphs[msb], 0, sizeof(GlyphCache*) * 256);
		}

		unsigned lsb = glyphCode & 0xFF;
		if (fGlyphs[msb][lsb])
			return 0; // already exists, do not overwrite

		GlyphCache* glyph
			= (GlyphCache*)fAllocator.allocate(sizeof(GlyphCache),
					sizeof(double));

		glyph->glyph_index = glyphIndex;
		glyph->data = fAllocator.allocate(dataSize);
		glyph->data_size = dataSize;
		glyph->data_type = dataType;
		glyph->bounds = bounds;
		glyph->advance_x = advanceX;
		glyph->advance_y = advanceY;

		return fGlyphs[msb][lsb] = glyph;
	}

 private:
	agg::pod_allocator		fAllocator;
	GlyphCache**			fGlyphs[256];
};

// #pragma mark -

// constructor
FontCacheEntry::FontCacheEntry()
	: fGlyphCache(new GlyphCachePool())
	, fEngine()
	, fLastUsedTime(LONGLONG_MIN)
	, fUseCounter(0)
{
}

// destructor
FontCacheEntry::~FontCacheEntry()
{
//printf("~FontCacheEntry()\n");
	delete fGlyphCache;
}

// Init
bool
FontCacheEntry::Init(const Font& font, bool forceOutline)
{
	// load the font file in the font engine
	font_family family;
	font_style style;
	font.GetFamilyAndStyle(&family, &style);

//printf("FontCacheEntry::Init(%s/%s, outline: %d)\n",
//	family, style, forceOutline);

	BAutolock _(FontManager::Default());

	const char* fontFilePath
		= FontManager::Default()->FontFileFor(family, style);

	if (!fontFilePath)
		return false;

	glyph_rendering renderingType = glyph_ren_native_gray8;
	if (forceOutline || font.Rotation() != 0.0 || font.Shear() != 90.0)
		renderingType = glyph_ren_outline;

	if (!fEngine.Init(fontFilePath, 0, font.Size(), FT_ENCODING_NONE,
			renderingType, font.Hinting())) {
		printf("FontCacheEntry::Init() - some error loading font "
			"file %s/%s\n", family, style);
		return false;
	}
	return true;
}

// HasGlyphs
bool
FontCacheEntry::HasGlyphs(uint16* glyphCodes, size_t count) const
{
	for (size_t i = 0; i < count; i++) {
		if (!fGlyphCache->FindGlyph(glyphCodes[i]))
			return false;
	}
	return true;
}

// Glyph
const GlyphCache*
FontCacheEntry::Glyph(uint16 glyphCode)
{
	const GlyphCache* glyph = fGlyphCache->FindGlyph(glyphCode);
	if (glyph) {
		return glyph;
	} else {
		if (fEngine.PrepareGlyph(glyphCode)) {
			glyph = fGlyphCache->CacheGlyph(glyphCode,
				fEngine.GlyphIndex(), fEngine.DataSize(),
				fEngine.DataType(), fEngine.Bounds(),
				fEngine.AdvanceX(), fEngine.AdvanceY());

			fEngine.WriteGlyphTo(glyph->data);

			return glyph;
		}
	}
	return 0;
}

// InitAdaptors
void
FontCacheEntry::InitAdaptors(const GlyphCache* glyph,
	double x, double y, GlyphMonoAdapter& monoAdapter,
	GlyphGray8Adapter& gray8Adapter, GlyphPathAdapter& pathAdapter,
	double scale)
{
	if (!glyph)
		return;

	switch(glyph->data_type) {
		case glyph_data_mono:
			monoAdapter.init(glyph->data, glyph->data_size, x, y);
			break;

		case glyph_data_gray8:
			gray8Adapter.init(glyph->data, glyph->data_size, x, y);
			break;

		case glyph_data_outline:
			pathAdapter.init(glyph->data, glyph->data_size, x, y, scale);
			break;

		default:
			break;
	}
}

// GetKerning
bool
FontCacheEntry::GetKerning(uint16 glyphCode1, uint16 glyphCode2,
	double* x, double* y)
{
	return fEngine.GetKerning(glyphCode1, glyphCode2, x, y);
}

// GenerateSignature
/*static*/ void
FontCacheEntry::GenerateSignature(char* signature, const Font& font,
	bool forceOutline)
{
	// TODO: read more of these from the font
	FT_Encoding charMap = FT_ENCODING_NONE;

	font_family family;
	font_style style;
	font.GetFamilyAndStyle(&family, &style);

	int faceIndex = 0;

	glyph_rendering renderingType = glyph_ren_native_gray8;
	if (font.Rotation() != 0.0 || font.Shear() != 90.0)
		renderingType = glyph_ren_outline;

	sprintf(signature, "%s%s_%u_%d_%d_%.3f_%d_%d", family, style, charMap,
		faceIndex, int(renderingType), font.Size(), font.Hinting(),
		forceOutline);
}

// UpdateUsage
void
FontCacheEntry::UpdateUsage()
{
	// this is a static lock to prevent usage of too many semaphores,
	// but on the other hand, it is not so nice to be using a lock
	// here at all
	// the hope is that the time is so short to hold this lock, that
	// there is not much contention
	BAutolock _(sUsageUpdateLock);

	fLastUsedTime = system_time();
	fUseCounter++;
}

