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

#include <stdbool.h>

#include <SDL2/SDL.h>
//#include <SDL2/SDL_byteorder.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_endian.h>

#include "gui.h"


SDL_Surface *surf = NULL;
SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;
TTF_Font *font = NULL;
S8 video_bpp ALIGN;

// SDL joystick input
SDL_Joystick *joystick;


// gui
extern SDL_Surface *copy_surface;			/* backup surface for example menues pixel overdraw */
extern SDL_Renderer *copy_renderer;
extern SDL_Surface *temp_surface;			/* new surface for menu */
extern SDL_Renderer *temp_renderer;

// sandbox file access
U1 get_sandbox_filename (U1 *filename, U1 *sandbox_filename, S2 max_name_len);

// sdl gfx functions --------------------------------------
U1 *sdl_open_screen (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// top of stack: byte bit
	// second: quadword height
	// third: quadword width

	S8 width ALIGN = 0, height ALIGN = 0;
	S8 bit ALIGN = 0;
	U1 err = 0;

	sp = stpopi ((U1 *) &bit, sp, sp_top);
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
		// error fail code
		sp = stpushi (1, sp, sp_bottom);
		if (sp == NULL)
		{
			printf ("sdl_open_screen: ERROR: stack corrupt!\n");
		}
		return (sp);
	}

	printf ("open_screen: %lli x %lli, %lli bit\n", width, height, bit);

	#if WINDOWS_10_WSL
	if (SDL_Init (SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) != 0)
	#else 
	if (SDL_Init (SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK) != 0)
	#endif
	{
		printf ("ERROR SDL_Init!!!\n");

		// error fail code
		sp = stpushi (1, sp, sp_bottom);
		if (sp == NULL)
		{
			printf ("sdl_open_screen: ERROR: stack corrupt!\n");
		}
		return (sp);
	}

	// Check for joystick
	if (SDL_NumJoysticks() > 0) {
    	// Open joystick

		SDL_JoystickEventState (SDL_ENABLE);

    	joystick = SDL_JoystickOpen (0);

    	if (joystick)
		{
	        printf ("Opened joystick 0\n");
	        printf ("Name: %s\n", SDL_JoystickNameForIndex (0));
        	printf ("Number of axes: %d\n", SDL_JoystickNumAxes (joystick));
        	printf ("Number of buttons: %d\n", SDL_JoystickNumButtons (joystick));
        	printf ("Number of balls: %d\n", SDL_JoystickNumBalls (joystick));
    	}
		else
		{
        	printf ("Couldn't open joystick 0\n");
    	}
	}

	#if ! WINDOWS_10_WSL
	// init audio
	int audio_rate = 44100; Uint16 audio_format = AUDIO_S16SYS;
	int audio_channels = 2; int audio_buffers = 4096;

	if (Mix_OpenAudio (audio_rate, audio_format, audio_channels, audio_buffers) != 0)
	{
		printf ("sdl_open_screen: ERROR: unable to initialize audio: %s\n", Mix_GetError());
		return (sp);
	}
	#endif


	/* key input settings */
	/*
	SDL_EnableUNICODE (SDL_ENABLE);
	SDL_EnableKeyRepeat (500, 125);
	*/
	if (TTF_Init () < 0)
	{
		printf ("ERROR TTF_Init!!!\n");

		// error fail code
		sp = stpushi (1, sp, sp_bottom);
		if (sp == NULL)
		{
			printf ("sdl_open_screen: ERROR: stack corrupt!\n");
		}
		return (sp);
	}

	printf ("SDL initialized...\n");

	// open SDL window
	window = SDL_CreateWindow ("L1VM", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_OPENGL);
	if (window == NULL)
	{
		printf( "sdl_open_screen: ERROR window can't be opened: %s\n", SDL_GetError ());

		// error fail code
		sp = stpushi (1, sp, sp_bottom);
		if (sp == NULL)
		{
			printf ("sdl_open_screen: ERROR: stack corrupt!\n");
		}
		return (sp);
	}

	// get window surface
	surf = SDL_GetWindowSurface (window);
	if (surf == NULL)
	{
		fprintf (stderr, "Couldn't set %lli x %lli x %lli video mode: %s\n", width, height, bit, SDL_GetError ());

		// error fail code
		sp = stpushi (1, sp, sp_bottom);
		if (sp == NULL)
		{
			printf ("sdl_open_screen: ERROR: stack corrupt!\n");
		}
		return (sp);
	}

	// set renderer
	// renderer = SDL_CreateRenderer (window, -1, SDL_RENDERER_ACCELERATED);
	// renderer = SDL_CreateRenderer (window, -1, SDL_RENDERER_SOFTWARE);

	renderer = SDL_CreateSoftwareRenderer (surf);

	SDL_RenderClear (renderer);

	SDL_SetRenderDrawColor (renderer, 0, 0, 0, 255);
	SDL_RenderClear (renderer);
	SDL_RenderPresent (renderer);

	// error OK code
	sp = stpushi (0, sp, sp_bottom);
	if (sp == NULL)
	{
		printf ("sdl_open_screen: ERROR: stack corrupt!\n");
	}
	return (sp);
}

U1 *sdl_open_screen_full (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// set SDL FULL SCREEN
	// top of stack: byte bit
	// second: quadword height
	// third: quadword width

	S8 width ALIGN = 0, height ALIGN = 0;
	S8 bit ALIGN = 0;
	U1 err = 0;

	sp = stpopi ((U1 *) &bit, sp, sp_top);
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
		// error fail code
		sp = stpushi (1, sp, sp_bottom);
		if (sp == NULL)
		{
			printf ("sdl_open_screen: ERROR: stack corrupt!\n");
		}
		return (sp);
	}

	printf ("open_screen: %lli x %lli, %lli bit\n", width, height, bit);

	#if WINDOWS_10_WSL
	if (SDL_Init (SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) != 0)
	#else
	if (SDL_Init (SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK) != 0)
	#endif
	{
		printf ("ERROR SDL_Init!!!\n");

		// error fail code
		sp = stpushi (1, sp, sp_bottom);
		if (sp == NULL)
		{
			printf ("sdl_open_screen: ERROR: stack corrupt!\n");
		}
		return (sp);
	}

	// Check for joystick
	if (SDL_NumJoysticks() > 0) {
    	// Open joystick

		SDL_JoystickEventState (SDL_ENABLE);

    	joystick = SDL_JoystickOpen (0);

    	if (joystick)
		{
	        printf ("Opened joystick 0\n");
	        printf ("Name: %s\n", SDL_JoystickNameForIndex (0));
        	printf ("Number of axes: %d\n", SDL_JoystickNumAxes (joystick));
        	printf ("Number of buttons: %d\n", SDL_JoystickNumButtons (joystick));
        	printf ("Number of balls: %d\n", SDL_JoystickNumBalls (joystick));
    	}
		else
		{
        	printf ("Couldn't open joystick 0\n");
    	}
	}

	#if ! WINDOWS_10_WSL
	// init audio
	int audio_rate = 44100; Uint16 audio_format = AUDIO_S16SYS;
	int audio_channels = 2; int audio_buffers = 4096;

	if (Mix_OpenAudio (audio_rate, audio_format, audio_channels, audio_buffers) != 0)
	{
		printf ("sdl_open_screen: ERROR: unable to initialize audio: %s\n", Mix_GetError());
		return (sp);
	}
	#endif

	/* key input settings */
	/*
	SDL_EnableUNICODE (SDL_ENABLE);
	SDL_EnableKeyRepeat (500, 125);
	*/
	if (TTF_Init () < 0)
	{
		printf ("ERROR TTF_Init!!!\n");

		// error fail code
		sp = stpushi (1, sp, sp_bottom);
		if (sp == NULL)
		{
			printf ("sdl_open_screen: ERROR: stack corrupt!\n");
		}
		return (sp);
	}

	printf ("SDL initialized...\n");

	// open SDL window
	window = SDL_CreateWindow ("L1VM", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN);
	if (window == NULL)
	{
		printf( "sdl_open_screen: ERROR window can't be opened: %s\n", SDL_GetError ());

		// error fail code
		sp = stpushi (1, sp, sp_bottom);
		if (sp == NULL)
		{
			printf ("sdl_open_screen: ERROR: stack corrupt!\n");
		}
		return (sp);
	}

	// get window surface
	surf = SDL_GetWindowSurface (window);
	if (surf == NULL)
	{
		fprintf (stderr, "Couldn't set %lli x %lli x %lli video mode: %s\n", width, height, bit, SDL_GetError ());

		// error fail code
		sp = stpushi (1, sp, sp_bottom);
		if (sp == NULL)
		{
			printf ("sdl_open_screen: ERROR: stack corrupt!\n");
		}
		return (sp);
	}

	// set renderer
	// renderer = SDL_CreateRenderer (window, -1, SDL_RENDERER_ACCELERATED);
	// renderer = SDL_CreateRenderer (window, -1, SDL_RENDERER_SOFTWARE);

	renderer = SDL_CreateSoftwareRenderer (surf);

	SDL_RenderClear (renderer);

	SDL_SetRenderDrawColor (renderer, 0, 0, 0, 255);
	SDL_RenderClear (renderer);
	SDL_RenderPresent (renderer);

	// error OK code
	sp = stpushi (0, sp, sp_bottom);
	if (sp == NULL)
	{
		printf ("sdl_open_screen: ERROR: stack corrupt!\n");
	}
	return (sp);
}

U1 *sdl_set_window_title (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 title_address;

	sp = stpopi ((U1 *) &title_address, sp, sp_top);
	if (sp == NULL)
	{
		printf ("sdl_set_window_title: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (window != NULL)
	{
		SDL_SetWindowTitle (window, (const char *) &data[title_address]);
	}
	
	return (sp);
}

U1 *sdl_toggle_mouse_pointer (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S2 enable;

	sp = stpopi ((U1 *) &enable, sp, sp_top);
	if (sp == NULL)
	{
		printf ("sdl_toggle_mouse_pointer: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (enable == 0)
	{
		SDL_ShowCursor (SDL_DISABLE);
	}
	else
	{
		SDL_ShowCursor (SDL_ENABLE);
	}
	return (sp);
}

void sdl_do_delay (S8 delay)
{
	const Uint32 startMs = SDL_GetTicks();

	while (SDL_GetTicks () - startMs < delay)
    {
		SDL_PumpEvents ();

		SDL_RenderPresent (renderer);
		SDL_UpdateWindowSurface (window);
    }
}

U1 *sdl_delay (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 delay;

	sp = stpopi ((U1 *) &delay, sp, sp_top);
	if (sp == NULL)
	{
		printf ("sdl_delay: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sdl_do_delay (delay);
    return (sp);
}

U1 *sdl_quit (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// free renderer
	SDL_DestroyRenderer (renderer);

	// gui
	if (copy_renderer) SDL_DestroyRenderer (copy_renderer);
	if (temp_renderer) SDL_DestroyRenderer (temp_renderer);

	if (copy_surface) SDL_FreeSurface (copy_surface);
	if (temp_surface) SDL_FreeSurface (temp_surface);

	// main surface
	if (surf) SDL_FreeSurface (surf);

	// destroy window
	SDL_DestroyWindow (window);
	TTF_CloseFont (font);
	TTF_Quit ();
	SDL_Quit ();

	return (sp);
}

U1 *sdl_update (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// printf ("sdl_update...\n");
	// SDL_Flip (surf);
	SDL_PumpEvents();

	SDL_RenderPresent (renderer);
	SDL_UpdateWindowSurface (window);
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

	U1 sandbox_filename[256];

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

#if SANDBOX
	if (get_sandbox_filename (&data[nameaddr], sandbox_filename, 255) != 0)
	{
		printf ("sdl_font_ttf: ERROR filename illegal: %s", &data[nameaddr]);
		return (NULL);
	}
	font = TTF_OpenFont ((const char *) sandbox_filename, size);
#else
	font = TTF_OpenFont ((const char *) &data[nameaddr], size);
#endif

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
		return (NULL);
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

	pixelRGBA (renderer, x, y, r, g, b, alpha);
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

	lineRGBA (renderer, x1, y1, x2, y2, r, g, b, alpha);
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

	rectangleRGBA (renderer, x1, y1, x2, y2, r, g, b, alpha);
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

	boxRGBA (renderer, x1, y1, x2, y2, r, g, b, alpha);
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

	circleRGBA (renderer, x, y, radius, r, g, b, alpha);
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

	filledCircleRGBA (renderer, x, y, radius, r, g, b, alpha);
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

	ellipseRGBA (renderer, x, y, xradius, yradius, r, g, b, alpha);
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

	filledEllipseRGBA (renderer, x, y, xradius, yradius, r, g, b, alpha);
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

	pieRGBA (renderer, x, y, radius, startangle, endangle, r, g, b, alpha);
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

	filledPieRGBA (renderer, x, y, radius, startangle, endangle, r, g, b, alpha);
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

	trigonRGBA (renderer, x1, y1, x2, y2, x3, y3, r, g, b, alpha);
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

	filledTrigonRGBA (renderer, x1, y1, x2, y2, x3, y3, r, g, b, alpha);
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
			#if __MACH__ 	// macOS little endianess
				return p[0] | p[1] << 8 | p[2] << 16;
			#else
				if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
				{
					return p[0] << 16 | p[1] << 8 | p[2];
				}
				else
				{
					return p[0] | p[1] << 8 | p[2] << 16;
				}
			#endif

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


U1 *sdl_load_picture (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// load picture
	// arguments: picture name, x, y
	// return: int error code

	SDL_Surface *picture;
	SDL_Rect dstrect;

	S8 nameaddr ALIGN;
	S8 x ALIGN;
	S8 y ALIGN;

	U1 sandbox_filename[256];

	sp = stpopi ((U1 *) &y, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("sdl_load_picture: ERROR stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &x, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("sdl_load_picture: ERROR stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &nameaddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("sdl_load_picture: ERROR stack corrupt!\n");
		return (NULL);
	}

	dstrect.x = x;
	dstrect.y = y;
	dstrect.w = 0;
	dstrect.h = 0;

#if SANDBOX
	if (get_sandbox_filename (&data[nameaddr], sandbox_filename, 255) != 0)
	{
		printf ("sdl_load_picture: ERROR filename illegal: %s\n", &data[nameaddr]);
		return (NULL);
	}
	picture = IMG_Load ((const char *) sandbox_filename);
#else
	picture = IMG_Load ((const char *) &data[nameaddr]);
#endif

	if (picture == NULL)
	{
		printf ("sdl_load_picture: can't load picture %s !\n", &data[nameaddr]);

		sp = stpushi (1, sp, sp_bottom);		// error fail code
		return (sp);
	}

	SDL_BlitSurface (picture, NULL, surf, &dstrect);
	SDL_FreeSurface (picture);

	sp = stpushi (0, sp, sp_bottom);		// error ok code
	return (sp);
}

U1 *sdl_save_picture (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// save picture as BMP picture
	//
	// arguments: picture name
	// return: int error code

	Uint32 rmask, gmask, bmask, amask;

	S8 nameaddr ALIGN;

	U1 sandbox_filename[256];
	S2 ret;

	#if __MACH__ 	// macOS little endianess
		rmask = 0x000000ff; gmask = 0x0000ff00; bmask = 0x00ff0000; amask = 0xff000000;
	#else
		#if SDL_BYTEORDER == SDL_BIG_ENDIAN
		rmask = 0xff000000; gmask = 0x00ff0000; bmask = 0x0000ff00; amask = 0x000000ff;
		#else
		rmask = 0x000000ff; gmask = 0x0000ff00; bmask = 0x00ff0000; amask = 0xff000000;
		#endif
	#endif

	sp = stpopi ((U1 *) &nameaddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("sdl_save_picture: ERROR stack corrupt!\n");
		return (NULL);
	}

	SDL_RenderPresent (renderer);

#if SANDBOX
	if (get_sandbox_filename (&data[nameaddr], sandbox_filename, 255) != 0)
	{
		printf ("sdl_save_picture: ERROR filename illegal: %s\n", &data[nameaddr]);
		return (NULL);
	}
	ret = SDL_SaveBMP (surf, (const char *) sandbox_filename);
#else
	ret = SDL_SaveBMP (surf, (const char *) &data[nameaddr]
#endif

	if (ret < 0)
	{
		// SDL_FreeSurface (output_surf);

		sp = stpushi (1, sp, sp_bottom);		// error fail code
		return (sp);
	}
	else
	{
		// 	SDL_FreeSurface (output_surf);

		sp = stpushi (0, sp, sp_bottom);		// error ok code
		return (sp);
	}
}

U1 *sdl_play_sound (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 ALIGN nameaddr;
	U1 sandbox_filename[256];

	Mix_Chunk *sound = NULL;
	int channel;

	sp = stpopi ((U1 *) &nameaddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("sdl_play_sound: ERROR stack corrupt!\n");
		return (NULL);
	}

	#if SANDBOX
		if (get_sandbox_filename (&data[nameaddr], sandbox_filename, 255) != 0)
		{
			printf ("sdl_play_sound: ERROR filename illegal: %s", &data[nameaddr]);
			return (NULL);
		}

		sound = Mix_LoadWAV ((const char *) sandbox_filename);
		if (sound == NULL)
		{
			 printf ("sdl_play_sound: ERROR: Unable to load WAV file: %s\n", Mix_GetError());
			 return (NULL);
		}
	#else
		sound = Mix_LoadWAV (&data[nameaddr]);
		if (sound == NULL)
		{
		 	printf ("sdl_play_sound: ERROR: Unable to load WAV file: %s\n", Mix_GetError());
		 	return (NULL);
		}
	#endif

	channel = Mix_PlayChannel (-1, sound, 0);
	if (channel == -1)
	{
		printf("sdl_play_sound: ERROR: Unable to play WAV file: %s\n", Mix_GetError());
		return (NULL);
	}

	// wait till sound play stops
	while (Mix_Playing (channel) != 0);
	Mix_FreeChunk (sound);
	// Mix_CloseAudio ();

	return (sp);
}
