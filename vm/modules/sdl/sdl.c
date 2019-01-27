/*
 * This file sdl.c is part of L1vm.
 *
 * (c) Copyright Stefan Pietzonke (jay-t@gmx.net), 2017
 *
 * L1vm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * L1vm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with L1vm.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "../../../include/global.h"
#include "../../../include/stack.h"

#include <SDL/SDL.h>
#include <SDL/SDL_byteorder.h>
#include <SDL/SDL_gfxPrimitives.h>
#include <SDL/SDL_ttf.h>
#include <SDL/SDL_image.h>

extern SDL_Surface *surf;
extern TTF_Font *font;
S8 video_bpp ALIGN;

// sdl gfx functions --------------------------------------

U1 *sdl_open_screen (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// top of stack: byte bit
	// second: quadword height
	// third: quadword width

	S8 width ALIGN = 0, height ALIGN = 0;
	U1 bit = 0;
	U1 err = 0;

	sp = stpopb (&bit, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	video_bpp = bit;

	sp = stpopi ((U1 *) &height, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &width, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	if (err == 1)
	{
		stpushb (1, sp, sp_bottom);		// error fail code
		printf ("sdl_open_screen: ERROR: stack corrupt!\n");
		return (NULL);
	}

	// printf ("open_screen: %lli x %lli, %i bit\n", width, height, bit);

	// open SDL screen
	surf = SDL_SetVideoMode (width, height, bit, SDL_SWSURFACE | SDL_SRCALPHA | SDL_RESIZABLE | SDL_ANYFORMAT);
	if (surf == NULL)
	{
		fprintf (stderr, "Couldn't set %lli x %lli x %i video mode: %s\n", width, height, bit, SDL_GetError ());
		sp = stpushb (1, sp, sp_bottom);		// error fail code
		return (NULL);
	}
	else
	{
		sp = stpushb (0, sp, sp_bottom);		// error ok code
		return (sp);
	}
}

U1 *sdl_quit (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// dummy function, closed by vm/main.c at exit
	return (sp);
}

U1 *sdl_update (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// printf ("sdl_update...\n");
	SDL_Flip (surf);
	return (sp);
}

// text functions -----------------------------------------

U1 *sdl_font_ttf (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// top of stack: quadword size
	// 2: name address

	S8 nameaddr;
	S8 size ALIGN;
	U1 err = 0;

	sp = stpopi ((U1 *) &size, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &nameaddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	if (err == 1)
	{
		printf ("sdl_font_ttf: ERROR: stack corrupt!\n");
		return (NULL);
	}

	// printf ("sdl_font_ttf: open: '%s'\n", &data[nameaddr]);

	if (font)
	{
		// close old font
		TTF_CloseFont (font);
	}

	font = TTF_OpenFont ((const char *) &data[nameaddr], size);
	if (font == NULL)
	{
		printf ("sdl_font_ttf: can't open font! %s\n", SDL_GetError ());
		return (NULL);
	}

	return (sp);
}

U1 *sdl_text_ttf (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// top of stack: name address
	// 2: byte blue
	// 3: byte green
	// 4: byte red
	// 5: quadword y
	// 6: quadword x

	S8 x ALIGN, y ALIGN;
	U1 r, g, b;
	U1 err = 0;
	S8 nameaddr;

	SDL_Surface *textsurf;
	SDL_Rect dstrect;
	SDL_Color color;

	sp = stpopi ((U1 *) &nameaddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopb (&b, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopb (&g, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopb (&r, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &y, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &x, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	if (err == 1)
	{
		printf ("sdl_text_ttf: ERROR: stack corrupt!\n");
		return (NULL);
	}

	color.r = r;
	color.g = g;
	color.b = b;

	textsurf = TTF_RenderText_Blended (font, (const char *) &data[nameaddr], color);
	if (textsurf == NULL)
	{
		printf ("sdl_text_ttf: can't render text! %s\n", SDL_GetError ());
	}

	dstrect.x = x;
	dstrect.y = y;
	dstrect.w = textsurf->w;
	dstrect.h = textsurf->h;

	SDL_BlitSurface (textsurf, NULL, surf, &dstrect);
	SDL_FreeSurface (textsurf);

	return (sp);
}

// graphics functions -------------------------------------

U1 *sdl_pixel_alpha (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// top of stack: byte alpha
	// 2: byte blue
	// 3: byte green
	// 4: byte red
	// 5: quadword y
	// 6: quadword x

	S8 x ALIGN, y ALIGN;
	U1 r, g, b;
	U1 alpha;
	U1 err = 0;

	sp = stpopb (&alpha, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopb (&b, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopb (&g, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopb (&r, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &y, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &x, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	if (err == 1)
	{
		printf ("sdl_pixel_alpha: ERROR: stack corrupt!\n");
		return (NULL);
	}

	// printf ("sdl_pixel_alpha: x: %lli, y: %lli r: %i, g: %i, b: %i, alpha: %i\n", x, y, r, g, b, alpha);

	pixelRGBA (surf, x, y, r, g, b, alpha);
	return (sp);
}

U1 *sdl_line_alpha (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// top of stack: byte alpha
	// 2: byte blue
	// 3: byte green
	// 4: byte red
	// 5: quadword y2
	// 6: quadword x2
	// 7: quadword y1
	// 8: quadword x1

	S8 x1 ALIGN, y1 ALIGN, x2 ALIGN, y2 ALIGN;
	U1 r, g, b;
	U1 alpha;
	U1 err = 0;

	sp = stpopb (&alpha, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopb (&b, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopb (&g, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopb (&r, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &y2, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &x2, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &y1, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &x1, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	if (err == 1)
	{
		printf ("sdl_line_alpha: ERROR: stack corrupt!\n");
		return (NULL);
	}

	lineRGBA (surf, x1, y1, x2, y2, r, g, b, alpha);
	return (sp);
}

U1 *sdl_rectangle_alpha (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// top of stack: byte alpha
	// 2: byte blue
	// 3: byte green
	// 4: byte red
	// 5: quadword y2
	// 6: quadword x2
	// 7: quadword y1
	// 8: quadword x1

	S8 x1 ALIGN, y1 ALIGN, x2 ALIGN, y2 ALIGN;
	U1 r, g, b;
	U1 alpha;
	U1 err = 0;

	sp = stpopb (&alpha, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopb (&b, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopb (&g, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopb (&r, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &y2, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &x2, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &y1, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &x1, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	if (err == 1)
	{
		printf ("sdl_rectangle_alpha: ERROR: stack corrupt!\n");
		return (NULL);
	}

	rectangleRGBA (surf, x1, y1, x2, y2, r, g, b, alpha);
	return (sp);
}

U1 *sdl_rectangle_fill_alpha (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// top of stack: byte alpha
	// 2: byte blue
	// 3: byte green
	// 4: byte red
	// 5: quadword y2
	// 6: quadword x2
	// 7: quadword y1
	// 8: quadword x1

	S8 x1 ALIGN, y1 ALIGN, x2 ALIGN, y2 ALIGN;
	U1 r, g, b;
	U1 alpha;
	U1 err = 0;

	sp = stpopb (&alpha, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopb (&b, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopb (&g, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopb (&r, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &y2, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &x2, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &y1, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &x1, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	if (err == 1)
	{
		printf ("sdl_rectangle_fill_alpha: ERROR: stack corrupt!\n");
		return (NULL);
	}

	boxRGBA (surf, x1, y1, x2, y2, r, g, b, alpha);
	return (sp);
}

U1 *sdl_circle_alpha (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// top of stack: byte alpha
	// 2: byte blue
	// 3: byte green
	// 4: byte red
	// 5: quadword radius
	// 6: quadword y
	// 7: quadword x

	S8 x ALIGN, y ALIGN, radius ALIGN;
	U1 r, g, b;
	U1 alpha;
	U1 err = 0;

	sp = stpopb (&alpha, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopb (&b, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopb (&g, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopb (&r, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &radius, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &y, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &x, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	if (err == 1)
	{
		printf ("sdl_circle_alpha: ERROR: stack corrupt!\n");
		return (NULL);
	}

	circleRGBA (surf, x, y, radius, r, g, b, alpha);
	return (sp);
}

U1 *sdl_circle_fill_alpha (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// top of stack: byte alpha
	// 2: byte blue
	// 3: byte green
	// 4: byte red
	// 5: quadword radius
	// 6: quadword y
	// 7: quadword x

	S8 x ALIGN, y ALIGN, radius ALIGN;
	U1 r, g, b;
	U1 alpha;
	U1 err = 0;

	sp = stpopb (&alpha, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopb (&b, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopb (&g, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopb (&r, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &radius, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &y, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &x, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	if (err == 1)
	{
		printf ("sdl_circle_fill_alpha: ERROR: stack corrupt!\n");
		return (NULL);
	}

	filledCircleRGBA (surf, x, y, radius, r, g, b, alpha);
	return (sp);
}

U1 *sdl_ellipse_alpha (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// top of stack: byte alpha
	// 2: byte blue
	// 3: byte green
	// 4: byte red
	// 5: quadword yradius
	// 6: quadword xradius
	// 7: quadword y
	// 8: quadword x

	S8 x ALIGN, y ALIGN, xradius ALIGN, yradius ALIGN;
	U1 r, g, b;
	U1 alpha;
	U1 err = 0;

	sp = stpopb (&alpha, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopb (&b, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopb (&g, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopb (&r, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &yradius, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &xradius, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &y, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &x, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	if (err == 1)
	{
		printf ("sdl_ellipse_alpha: ERROR: stack corrupt!\n");
		return (NULL);
	}

	ellipseRGBA (surf, x, y, xradius, yradius, r, g, b, alpha);
	return (sp);
}

U1 *sdl_ellipse_fill_alpha (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// top of stack: byte alpha
	// 2: byte blue
	// 3: byte green
	// 4: byte red
	// 5: quadword yradius
	// 6: quadword xradius
	// 7: quadword y
	// 8: quadword x

	S8 x ALIGN, y ALIGN, xradius ALIGN, yradius ALIGN;
	U1 r, g, b;
	U1 alpha;
	U1 err = 0;

	sp = stpopb (&alpha, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopb (&b, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopb (&g, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopb (&r, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &yradius, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &xradius, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &y, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &x, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	if (err == 1)
	{
		printf ("sdl_ellipse_fill_alpha: ERROR: stack corrupt!\n");
		return (NULL);
	}

	filledEllipseRGBA (surf, x, y, xradius, yradius, r, g, b, alpha);
	return (sp);
}

U1 *sdl_pie_alpha (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// top of stack: byte alpha
	// 2: byte blue
	// 3: byte green
	// 4: byte red
	// 5: quadword endangle
	// 6: quadword startangle
	// 7: quadword radius
	// 8: quadword y
	// 9: quadword x

	S8 x ALIGN, y ALIGN, radius ALIGN, startangle ALIGN, endangle ALIGN;
	U1 r, g, b;
	U1 alpha;
	U1 err = 0;

	sp = stpopb (&alpha, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopb (&b, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopb (&g, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopb (&r, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &endangle, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &startangle, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &radius, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &y, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &x, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	if (err == 1)
	{
		printf ("sdl_pie_alpha: ERROR: stack corrupt!\n");
		return (NULL);
	}

	pieRGBA (surf, x, y, radius, startangle, endangle, r, g, b, alpha);
	return (sp);
}

U1 *sdl_pie_fill_alpha (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// top of stack: byte alpha
	// 2: byte blue
	// 3: byte green
	// 4: byte red
	// 5: quadword endangle
	// 6: quadword startangle
	// 7: quadword radius
	// 8: quadword y
	// 9: quadword x

	S8 x ALIGN, y ALIGN, radius ALIGN, startangle ALIGN, endangle ALIGN;
	U1 r, g, b;
	U1 alpha;
	U1 err = 0;

	sp = stpopb (&alpha, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopb (&b, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopb (&g, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopb (&r, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &endangle, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &startangle, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &radius, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &y, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &x, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	if (err == 1)
	{
		printf ("sdl_pie_fill_alpha: ERROR: stack corrupt!\n");
		return (NULL);
	}

	filledPieRGBA (surf, x, y, radius, startangle, endangle, r, g, b, alpha);
	return (sp);
}

U1 *sdl_trigon_alpha (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// top of stack: byte alpha
	// 2: byte blue
	// 3: byte green
	// 4: byte red
	// 5: quadword y3
	// 6: quadword x3
	// 7: quadword y2
	// 8: quadword x2
	// 9: quadword y1
	// 10: quadword x1

	S8 x1 ALIGN, y1 ALIGN, x2 ALIGN, y2 ALIGN, x3 ALIGN, y3 ALIGN;
	U1 r, g, b;
	U1 alpha;
	U1 err = 0;

	sp = stpopb (&alpha, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopb (&b, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopb (&g, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopb (&r, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &y3, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &x3, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &y2, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &x2, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &y1, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &x1, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	if (err == 1)
	{
		printf ("sdl_trigon_alpha: ERROR: stack corrupt!\n");
		return (NULL);
	}

	trigonRGBA (surf, x1, y1, x2, y2, x3, y3, r, g, b, alpha);
	return (sp);
}

U1 *sdl_trigon_fill_alpha (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// top of stack: byte alpha
	// 2: byte blue
	// 3: byte green
	// 4: byte red
	// 5: quadword y3
	// 6: quadword x3
	// 7: quadword y2
	// 8: quadword x2
	// 9: quadword y1
	// 10: quadword x1

	S8 x1 ALIGN, y1 ALIGN, x2 ALIGN, y2 ALIGN, x3 ALIGN, y3 ALIGN;
	U1 r, g, b;
	U1 alpha;
	U1 err = 0;

	sp = stpopb (&alpha, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopb (&b, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopb (&g, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopb (&r, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &y3, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &x3, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &y2, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &x2, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &y1, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &x1, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	if (err == 1)
	{
		printf ("sdl_trigon_fill_alpha: ERROR: stack corrupt!\n");
		return (NULL);
	}

	filledTrigonRGBA (surf, x1, y1, x2, y2, x3, y3, r, g, b, alpha);
	return (sp);
}

/*
 * Return the pixel value at (x, y)
 * NOTE: The surface must be locked before calling this!
 */
Uint32 getpixel (SDL_Surface *surface, Sint16 x, Sint16 y)
{
    Uint8 bpp = surface->format->BytesPerPixel;
    /* Here p is the address to the pixel we want to retrieve */
    Uint8 *p = (Uint8 *) surface->pixels + y * surface->pitch + x * bpp;

    switch (bpp)
    {
        case 1:
            return *p;

        case 2:
            return *(Uint16 *)p;

        case 3:
            if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
            {
                return p[0] << 16 | p[1] << 8 | p[2];
            }
            else
            {
                return p[0] | p[1] << 8 | p[2] << 16;
            }

        case 4:
            return *(Uint32 *)p;

        default:
            return 0;       /* shouldn't happen, but avoids warnings */
    }
}

U1 *sdl_get_pixelcolor (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// top of stack: quadword y
	// 2: quadword x
	S8 x ALIGN, y ALIGN;
	Uint32 pixel;
	Uint8 r, g, b;
	U1 err = 0;

	sp = stpopi ((U1 *) &y, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &x, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	if (err == 1)
	{
		printf ("sdl_get_pixelcolor: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (SDL_LockSurface (surf) < 0)
	{
		printf ("sdl_get_pixelcolor: can't lock surface!\n");

//	-1 = ERROR code!
		sp = stpushi (-1, sp, sp_bottom);
		sp = stpushi (-1, sp, sp_bottom);
		sp = stpushi (-1, sp, sp_bottom);

		return (sp);
	}

	pixel = getpixel (surf, x, y);
    SDL_UnlockSurface (surf);
    SDL_GetRGB (pixel, surf->format, &r, &g, &b);

	sp = stpushi (b, sp, sp_bottom);
	sp = stpushi (g, sp, sp_bottom);
	sp = stpushi (r, sp, sp_bottom);
	return (sp);
}
