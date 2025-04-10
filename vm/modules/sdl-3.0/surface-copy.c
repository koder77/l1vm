SDL_Surface *copy_area_surface = NULL;      /* for do_array_copy, the user GUI area copy */
SDL_Renderer *copy_area_renderer = NULL;


// for user area copy
int do_area_copy (Sint16 x, Sint16 y, Sint16 width, Sint16 height)
{
    SDL_Rect menu_rect, copy_rect;

    /* allocate backup surface, to copy the area that will be covered by the menu */

	copy_area_surface = SDL_CreateRGBSurface (SDL_SWSURFACE, width, height, video_bpp, rmask, gmask, bmask, amask);
	if (copy_area_surface == NULL)
	{
		printf ("do_area_copy: error can't allocate copy %i surface!\n", gadget_index);
		return (FALSE);
	}

    /* Use alpha blending */
	if (SDL_SetSurfaceBlendMode (copy_area_surface, SDL_BLENDMODE_BLEND) < 0)
    {
		printf ("do_area_copy: error can't set copy alpha channel %i surface!\n", gadget_index);
		return (FALSE);
	}

	copy_area_renderer = SDL_CreateSoftwareRenderer (copy_surface);

    /* make copy of menu area */

    menu_rect.x = x;
    menu_rect.y = y;
    menu_rect.w = width;
    menu_rect.h = height;

    copy_rect.x = 0;
    copy_rect.y = 0;

	if (do_copy_surface (copy_area_renderer, surf, &menu_rect, copy_area_surface, &copy_rect) < 0)
	{
		printf ("do_area_copy: error can't blit copy %i surface!\n", gadget_index);
		return (FALSE);
	}

    return (TRUE);
}

int restore_area_copy (Sint16 x, Sint16 y, Sint16 width, Sint16 height)
{
    SDL_Rect menu_rect, copy_rect;

    menu_rect.x = x;
    menu_rect.y = y;
    menu_rect.w = width;
    menu_rect.h = height;

	if (do_copy_surface (copy_area_renderer, copy_area_surface, NULL, surf, &menu_rect) < 0)
	{
		printf ("restore_area_copy: error can't blit copy %i surface!\n", gadget_index);
		return (FALSE);
	}

    update_rect (x, y, width, height);
	SDL_UpdateWindowSurface (window);

    return (TRUE);
}
