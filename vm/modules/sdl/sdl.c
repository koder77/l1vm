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

#include <SDL/SDL.h>
#include <SDL/SDL_byteorder.h>
#include <SDL/SDL_gfxPrimitives.h>
#include <SDL/SDL_ttf.h>
#include <SDL/SDL_image.h>


static SDL_Surface *surf = NULL;


// stack operations ---------------------------------------
// byte 

U1 *stpushb (U1 data, U1 *sp, U1 *sp_bottom)
{
	if (sp >= sp_bottom)
	{
		sp--;
		
		*sp = data;
		return (sp);		// success
	}
	else
	{
		// fatal ERROR: stack pointer can't go below address ZERO!
		return (NULL);		// FAIL
	}
}

U1 *stpopb (U1 *data, U1 *sp, U1 *sp_top)
{
	if (sp == sp_top)
	{
		// nothing on stack!! can't pop!!
		return (NULL);		// FAIL
	}
	
	*data = *sp;
	
	sp++;
	return (sp);			// success
}

// quadword

U1 *stpushi (S8 data, U1 *sp, U1 *sp_bottom)
{
	U1 *bptr;
	
	if (sp >= sp_bottom + 8)
	{
		// set stack pointer to lower address
		
		bptr = (U1 *) &data;
		
		sp--;
		*sp-- = *bptr;
		bptr++;
		*sp-- = *bptr;
		bptr++;
		*sp-- = *bptr;
		bptr++;
		*sp-- = *bptr;
		bptr++;
		*sp-- = *bptr;
		bptr++;
		*sp-- = *bptr;
		bptr++;
		*sp-- = *bptr;
		bptr++;
		*sp = *bptr;
		
		return (sp);			// success
	}
	else
	{
		// fatal ERROR: stack pointer can't go below address ZERO!
		return (NULL);			// FAIL
	}
}

U1 *stpopi (U1 *data, U1 *sp, U1 *sp_top)
{
	if (sp == sp_top)
	{
		// nothing on stack!! can't pop!!
		return (NULL);			// FAIL
	}
	
	data[7] = *sp++;
	data[6] = *sp++;
	data[5] = *sp++;
	data[4] = *sp++;
	data[3] = *sp++;
	data[2] = *sp++;
	data[1] = *sp++;
	data[0] = *sp++;
	
	return (sp);			// success
}

// --------------------------------------------------------

// sdl gfx functions --------------------------------------

U1 *sdl_open_screen (U1 *sp, U1 *sp_top, U1 *sp_bottom)
{
	// top of stack: quadword bit
	// second: quadword height
	// third: quadword width
	
	S8 width = 0, height = 0;
	U1 bit = 0;
	U1 err = 0;
	
	sp = stpopb (&bit, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}
	
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
	
	printf ("open_screen: %lli x %lli, %i bit\n", width, height, bit);
	
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

U1 *sdl_quit (U1 *sp, U1 *sp_top, U1 *sp_bottom)
{
	SDL_Quit ();
	return (sp);
}

U1 *sdl_update (U1 *sp, U1 *sp_top, U1 *sp_bottom)
{
	SDL_Flip (surf);
	return (sp);
}

U1 *sdl_pixel_alpha (U1 *sp, U1 *sp_top, U1 *sp_bottom)
{
	// top of stack: byte alpha
	// 2: byte blue
	// 3: byte green
	// 4: byte red 
	// 5: quadword y 
	// 6: quadword x
	
	S8 x, y;
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

