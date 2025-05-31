/* This file gui.c is part of L1vm.
*
* (c) Copyright Stefan Pietzonke (jay-t@gmx.net), 2025
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
*
* Includes code from "flow" my Nano VM GUI server.
*/


#include "../../../include/global.h"
//#include "../../../include/stack.h"

#include <SDL3/SDL.h>
// #include <SDL3/SDL_byteorder.h>
#include <SDL3/SDL_camera.h>
#include <SDL3_image/SDL_image.h>

// proto
U1 *stpushi (U1 data, U1 *sp, U1 *sp_bottom);

extern SDL_Window *window;
extern SDL_Renderer *renderer;

U1 open_camera;
SDL_CameraID *devices = NULL;
int devcount = 0;
static SDL_Camera *camera = NULL;
static SDL_Texture *camera_texture = NULL;
U1 camera_open = 0;
SDL_Surface *camera_frame;

U1 *sdl_open_camera (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	if (camera_open == 1)
	{
		// camera already opened, return
		sp = stpushi (0, sp, sp_bottom);
		if (sp == NULL)
		{
			printf ("sdl_open_camera: ERROR: stack corrupt!\n");
		}
		return (sp);
	}

	devices = SDL_GetCameras (&devcount);
	if (devices == NULL)
	{
		SDL_Log ("Couldn't enumerate camera devices: %s", SDL_GetError());
		sp = stpushi (1, sp, sp_bottom);
		if (sp == NULL)
		{
			printf ("sdl_open_camera: ERROR: stack corrupt!\n");
		}
		return (sp);
	}
	else if (devcount == 0)
	{
		SDL_Log ("Couldn't find any camera devices! Please connect a camera and try again.");
		sp = stpushi (1, sp, sp_bottom);
		if (sp == NULL)
		{
			printf ("sdl_open_camera ERROR: stack corrupt!\n");
		}
		return (sp);
	}

	camera = SDL_OpenCamera (devices[0], NULL);  // just take the first thing we see in any format it wants.
	SDL_free (devices);
	if (camera == NULL)
	{
		SDL_Log ("Couldn't open camera: %s", SDL_GetError());
		sp = stpushi (1, sp, sp_bottom);
		if (sp == NULL)
		{
			printf ("sdl_open_camera: ERROR: stack corrupt!\n");
		}
		return (sp);
	}

	camera_open = 1;
	sp = stpushi (0, sp, sp_bottom);
	if (sp == NULL)
	{
		printf ("sdl_open_camera: ERROR: stack corrupt!\n");
	}
	return (sp);
}

U1 *sdl_get_camera_frame (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	Uint64 timestampNS = 0;
	camera_frame = SDL_AcquireCameraFrame (camera, &timestampNS);

	if (camera_frame == NULL)
	{
		// no frame grabbed from camera
		sp = stpushi (1, sp, sp_bottom);
		if (sp == NULL)
		{
			printf ("sdl_get_camera_frame ERROR: stack corrupt!\n");
		}
		return (sp);
	}
	else
	{
		if (! camera_texture)
		{
			SDL_SetWindowSize (window, camera_frame->w, camera_frame->h);  /* Resize the window to match */
			camera_texture = SDL_CreateTexture (renderer, camera_frame->format, SDL_TEXTUREACCESS_STREAMING, camera_frame->w, camera_frame->h);
		}

		if (camera_texture)
		{
			SDL_UpdateTexture (camera_texture, NULL, camera_frame->pixels, camera_frame->pitch);
		}

		SDL_ReleaseCameraFrame (camera, camera_frame);

		SDL_SetRenderDrawColor (renderer, 0x99, 0x99, 0x99, SDL_ALPHA_OPAQUE);
		SDL_RenderClear (renderer);
		if (camera_texture)
		{  /* draw the latest camera frame, if available. */
			SDL_RenderTexture (renderer, camera_texture, NULL, NULL);
		}
		SDL_RenderPresent (renderer);
		SDL_UpdateWindowSurface (window);

		// frame grabbed from camera
		sp = stpushi (0, sp, sp_bottom);
		if (sp == NULL)
		{
			printf ("sdl_get_camera_frame ERROR: stack corrupt!\n");
		}
		return (sp);
	}
}

U1 *sdl_close_camera (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	SDL_CloseCamera (camera);
	SDL_DestroyTexture(camera_texture);
	/* SDL will clean up the window/renderer for us. */

	return (sp);
}


