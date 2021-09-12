/*
 * This file gui.c is part of L1vm.
 *
 * (c) Copyright Stefan Pietzonke (jay-t@gmx.net), 2019
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
// #include "../../../include/stack.h"

#include <SDL2/SDL.h>
// #include <SDL2/SDL_byteorder.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>

#include "gui.h"

// protos
U1 *stpopb (U1 *data, U1 *sp, U1 *sp_top);

size_t strlen_safe (const char * str, int maxlen);

extern SDL_Surface *surf;
extern SDL_Window *window;
extern SDL_Renderer *renderer;
extern TTF_Font *font;
extern S8 video_bpp;

// SDL joystick input
extern SDL_Joystick *joystick;

// SDL_Surface *bmap_copy;
// SDL_Surface *blitscreen;
struct gadget *gadget = NULL;
struct gadget_color gadget_color;
S8 ALIGN gadgets;
S8 ALIGN screennum = 0;


// cycle gadget
SDL_Surface *copy_surface = NULL;			/* backup surface for example menues pixel overdraw */
SDL_Renderer *copy_renderer = NULL;
SDL_Surface *temp_surface = NULL;			/* new surface for menu */
SDL_Renderer *temp_renderer = NULL;

void free_gadgets (void);

static struct screen screen[MAXSCREEN];

// memory functions

U1 **alloc_array_U1 (S4 x, S4 y)
{
    S4 i, alloc;
    U1 **array = NULL;
    U1 err = FALSE;

    array = (U1 **) malloc (x * sizeof (*array));
    if (array != NULL)
    {
        for (i = 0; i < x; i++)
        {
            array[i] = (U1 *) malloc (y * sizeof (*array));
            if (array[i] == NULL)
            {
                alloc = i - 1;
                err = TRUE;
                break;
            }
        }
        if (err)
        {
            if (alloc >= 0)
            {
                for (i = 0; i <= alloc; i++)
                {
                    free (array[i]);
                }
            }
            free (array);
            array = NULL;
        }
    }
    return (array);
}

void dealloc_array_U1 (U1 **array, S4 x)
{
    S4 i;

    for (i = 0; i < x; i++)
    {
        if (array[i])
        {
            free (array[i]);
        }
    }
    free (array);
}

void update_rect (Sint16 x, Sint16 y, Sint16 w, Sint16 h)
{
	SDL_Rect rect;

	rect.x = x; rect.y = y;
	rect.w = w; rect.h = h;

	SDL_UpdateWindowSurfaceRects (window, &rect, 1);
	SDL_UpdateWindowSurface (window);
}

void set_default_colors (void)
{
	screen[screennum].gadget_color.border_light.r = 232;
    screen[screennum].gadget_color.border_light.g = 239;
    screen[screennum].gadget_color.border_light.b = 247;

	screen[screennum].gadget_color.border_shadow.r = 194;
    screen[screennum].gadget_color.border_shadow.g = 201;
    screen[screennum].gadget_color.border_shadow.b = 209;

	screen[screennum].gadget_color.backgr_light.r = 143;
	screen[screennum].gadget_color.backgr_light.g = 147;
	screen[screennum].gadget_color.backgr_light.b = 151;

	screen[screennum].gadget_color.backgr_shadow.r = 85;
	screen[screennum].gadget_color.backgr_shadow.g = 87;
	screen[screennum].gadget_color.backgr_shadow.b = 90;

	screen[screennum].gadget_color.text_light.r = 0;
    screen[screennum].gadget_color.text_light.g = 0;
    screen[screennum].gadget_color.text_light.b = 0;

	screen[screennum].gadget_color.text_shadow.r = 255;
	screen[screennum].gadget_color.text_shadow.g = 255;
	screen[screennum].gadget_color.text_shadow.b = 255;
}


U1 *init_gadgets (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    /* allocate space for gadgets */

    S2 i;

	sp = stpopi ((U1 *) &gadgets, sp, sp_top);
	if (sp == NULL)
	{
		printf ("init_gadgets: ERROR stack corrupt!\n");
		return (NULL);
	}

    if (screen[screennum].gadget)
    {
        free_gadgets ();
    }

    screen[screennum].gadget = (struct gadget *) malloc (gadgets * sizeof (struct gadget));
    if (screen[screennum].gadget == NULL)
    {
        printf ("error: can't allocate gadget list!\n");
    }

    screen[screennum].gadgets = gadgets;

    for (i = 0; i < gadgets; i++)
    {
        screen[screennum].gadget[i].gptr = NULL;
        screen[screennum].gadget[i].type = 0;
    }

    screen[screennum].bmap = surf;
	screen[screennum].font_ttf.font = font;
	screen[screennum].gadget_string_string_len = 1024; // max number of chars in string

	printf ("init_gadgets: %lli gadgets allocated!\n", gadgets);

	set_default_colors ();

	return (sp);
}

void free_gadget (S2 screennum, S2 gadget_index)
{
    struct gadget_button *button;
    struct gadget_checkbox *checkbox;
    struct gadget_cycle *cycle;
    struct gadget_string *string;

    if (screen[screennum].gadget[gadget_index].gptr)
    {
        switch (screen[screennum].gadget[gadget_index].type)
        {
            case GADGET_BUTTON:
                button = (struct gadget_button *) screen[screennum].gadget[gadget_index].gptr;

                if (button->text)
                {
                    free (button->text);
                    button->text = NULL;
                    printf ("free_gadget: %i button->text freed.\n", gadget_index);
                }
                break;

            case GADGET_CHECKBOX:
                checkbox = (struct gadget_checkbox *) screen[screennum].gadget[gadget_index].gptr;

                if (checkbox->text)
                {
                    free (checkbox->text);
                    checkbox->text = NULL;
                    printf ("free_gadget: %i checkbox->text freed.\n", gadget_index);
                }
                break;

            case GADGET_CYCLE:
                cycle = (struct gadget_cycle *) screen[screennum].gadget[gadget_index].gptr;

                if (cycle->text)
                {
                    dealloc_array_U1 (cycle->text, cycle->menu_entries);
                    cycle->text = NULL;
                    printf ("free_gadget: %i cycle->text freed.\n", gadget_index);
                }
                break;

            case GADGET_STRING:
                string = (struct gadget_string *) screen[screennum].gadget[gadget_index].gptr;

                if (string->text)
                {
                    free (string->text);
                    string->text = NULL;
                    printf ("free_gadget: %i string->text freed.\n", gadget_index);
                }

                if (string->value)
                {
                    free (string->value);
                    string->value = NULL;
                    printf ("free_gadget: %i string->value freed.\n", gadget_index);
                }

                if (string->display)
                {
                    free (string->display);
                    string->display = NULL;
                    printf ("free_gadget: %i string->display freed.\n", gadget_index);
                }
                break;
        }

        free (screen[screennum].gadget[gadget_index].gptr);
        screen[screennum].gadget[gadget_index].gptr = NULL;
        screen[screennum].gadget[gadget_index].type = 0;
        printf ("free_gadget: %i gadget freed.\n", gadget_index);
   }
}

void free_gadgets (void)
{
    S2 i, j;

    for (i = 0; i < MAXSCREEN; i++)
    {
        if (screen[i].gadget)
        {
            for (j = 0; j < screen[i].gadgets; j++)
            {
                free_gadget (i, j);
            }
        }
    }
}


U1 set_ttf_style (S2 screennum)
{
	U1 style = TTF_STYLE_NORMAL;

	if (screen[screennum].font_ttf.font == NULL)
	{
		printf ("set_ttf_style: can't set style. No font opened!\n");
		return (FALSE);
	}

	if (! screen[screennum].font_ttf.style_normal)
	{
		if (screen[screennum].font_ttf.style_bold)
		{
			style = style | TTF_STYLE_BOLD;
		}

		if (screen[screennum].font_ttf.style_italic)
		{
			style = style | TTF_STYLE_ITALIC;
		}

		if (screen[screennum].font_ttf.style_underline)
		{
			style = style | TTF_STYLE_UNDERLINE;
		}
	}

	TTF_SetFontStyle (screen[screennum].font_ttf.font, style);

	return (TRUE);
}

U1 draw_text_ttf (SDL_Surface *surface, S2 screennum, U1 *textstr, Sint16 x, Sint16 y, Uint8 r, Uint8 g, Uint8 b)
{
	SDL_Surface *text;
	SDL_Rect dstrect;
	SDL_Color color;
	SDL_Texture *texture;

	color.r = r;
	color.g = g;
	color.b = b;

	int texture_width = 0;
	int texture_height = 0;

	if (screen[screennum].font_ttf.font == NULL)
	{
		printf ("draw_text_ttf: can't draw text. No font opened!\n");
		return (FALSE);
	}

	if (! set_ttf_style (screennum))
	{
		return (FALSE);
	}


	// text = TTF_RenderText_Blended (screen[screennum].font_ttf.font, (const char *) textstr, color);
	// text = TTF_RenderText_Solid (font, (const char *) textstr, color);
	text = TTF_RenderText_Blended (font, (const char*) textstr, color);
	if (text == NULL)
	{
		printf ("draw_text_ttf: can't render text! %s\n", SDL_GetError ());
		return (FALSE);
	}

	texture = SDL_CreateTextureFromSurface (renderer, text);

	SDL_QueryTexture (texture, NULL, NULL, &texture_width, &texture_height);

	dstrect.x = x;
	dstrect.y = y;
	dstrect.w = texture_width;
	dstrect.h = texture_height;

	SDL_RenderCopy (renderer, texture, NULL, &dstrect);
	SDL_RenderPresent (renderer);

	return (TRUE);
}

/* border colors */

void set_gadget_color_border_light (S2 screennum)
{
    screen[screennum].gadget_color.border_light.r = screen[screennum].r;
    screen[screennum].gadget_color.border_light.g = screen[screennum].g;
    screen[screennum].gadget_color.border_light.b = screen[screennum].b;
}

void set_gadget_color_border_shadow (S2 screennum)
{
    screen[screennum].gadget_color.border_shadow.r = screen[screennum].r;
    screen[screennum].gadget_color.border_shadow.g = screen[screennum].g;
    screen[screennum].gadget_color.border_shadow.b = screen[screennum].b;
}

/* background colors */

void set_gadget_color_backgr_light (S2 screennum)
{
    screen[screennum].gadget_color.backgr_light.r = screen[screennum].r;
    screen[screennum].gadget_color.backgr_light.g = screen[screennum].g;
    screen[screennum].gadget_color.backgr_light.b = screen[screennum].b;
}

void set_gadget_color_backgr_shadow (S2 screennum)
{
    screen[screennum].gadget_color.backgr_shadow.r = screen[screennum].r;
    screen[screennum].gadget_color.backgr_shadow.g = screen[screennum].g;
    screen[screennum].gadget_color.backgr_shadow.b = screen[screennum].b;
}

/* text colors */

void set_gadget_color_text_light (S2 screennum)
{
    screen[screennum].gadget_color.text_light.r = screen[screennum].r;
    screen[screennum].gadget_color.text_light.g = screen[screennum].g;
    screen[screennum].gadget_color.text_light.b = screen[screennum].b;
}

void set_gadget_color_text_shadow (S2 screennum)
{
    screen[screennum].gadget_color.text_shadow.r = screen[screennum].r;
    screen[screennum].gadget_color.text_shadow.g = screen[screennum].g;
    screen[screennum].gadget_color.text_shadow.b = screen[screennum].b;
}


void draw_gadget_light (SDL_Renderer *renderer, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2)
{
    lineRGBA (renderer, x, y, x2, y, screen[screennum].gadget_color.border_light.r, screen[screennum].gadget_color.border_light.g, screen[screennum].gadget_color.border_light.b, 255);

    lineRGBA (renderer, x2, y, x2, y2, screen[screennum].gadget_color.border_light.r, screen[screennum].gadget_color.border_light.g, screen[screennum].gadget_color.border_light.b, 255);

    lineRGBA (renderer, x, y + 1, x, y2, screen[screennum].gadget_color.border_shadow.r, screen[screennum].gadget_color.border_shadow.g, screen[screennum].gadget_color.border_shadow.b, 255);

    lineRGBA (renderer, x, y2, x2 - 1, y2, screen[screennum].gadget_color.border_shadow.r, screen[screennum].gadget_color.border_shadow.g, screen[screennum].gadget_color.border_shadow.b, 255);

    boxRGBA (renderer, x + 1, y + 1, x2 - 1, y2 - 1, screen[screennum].gadget_color.backgr_light.r, screen[screennum].gadget_color.backgr_light.g, screen[screennum].gadget_color.backgr_light.b, 255);
}

void draw_gadget_shadow (SDL_Renderer *renderer, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2)
{
    lineRGBA (renderer, x, y, x2, y, screen[screennum].gadget_color.border_shadow.r, screen[screennum].gadget_color.border_shadow.g, screen[screennum].gadget_color.border_shadow.b, 255);

    lineRGBA (renderer, x2, y, x2, y2, screen[screennum].gadget_color.border_shadow.r, screen[screennum].gadget_color.border_shadow.g, screen[screennum].gadget_color.border_shadow.b, 255);

    lineRGBA (renderer, x, y + 1, x, y2, screen[screennum].gadget_color.border_light.r, screen[screennum].gadget_color.border_light.g, screen[screennum].gadget_color.border_light.b, 255);

    lineRGBA (renderer, x, y2, x2 - 1, y2, screen[screennum].gadget_color.border_light.r, screen[screennum].gadget_color.border_light.g, screen[screennum].gadget_color.border_light.b, 255);

    boxRGBA (renderer, x + 1, y + 1, x2 - 1, y2 - 1, screen[screennum].gadget_color.backgr_shadow.r, screen[screennum].gadget_color.backgr_shadow.g, screen[screennum].gadget_color.backgr_shadow.b, 255);
}

void draw_gadget_input (SDL_Renderer *renderer, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2)
{
    /* outer line */

    lineRGBA (renderer, x, y, x2, y, screen[screennum].gadget_color.border_light.r, screen[screennum].gadget_color.border_light.g, screen[screennum].gadget_color.border_light.b, 255);

    lineRGBA (renderer, x2, y, x2, y2, screen[screennum].gadget_color.border_light.r, screen[screennum].gadget_color.border_light.g, screen[screennum].gadget_color.border_light.b, 255);

    lineRGBA (renderer, x, y + 1, x, y2, screen[screennum].gadget_color.border_shadow.r, screen[screennum].gadget_color.border_shadow.g, screen[screennum].gadget_color.border_shadow.b, 255);

    lineRGBA (renderer, x, y2, x2 - 1, y2, screen[screennum].gadget_color.border_shadow.r, screen[screennum].gadget_color.border_shadow.g, screen[screennum].gadget_color.border_shadow.b, 255);

    /* inner line */

    x++;
    y++;
    x2--;
    y2--;

    lineRGBA (renderer, x, y, x2, y, screen[screennum].gadget_color.border_shadow.r, screen[screennum].gadget_color.border_shadow.g, screen[screennum].gadget_color.border_shadow.b, 255);

    lineRGBA (renderer, x2, y, x2, y2, screen[screennum].gadget_color.border_shadow.r, screen[screennum].gadget_color.border_shadow.g, screen[screennum].gadget_color.border_shadow.b, 255);

    lineRGBA (renderer, x, y + 1, x, y2, screen[screennum].gadget_color.border_light.r, screen[screennum].gadget_color.border_light.g, screen[screennum].gadget_color.border_light.b, 255);

    lineRGBA (renderer, x, y2, x2 - 1, y2, screen[screennum].gadget_color.border_light.r, screen[screennum].gadget_color.border_light.g, screen[screennum].gadget_color.border_light.b, 255);

    /* clear inside */

    x++;
    y++;
    x2--;
    y2--;

    boxRGBA (renderer, x, y, x2, y2, screen[screennum].gadget_color.backgr_light.r, screen[screennum].gadget_color.backgr_light.g, screen[screennum].gadget_color.backgr_light.b, 255);
}

void draw_ghost_gadget (SDL_Renderer *renderer, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2)
{
    Sint16 dx, dy;

    for (dy = y + 2; dy <= y2 - 2; dy += 2)
    {
        for (dx = x + 2; dx <= x2 - 2; dx += 2)
        {
            pixelRGBA (renderer, dx, dy, screen[screennum].gadget_color.backgr_shadow.r, screen[screennum].gadget_color.backgr_shadow.g, screen[screennum].gadget_color.backgr_shadow.b, 255);
        }
    }
}

U1 draw_gadget_button (S2 screennum, U2 gadget_index, U1 selected)
{
    struct gadget_button *button;

    button = (struct gadget_button *) screen[screennum].gadget[gadget_index].gptr;

    if (selected == GADGET_NOT_SELECTED)
    {
        draw_gadget_light (renderer, button->x, button->y, button->x2, button->y2);

        if (! draw_text_ttf (surf, screennum, button->text, button->text_x, button->text_y, screen[screennum].gadget_color.text_light.r, screen[screennum].gadget_color.text_light.g, screen[screennum].gadget_color.text_light.b))
        {
            return (FALSE);
        }
    }
    else
    {
        draw_gadget_shadow (renderer, button->x, button->y, button->x2, button->y2);

		if (! draw_text_ttf (surf, screennum, button->text, button->text_x, button->text_y, screen[screennum].gadget_color.text_shadow.r, screen[screennum].gadget_color.text_shadow.g, screen[screennum].gadget_color.text_shadow.b))
        {
            return (FALSE);
        }
    }

    if (button->status == GADGET_NOT_ACTIVE)
    {
        draw_ghost_gadget (renderer, button->x, button->y, button->x2, button->y2);
    }

    // SDL_UpdateRect (renderer, button->x, button->y, button->x2 - button->x + 1, button->y2 - button->y + 1);

    SDL_RenderPresent (renderer);
	SDL_UpdateWindowSurface (window);
    return (TRUE);
}
void draw_checkmark_light (SDL_Renderer *renderer, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2)
{
    Sint16 dx, dy, dx2, dy2, dx3, dy3, dx4, dy4, height, width;

    height = y2 - y;
    width = x2 - x;

    dx = x + width / 4;
    dy = y + height / 2;

    dx2 = x + width / 2;
    dy2 = y2 - height / 4;

    dx3 = dx2 + width / 10;
    dy3 = dy2 - height / 15;

    dx4 = x2 - width / 4;
    dy4 = y + height / 4;

    filledTrigonRGBA (renderer, dx, dy, dx2, dy2, dx3, dy3, screen[screennum].gadget_color.text_light.r, screen[screennum].gadget_color.text_light.g, screen[screennum].gadget_color.text_light.b, 255);

    filledTrigonRGBA (renderer, dx2, dy2, dx3, dy3, dx4, dy4, screen[screennum].gadget_color.text_light.r, screen[screennum].gadget_color.text_light.g, screen[screennum].gadget_color.text_light.b, 255);
}

void draw_checkmark_shadow (SDL_Renderer *renderer, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2)
{
    Sint16 dx, dy, dx2, dy2, dx3, dy3, dx4, dy4, height, width;

    height = y2 - y;
    width = x2 - x;

    dx = x + width / 4;
    dy = y + height / 2;

    dx2 = x + width / 2;
    dy2 = y2 - height / 4;

    dx3 = dx2 + width / 10;
    dy3 = dy2 - height / 15;

    dx4 = x2 - width / 4;
    dy4 = y + height / 4;

    filledTrigonRGBA (renderer, dx, dy, dx2, dy2, dx3, dy3, screen[screennum].gadget_color.text_shadow.r, screen[screennum].gadget_color.text_shadow.g, screen[screennum].gadget_color.text_shadow.b, 255);

    filledTrigonRGBA (renderer, dx2, dy2, dx3, dy3, dx4, dy4, screen[screennum].gadget_color.text_shadow.r, screen[screennum].gadget_color.text_shadow.g, screen[screennum].gadget_color.text_shadow.b, 255);
}

U1 draw_gadget_checkbox (S2 screennum, U2 gadget_index, U1 selected, U1 value)
{
    struct gadget_checkbox *checkbox;

    checkbox = (struct gadget_checkbox *) screen[screennum].gadget[gadget_index].gptr;

    if (selected == GADGET_NOT_SELECTED)
    {
		draw_gadget_light (renderer, checkbox->x, checkbox->y, checkbox->x2, checkbox->y2);

        if (value == GADGET_CHECKBOX_TRUE)
        {
            draw_checkmark_light (renderer, checkbox->x, checkbox->y, checkbox->x2, checkbox->y2);
        }
    }
    else
    {
        draw_gadget_shadow (renderer, checkbox->x, checkbox->y, checkbox->x2, checkbox->y2);

        if (value == GADGET_CHECKBOX_TRUE)
        {
            draw_checkmark_shadow (renderer, checkbox->x, checkbox->y, checkbox->x2, checkbox->y2);
        }
    }

    if (checkbox->status == GADGET_NOT_ACTIVE)
    {
        draw_ghost_gadget (renderer, checkbox->x, checkbox->y, checkbox->x2, checkbox->y2);
    }

    boxRGBA (renderer, checkbox->text_x, checkbox->text_y, checkbox->text_x2, checkbox->text_y2, screen[screennum].gadget_color.backgr_light.r, screen[screennum].gadget_color.backgr_light.g, screen[screennum].gadget_color.backgr_light.b, 255);

    if (! draw_text_ttf (surf, screennum, checkbox->text, checkbox->text_x, checkbox->text_y, screen[screennum].gadget_color.text_light.r, screen[screennum].gadget_color.text_light.g, screen[screennum].gadget_color.text_light.b))
    {
        return (FALSE);
    }

    // SDL_UpdateRect (screen[screennum].bmap, checkbox->x, checkbox->y, checkbox->x2 - checkbox->x + 1, checkbox->y2 - checkbox->y + 1);
    // SDL_UpdateRect (renderer, checkbox->text_x, checkbox->text_y, checkbox->text_x2 - checkbox->text_x + 1, checkbox->text_y2 - checkbox->text_y + 1);

    SDL_RenderPresent (renderer);
	SDL_UpdateWindowSurface (window);
    return (TRUE);
}

void draw_cycle_arrow_light (S2 screennum, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2)
{
    Sint16 dx, dy, dx2, dy2, dx3, dy3, dx4, dy4, dx5, dy5, dx6, dy6, dx7, dy7, dx8, dy8, dx9, dy9, height, width;

    height = y2 - y;
    width = x2 - x;

    /* arrow */

    dx = x + width / 4;
    dy = y2 - height / 4;

    dx2 = dx - width / 8;
    dy2 = dy - height / 8;

    dx3 = dx + width / 8;
    dy3 = dy2;

    /* box left */

    dx4 = dx - width / 24;
    dy4 = y + height / 4;
    dx5 = dx + width / 24;
    dy5 = dy2;

    /* box top */

    dx6 = dx5;
    dy6 = dy4;
    dx7 = x2 - width / 4;
    dy7 = dy4 + height / 24;

    /* box right */

    dx8 = dx7 - width / 24;
    dy8 = dy7;
    dx9 = dx7;
    dy9 = dy2;

    filledTrigonRGBA (renderer, dx, dy, dx2, dy2, dx3, dy3, screen[screennum].gadget_color.text_light.r, screen[screennum].gadget_color.text_light.g, screen[screennum].gadget_color.text_light.b, 255);

    boxRGBA (renderer, dx4, dy4, dx5, dy5, screen[screennum].gadget_color.text_light.r, screen[screennum].gadget_color.text_light.g, screen[screennum].gadget_color.text_light.b, 255);

    boxRGBA (renderer, dx6, dy6, dx7, dy7, screen[screennum].gadget_color.text_light.r, screen[screennum].gadget_color.text_light.g, screen[screennum].gadget_color.text_light.b, 255);

    boxRGBA (renderer, dx8, dy8, dx9, dy9, screen[screennum].gadget_color.text_light.r, screen[screennum].gadget_color.text_light.g, screen[screennum].gadget_color.text_light.b, 255);
}

void draw_cycle_arrow_shadow (S2 screennum, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2)
{
    Sint16 dx, dy, dx2, dy2, dx3, dy3, dx4, dy4, dx5, dy5, dx6, dy6, dx7, dy7, dx8, dy8, dx9, dy9, height, width;

    height = y2 - y;
    width = x2 - x;

    /* arrow */

    dx = x + width / 4;
    dy = y2 - height / 4;

    dx2 = dx - width / 8;
    dy2 = dy - height / 8;

    dx3 = dx + width / 8;
    dy3 = dy2;

    /* box left */

    dx4 = dx - width / 24;
    dy4 = y + height / 4;
    dx5 = dx + width / 24;
    dy5 = dy2;

    /* box top */

    dx6 = dx5;
    dy6 = dy4;
    dx7 = x2 - width / 4;
    dy7 = dy4 + height / 24;

    /* box right */

    dx8 = dx7 - width / 24;
    dy8 = dy7;
    dx9 = dx7;
    dy9 = dy2;

    filledTrigonRGBA (renderer, dx, dy, dx2, dy2, dx3, dy3, screen[screennum].gadget_color.text_shadow.r, screen[screennum].gadget_color.text_shadow.g, screen[screennum].gadget_color.text_shadow.b, 255);

    boxRGBA (renderer, dx4, dy4, dx5, dy5, screen[screennum].gadget_color.text_shadow.r, screen[screennum].gadget_color.text_shadow.g, screen[screennum].gadget_color.text_shadow.b, 255);

    boxRGBA (renderer, dx6, dy6, dx7, dy7, screen[screennum].gadget_color.text_shadow.r, screen[screennum].gadget_color.text_shadow.g, screen[screennum].gadget_color.text_shadow.b, 255);

    boxRGBA (renderer, dx8, dy8, dx9, dy9, screen[screennum].gadget_color.text_shadow.r, screen[screennum].gadget_color.text_shadow.g, screen[screennum].gadget_color.text_shadow.b, 255);
}

int do_copy_surface (SDL_Renderer *dest_renderer, SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect)
{
	Uint32 pixel;
	Uint8 r, g, b;
	Sint16 src_x, src_y, dst_x, dst_y;
	Sint16 srcrect_x, srcrect_y, srcrect_w, srcrect_h;
	Sint16 dstrect_x, dstrect_y;

	if (srcrect == NULL)
	{
		srcrect_x = 0;
		srcrect_y = 0;
		srcrect_w = src->w;
		srcrect_h = src->h;
	}
	else
	{
		srcrect_x = srcrect->x;
		srcrect_y = srcrect->y;
		srcrect_w = srcrect->w;
		srcrect_h = srcrect->h;
	}

	if (dstrect == NULL)
	{
		dstrect_x = 0;
		dstrect_y = 0;
	}
	else
	{
		dstrect_x = dstrect->x;
		dstrect_y = dstrect->y;
	}

	dst_y = dstrect_y;

	for (src_y = srcrect_y; src_y < srcrect_y + srcrect_h; src_y++)
	{
		dst_x = dstrect_x;

		for (src_x = srcrect_x; src_x < srcrect_x + srcrect_w; src_x++)
		{
			if (SDL_LockSurface (src) < 0)
			{
				printf ("copy_surface: can't lock source surface!\n");
				return (-1);
			}

			pixel = getpixel (src, src_x, src_y);
			SDL_UnlockSurface (src);
			SDL_GetRGB (pixel, src->format, &r, &g, &b);

			pixelRGBA (dest_renderer, dst_x, dst_y, r, g, b, 255);

			dst_x++;
		}

		dst_y++;
	}
	return (0);
}

U1 draw_gadget_cycle (S2 screennum, U2 gadget_index, U1 selected, S4 value)
{
    struct gadget_cycle *cycle;
    Uint32 rmask, gmask, bmask, amask;
    SDL_Rect menu_rect, copy_rect;
    S4 i;
    Sint16 text_x, text_y;

    /* SDL interprets each pixel as a 32-bit number, so our masks must depend */
    /* on the endianness (byte order) of the machine */

    #if SDL_BYTEORDER == SDL_BIG_ENDIAN
        rmask = 0xff000000;
        gmask = 0x00ff0000;
        bmask = 0x0000ff00;
        amask = 0x000000ff;
    #else
        rmask = 0x000000ff;
        gmask = 0x0000ff00;
        bmask = 0x00ff0000;
        amask = 0xff000000;
    #endif

    cycle = (struct gadget_cycle *) screen[screennum].gadget[gadget_index].gptr;

    switch (selected)
    {
        case GADGET_NOT_SELECTED:
            draw_gadget_light (renderer, cycle->x, cycle->y, cycle->x2, cycle->y2);

            if (! draw_text_ttf (surf, screennum, cycle->text[value], cycle->text_x, cycle->text_y, screen[screennum].gadget_color.text_light.r, screen[screennum].gadget_color.text_light.g, screen[screennum].gadget_color.text_light.b))
            {
                return (FALSE);
            }

            draw_cycle_arrow_light (screennum, cycle->arrow_x, cycle->arrow_y, cycle->x2, cycle->y2);
            break;

        case GADGET_SELECTED:
            draw_gadget_shadow (renderer, cycle->x, cycle->y, cycle->x2, cycle->y2);

            if (! draw_text_ttf (surf, screennum, cycle->text[value], cycle->text_x, cycle->text_y, screen[screennum].gadget_color.text_shadow.r, screen[screennum].gadget_color.text_shadow.g, screen[screennum].gadget_color.text_shadow.b))
            {
                return (FALSE);
            }

            draw_cycle_arrow_shadow (screennum, cycle->arrow_x, cycle->arrow_y, cycle->x2, cycle->y2);
            break;

        case GADGET_MENU_DOWN:
            if (! cycle->menu)
            {
				/* allocate backup surface, to copy the area that will be covered by the menu */

				copy_surface = SDL_CreateRGBSurface (SDL_SWSURFACE, cycle->menu_x2 - cycle->menu_x + 1, cycle->menu_y2 - cycle->menu_y + 1, video_bpp, rmask, gmask, bmask, amask);
				if (copy_surface == NULL)
				{
					printf ("draw_gadget_cycle: error can't allocate copy %i surface!\n", gadget_index);
					return (FALSE);
				}

                /* Use alpha blending */
				if (SDL_SetSurfaceBlendMode (copy_surface, SDL_BLENDMODE_BLEND) < 0)
				{
					printf ("draw_gadget_cycle: error can't set copy alpha channel %i surface!\n", gadget_index);
					return (FALSE);
				}

				copy_renderer = SDL_CreateSoftwareRenderer (copy_surface);

                /* make copy of menu area */

                menu_rect.x = cycle->menu_x;
                menu_rect.y = cycle->menu_y;
                menu_rect.w = cycle->menu_x2 - cycle->menu_x + 1;
                menu_rect.h = cycle->menu_y2 - cycle->menu_y + 1;

                copy_rect.x = 0;
                copy_rect.y = 0;


				// DEBUG
				printf ("draw_gadget_cycle: menu_rect.x: %i, menu_rect.y: %i\n", menu_rect.x, menu_rect.y);

				// int copy_surface (SDL_Renderer *dest_renderer, SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect)

				if (do_copy_surface (copy_renderer, surf, &menu_rect, copy_surface, &copy_rect) < 0)
				{
					printf ("draw_gadget_cycle: error can't blit copy %i surface!\n", gadget_index);
					return (FALSE);
				}

				/* allocate menu surface */

				temp_surface = SDL_CreateRGBSurface (SDL_SWSURFACE, cycle->menu_x2 - cycle->menu_x + 1, cycle->menu_y2 - cycle->menu_y + 1, video_bpp, rmask, gmask, bmask, amask);
				if (temp_surface == NULL)
				{
					printf ("draw_gadget_cycle: error can't allocate menu %i surface!\n", gadget_index);
					return (FALSE);
				}

				/* Use alpha blending */
				if (SDL_SetSurfaceBlendMode (temp_surface, SDL_BLENDMODE_BLEND) < 0)
				{
					printf ("draw_gadget_cycle: error can't set menu alpha channel %i surface!\n", gadget_index);
					return (FALSE);
				}

				/* set renderer */
				temp_renderer = SDL_CreateSoftwareRenderer (temp_surface);
				SDL_UpdateWindowSurface (window);
				cycle->menu = TRUE;
			}


            /* draw menu */

            draw_gadget_light (temp_renderer, 0, 0, cycle->menu_x2 - cycle->menu_x, cycle->menu_y2 - cycle->menu_y);

            text_x = cycle->text_x - cycle->menu_x;
            text_y = cycle->text_y - cycle->y;

			// Neu

			// DEBUG
			printf ("draw_gadget_cycle: text_x: %i, text_y: %i\n", text_x, text_y);

            for (i = 0; i < cycle->menu_entries; i++)
            {
                if (i == value)
                {
                    boxRGBA (temp_renderer, 1, text_y, cycle->menu_x2 - cycle->menu_x - 1, text_y + cycle->text_height, screen[screennum].gadget_color.backgr_shadow.r, screen[screennum].gadget_color.backgr_shadow.g, screen[screennum].gadget_color.backgr_shadow.b, 255);

					if (! draw_text_ttf (temp_surface, screennum, cycle->text[i], cycle->menu_x + text_x, cycle->menu_y + text_y, screen[screennum].gadget_color.text_shadow.r, screen[screennum].gadget_color.text_shadow.g, screen[screennum].gadget_color.text_shadow.b))
                    {
                        return (FALSE);
                    }
                    // SDL_UpdateWindowSurface (window);
                }
                else
                {
					if (! draw_text_ttf (temp_surface, screennum, cycle->text[i], cycle->menu_x + text_x, cycle->menu_y + text_y, screen[screennum].gadget_color.text_light.r, screen[screennum].gadget_color.text_light.g, screen[screennum].gadget_color.text_light.b))
                    {
                        return (FALSE);
                    }
                    //SDL_UpdateWindowSurface (window);
                }

                text_y += cycle->text_height + (cycle->text_height / 2);
            }

            sdl_do_delay (200);
            // SDL_UpdateWindowSurface (window);

            menu_rect.x = cycle->menu_x;
            menu_rect.y = cycle->menu_y;

			if (do_copy_surface (renderer, temp_surface, NULL, surf, &menu_rect) < 0)
			{
				printf ("draw_gadget_cycle: error can't blit menu %i surface!\n", gadget_index);
				return (FALSE);
			}

			update_rect (cycle->menu_x, cycle->menu_y, cycle->menu_x2 - cycle->menu_x + 1, cycle->menu_y2 - cycle->menu_y + 1);
			SDL_UpdateWindowSurface (window);

			// DEBUG
			/*
			while (1)
			{
				usleep (10000);
			} */
            break;

        case GADGET_MENU_UP:
            /* close menu: copy backup surface */


			menu_rect.x = cycle->menu_x;
			menu_rect.y = cycle->menu_y;

			if (do_copy_surface (renderer, copy_surface, NULL, surf, &menu_rect) < 0)
			{
				printf ("draw_gadget_cycle: error can't blit copy %i surface!\n", gadget_index);
				return (FALSE);
			}

			update_rect (cycle->menu_x, cycle->menu_y, cycle->menu_x2 - cycle->menu_x + 1, cycle->menu_y2 - cycle->menu_y + 1);
			SDL_UpdateWindowSurface (window);

			/* redraw cycle gadget */

			draw_gadget_light (renderer, cycle->x, cycle->y, cycle->x2, cycle->y2);

			if (! draw_text_ttf (surf, screennum, cycle->text[value], cycle->text_x, cycle->text_y, screen[screennum].gadget_color.text_light.r, screen[screennum].gadget_color.text_light.g, screen[screennum].gadget_color.text_light.b))
			{
				return (FALSE);
			}

			draw_cycle_arrow_light (screennum, cycle->arrow_x, cycle->arrow_y, cycle->x2, cycle->y2);

			/* free bitmaps */

			if (copy_surface)
			{
				SDL_FreeSurface (copy_surface);
				copy_surface = NULL;
			}
			if (temp_surface)
			{
				SDL_FreeSurface (temp_surface);
				temp_surface = NULL;
			}

			cycle->menu = FALSE;

			SDL_UpdateWindowSurface (window);
			break;

    }

    if (cycle->status == GADGET_NOT_ACTIVE)
    {
        draw_ghost_gadget (renderer, cycle->x, cycle->y, cycle->x2, cycle->y2);
    }

    update_rect (cycle->x, cycle->y, cycle->x2 - cycle->x + 1, cycle->y2 - cycle->y + 1);
	SDL_UpdateWindowSurface (window);
    return (TRUE);
}

U1 draw_gadget_progress_bar (S2 screennum, U2 gadget_index, U1 selected)
{
    struct gadget_progress_bar *progress;
	Sint16 bar_x, bar_len;

    progress = (struct gadget_progress_bar *) screen[screennum].gadget[gadget_index].gptr;

	bar_len = (progress->x2 - progress->x) - 2;
	bar_x = progress->x + 1 + ((bar_len * progress->value) / 100);

    if (selected == GADGET_NOT_SELECTED)
    {
        draw_gadget_light (renderer, progress->x, progress->y, progress->x2, progress->y2);

		boxRGBA (renderer, progress->x + 1, progress->y + 1, bar_x, progress->y2 - 1, 18, 98, 245, 255);

        if (! draw_text_ttf (surf, screennum, progress->text, progress->text_x, progress->text_y, screen[screennum].gadget_color.text_light.r, screen[screennum].gadget_color.text_light.g, screen[screennum].gadget_color.text_light.b))
        {
            return (FALSE);
        }
    }
    else
    {
        draw_gadget_shadow (renderer, progress->x, progress->y, progress->x2, progress->y2);

		boxRGBA (renderer, progress->x + 1, progress->y + 1, bar_x, progress->y2 - 1, 18, 98, 245, 255);

        if (! draw_text_ttf (surf, screennum, progress->text, progress->text_x, progress->text_y, screen[screennum].gadget_color.text_shadow.r, screen[screennum].gadget_color.text_shadow.g, screen[screennum].gadget_color.text_shadow.b))
        {
            return (FALSE);
        }
    }

    if (progress->status == GADGET_NOT_ACTIVE)
    {
        draw_ghost_gadget (renderer, progress->x, progress->y, progress->x2, progress->y2);
    }

    // SDL_UpdateRect (renderer, progress->x, progress->y, progress->x2 - progress->x + 1, progress->y2 - progress->y + 1);

    SDL_RenderPresent (renderer);
	SDL_UpdateWindowSurface (window);
    return (TRUE);
}

U1 draw_gadget_slider (S2 screennum, U2 gadget_index, U1 selected)
{
    struct gadget_slider *slider;

    S8 range ALIGN;
    S8 value_gadget ALIGN;
    S8 pixel_ratio ALIGN;
    Sint16 range_pixel;
    S8 value_percent ALIGN;
    Sint16 slider_x1;
    Sint16 slider_x2;

    slider = (struct gadget_slider *) screen[screennum].gadget[gadget_index].gptr;

    // calculate position and size of slider
    // avoid drawing over right gadget border
    if (slider->value >= slider->max)
    {
        slider->value = slider->max - 1;
    }

    range = slider->max - slider->min;
    range_pixel = slider->x2 - slider->x;
    value_gadget = slider->value - slider->min;

    value_percent = 100 * value_gadget / range;

    pixel_ratio = range_pixel / range;      // pixels per each value
    slider_x1 = slider->x + ((range_pixel * value_percent) / 100);

    printf ("draw_gagdet_slider: slider->x: %i\n", slider->x);
    printf ("draw_gagdet_slider: slider_x1: %i\n", slider_x1);
    printf ("draw_gagdet_slider: range_pixel: %i\n", range_pixel);
    printf ("draw_gagdet_slider: slider->value: %lli\n", slider->value);
    printf ("draw_gagdet_slider: slider->min: %lli\n", slider->min);
    printf ("draw_gagdet_slider: slider->max: %lli\n", slider->max);

    // calculate slider width
    slider_x2 = slider_x1 + pixel_ratio;

    if (selected == GADGET_NOT_SELECTED)
    {
        draw_gadget_light (renderer, slider->x, slider->y, slider->x2, slider->y2);

        boxRGBA (renderer, slider_x1, slider->y + 1, slider_x2, slider->y2 - 1, 18, 98, 245, 255);

        if (! draw_text_ttf (surf, screennum, slider->text, slider->text_x, slider->text_y, screen[screennum].gadget_color.text_light.r, screen[screennum].gadget_color.text_light.g, screen[screennum].gadget_color.text_light.b))
        {
            return (FALSE);
        }
    }
    else
    {
        draw_gadget_shadow (renderer, slider->x, slider->y, slider->x2, slider->y2);

        boxRGBA (renderer, slider_x1, slider->y + 1, slider_x2, slider->y2 - 1, 18, 98, 245, 255);

        if (! draw_text_ttf (surf, screennum, slider->text, slider->text_x, slider->text_y, screen[screennum].gadget_color.text_shadow.r, screen[screennum].gadget_color.text_shadow.g, screen[screennum].gadget_color.text_shadow.b))
        {
            return (FALSE);
        }
    }

    if (slider->status == GADGET_NOT_ACTIVE)
    {
        draw_ghost_gadget (renderer, slider->x, slider->y, slider->x2, slider->y2);
    }

    SDL_RenderPresent (renderer);
    SDL_UpdateWindowSurface (window);
    return (TRUE);
}

U1 draw_gadget_slider_vert (S2 screennum, U2 gadget_index, U1 selected)
{
    struct gadget_slider *slider;

    S8 range ALIGN;
    S8 value_gadget ALIGN;
    S8 pixel_ratio ALIGN;
    Sint16 range_pixel;
    S8 value_percent ALIGN;
    Sint16 slider_y1;
    Sint16 slider_y2;

    slider = (struct gadget_slider *) screen[screennum].gadget[gadget_index].gptr;

    // calculate position and size of slider
    // avoid drawing over right gadget border
    if (slider->value >= slider->max)
    {
        slider->value = slider->max - 1;
    }

    range = slider->max - slider->min;
    range_pixel = slider->y2 - slider->y;
    value_gadget = slider->value - slider->min;

    value_percent = 100 * value_gadget / range;

    pixel_ratio = range_pixel / range;      // pixels per each value
    slider_y1 = slider->y + ((range_pixel * value_percent) / 100);

    printf ("draw_gagdet_slider_vert: slider->x: %i\n", slider->x);
    printf ("draw_gagdet_slide_vert: slider_y1: %i\n", slider_y1);
    printf ("draw_gagdet_slider_vert: range_pixel: %i\n", range_pixel);
    printf ("draw_gagdet_slider_vert: slider->value: %lli\n", slider->value);
    printf ("draw_gagdet_slider_vert: slider->min: %lli\n", slider->min);
    printf ("draw_gagdet_slider_vert: slider->max: %lli\n", slider->max);

    // calculate slider width
    slider_y2 = slider_y1 + pixel_ratio;

    if (selected == GADGET_NOT_SELECTED)
    {
        draw_gadget_light (renderer, slider->x, slider->y, slider->x2, slider->y2);

        boxRGBA (renderer, slider->x + 1, slider_y1, slider->x2 - 1, slider_y2 - 1, 18, 98, 245, 255);
    }
    else
    {
        draw_gadget_shadow (renderer, slider->x, slider->y, slider->x2, slider->y2);

        boxRGBA (renderer, slider->x + 1, slider_y1, slider->x2 - 1, slider_y2 - 1, 18, 98, 245, 255);
    }

    if (slider->status == GADGET_NOT_ACTIVE)
    {
        draw_ghost_gadget (renderer, slider->x, slider->y, slider->x2, slider->y2);
    }

    SDL_RenderPresent (renderer);
    SDL_UpdateWindowSurface (window);
    return (TRUE);
}

U1 draw_gadget_string (S2 screennum, U2 gadget_index, U1 selected)
{
    S2 value_len;
    Sint16 cursor_x;
	U1 stars[GADGET_STRING_PASSWD_STARS];
	S2 i;

    struct gadget_string *string;

    string = (struct gadget_string *) screen[screennum].gadget[gadget_index].gptr;

    draw_gadget_input (renderer, string->x, string->y, string->x2, string->y2);

    boxRGBA (renderer, string->text_x, string->text_y, string->text_x2, string->text_y2, screen[screennum].gadget_color.backgr_light.r, screen[screennum].gadget_color.backgr_light.g, screen[screennum].gadget_color.backgr_light.b, 255);

    if (! draw_text_ttf (surf, screennum, string->text, string->text_x, string->text_y, screen[screennum].gadget_color.text_light.r, screen[screennum].gadget_color.text_light.g, screen[screennum].gadget_color.text_light.b))
    {
        return (FALSE);
    }

    /* draw value string (input) */

    value_len = strlen_safe ((const char *) string->value, MAXLINELEN);
    if (value_len > 0)
    {
        if (value_len <= string->visible_len)
        {
            /* value string fits in gadget */

			if (string->passwd == 0)
			{
	            if (! draw_text_ttf (surf, screennum, string->value, string->input_x, string->input_y, screen[screennum].gadget_color.text_light.r, screen[screennum].gadget_color.text_light.g, screen[screennum].gadget_color.text_light.b))
            	{
                	return (FALSE);
            	}
			}
			else
			{
				// show value_len chars as password input in stars as chars
				for (i = 0; i < value_len; i++)
				{
					stars[i] = '*';
				}
				stars[i] = '\0';

				if (! draw_text_ttf (surf, screennum, stars, string->input_x, string->input_y, screen[screennum].gadget_color.text_light.r, screen[screennum].gadget_color.text_light.g, screen[screennum].gadget_color.text_light.b))
            	{
                	return (FALSE);
            	}
			}
        }
    }

    if (selected == GADGET_SELECTED)
    {
        if (string->cursor_pos == -1)
        {
            /* init cursor position */

            if (value_len <= string->visible_len)
            {
                string->cursor_pos = value_len;
            }
            else
            {
                string->cursor_pos = string->visible_len;
            }

            string->insert_pos = string->cursor_pos;
        }

        /* draw cursor */

        cursor_x = string->input_x + (string->cursor_width * string->cursor_pos);

        boxRGBA (renderer, cursor_x, string->input_y, cursor_x + string->cursor_width, string->input_y + string->cursor_height, screen[screennum].gadget_color.text_light.r, screen[screennum].gadget_color.text_light.g, screen[screennum].gadget_color.text_light.b, 50);
    }

    if (string->status == GADGET_NOT_ACTIVE)
    {
        draw_ghost_gadget (renderer, string->x, string->y, string->x2, string->y2);
    }

    // SDL_UpdateRect (renderer, string->x, string->y, string->x2 - string->x + 1, string->y2 - string->y + 1);
    // SDL_UpdateRect (renderer, string->text_x, string->text_y, string->text_x2 - string->text_x + 1, string->text_y2 - string->text_y + 1);

    SDL_RenderPresent (renderer);
    SDL_UpdateWindowSurface (window);
    return (TRUE);
}

U1 draw_gadget_string_multiline (S2 screennum, U2 gadget_index, U1 selected)
{
    Sint16 cursor_x, cursor_y;
    struct gadget_string_multiline *string;

    string = (struct gadget_string_multiline *) screen[screennum].gadget[gadget_index].gptr;

    draw_gadget_input (renderer, string->x, string->y, string->x2, string->y2);

    boxRGBA (renderer, string->text_x, string->text_y, string->text_x2, string->text_y2, screen[screennum].gadget_color.backgr_light.r, screen[screennum].gadget_color.backgr_light.g, screen[screennum].gadget_color.backgr_light.b, 255);

	if (! draw_text_ttf (surf, screennum, string->text, string->text_x, string->text_y, screen[screennum].gadget_color.text_light.r, screen[screennum].gadget_color.text_light.g, screen[screennum].gadget_color.text_light.b))
    {
        return (FALSE);
    }

	{
		// draw string gadget text into text edit box
		U1 *str_show;
		U1 ch;
		S8 string_y ALIGN;
		S8 string_x ALIGN;
		S8 string_x_end ALIGN;
		S8 i ALIGN;
		S8 j ALIGN;
		Sint16 text_x, text_y;

		str_show = (U1 *) calloc ((string->visible_len + 1), sizeof (U1));
		if (str_show == NULL)
		{
			return (FALSE);
		}

		text_x = string->input_x;
		text_y = string->input_y;

		for (string_y = 0; string_y < string->text_lines; string_y++)
		{
			string_x = string_y * string->visible_len;
			string_x_end = string_x + string->visible_len;
			j = 0;

			// printf ("\nDEBUG: string_x: %lli, string_x_end: %lli\n", string_x, string_x_end);

			for (i = string_x; i < string_x_end; i++)
			{
				string->display[i] = string->value[i];

				ch = string->value[i];
				if (ch != '\0')
				{
					str_show[j] = string->value[i];
				}
				j++;
			}
			str_show[j] = '\0';

			// printf ("DEBUG: str_show: '%s'\n\n", str_show);

			if (strlen_safe ((const char *) str_show, MAXLINELEN) != 0)
			{
				// display text lines if string has chars
				if (! draw_text_ttf (surf, screennum, str_show, text_x, text_y, screen[screennum].gadget_color.text_light.r, screen[screennum].gadget_color.text_light.g, screen[screennum].gadget_color.text_light.b))
		    	{
		        	return (FALSE);
		    	}
			}
			text_y = text_y + string->text_height;

			// reset text buffer
			for (i = 0; i < string->visible_len; i++)
			{
				str_show[i] = '\0';
			}
		}
		free (str_show);
	}

    if (selected == GADGET_SELECTED)
    {
        if (string->cursor_pos_x == -1 && string->cursor_pos_y == -1)
        {
            /* init cursor position */
			string->cursor_pos_x = 0;
			string->cursor_pos_y = string->input_y;
        }

        /* draw cursor */

        cursor_x = string->input_x + (string->cursor_width * string->cursor_pos_x);
		cursor_y = string->input_y + (string-> cursor_height * string->cursor_pos_y);
        boxRGBA (renderer, cursor_x, cursor_y, cursor_x + string->cursor_width, cursor_y + string->cursor_height, screen[screennum].gadget_color.text_light.r, screen[screennum].gadget_color.text_light.g, screen[screennum].gadget_color.text_light.b, 50);
    }

    if (string->status == GADGET_NOT_ACTIVE)
    {
        draw_ghost_gadget (renderer, string->x, string->y, string->x2, string->y2);
    }

    // SDL_UpdateRect (renderer, string->x, string->y, string->x2 - string->x + 1, string->y2 - string->y + 1);
    // SDL_UpdateRect (renderer, string->text_x, string->text_y, string->text_x2 - string->text_x + 1, string->text_y2 - string->text_y + 1);

    SDL_RenderPresent (renderer);
    SDL_UpdateWindowSurface (window);
    return (TRUE);
}


U1 event_gadget_string (S2 screennum, U2 gadget_index)
{
	/*
	 * String gadget event handling using SDL TextInput ()
	 *
	 * Keymap for a German keyboard layout
	 * with German umlauts like ae, oe and ue
	 *
	 */


    SDL_Event event;
	SDL_Keycode key;

    U1 *string_buf, wait;
    S2 value_len;

	S2 i, insert_pos;
	S2 char_code;

	S2 shift = 0; // set to 1 if shift is pressed down

    struct gadget_string *string;

    string = (struct gadget_string *) screen[screennum].gadget[gadget_index].gptr;

    /* allocate string buffer */

    string_buf = (U1 *) malloc ((string->string_len + 1) * sizeof (U1));
    if (string_buf == NULL)
    {
        printf ("event_gadget_string: error can't allocate string buffer!\n");
        return (FALSE);
    }

    if (! draw_gadget_string (screennum, gadget_index, GADGET_SELECTED))
    {
        free (string_buf);
        return (FALSE);
    }

    /* wait for event */

	SDL_StartTextInput ();

	wait = TRUE;

	while (wait)
	{
		if (! SDL_WaitEvent (&event))
		{
			printf ("event_gadget_string: error can't wait for event!\n");
			free (string_buf);
			return FALSE;
		}

		value_len = strlen ((const char *) string->value);

		switch (event.type)
		{
			case SDL_KEYDOWN:
				key = event.key.keysym.sym;
				switch (key)
				{
					case SDLK_BACKSPACE:
						printf ("event_gadget_string: BACKSPACE\n");

						if (string->insert_pos > 0)
						{
							strremoveleft (string_buf, string->value, string->insert_pos);
							my_strcpy (string->value, string_buf);

							string->cursor_pos--;
							string->insert_pos--;

							if (! draw_gadget_string (screennum, gadget_index, GADGET_SELECTED))
							{
								free (string_buf);
								return (FALSE);
							}
						}
						break;

					case SDLK_DELETE:
						printf ("event_gadget_string: DELETE\n");

						strremoveright (string_buf, string->value, string->insert_pos);
						my_strcpy (string->value, string_buf);

						if (! draw_gadget_string (screennum, gadget_index, GADGET_SELECTED))
						{
							free (string_buf);
							return (FALSE);
						}
						break;

						break;

					case SDLK_LEFT:
						printf ("event_gadget_string: CURSOR LEFT\n");

						if (string->cursor_pos > 0)
						{
							string->cursor_pos--;
							string->insert_pos--;

							if (! draw_gadget_string (screennum, gadget_index, GADGET_SELECTED))
							{
								free (string_buf);
								return (FALSE);
							}
						}

						break;

					case SDLK_RIGHT:
						printf ("event_gadget_string: CURSOR RIGHT\n");

						if (string->cursor_pos < value_len)
						{
							string->cursor_pos++;
							string->insert_pos++;

							if (! draw_gadget_string (screennum, gadget_index, GADGET_SELECTED))
							{
								free (string_buf);
								return (FALSE);
							}
						}
						break;

					case SDLK_RETURN:
						printf ("event_gadget_string: RETURN\n");
						wait = FALSE;
						break;

					case SDLK_LSHIFT:
						shift = 1;
						break;

					case SDLK_RSHIFT:
						shift = 1;
						break;
				}
				break;

				case SDL_TEXTINPUT:
					/* update the composition text */

					printf ("DEBUG: SDL_TEXTINPUT\n");

					i = 0; insert_pos = string->insert_pos;

					while (event.text.text[i] != '\0')
					{
						char_code = event.text.text[i];
						// DEBUG ONLY:
						// printf ("event_gadget_string: text[%i]: %i\n", i, char_code);

						if (i > 0)
						{
							if (value_len < string->visible_len)
							{
								// ä Ä
								if (event.text.text[i] == -92 && event.text.text[i - 1] == -61)
								{
									// ä
									printf ("key: ä\n");

									strinsertchar (string_buf, string->value, 228, insert_pos);

									my_strcpy (string->value, string_buf);

									i = i + 1;
									insert_pos = insert_pos + 1;
									value_len = value_len + 1;

									string->cursor_pos++;
									string->insert_pos++;
									continue;
								}

								if (event.text.text[i] == -124 && event.text.text[i - 1] == -61)
								{
									// Ä
									printf ("key: Ä\n");

									strinsertchar (string_buf, string->value, 196, insert_pos);

									my_strcpy (string->value, string_buf);

									i = i + 1;
									insert_pos = insert_pos + 1;
									value_len = value_len + 1;

									string->cursor_pos++;
									string->insert_pos++;
									continue;
								}

								// ö Ö
								if (event.text.text[i] == -74 && event.text.text[i - 1] == -61)
								{
									// ö
									printf ("key: ö\n");

									strinsertchar (string_buf, string->value, 246, insert_pos);

									my_strcpy (string->value, string_buf);

									i = i + 1;
									insert_pos = insert_pos + 1;
									value_len = value_len + 1;

									string->cursor_pos++;
									string->insert_pos++;
									continue;
								}

								if (event.text.text[i] == -106 && event.text.text[i - 1] == -61)
								{
									// Ö
									printf ("key: Ö\n");

									strinsertchar (string_buf, string->value, 214, insert_pos);

									my_strcpy (string->value, string_buf);

									i = i + 1;
									insert_pos = insert_pos + 1;
									value_len = value_len + 1;

									string->cursor_pos++;
									string->insert_pos++;
									continue;
								}

								// ü Ü
								if (event.text.text[i] == -68 && event.text.text[i - 1] == -61)
								{
									printf ("key: ü\n");
									// ü
									strinsertchar (string_buf, string->value, 252, insert_pos);

									my_strcpy (string->value, string_buf);

									i = i + 1;
									insert_pos = insert_pos + 1;
									value_len = value_len + 1;

									string->cursor_pos++;
									string->insert_pos++;
									continue;
								}

								if (event.text.text[i] == -100 && event.text.text[i - 1] == -61)
								{
									// Ü
									printf ("key: Ü\n");

									strinsertchar (string_buf, string->value, 220, insert_pos);

									my_strcpy (string->value, string_buf);

									i = i + 1;
									insert_pos = insert_pos + 1;
									value_len = value_len + 1;

									string->cursor_pos++;
									string->insert_pos++;
									continue;
								}

								if (event.text.text[i] == -97 && event.text.text[i - 1] == -61)
								{
									// ß
									printf ("key: ß\n");

									strinsertchar (string_buf, string->value, 223, insert_pos);

									my_strcpy (string->value, string_buf);

									i = i + 1;
									insert_pos = insert_pos + 1;
									value_len = value_len + 1;

									string->cursor_pos++;
									string->insert_pos++;
									continue;
								}

								if (event.text.text[i] == -89 && event.text.text[i - 1] == -62)
								{
									// §
									printf ("key: §\n");

									strinsertchar (string_buf, string->value, 167, insert_pos);

									my_strcpy (string->value, string_buf);

									i = i + 1;
									insert_pos = insert_pos + 1;
									value_len = value_len + 1;

									string->cursor_pos++;
									string->insert_pos++;
									continue;
								}


								if (event.text.text[i] == -126 && event.text.text[i - 1] == -30)
								{
									// € Euro char
									printf ("key: €\n");

									strinsertchar (string_buf, string->value, 128, insert_pos);

									my_strcpy (string->value, string_buf);

									i = i + 1;
									insert_pos = insert_pos + 1;
									value_len = value_len + 1;

									string->cursor_pos++;
									string->insert_pos++;
									continue;
								}
							}
						}

						if (event.text.text[i] >= 0)
						{
							if (value_len < string->visible_len)
							{
								strinsertchar (string_buf, string->value, event.text.text[i], insert_pos);
								my_strcpy (string->value, string_buf);

								i = i + 1;
								insert_pos = insert_pos + 1;
								value_len = value_len + 1;

								string->cursor_pos++;
								string->insert_pos++;
							}
						}
						if (i >= 1)
						{
							break;
						}
						if (i == 0)
						{
							i++;
						}
					}

					if (! draw_gadget_string (screennum, gadget_index, GADGET_SELECTED))
					{
						free (string_buf);
						return (FALSE);
					}
					break;

			case SDL_KEYUP:
				key = event.key.keysym.sym;
				switch (key)
				{
					case SDLK_LSHIFT:
						shift = 0;
						break;

					case SDLK_RSHIFT:
						shift = 0;
						break;
				}
				break;
		}

	}

	SDL_StopTextInput();

	free (string_buf);
	return (TRUE);
}

U1 event_gadget_string_multiline (S2 screennum, U2 gadget_index)
{
	/*
	 * String gadget event handling using SDL TextInput ()
	 *
	 * Keymap for a German keyboard layout
	 * with German umlauts like ae, oe and ue
	 *
	 */

    SDL_Event event;
	SDL_Keycode key;

    U1 *string_buf, wait;
    S2 value_len;

	S2 i, insert_pos;
	S2 char_code;

	S2 string_len;

	S2 shift = 0; // set to 1 if shift is pressed down

    struct gadget_string_multiline *string;

    string = (struct gadget_string_multiline *) screen[screennum].gadget[gadget_index].gptr;

    /* allocate string buffer */

    string_buf = (U1 *) malloc ((string->text_lines * (string->visible_len + 1)) * sizeof (U1));
    if (string_buf == NULL)
    {
        printf ("event_gadget_string_multiline: error can't allocate string buffer!\n");
        return (FALSE);
    }

    if (! draw_gadget_string_multiline (screennum, gadget_index, GADGET_SELECTED))
    {
        free (string_buf);
        return (FALSE);
    }

    /* wait for event */

	SDL_StartTextInput ();

	wait = TRUE;

	while (wait)
	{
		if (! SDL_WaitEvent (&event))
		{
			printf ("event_gadget_string_multiline: error can't wait for event!\n");
			free (string_buf);
			return FALSE;
		}

		value_len = string->visible_len;

		switch (event.type)
		{
			case SDL_KEYDOWN:
				key = event.key.keysym.sym;
				switch (key)
				{
					case SDLK_BACKSPACE:
						printf ("event_gadget_string_multiline: BACKSPACE\n");

						if (string->insert_pos > 0)
						{
							strremoveleft (string_buf, string->value, string->insert_pos);
							my_strcpy (string->value, string_buf);

							if (string->cursor_pos_x > 0)
							{
								string->cursor_pos_x--;
							}
							else
							{
								// check if line > 0 and then set cursor one line above
								if (string->cursor_pos_y > 0)
								{
									string->cursor_pos_y--;
									string->cursor_pos_x = string->visible_len - 1;
								}
							}

							string->insert_pos--;

							if (! draw_gadget_string_multiline (screennum, gadget_index, GADGET_SELECTED))
							{
								free (string_buf);
								return (FALSE);
							}
						}
						break;

					case SDLK_DELETE:
						printf ("event_gadget_string_multiline: DELETE\n");

						strremoveright (string_buf, string->value, string->insert_pos);
						my_strcpy (string->value, string_buf);

						if (! draw_gadget_string_multiline (screennum, gadget_index, GADGET_SELECTED))
						{
							free (string_buf);
							return (FALSE);
						}
						break;

						break;

					case SDLK_LEFT:
						printf ("event_gadget_string_multiline: CURSOR LEFT\n");

						if (string->cursor_pos_x > 0)
						{
							string->cursor_pos_x--;
							string->insert_pos--;
						}
						else
						{
							if (string->cursor_pos_y > 0)
							{
								string->cursor_pos_y--;
								string->cursor_pos_x = string->visible_len - 1;
								string->insert_pos--;
							}
						}

						if (! draw_gadget_string_multiline (screennum, gadget_index, GADGET_SELECTED))
						{
							free (string_buf);
							return (FALSE);
						}
						break;

					case SDLK_RIGHT:
						string_len = strlen_safe ((const char *) string->value, (string->visible_len * string->text_lines));

						if (string->insert_pos < string_len)
						{
							if (string->cursor_pos_x < string->visible_len - 1)
							{
								string->cursor_pos_x++;
								string->insert_pos++;
							}
							else
							{
								if (string->cursor_pos_y < string->text_lines - 1)
				            	{
				                	string->cursor_pos_x = 0;
				                	string->cursor_pos_y++;
									string->insert_pos++;
				            	}
							}
						}

						if (! draw_gadget_string_multiline (screennum, gadget_index, GADGET_SELECTED))
						{
							free (string_buf);
							return (FALSE);
						}
						break;

						/*
						// FIXME, how can it be done safe???
					case SDLK_UP:
						printf ("event_gadget_string_multiline: CURSOR UP\n");

						if (string->cursor_pos_y > 0)
						{
							string->cursor_pos_y--;
							string->insert_pos = (string->cursor_pos_y * string->visible_len) + string->cursor_pos_x;

							if (! draw_gadget_string_multiline (screennum, gadget_index, GADGET_SELECTED))
							{
								free (string_buf);
								return (FALSE);
							}
						}
						break;

					case SDLK_DOWN:
						printf ("event_gadget_string_multiline: CURSOR DOWN\n");

						if (string->cursor_pos_y < string->text_lines - 1)
						{
							string->cursor_pos_y++;
							string->insert_pos = (string->cursor_pos_y * string->visible_len) + string->cursor_pos_x;

							if (! draw_gadget_string_multiline (screennum, gadget_index, GADGET_SELECTED))
							{
								free (string_buf);
								return (FALSE);
							}
						}
						break;
						*/

					case SDLK_RETURN:
						printf ("event_gadget_string_multiline: RETURN\n");
						wait = FALSE;
						break;

					case SDLK_LSHIFT:
						shift = 1;
						break;

					case SDLK_RSHIFT:
						shift = 1;
						break;
				}
				break;

				case SDL_TEXTINPUT:
					/* update the composition text */

					printf ("DEBUG: SDL_TEXTINPUT\n");

					i = 0; insert_pos = string->insert_pos;

					while (event.text.text[i] != '\0')
					{
						char_code = event.text.text[i];
						// DEBUG ONLY:
						// printf ("event_gadget_string_multiline: text[%i]: %i\n", i, char_code);

						if (i > 0)
						{
							// if (value_len < string->visible_len)
							// {
								// ä Ä
								if (event.text.text[i] == -92 && event.text.text[i - 1] == -61)
								{
									// ä
									printf ("key: ä\n");

									if (strlen_safe ((const char *) string_buf, (string->visible_len * string->text_lines)) < (string->visible_len * string->text_lines) - 1)
									{
										strinsertchar (string_buf, string->value, 228, insert_pos);

										my_strcpy (string->value, string_buf);

										i = i + 1;
										insert_pos = insert_pos + 1;
										value_len = value_len + 1;

										if (string->cursor_pos_x < string->visible_len - 1)
										{
											string->cursor_pos_x++;
										}
										else
										{
											if (string->cursor_pos_y < string->text_lines - 1)
											{
												string->cursor_pos_x = 0;
												string->cursor_pos_y++;
											}
										}

										string->insert_pos++;
									}
									else
									{
										break;
									}
									continue;
								}

								if (event.text.text[i] == -124 && event.text.text[i - 1] == -61)
								{
									// Ä
									printf ("key: Ä\n");

									if (strlen_safe ((const char *) string_buf, (string->visible_len * string->text_lines)) < (string->visible_len * string->text_lines) - 1)
									{
										strinsertchar (string_buf, string->value, 196, insert_pos);

										my_strcpy (string->value, string_buf);

										i = i + 1;
										insert_pos = insert_pos + 1;
										value_len = value_len + 1;

										if (string->cursor_pos_x < string->visible_len - 1)
										{
											string->cursor_pos_x++;
										}
										else
										{
											if (string->cursor_pos_y < string->text_lines -1)
											{
												string->cursor_pos_x = 0;
												string->cursor_pos_y++;
											}
										}

										string->insert_pos++;
									}
									else
									{
										break;
									}
									continue;
								}

								// ö Ö
								if (event.text.text[i] == -74 && event.text.text[i - 1] == -61)
								{
									// ö
									printf ("key: ö\n");

									if (strlen_safe ((const char *) string_buf, (string->visible_len * string->text_lines)) < (string->visible_len * string->text_lines) - 1)
									{
										strinsertchar (string_buf, string->value, 246, insert_pos);

										my_strcpy (string->value, string_buf);

										i = i + 1;
										insert_pos = insert_pos + 1;
										value_len = value_len + 1;

										if (string->cursor_pos_x < string->visible_len - 1)
										{
											string->cursor_pos_x++;
										}
										else
										{
											if (string->cursor_pos_y < string->text_lines -1)
											{
												string->cursor_pos_x = 0;
												string->cursor_pos_y++;
											}
										}

										string->insert_pos++;
									}
									else
									{
										break;
									}
									continue;
								}

								if (event.text.text[i] == -106 && event.text.text[i - 1] == -61)
								{
									// Ö
									printf ("key: Ö\n");

									if (strlen_safe ((const char *) string_buf, (string->visible_len * string->text_lines)) < (string->visible_len * string->text_lines) - 1)
									{
										strinsertchar (string_buf, string->value, 214, insert_pos);

										my_strcpy (string->value, string_buf);

										i = i + 1;
										insert_pos = insert_pos + 1;
										value_len = value_len + 1;

										if (string->cursor_pos_x < string->visible_len - 1)
										{
											string->cursor_pos_x++;
										}
										else
										{
											if (string->cursor_pos_y < string->text_lines -1)
											{
												string->cursor_pos_x = 0;
												string->cursor_pos_y++;
											}
										}

										string->insert_pos++;
									}
									else
									{
										break;
									}
									continue;
								}

								// ü Ü
								if (event.text.text[i] == -68 && event.text.text[i - 1] == -61)
								{
									printf ("key: ü\n");
									// ü

									if (strlen_safe ((const char *) string_buf, (string->visible_len * string->text_lines)) < (string->visible_len * string->text_lines) - 1)
									{
										strinsertchar (string_buf, string->value, 252, insert_pos);

										my_strcpy (string->value, string_buf);

										i = i + 1;
										insert_pos = insert_pos + 1;
										value_len = value_len + 1;

										if (string->cursor_pos_x < string->visible_len - 1)
										{
											string->cursor_pos_x++;
										}
										else
										{
											if (string->cursor_pos_y < string->text_lines -1)
											{
												string->cursor_pos_x = 0;
												string->cursor_pos_y++;
											}
										}

										string->insert_pos++;
									}
									else
									{
										break;
									}
									continue;
								}

								if (event.text.text[i] == -100 && event.text.text[i - 1] == -61)
								{
									// Ü
									printf ("key: Ü\n");

									if (strlen_safe ((const char *) string_buf, (string->visible_len * string->text_lines)) < (string->visible_len * string->text_lines) - 1)
									{
										strinsertchar (string_buf, string->value, 220, insert_pos);

										my_strcpy (string->value, string_buf);

										i = i + 1;
										insert_pos = insert_pos + 1;
										value_len = value_len + 1;

										if (string->cursor_pos_x < string->visible_len - 1)
										{
											string->cursor_pos_x++;
										}
										else
										{
											if (string->cursor_pos_y < string->text_lines -1)
											{
												string->cursor_pos_x = 0;
												string->cursor_pos_y++;
											}
										}

										string->insert_pos++;
									}
									else
									{
										break;
									}
									continue;
								}

								if (event.text.text[i] == -97 && event.text.text[i - 1] == -61)
								{
									// ß
									printf ("key: ß\n");

									if (strlen_safe ((const char *) string_buf, (string->visible_len * string->text_lines)) < (string->visible_len * string->text_lines) - 1)
									{
										strinsertchar (string_buf, string->value, 223, insert_pos);

										my_strcpy (string->value, string_buf);

										i = i + 1;
										insert_pos = insert_pos + 1;
										value_len = value_len + 1;

										if (string->cursor_pos_x < string->visible_len - 1)
										{
											string->cursor_pos_x++;
										}
										else
										{
											if (string->cursor_pos_y < string->text_lines -1)
											{
												string->cursor_pos_x = 0;
												string->cursor_pos_y++;
											}
										}

										string->insert_pos++;
									}
									else
									{
										break;
									}
									continue;
								}

								if (event.text.text[i] == -89 && event.text.text[i - 1] == -62)
								{
									// §
									printf ("key: §\n");

									if (strlen_safe ((const char *) string_buf, (string->visible_len * string->text_lines)) < (string->visible_len * string->text_lines) - 1)
									{
										strinsertchar (string_buf, string->value, 167, insert_pos);

										my_strcpy (string->value, string_buf);

										i = i + 1;
										insert_pos = insert_pos + 1;
										value_len = value_len + 1;

										if (string->cursor_pos_x < string->visible_len - 1)
										{
											string->cursor_pos_x++;
										}
										else
										{
											if (string->cursor_pos_y < string->text_lines -1)
											{
												string->cursor_pos_x = 0;
												string->cursor_pos_y++;
											}
										}

										string->insert_pos++;
									}
									else
									{
										break;
									}
									continue;
								}


								if (event.text.text[i] == -126 && event.text.text[i - 1] == -30)
								{
									// € Euro char
									printf ("key: €\n");

									if (strlen_safe ((const char *) string_buf, (string->visible_len * string->text_lines)) < (string->visible_len * string->text_lines) - 1)
									{
										strinsertchar (string_buf, string->value, 128, insert_pos);

										my_strcpy (string->value, string_buf);

										i = i + 1;
										insert_pos = insert_pos + 1;
										value_len = value_len + 1;

										if (string->cursor_pos_x < string->visible_len - 1)
										{
											string->cursor_pos_x++;
										}
										else
										{
											if (string->cursor_pos_y < string->text_lines -1)
											{
												string->cursor_pos_x = 0;
												string->cursor_pos_y++;
											}
										}

										string->insert_pos++;
									}
									else
									{
										break;
									}
									continue;
								}
							//}
						}

						if (event.text.text[i] >= 0)
						{
							// if (value_len < string->visible_len)
							//{
								if (strlen_safe ((const char *) string_buf, (string->visible_len * string->text_lines)) < (string->visible_len * string->text_lines) - 1)
								{
									strinsertchar (string_buf, string->value, event.text.text[i], insert_pos);
									my_strcpy (string->value, string_buf);

									i = i + 1;
									insert_pos = insert_pos + 1;
									value_len = value_len + 1;

									if (string->cursor_pos_x < string->visible_len - 1)
									{
										string->cursor_pos_x++;
									}
									else
									{
										if (string->cursor_pos_y < string->text_lines -1)
										{
											string->cursor_pos_x = 0;
											string->cursor_pos_y++;
										}
									}

									string->insert_pos++;
								}
								else
								{
									break;
								}

							//}
						}
						if (i >= 1)
						{
							break;
						}
						if (i == 0)
						{
							i++;
						}
					}

					if (! draw_gadget_string_multiline (screennum, gadget_index, GADGET_SELECTED))
					{
						free (string_buf);
						return (FALSE);
					}
					break;

			case SDL_KEYUP:
				key = event.key.keysym.sym;
				switch (key)
				{
					case SDLK_LSHIFT:
						shift = 0;
						break;

					case SDLK_RSHIFT:
						shift = 0;
						break;
				}
				break;
		}

	}

	SDL_StopTextInput();

	free (string_buf);
	return (TRUE);
}

U1 *event_gadget_slider (S2 screennum, U2 gadget_index)
{
	/*
	 * slider gadget event handling
	 */

     Uint8 buttonmask, mouse_button, mouse_button_slide;

     S8 range ALIGN;
     S8 value_gadget ALIGN;
     S8 pixel_ratio ALIGN;
     Sint16 range_pixel;
     S8 value_percent ALIGN;
     Sint16 slider_x1;
     Sint16 slider_x2;
     int x, y;
     U1 wait;

     struct gadget_slider *slider;
     slider = (struct gadget_slider *) screen[screennum].gadget[gadget_index].gptr;

     // calculate position and size of slider

     range = slider->max - slider->min;
     range_pixel = slider->x2 - slider->x;
     value_gadget = slider->value - slider->min;

     value_percent = 100 * value_gadget / range;

     pixel_ratio = range_pixel / range;      // pixels per each value
     slider_x1 = slider->x + ((range_pixel * value_percent) / 100);

     // calculate slider width
     slider_x2 = slider_x1 + pixel_ratio;

     SDL_Event event;
	 // SDL_Keycode key;

     /* allocate string buffer */

     if (! draw_gadget_slider (screennum, gadget_index, GADGET_SELECTED))
     {
         return (FALSE);
     }

     /* wait for event */
     while (1 == 1)
     {
         wait = TRUE;

         while (wait)
         {
 			/* draw mouse cursor */

 			// SDL_GetMouseState (&mouse_x, &mouse_y);
 			// draw_cursor (cursor, mouse_x, mouse_y);
 			// update_screen (0);

 			SDL_UpdateWindowSurface (window);

 			/* wait for slider event */
             if (! SDL_WaitEvent (&event))
             {
                 printf ("gadget_event: error can't wait for event!\n");
                 return (NULL);
             }

             switch (event.type)
             {
                 case SDL_MOUSEBUTTONDOWN:
                     if (event.button.button == 1)
                     {
                         x = event.button.x;
                         y = event.button.y;
                         wait = FALSE;
                     }
                     break;

 #if __ANDROID__
 				case SDL_ACTIVEEVENT:
 					resized = 0;

 					while (resized == 0)
 					{
 						while (SDL_PollEvent (&event))
 						{
 							if (event.type == SDL_VIDEORESIZE)
 							{
 								//If the window was iconified or restored

 								reopen_screen (screennum, event.resize.w, event.resize.h);
 								restore_screen ();

 								resized = 1;
 							}
 						}
 					}
 					break;
 #endif
             }

             if ((x > slider->x && x < slider->x2) && (y > slider->y && y < slider->y2) && slider->status == GADGET_ACTIVE)
             {
                 mouse_button = 1;
                 while (mouse_button == 1)
                 {
                     SDL_Delay (100);

                     SDL_PumpEvents ();
                     buttonmask = SDL_GetMouseState (&x, &y);

                     if (! (buttonmask & SDL_BUTTON (1)))
                     {
                         mouse_button = 0;
                     }

                     if ((x > slider->x && x < slider->x2) && (y > slider->y && y < slider->y2))
                     {
                         printf ("gadget_slider: drawing slider %i selected.\n", gadget_index);

                         if (! draw_gadget_slider (screennum, gadget_index, GADGET_SELECTED))
                         {
                             printf ("gadget_slider: error can't draw gadget!\n");
                             return (NULL);
                         }
                    }
                    else
                    {
                        printf ("gadget_slider: drawing button %i normal.\n", gadget_index);

                        if (! draw_gadget_slider (screennum, gadget_index, GADGET_NOT_SELECTED))
                        {
                            printf ("gadget_slider: error can't draw gadget!\n");
                            return (NULL);
                        }
                    }
                }

                if (! draw_gadget_slider (screennum, gadget_index, GADGET_NOT_SELECTED))
                {
                    printf ("gadget_slider: error can't draw gadget!\n");
                    return (NULL);
                }

                if ((x > slider->x && x < slider->x2) && (y > slider->y && y < slider->y2))
                {
                    /* gadget was selected */

                    printf ("gadget_slider: drawing slider %i selected.\n", gadget_index);

                    mouse_button_slide = 1;
                    while (mouse_button_slide == 1)
                    {
                        SDL_Delay (100);

                        SDL_PumpEvents ();
                        buttonmask = SDL_GetMouseState (&x, &y);

                        if (! (buttonmask & SDL_BUTTON (1)))
                        {
                            mouse_button_slide = 0;
                        }

                        // calculate position and size of slider

                        range = slider->max - slider->min;
                        range_pixel = slider->x2 - slider->x;
                        value_gadget = slider->value - slider->min;

                        value_percent = 100 * value_gadget / range;

                        pixel_ratio = range_pixel / range;      // pixels per each value
                        slider_x1 = slider->x + ((range_pixel * value_percent) / 100);

                        // calculate slider width
                        slider_x2 = slider_x1 + pixel_ratio;

                        if (x <= slider_x1 - pixel_ratio)
                        {
                            if (slider->value > slider->min)
                            {
                                slider->value--;
                            }
                        }
                        if (x >= slider_x2 + pixel_ratio)
                        {
                            if (slider->value < slider->max)
                            {
                                slider->value++;
                            }
                        }

                        if (! draw_gadget_slider (screennum, gadget_index, GADGET_SELECTED))
                        {
                            printf ("gadget_slider: error can't draw gadget!\n");
                            return (NULL);
                        }
                    }
                    wait = 0;

                    if (! draw_gadget_slider (screennum, gadget_index, GADGET_NOT_SELECTED))
                    {
                        printf ("gadget_slider: error can't draw gadget!\n");
                        return (NULL);
                    }
                }
                else
                {
                    if (! draw_gadget_slider (screennum, gadget_index, GADGET_NOT_SELECTED))
                    {
                        printf ("gadget_slider: error can't draw gadget!\n");
                        return (NULL);
                    }
                    mouse_button = 0;
                    wait = 0;
                }
            }
        }
        break;
    }
    return ((U1 *) 1);
}

U1 *event_gadget_slider_vert (S2 screennum, U2 gadget_index)
{
	/*
	 * slider vertical gadget event handling
	 */

     Uint8 buttonmask, mouse_button, mouse_button_slide;

     S8 range ALIGN;
     S8 value_gadget ALIGN;
     S8 pixel_ratio ALIGN;
     Sint16 range_pixel;
     S8 value_percent ALIGN;
     Sint16 slider_y1;
     Sint16 slider_y2;
     Sint16 slider_x1;
     Sint16 slider_x2;
     int x, y;
     U1 wait;

     struct gadget_slider *slider;
     slider = (struct gadget_slider *) screen[screennum].gadget[gadget_index].gptr;

     // calculate position and size of slider

     range = slider->max - slider->min;
     range_pixel = slider->y2 - slider->y;
     value_gadget = slider->value - slider->min;

     value_percent = 100 * value_gadget / range;

     pixel_ratio = range_pixel / range;      // pixels per each value
     slider_y1 = slider->y + ((range_pixel * value_percent) / 100);

     // calculate slider width
     slider_y2 = slider_y1 + pixel_ratio;

     slider_x1 = slider->x + 1;

     SDL_Event event;
	 // SDL_Keycode key;

     /* allocate string buffer */

     if (! draw_gadget_slider_vert (screennum, gadget_index, GADGET_SELECTED))
     {
         return (FALSE);
     }

     /* wait for event */
     while (1 == 1)
     {
         wait = TRUE;

         while (wait)
         {
 			/* draw mouse cursor */

 			// SDL_GetMouseState (&mouse_x, &mouse_y);
 			// draw_cursor (cursor, mouse_x, mouse_y);
 			// update_screen (0);

 			SDL_UpdateWindowSurface (window);

 			/* wait for slider event */
             if (! SDL_WaitEvent (&event))
             {
                 printf ("gadget_event: error can't wait for event!\n");
                 return (NULL);
             }

             switch (event.type)
             {
                 case SDL_MOUSEBUTTONDOWN:
                     if (event.button.button == 1)
                     {
                         x = event.button.x;
                         y = event.button.y;
                         wait = FALSE;
                     }
                     break;

 #if __ANDROID__
 				case SDL_ACTIVEEVENT:
 					resized = 0;

 					while (resized == 0)
 					{
 						while (SDL_PollEvent (&event))
 						{
 							if (event.type == SDL_VIDEORESIZE)
 							{
 								//If the window was iconified or restored

 								reopen_screen (screennum, event.resize.w, event.resize.h);
 								restore_screen ();

 								resized = 1;
 							}
 						}
 					}
 					break;
 #endif
             }

             if ((x > slider->x && x < slider->x2) && (y > slider->y && y < slider->y2) && slider->status == GADGET_ACTIVE)
             {
                 mouse_button = 1;
                 while (mouse_button == 1)
                 {
                     SDL_Delay (100);

                     SDL_PumpEvents ();
                     buttonmask = SDL_GetMouseState (&x, &y);

                     if (! (buttonmask & SDL_BUTTON (1)))
                     {
                         mouse_button = 0;
                     }

                     if ((x > slider->x && x < slider->x2) && (y > slider->y && y < slider->y2))
                     {
                         printf ("gadget_slider: drawing slider %i selected.\n", gadget_index);

                         if (! draw_gadget_slider_vert (screennum, gadget_index, GADGET_SELECTED))
                         {
                             printf ("gadget_slider: error can't draw gadget!\n");
                             return (NULL);
                         }
                    }
                    else
                    {
                        printf ("gadget_slider: drawing button %i normal.\n", gadget_index);

                        if (! draw_gadget_slider_vert (screennum, gadget_index, GADGET_NOT_SELECTED))
                        {
                            printf ("gadget_slider: error can't draw gadget!\n");
                            return (NULL);
                        }
                    }
                }

                if (! draw_gadget_slider_vert (screennum, gadget_index, GADGET_NOT_SELECTED))
                {
                    printf ("gadget_slider: error can't draw gadget!\n");
                    return (NULL);
                }

                if ((x > slider->x && x < slider->x2) && (y > slider->y && y < slider->y2))
                {
                    /* gadget was selected */

                    printf ("gadget_slider: drawing slider %i selected.\n", gadget_index);

                    mouse_button_slide = 1;
                    while (mouse_button_slide == 1)
                    {
                        SDL_Delay (100);

                        SDL_PumpEvents ();
                        buttonmask = SDL_GetMouseState (&x, &y);

                        if (! (buttonmask & SDL_BUTTON (1)))
                        {
                            mouse_button_slide = 0;
                        }

                        // calculate position and size of slider

                        range = slider->max - slider->min;
                        range_pixel = slider->y2 - slider->y;
                        value_gadget = slider->value - slider->min;

                        value_percent = 100 * value_gadget / range;

                        pixel_ratio = range_pixel / range;      // pixels per each value
                        slider_y1 = slider->y + ((range_pixel * value_percent) / 100);

                        // calculate slider width
                        slider_x2 = slider_x1 + pixel_ratio;

                        if (y <= slider_y1 - pixel_ratio)
                        {
                            if (slider->value > slider->min)
                            {
                                slider->value--;
                            }
                        }
                        if (y >= slider_y2 + pixel_ratio)
                        {
                            if (slider->value < slider->max)
                            {
                                slider->value++;
                            }
                        }

                        if (! draw_gadget_slider_vert (screennum, gadget_index, GADGET_SELECTED))
                        {
                            printf ("gadget_slider: error can't draw gadget!\n");
                            return (NULL);
                        }
                    }
                    wait = 0;

                    if (! draw_gadget_slider_vert (screennum, gadget_index, GADGET_NOT_SELECTED))
                    {
                        printf ("gadget_slider: error can't draw gadget!\n");
                        return (NULL);
                    }
                }
                else
                {
                    if (! draw_gadget_slider_vert (screennum, gadget_index, GADGET_NOT_SELECTED))
                    {
                        printf ("gadget_slider: error can't draw gadget!\n");
                        return (NULL);
                    }
                    mouse_button = 0;
                    wait = 0;
                }
            }
        }
        break;
    }
    return ((U1 *) 1);
}

U1 *gadget_event (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    SDL_Event event;
    U1 wait, cycle_menu;
    Uint8 buttonmask, mouse_button, double_click, select;
    U2 i;
    S4 value, old_value;
    int x, y;
	S8 text_address ALIGN;
	// U1 resized = 0;

    struct gadget_button *button;
    struct gadget_checkbox *checkbox;
    struct gadget_cycle *cycle;
    struct gadget_string *string;
	struct gadget_string_multiline *string_multiline;
    struct gadget_box *box;
    struct gadget_slider *slider;

	sp = stpopi ((U1 *) &text_address, sp, sp_top);
    if (sp == NULL)
    {
 	   printf ("gadget_event: ERROR, stack corrupt!\n");
	   return (NULL);
    }

    if (! screen[screennum].gadget)
    {
        printf ("gadget_event: error gadget list not allocated!\n");
        return (NULL);
    }

    while (1 == 1)
    {
        wait = TRUE;

        while (wait)
        {
			/* draw mouse cursor */

			// SDL_GetMouseState (&mouse_x, &mouse_y);
			// draw_cursor (cursor, mouse_x, mouse_y);
			// update_screen (0);

			SDL_UpdateWindowSurface (window);

			/* wait for event */
            if (! SDL_WaitEvent (&event))
            {
                printf ("gadget_event: error can't wait for event!\n");
                return (NULL);
            }

            switch (event.type)
            {
                case SDL_MOUSEBUTTONDOWN:
                    if (event.button.button == 1)
                    {
                        x = event.button.x;
                        y = event.button.y;
                        wait = FALSE;
                    }
                    break;

#if __ANDROID__
				case SDL_ACTIVEEVENT:
					resized = 0;

					while (resized == 0)
					{
						while (SDL_PollEvent (&event))
						{
							if (event.type == SDL_VIDEORESIZE)
							{
								//If the window was iconified or restored

								reopen_screen (screennum, event.resize.w, event.resize.h);
								restore_screen ();

								resized = 1;
							}
						}
					}
					break;
#endif
            }
        }

        for (i = 0; i < screen[screennum].gadgets; i++)
        {
            if (screen[screennum].gadget[i].gptr)
            {
                switch (screen[screennum].gadget[i].type)
                {
                    case GADGET_BUTTON:
                        button = (struct gadget_button *) screen[screennum].gadget[i].gptr;

                        if ((x > button->x && x < button->x2) && (y > button->y && y < button->y2) && button->status == GADGET_ACTIVE)
                        {
                            mouse_button = 1;
                            while (mouse_button == 1)
                            {
                                SDL_Delay (100);

                                SDL_PumpEvents ();
                                buttonmask = SDL_GetMouseState (&x, &y);

                                if (! (buttonmask & SDL_BUTTON (1)))
                                {
                                    mouse_button = 0;
                                }

                                if ((x > button->x && x < button->x2) && (y > button->y && y < button->y2))
                                {
                                    printf ("gadget_event: drawing button %i selected.\n", i);

                                    if (! draw_gadget_button (screennum, i, GADGET_SELECTED))
                                    {
                                        printf ("gadget_event: error can't draw gadget!\n");
                                        return (NULL);
                                    }
                                }
                                else
                                {
                                    printf ("gadget_event: drawing button %i normal.\n", i);

                                    if (! draw_gadget_button (screennum, i, GADGET_NOT_SELECTED))
                                    {
                                        printf ("gadget_event: error can't draw gadget!\n");
                                        return (NULL);
                                    }
                                }
                            }

                            if (! draw_gadget_button (screennum, i, GADGET_NOT_SELECTED))
                            {
                                printf ("gadget_event: error can't draw gadget!\n");
                                return (NULL);
                            }

                            if ((x > button->x && x < button->x2) && (y > button->y && y < button->y2))
                            {
                                /* gadget was selected */

                                printf ("gadget_event: button gadget %i selected.\n", i);

								sp = stpushi (i, sp, sp_bottom);
								if (sp == NULL)
								{
									// error
									printf ("gadget_event: ERROR: stack corrupt!\n");
									return (NULL);
								}

								//	dummy push value
								sp = stpushi (0, sp, sp_bottom);
								if (sp == NULL)
								{
									// error
									printf ("gadget_event: ERROR: stack corrupt!\n");
									return (NULL);
								}

								// push 1 = all OK!
								sp = stpushi (1, sp, sp_bottom);
								if (sp == NULL)
								{
									// error
									printf ("gadget_event: ERROR: stack corrupt!\n");
									return (NULL);
								}

                                return (sp);
                            }
                        }
                        break;

                    case GADGET_CHECKBOX:
                        checkbox = (struct gadget_checkbox *) screen[screennum].gadget[i].gptr;

                        if ((x > checkbox->x && x < checkbox->x2) && (y > checkbox->y && y < checkbox->y2) && checkbox->status == GADGET_ACTIVE)
                        {
                            mouse_button = 1;
                            while (mouse_button == 1)
                            {
                                SDL_Delay (100);

                                SDL_PumpEvents ();
                                buttonmask = SDL_GetMouseState (&x, &y);

                                if (! (buttonmask & SDL_BUTTON (1)))
                                {
                                    mouse_button = 0;
                                }

                                if ((x > checkbox->x && x < checkbox->x2) && (y > checkbox->y && y < checkbox->y2))
                                {
                                    printf ("gadget_event: drawing checkbox %i selected.\n", i);

                                    if (! draw_gadget_checkbox (screennum, i, GADGET_SELECTED, checkbox->value))
                                    {
                                        printf ("gadget_event: error can't draw gadget!\n");
                                        return (NULL);
                                    }
                                }
                                else
                                {
                                    printf ("gadget_event: drawing checkbox %i normal.\n", i);

                                    if (! draw_gadget_checkbox (screennum, i, GADGET_NOT_SELECTED, checkbox->value))
                                    {
                                        printf ("gadget_event: error can't draw gadget!\n");
                                        return (NULL);
                                    }
                                }
                            }

                            if (! draw_gadget_checkbox (screennum, i, GADGET_NOT_SELECTED, checkbox->value))
                            {
                                printf ("gadget_event: error can't draw gadget!\n");
                                return (NULL);
                            }

                            if ((x > checkbox->x && x < checkbox->x2) && (y > checkbox->y && y < checkbox->y2))
                            {
                                /* gadget was selected */

                                if (checkbox->value == GADGET_CHECKBOX_TRUE)
                                {
                                    checkbox->value = GADGET_CHECKBOX_FALSE;
                                }
                                else
                                {
                                    checkbox->value = GADGET_CHECKBOX_TRUE;
                                }

                                if (! draw_gadget_checkbox (screennum, i, GADGET_NOT_SELECTED, checkbox->value))
                                {
                                    printf ("gadget_event: error can't draw gadget!\n");
                                    return (NULL);
                                }

                                printf ("gadget_event: checkbox gadget %i selected.\n", i);

								sp = stpushi (i, sp, sp_bottom);
								if (sp == NULL)
								{
									// error
									printf ("gadget_event: ERROR: stack corrupt!\n");
									return (NULL);
								}

								sp = stpushi (checkbox->value, sp, sp_bottom);
								if (sp == NULL)
								{
									// error
									printf ("gadget_event: ERROR: stack corrupt!\n");
									return (NULL);
								}

								// push 1 = all OK!
								sp = stpushi (1, sp, sp_bottom);
								if (sp == NULL)
								{
									// error
									printf ("gadget_event: ERROR: stack corrupt!\n");
									return (NULL);
								}

                                return (sp);
                            }
                        }
                        break;

                    case GADGET_CYCLE:
                        cycle = (struct gadget_cycle *) screen[screennum].gadget[i].gptr;

                        if ((x > cycle->x && x < cycle->x2) && (y > cycle->y && y < cycle->y2) && cycle->status == GADGET_ACTIVE)
                        {
                            mouse_button = 1;
                            while (mouse_button == 1)
                            {
                                SDL_Delay (100);

                                SDL_PumpEvents ();
                                buttonmask = SDL_GetMouseState (&x, &y);

                                if (! (buttonmask & SDL_BUTTON (1)))
                                {
                                    mouse_button = 0;
                                }

                                if ((x > cycle->x && x < cycle->x2) && (y > cycle->y && y < cycle->y2))
                                {
                                    printf ("gadget_event: drawing cycle %i selected.\n", i);

                                    if (! draw_gadget_cycle (screennum, i, GADGET_SELECTED, cycle->value))
                                    {
                                        printf ("gadget_event: error can't draw gadget!\n");
                                        return (NULL);
                                    }
                                }
                                else
                                {
                                    printf ("gadget_event: drawing cycle %i normal.\n", i);

                                    if (! draw_gadget_cycle (screennum, i, GADGET_NOT_SELECTED, cycle->value))
                                    {
                                        printf ("gadget_event: error can't draw gadget!\n");
                                        return (NULL);
                                    }
                                }
                            }

                            if (! draw_gadget_cycle (screennum, i, GADGET_NOT_SELECTED, cycle->value))
                            {
                                printf ("gadget_event: error can't draw gadget!\n");
                                return (NULL);
                            }

                            if ((x > cycle->x && x < cycle->x2) && (y > cycle->y && y < cycle->y2))
                            {
                                /* gadget was selected */

                                if (! draw_gadget_cycle (screennum, i, GADGET_MENU_DOWN, cycle->value))
                                {
                                    printf ("gadget_event: error can't draw gadget!\n");
                                    return (NULL);
                                }

                                cycle_menu = TRUE;
                                while (cycle_menu)
                                {
                                    mouse_button = 0; double_click = 0; select = 0;
									old_value = -1;

/* On Android the highlighted menu entry will be selected with a double click
 * this was needed, otherwise the second and following entries could never be selected!
 * On normal desktop system one click is used!
 *
 * This was a FIX to make this usable on Android.
 */


#if GADGET_CYCLE_DOUBLECLICK
									while (mouse_button == 0)
                                    {
										select = 0;
										SDL_Delay (100);

                                        SDL_PumpEvents ();
                                        buttonmask = SDL_GetMouseState (&x, &y);

                                        if (buttonmask & SDL_BUTTON (1))
                                        {
                                            select = 1;
                                        }

                                        if ((x > cycle->menu_x && x < cycle->menu_x2) && (y > cycle->menu_y && y < cycle->menu_y2))
                                        {
                                            for (value = 0; value < cycle->menu_entries; value++)
                                            {
                                                if ((y > cycle->menu_y + (value * (cycle->text_height + (cycle->text_height / 2)))) && ((y < cycle->menu_y + ((value + 1) * (cycle->text_height + (cycle->text_height / 2))))))
                                                {
                                                    cycle->value = value;

													if (value == old_value)
													{
														if (select == 1)
														{
															/* mousebutton pressed, wait for release */

															double_click = 0;

															while (double_click == 0)
															{
																SDL_Delay (100);

																SDL_PumpEvents ();
																buttonmask = SDL_GetMouseState (&x, &y);

																if (! (buttonmask & SDL_BUTTON (1)))
																{
																	double_click = 1;

																	/* button down */
																}
															}



															if (double_click == 1)
															{
																/* button up = selected */
																if ((y > cycle->menu_y + (value * (cycle->text_height + (cycle->text_height / 2)))) && ((y < cycle->menu_y + ((value + 1) * (cycle->text_height + (cycle->text_height / 2))))))
																{
																	/* click in the same menu entry: select item */
																	mouse_button = 1;
																}
															}
														}
													}
													old_value = value;
                                                }
                                            }

                                            if (! draw_gadget_cycle (screennum, i, GADGET_MENU_DOWN, cycle->value))
                                            {
                                                printf ("gadget_event: error can't draw gadget!\n");
                                                return (NULL);
                                            }
                                        }
									}
#else
                                    while (mouse_button == 0)
                                    {
                                        SDL_Delay (100);

                                        SDL_PumpEvents ();
                                        buttonmask = SDL_GetMouseState (&x, &y);

                                        if (buttonmask & SDL_BUTTON (1))
                                        {
                                            mouse_button = 1;
                                        }

                                        if ((x > cycle->menu_x && x < cycle->menu_x2) && (y > cycle->menu_y && y < cycle->menu_y2))
                                        {
                                            for (value = 0; value < cycle->menu_entries; value++)
                                            {
                                                if ((y > cycle->menu_y + (value * (cycle->text_height + (cycle->text_height / 2)))) && ((y < cycle->menu_y + ((value + 1) * (cycle->text_height + (cycle->text_height / 2))))))
                                                {
                                                    cycle->value = value;
                                                }
                                            }

                                            if (! draw_gadget_cycle (screennum, i, GADGET_MENU_DOWN, cycle->value))
                                            {
                                                printf ("gadget_event: error can't draw gadget!\n");
                                                return (NULL);
                                            }
                                        }
                                    }
#endif

                                    if ((x > cycle->menu_x && x < cycle->menu_x2) && (y > cycle->menu_y && y < cycle->menu_y2))
                                    {
                                        /* gadget was selected */

                                        cycle_menu = FALSE;
                                    }
                                }

                                /* remove all pending mouse down events */

                                wait = TRUE;

                                while (wait)
                                {
                                    while (SDL_PollEvent (&event))
                                    {
                                        if (event.type == SDL_MOUSEBUTTONUP && event.button.state == SDL_RELEASED)
                                        {
                                            wait = FALSE;
                                        }
                                    }
                                }
                                SDL_Delay (100);

                                if (! draw_gadget_cycle (screennum, i, GADGET_MENU_UP, cycle->value))
                                {
                                    printf ("gadget_event: error can't draw gadget!\n");
                                    return (NULL);
                                }

                                printf ("gadget_event: cycle gadget %i selected.\n", i);

								sp = stpushi (i, sp, sp_bottom);
								if (sp == NULL)
								{
									// error
									printf ("gadget_event: ERROR: stack corrupt!\n");
									return (NULL);
								}

								sp = stpushi (cycle->value, sp, sp_bottom);
								if (sp == NULL)
								{
									// error
									printf ("gadget_event: ERROR: stack corrupt!\n");
									return (NULL);
								}

								// push 1 = all OK!
								sp = stpushi (1, sp, sp_bottom);
								if (sp == NULL)
								{
									// error
									printf ("gadget_event: ERROR: stack corrupt!\n");
									return (NULL);
								}

                                return (sp);
                            }
                        }
                        break;

                    case GADGET_STRING:
                        string = (struct gadget_string *) screen[screennum].gadget[i].gptr;

                        if ((x > string->x && x < string->x2) && (y > string->y && y < string->y2) && string->status == GADGET_ACTIVE)
                        {
                            mouse_button = 1;
                            while (mouse_button == 1)
                            {
                                SDL_Delay (100);

                                SDL_PumpEvents ();
                                buttonmask = SDL_GetMouseState (&x, &y);

                                if (! (buttonmask & SDL_BUTTON (1)))
                                {
                                    mouse_button = 0;
                                }

                                if ((x > string->x && x < string->x2) && (y > string->y && y < string->y2))
                                {
                                    printf ("gadget_event: drawing string %i selected.\n", i);

                                    if (! event_gadget_string (screennum, i))
                                    {
                                        printf ("gadget_event: error can't draw gadget!\n");
                                        return (NULL);
                                    }
                                    else
									{
										/* gadget was selected */

										printf ("gadget_event: drawing string %i normal.\n", i);

										if (! draw_gadget_string (screennum, i, GADGET_NOT_SELECTED))
										{
											printf ("gadget_event: error can't draw gadget!\n");
											return (NULL);
										}

										sp = stpushi (i, sp, sp_bottom);
										if (sp == NULL)
										{
											// error
											printf ("gadget_event: ERROR: stack corrupt!\n");
											return (NULL);
										}

										strcpy ((char *) &data[text_address], (const char *) string->value);

										// value = dummy
										sp = stpushi (0, sp, sp_bottom);
										if (sp == NULL)
										{
											// error
											printf ("gadget_event: ERROR: stack corrupt!\n");
											return (NULL);
										}

										// push 1 = all OK!
										sp = stpushi (1, sp, sp_bottom);
										if (sp == NULL)
										{
											// error
											printf ("gadget_event: ERROR: stack corrupt!\n");
											return (NULL);
										}

										return (sp);
									}
                                }
                                else
                                {
                                    printf ("gadget_event: drawing string %i normal.\n", i);

                                    if (! draw_gadget_string (screennum, i, GADGET_NOT_SELECTED))
                                    {
                                        printf ("gadget_event: error can't draw gadget!\n");
                                        return (NULL);
                                    }
                                }
                            }

                            if (! draw_gadget_string (screennum, i, GADGET_NOT_SELECTED))
                            {
                                printf ("gadget_event: error can't draw gadget!\n");
                                return (NULL);
                            }

                            if ((x > string->x && x < string->x2) && (y > string->y && y < string->y2))
                            {
                                /* gadget was selected */

                                printf ("gadget_event: string gadget %i selected.\n", i);

								sp = stpushi (i, sp, sp_bottom);
								if (sp == NULL)
								{
									// error
									printf ("gadget_event: ERROR: stack corrupt!\n");
									return (NULL);
								}

								strcpy ((char *) &data[text_address], (const char *) string->value);

								// value = dummy
								sp = stpushi (0, sp, sp_bottom);
								if (sp == NULL)
								{
									// error
									printf ("gadget_event: ERROR: stack corrupt!\n");
									return (NULL);
								}

								// push 1 = all OK!
								sp = stpushi (1, sp, sp_bottom);
								if (sp == NULL)
								{
									// error
									printf ("gadget_event: ERROR: stack corrupt!\n");
									return (NULL);
								}

                                return (sp);
                            }
                        }
                        break;

						case GADGET_STRING_MULTILINE:
					    	string_multiline = (struct gadget_string_multiline *) screen[screennum].gadget[i].gptr;

						    if ((x > string_multiline->x && x < string_multiline->x2) && (y > string_multiline->y && y < string_multiline->y2) && string_multiline->status == GADGET_ACTIVE)
						    {
						        mouse_button = 1;
						        while (mouse_button == 1)
						        {
						            SDL_Delay (100);

						            SDL_PumpEvents ();
						            buttonmask = SDL_GetMouseState (&x, &y);

						            if (! (buttonmask & SDL_BUTTON (1)))
						            {
						                mouse_button = 0;
						            }

						            if ((x > string_multiline->x && x < string_multiline->x2) && (y > string_multiline->y && y < string_multiline->y2))
						            {
						                printf ("gadget_event: drawing string_multiline %i selected.\n", i);

						                if (! event_gadget_string_multiline (screennum, i))
						                {
						                    printf ("gadget_event: error can't draw gadget!\n");
						                    return (NULL);
						                }
						                else
						                {
						                    /* gadget was selected */

						                    printf ("gadget_event: drawing string_multiline %i normal.\n", i);

						                    if (! draw_gadget_string_multiline (screennum, i, GADGET_NOT_SELECTED))
						                    {
						                        printf ("gadget_event: error can't draw gadget!\n");
						                        return (NULL);
						                    }

						                    sp = stpushi (i, sp, sp_bottom);
						                    if (sp == NULL)
						                    {
						                        // error
						                        printf ("gadget_event: ERROR: stack corrupt!\n");
						                        return (NULL);
						                    }

						                    strcpy ((char *) &data[text_address], (const char *) string_multiline->value);

						                    // value = dummy
						                    sp = stpushi (0, sp, sp_bottom);
						                    if (sp == NULL)
						                    {
						                        // error
						                        printf ("gadget_event: ERROR: stack corrupt!\n");
						                        return (NULL);
						                    }

						                    // push 1 = all OK!
						                    sp = stpushi (1, sp, sp_bottom);
						                    if (sp == NULL)
						                    {
						                        // error
						                        printf ("gadget_event: ERROR: stack corrupt!\n");
						                        return (NULL);
						                    }

						                    return (sp);
						                }
						            }
						            else
						            {
						                printf ("gadget_event: drawing string_multiline %i normal.\n", i);

						                if (! draw_gadget_string_multiline (screennum, i, GADGET_NOT_SELECTED))
						                {
						                    printf ("gadget_event: error can't draw gadget!\n");
						                    return (NULL);
						                }
						            }
						        }

						        if (! draw_gadget_string_multiline (screennum, i, GADGET_NOT_SELECTED))
						        {
						            printf ("gadget_event: error can't draw gadget!\n");
						            return (NULL);
						        }

						        if ((x > string_multiline->x && x < string_multiline->x2) && (y > string_multiline->y && y < string_multiline->y2))
						        {
						            /* gadget was selected */

						            printf ("gadget_event: string_multiline gadget %i selected.\n", i);

						            sp = stpushi (i, sp, sp_bottom);
						            if (sp == NULL)
						            {
						                // error
						                printf ("gadget_event: ERROR: stack corrupt!\n");
						                return (NULL);
						            }

						            strcpy ((char *) &data[text_address], (const char *) string_multiline->value);

						            // value = dummy
						            sp = stpushi (0, sp, sp_bottom);
						            if (sp == NULL)
						            {
						                // error
						                printf ("gadget_event: ERROR: stack corrupt!\n");
						                return (NULL);
						            }

						            // push 1 = all OK!
						            sp = stpushi (1, sp, sp_bottom);
						            if (sp == NULL)
						            {
						                // error
						                printf ("gadget_event: ERROR: stack corrupt!\n");
						                return (NULL);
						            }

						            return (sp);
						        }
						    }
						    break;

                    case GADGET_BOX:
                        box = (struct gadget_box *) screen[screennum].gadget[i].gptr;

                        if ((x > box->x && x < box->x2) && (y > box->y && y < box->y2) && box->status == GADGET_ACTIVE)
                        {
                            mouse_button = 1;
                            while (mouse_button == 1)
                            {
                                SDL_Delay (100);

                                SDL_PumpEvents ();
                                buttonmask = SDL_GetMouseState (&x, &y);

                                if (! (buttonmask & SDL_BUTTON (1)))
                                {
                                    mouse_button = 0;
                                }
                            }

                            if ((x > box->x && x < box->x2) && (y > box->y && y < box->y2))
                            {
                                /* gadget was selected */

								if (box->selected == 0)
								{
									box->selected = 1;
								}
								else
								{
									box->selected = 0;
								}

                                printf ("gadget_event: box gadget %i selected.\n", i);

                                /* send data */

								sp = stpushi (i, sp, sp_bottom);
								if (sp == NULL)
								{
									// error
									printf ("gadget_event: ERROR: stack corrupt!\n");
									return (NULL);
								}

								//	dummy push value
								sp = stpushi (0, sp, sp_bottom);
								if (sp == NULL)
								{
									// error
									printf ("gadget_event: ERROR: stack corrupt!\n");
									return (NULL);
								}

								// push 1 = all OK!
								sp = stpushi (1, sp, sp_bottom);
								if (sp == NULL)
								{
									// error
									printf ("gadget_event: ERROR: stack corrupt!\n");
									return (NULL);
								}

                                return (sp);
                            }
                        }
                        break;

                    case GADGET_SLIDER:
                        slider = (struct gadget_slider *) screen[screennum].gadget[i].gptr;

                        if ((x > slider->x && x < slider->x2) && (y > slider->y && y < slider->y2) && slider->status == GADGET_ACTIVE)
                        {
                            mouse_button = 1;
                            while (mouse_button == 1)
                            {
                                SDL_Delay (100);

                                SDL_PumpEvents ();
                                buttonmask = SDL_GetMouseState (&x, &y);

                                if (! (buttonmask & SDL_BUTTON (1)))
                                {
                                    mouse_button = 0;
                                }
                            }

                            if ((x > slider->x && x < slider->x2) && (y > slider->y && y < slider->y2))
                            {
                                printf ("gadget_event: drawing slider %i selected.\n", i);

                                if (! event_gadget_slider (screennum, i))
                                {
                                    printf ("gadget_event: error can't draw gadget!\n");
                                    return (NULL);
                                }
                                else
                                {
                                    /* gadget was selected */

                                    printf ("gadget_event: drawing slider %i normal.\n", i);

                                    if (! draw_gadget_slider (screennum, i, GADGET_NOT_SELECTED))
                                    {
                                        printf ("gadget_event: error can't draw gadget!\n");
                                        return (NULL);
                                    }

                                    sp = stpushi (i, sp, sp_bottom);
                                    if (sp == NULL)
                                    {
                                        // error
                                        printf ("gadget_event: ERROR: stack corrupt!\n");
                                        return (NULL);
                                    }

                                    // value
                                    sp = stpushi (slider->value, sp, sp_bottom);
                                    if (sp == NULL)
                                    {
                                        // error
                                        printf ("gadget_event: ERROR: stack corrupt!\n");
                                        return (NULL);
                                    }

                                    // push 1 = all OK!
                                    sp = stpushi (1, sp, sp_bottom);
                                    if (sp == NULL)
                                    {
                                        // error
                                        printf ("gadget_event: ERROR: stack corrupt!\n");
                                        return (NULL);
                                    }

                                    return (sp);
                                }
                            }
                            else
                            {
                                printf ("gadget_event: drawing slider gadget %i normal.\n", i);

                                if (! draw_gadget_slider (screennum, i, GADGET_NOT_SELECTED))
                                {
                                    printf ("gadget_event: error can't draw gadget!\n");
                                    return (NULL);
                                }
                            }
                        }

                        if (! draw_gadget_slider (screennum, i, GADGET_NOT_SELECTED))
                        {
                            printf ("gadget_event: error can't draw gadget!\n");
                            return (NULL);
                        }

                        if ((x > slider->x && x < slider->x2) && (y > slider->y && y < slider->y2))
                        {
                            /* gadget was selected */

                            printf ("gadget_event: slider gadget %i selected.\n", i);

                            sp = stpushi (i, sp, sp_bottom);
                            if (sp == NULL)
                            {
                                // error
                                printf ("gadget_event: ERROR: stack corrupt!\n");
                                return (NULL);
                            }

                            // value
                            sp = stpushi (slider->value, sp, sp_bottom);
                            if (sp == NULL)
                            {
                                // error
                                printf ("gadget_event: ERROR: stack corrupt!\n");
                                return (NULL);
                            }

                            // push 1 = all OK!
                            sp = stpushi (1, sp, sp_bottom);
                            if (sp == NULL)
                            {
                                // error
                                printf ("gadget_event: ERROR: stack corrupt!\n");
                                return (NULL);
                            }
                            return (sp);
                        }
                        break;

                    case GADGET_SLIDER_VERT:
                        slider = (struct gadget_slider *) screen[screennum].gadget[i].gptr;

                        if ((x > slider->x && x < slider->x2) && (y > slider->y && y < slider->y2) && slider->status == GADGET_ACTIVE)
                        {
                            mouse_button = 1;
                            while (mouse_button == 1)
                            {
                                SDL_Delay (100);

                                SDL_PumpEvents ();
                                buttonmask = SDL_GetMouseState (&x, &y);

                                if (! (buttonmask & SDL_BUTTON (1)))
                                {
                                    mouse_button = 0;
                                }
                            }

                            if ((x > slider->x && x < slider->x2) && (y > slider->y && y < slider->y2))
                            {
                                printf ("gadget_event: drawing slider vert %i selected.\n", i);

                                if (! event_gadget_slider_vert (screennum, i))
                                {
                                    printf ("gadget_event: error can't draw gadget!\n");
                                    return (NULL);
                                }
                                else
                                {
                                    /* gadget was selected */

                                    printf ("gadget_event: drawing slider vert %i normal.\n", i);

                                    if (! draw_gadget_slider_vert (screennum, i, GADGET_NOT_SELECTED))
                                    {
                                        printf ("gadget_event: error can't draw gadget!\n");
                                        return (NULL);
                                    }

                                    sp = stpushi (i, sp, sp_bottom);
                                    if (sp == NULL)
                                    {
                                        // error
                                        printf ("gadget_event: ERROR: stack corrupt!\n");
                                        return (NULL);
                                    }

                                    // value
                                    sp = stpushi (slider->value, sp, sp_bottom);
                                    if (sp == NULL)
                                    {
                                        // error
                                        printf ("gadget_event: ERROR: stack corrupt!\n");
                                        return (NULL);
                                    }

                                    // push 1 = all OK!
                                    sp = stpushi (1, sp, sp_bottom);
                                    if (sp == NULL)
                                    {
                                        // error
                                        printf ("gadget_event: ERROR: stack corrupt!\n");
                                        return (NULL);
                                    }

                                    return (sp);
                                }
                            }
                            else
                            {
                                printf ("gadget_event: drawing slider gadget vert %i normal.\n", i);

                                if (! draw_gadget_slider_vert (screennum, i, GADGET_NOT_SELECTED))
                                {
                                    printf ("gadget_event: error can't draw gadget!\n");
                                    return (NULL);
                                }
                            }
                        }

                        if (! draw_gadget_slider_vert (screennum, i, GADGET_NOT_SELECTED))
                        {
                            printf ("gadget_event: error can't draw gadget!\n");
                            return (NULL);
                        }

                        if ((x > slider->x && x < slider->x2) && (y > slider->y && y < slider->y2))
                        {
                            /* gadget was selected */

                            printf ("gadget_event: slider gadget vert %i selected.\n", i);

                            sp = stpushi (i, sp, sp_bottom);
                            if (sp == NULL)
                            {
                                // error
                                printf ("gadget_event: ERROR: stack corrupt!\n");
                                return (NULL);
                            }

                            // value
                            sp = stpushi (slider->value, sp, sp_bottom);
                            if (sp == NULL)
                            {
                                // error
                                printf ("gadget_event: ERROR: stack corrupt!\n");
                                return (NULL);
                            }

                            // push 1 = all OK!
                            sp = stpushi (1, sp, sp_bottom);
                            if (sp == NULL)
                            {
                                // error
                                printf ("gadget_event: ERROR: stack corrupt!\n");
                                return (NULL);
                            }
                            return (sp);
                        }
                        break;
                }
            }
        }
    }
}


U1 *set_gadget_button (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S2 text_len;
	Sint16 x2, y2, text_x, text_y;
	int text_height, text_width, dummy;
	struct gadget_button *button;
	U1 err = 0;
	S8 gadget_index ALIGN;
	S8 x ALIGN;
	S8 y ALIGN;
	S8 text_address ALIGN;
	S8 status ALIGN;

	// gadget number, x, y, text, status

	sp = stpopi ((U1 *) &status, sp, sp_top);
	if (sp == NULL)
	{
	   // error
	   err = 1;
   }

   sp = stpopi ((U1 *) &text_address, sp, sp_top);
   if (sp == NULL)
   {
	   err = 1;
   }

   sp = stpopi ((U1 *) &y, sp, sp_top);
   if (sp == NULL)
   {
	   err = 1;
   }

   sp = stpopi ((U1 *) &x, sp, sp_top);
   if (sp == NULL)
   {
	   err = 1;
   }

   sp = stpopi ((U1 *) &gadget_index, sp, sp_top);
   if (sp == NULL)
   {
	   err = 1;
   }

   if (err == 1)
   {
	   printf ("set_gadget_button: ERROR, stack corrupt!\n");
	   return (NULL);
   }

	text_len = strlen_safe ((const char *) &data[text_address], MAXLINELEN);

	if (! screen[screennum].gadget)
	{
		printf ("set_gadget_button: error gadget list not allocated!\n");
		return (NULL);
	}

	if (gadget_index >= gadgets)
	{
		printf ("set_gadget_button: error gadget index out of range!\n");
		return (NULL);
	}

	if (! font)
	{
		printf ("set_gadget_button: error no ttf font loaded!\n");
		return (NULL);
	}

    free_gadget (screennum, gadget_index);

    screen[screennum].gadget[gadget_index].gptr = (struct gadget_button *) malloc (sizeof (struct gadget_button));
    if (screen[screennum].gadget[gadget_index].gptr == NULL)
    {
        printf ("set_gadget_button: error can't allocate structure!\n");
        return (NULL);
    }

    button = (struct gadget_button *) screen[screennum].gadget[gadget_index].gptr;

    button->text = (U1 *) malloc ((text_len + 1) * sizeof (U1));
    if (button->text == NULL)
    {
        printf ("set_gadget_button: error can't allocate text!\n");
        return (NULL);
    }

    /* get text width and height */

    text_height = TTF_FontHeight (screen[screennum].font_ttf.font);
    if (TTF_SizeText (screen[screennum].font_ttf.font, (const char *) &data[text_address], &text_width, &dummy) != 0)
    {
        printf ("set_gadget_button: error can't get text width!\n");
        return (NULL);
    }

    /* layout button */

    x2 = x + text_width + text_height * 2;
    y2 = y + text_height + (text_height / 2);

    text_x = x + text_height;
    text_y = y + (text_height / 4);

    screen[screennum].gadget[gadget_index].type = GADGET_BUTTON;
    strcpy ((char *) button->text, (const char *) &data[text_address]);
    button->status = status;
    button->x = x;
    button->y = y;
    button->x2 = x2;
    button->y2 = y2;
    button->text_x = text_x;
    button->text_y = text_y;

    if (! draw_gadget_button (screennum, gadget_index, GADGET_NOT_SELECTED))
    {
        printf ("set_gadget_button: error can't draw gadget!\n");
        return (NULL);
    }
    else
    {
        return (sp);
    }
}

U1 *set_gadget_progress_bar (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S2 text_len;
    Sint16 x2, y2, text_x, text_y;
    int text_height, text_width, dummy;
	U1 err = 0;
    S8 gadget_index ALIGN;
	S8 x ALIGN;
	S8 y ALIGN;
	S8 text_address ALIGN;
	S8 status ALIGN;
	S8 value ALIGN;
    struct gadget_progress_bar *progress;

	// gadget number, x, y, text, value, status

	sp = stpopi ((U1 *) &status, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &value, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &text_address, sp, sp_top);
	if (sp == NULL)
	{
	   err = 1;
	}

	sp = stpopi ((U1 *) &y, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &x, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &gadget_index, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	if (err == 1)
    {
 	   printf ("set_gadget_progress_bar: ERROR, stack corrupt!\n");
 	   return (NULL);
    }

    text_len = strlen_safe ((const char *) &data[text_address], MAXLINELEN);

    if (! screen[screennum].gadget)
    {
        printf ("set_gadget_progress_bar: error gadget list not allocated!\n");
        return (NULL);
    }

    if (gadget_index >= screen[screennum].gadgets)
    {
        printf ("set_gadget_progress_bar: error gadget index out of range!\n");
        return (NULL);
    }

    if (! screen[screennum].font_ttf.font)
    {
        printf ("set_gadget_progress_bar: error no ttf font loaded!\n");
        return (NULL);
    }

    free_gadget (screennum, gadget_index);

    screen[screennum].gadget[gadget_index].gptr = (struct gadget_progress_bar *) malloc (sizeof (struct gadget_progress_bar));
    if (screen[screennum].gadget[gadget_index].gptr == NULL)
    {
        printf ("set_gadget_progress_bar: error can't allocate structure!\n");
        return (NULL);
    }

    progress = (struct gadget_progress_bar *) screen[screennum].gadget[gadget_index].gptr;

    progress->text = (U1 *) malloc ((text_len + 1) * sizeof (U1));
    if (progress->text == NULL)
    {
        printf ("set_gadget_progress_bar: error can't allocate text!\n");
        return (NULL);
    }

    /* get text width and height */

    text_height = TTF_FontHeight (screen[screennum].font_ttf.font);
    if (TTF_SizeText (screen[screennum].font_ttf.font, (const char *) &data[text_address], &text_width, &dummy) != 0)
    {
        printf ("set_gadget_progress_bar: error can't get text width!\n");
        return (NULL);
    }

    /* layout progress bar */

    x2 = x + text_width + text_height * 2;
    y2 = y + text_height + (text_height / 2);

    text_x = x + text_height;
    text_y = y + (text_height / 4);

    screen[screennum].gadget[gadget_index].type = GADGET_PROGRESS_BAR;
    strcpy ((char *) progress->text, (const char *) &data[text_address]);
    progress->status = status;
    progress->x = x;
    progress->y = y;
    progress->x2 = x2;
    progress->y2 = y2;
    progress->text_x = text_x;
    progress->text_y = text_y;
	progress->value = value;

	printf ("progress_bar value: %lli\n", progress->value);

    if (! draw_gadget_progress_bar (screennum, gadget_index, GADGET_NOT_SELECTED))
    {
        printf ("set_gadget_progress_bar: error can't draw gadget!\n");
        return (NULL);
    }
    else
    {
        return (sp);
    }
}

U1 *set_gadget_checkbox (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S2 text_len;
    Sint16 x2, y2, text_x, text_y;
    int text_height, text_width, dummy;
    struct gadget_checkbox *checkbox;
	S8 gadget_index ALIGN;
	S8 x ALIGN;
	S8 y ALIGN;
	S8 text_address ALIGN;
	S8 status ALIGN;
	S8 value ALIGN;
	U1 err = 0;

	// gadget number, x, y, text, value, status

	sp = stpopi ((U1 *) &status, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &value, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &text_address, sp, sp_top);
	if (sp == NULL)
	{
	   err = 1;
	}

	sp = stpopi ((U1 *) &y, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &x, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &gadget_index, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	if (err == 1)
    {
 	   printf ("set_gadget_progress_bar: ERROR, stack corrupt!\n");
 	   return (NULL);
    }

    text_len = strlen_safe ((const char*) &data[text_address], MAXLINELEN);

    if (! screen[screennum].gadget)
    {
        printf ("set_gadget_checkbox: error gadget list not allocated!\n");
        return (NULL);
    }

    if (gadget_index >= screen[screennum].gadgets)
    {
        printf ("set_gadget_checkbox: error gadget index out of range!\n");
        return (NULL);
    }

    if (! screen[screennum].font_ttf.font)
    {
        printf ("set_gadget_checkbox: error no ttf font loaded!\n");
        return (NULL);
    }

    free_gadget (screennum, gadget_index);

    screen[screennum].gadget[gadget_index].gptr = (struct gadget_checkbox *) malloc (sizeof (struct gadget_checkbox));
    if (screen[screennum].gadget[gadget_index].gptr == NULL)
    {
        printf ("set_gadget_checkbox: error can't allocate structure!\n");
        return (NULL);
    }

    checkbox = (struct gadget_checkbox *) screen[screennum].gadget[gadget_index].gptr;

    checkbox->text = (U1 *) malloc ((text_len + 1) * sizeof (U1));
    if (checkbox->text == NULL)
    {
        printf ("set_gadget_checkbox: error can't allocate text!\n");
        return (NULL);
    }

    /* get text width and height */

    text_height = TTF_FontHeight (screen[screennum].font_ttf.font);
    if (TTF_SizeText (screen[screennum].font_ttf.font, (const char *) &data[text_address], &text_width, &dummy) != 0)
    {
        printf ("set_gadget_checkbox: error can't get text width!\n");
        return (NULL);
    }

    /* layout checkbox */

    x2 = x + text_height + (text_height / 2);
    y2 = y + text_height + (text_height / 2);

    text_x = x2 + (text_height + (text_height / 2)) / 2;
    text_y = y + (text_height / 4);

    screen[screennum].gadget[gadget_index].type = GADGET_CHECKBOX;
    strcpy ((char *) checkbox->text, (const char *) &data[text_address]);
    checkbox->status = status;
    checkbox->value = value;
    checkbox->x = x;
    checkbox->y = y;
    checkbox->x2 = x2;
    checkbox->y2 = y2;
    checkbox->text_x = text_x;
    checkbox->text_y = text_y;
    checkbox->text_x2 = text_x + text_width;
    checkbox->text_y2 = text_y + text_height;

    if (! draw_gadget_checkbox (screennum, gadget_index, GADGET_NOT_SELECTED, value))
    {
        printf ("set_gadget_checkbox: error can't draw gadget!\n");
        return (NULL);
    }
    else
    {
        return (sp);
    }
}

U1 *set_gadget_cycle (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S2 text_len;
	// S2 max_text_len = 0;
    Sint16 x2, y2, text_x, text_y, menu_x, menu_y, menu_x2, menu_y2, menu_height;
    int text_height, text_width, max_text_width = 0, dummy;
    U2 i;
    struct gadget_cycle *cycle;

	U1 err = 0;
	S8 gadget_index ALIGN;
	S8 x ALIGN;
	S8 y ALIGN;
	S8 max_texts ALIGN;
	S8 text_address ALIGN;
	S8 status ALIGN;
	S8 value ALIGN;

	// x, y, text, status, value, text_address, ..., max_texts, gadget number

	sp = stpopi ((U1 *) &gadget_index, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &max_texts, sp, sp_top);
	if (sp == NULL)
	{
		printf ("set_gadget_cycle: ERROR, stack corrupt!\n");
 	   	return (NULL);
   	}

	if (! screen[screennum].gadget)
    {
        printf ("set_gadget_cycle: error gadget list not allocated!\n");
        return (NULL);
    }

    if (gadget_index >= screen[screennum].gadgets)
    {
        printf ("set_gadget_cycle: error gadget index out of range!\n");
        return (NULL);
    }

    if (! screen[screennum].font_ttf.font)
    {
        printf ("set_gadget_cycle: error no ttf font loaded!\n");
        return (NULL);
    }

    free_gadget (screennum, gadget_index);

    screen[screennum].gadget[gadget_index].gptr = (struct gadget_cycle *) malloc (sizeof (struct gadget_cycle));
    if (screen[screennum].gadget[gadget_index].gptr == NULL)
    {
        printf ("set_gadget_cycle: error can't allocate structure!\n");
        return (NULL);
    }

    cycle = (struct gadget_cycle *) screen[screennum].gadget[gadget_index].gptr;

    /* copy menu text list */
	cycle->text = (U1 **) malloc (max_texts * sizeof (U1 *));
	if (cycle->text == NULL)
	{
		printf ("set_gadget_cycle: error can't allocate menu text list!\n");
		return (FALSE);
	}

    for (i = 0; i < max_texts; i++)
    {
		sp = stpopi ((U1 *) &text_address, sp, sp_top);
		if (sp == NULL)
		{
		   // error
		   printf ("set_gadget_cycle: ERROR stack error!\n");
		   return (NULL);
	   	}

        text_len = strlen_safe ((const char *) &data[text_address], MAXLINELEN);

        cycle->text[i] = (U1 *) malloc ((text_len + 1) * sizeof (U1));
        if (cycle->text[i] == NULL)
        {
            printf ("set_gadget_cycle: error can't allocate menu text!\n");
            return (NULL);
        }

        strcpy ((char *) cycle->text[i], (const char *) &data[text_address]);

        if (TTF_SizeText (screen[screennum].font_ttf.font, (const char *) cycle->text[i], &text_width, &dummy) != 0)
        {
            printf ("set_gadget_cycle: error can't get text width!\n");
            return (NULL);
        }

        if (text_width > max_text_width)
        {
            max_text_width = text_width;
        }
    }

    text_height = TTF_FontHeight (screen[screennum].font_ttf.font);

	// -------------------------------------------------------

	sp = stpopi ((U1 *) &status, sp, sp_top);
	if (sp == NULL)
	{
	   // error
	   err = 1;
   	}

	sp = stpopi ((U1 *) &value, sp, sp_top);
	if (sp == NULL)
	{
	   // error
	   err = 1;
   	}

   sp = stpopi ((U1 *) &text_address, sp, sp_top);
   if (sp == NULL)
   {
	   err = 1;
   }

   sp = stpopi ((U1 *) &y, sp, sp_top);
   if (sp == NULL)
   {
	   err = 1;
   }

   sp = stpopi ((U1 *) &x, sp, sp_top);
   if (sp == NULL)
   {
	   err = 1;
   }

   if (err == 1)
   {
	   printf ("set_gadget_cycle: ERROR, stack corrupt!\n");
	   return (NULL);
   }

    /* layout cycle */

    x2 = x + max_text_width + text_height * 2 + (text_height + (text_height / 2));
    y2 = y + text_height + (text_height / 2);

    text_x = x + text_height;
    text_y = y + (text_height / 4);

    menu_height = max_texts * (text_height + (text_height / 2));

    menu_x = x;

    if (y + menu_height <= screen[screennum].height - 1)
    {
        menu_y = y;
    }
    else
    {
        menu_y = y2 - menu_height;
    }

    menu_x2 = x2;
    menu_y2 = menu_y + menu_height;

    screen[screennum].gadget[gadget_index].type = GADGET_CYCLE;
    cycle->status = status;
    cycle->value = value;
    cycle->menu_entries = max_texts;
    cycle->x = x;
    cycle->y = y;
    cycle->x2 = x2;
    cycle->y2 = y2;
    cycle->text_x = text_x;
    cycle->text_y = text_y;
    cycle->text_height = text_height;
    cycle->menu_x = menu_x;
    cycle->menu_y = menu_y;
    cycle->menu_x2 = menu_x2;
    cycle->menu_y2 = menu_y2;
    cycle->arrow_x = x2 - (text_height + (text_height / 2));
    cycle->arrow_y = y;
    cycle->menu = FALSE;

    if (! draw_gadget_cycle (screennum, gadget_index, GADGET_NOT_SELECTED, value))
    {
        printf ("set_gadget_cycle: error can't draw gadget!\n");
        return (NULL);
    }
    else
    {
        return (sp);
    }
}

U1 *set_gadget_string (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S2 text_len;
    Sint16 x2, y2, text_x, text_y, input_x, input_y;
    int text_height, text_width, char_width, dummy;
    struct gadget_string *string;
	S8 gadget_index ALIGN;
	S8 x ALIGN;
	S8 y ALIGN;
	S8 text_address ALIGN;
	S8 status ALIGN;
	S8 value_address ALIGN;
	S8 string_visible_len ALIGN;
	U1 err = 0;

	// gadget number, x, y, textstr, valuestr, visible_len, status

	sp = stpopi ((U1 *) &status, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &string_visible_len, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &value_address, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &text_address, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &y, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &x, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &gadget_index, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	if (err == 1)
    {
 	   printf ("set_gadget_string: ERROR, stack corrupt!\n");
 	   return (NULL);
    }

    if (strlen_safe ((const char *) &data[value_address], MAXLINELEN) > screen[screennum].gadget_string_string_len)
    {
        printf ("set_gadget_string: error string value overflow!\n");
        return (NULL);
    }

    text_len = strlen_safe ((const char *) &data[text_address], MAXLINELEN);

    if (! screen[screennum].gadget)
    {
        printf ("set_gadget_string: error gadget list not allocated!\n");
        return (NULL);
    }

    if (gadget_index >= screen[screennum].gadgets)
    {
        printf ("set_gadget_string: error gadget index out of range!\n");
        return (NULL);
    }

    if (! screen[screennum].font_ttf.font)
    {
        printf ("set_gadget_string: error no ttf font loaded!\n");
        return (NULL);
    }

    free_gadget (screennum, gadget_index);

    screen[screennum].gadget[gadget_index].gptr = (struct gadget_string *) malloc (sizeof (struct gadget_string));
    if (screen[screennum].gadget[gadget_index].gptr == NULL)
    {
        printf ("set_gadget_string: error can't allocate structure!\n");
        return (NULL);
    }

    string = (struct gadget_string *) screen[screennum].gadget[gadget_index].gptr;

    /* allocate space for gadget title */

    string->text = (U1 *) malloc ((text_len + 1) * sizeof (U1));
    if (string->text == NULL)
    {
        printf ("set_gadget_string: error can't allocate text!\n");
        return (NULL);
    }

    /* allocate space for input string */

    string->value = (U1 *) malloc ((screen[screennum].gadget_string_string_len + 1) * sizeof (U1));
    if (string->value == NULL)
    {
        printf ("set_gadget_string: error can't allocate value!\n");
        return (NULL);
    }

    /* allocate space for display-buffer string */

    string->display = (U1 *) malloc ((screen[screennum].gadget_string_string_len + 1) * sizeof (U1));
    if (string->display == NULL)
    {
        printf ("set_gadget_string: error can't allocate display!\n");
        return (NULL);
    }


    /* get text width and height */

    text_height = TTF_FontHeight (screen[screennum].font_ttf.font);
    if (TTF_SizeText (screen[screennum].font_ttf.font, (const char *) &data[text_address], &text_width, &dummy) != 0)
    {
        printf ("set_gadget_string: error can't get text width!\n");
        return (NULL);
    }

    /* get width of one char */

    if (TTF_SizeText (screen[screennum].font_ttf.font, "W", &char_width, &dummy) != 0)
    {
        printf ("set_gadget_string: error can't get char width!\n");
        return (NULL);
    }

    /* layout string */

    text_x = x;
    text_y = y + (text_height / 4);

    x = text_x + text_width + text_height;
    x2 = (x + (string_visible_len + 1) * char_width) + (char_width * 2);
    y2 = y + text_height + (text_height / 2);

    input_x = x + char_width;
    input_y = text_y;

    screen[screennum].gadget[gadget_index].type = GADGET_STRING;
    strcpy ((char *) string->text, (const char *) &data[text_address]);
    strcpy ((char *) string->value, (const char *) &data[value_address]);
    string->status = status;
    string->x = x;
    string->y = y;
    string->x2 = x2;
    string->y2 = y2;
    string->text_x = text_x;
    string->text_y = text_y;
    string->text_x2 = text_x + text_width;
    string->text_y2 = text_y + text_height;

    string->input_x = input_x;
    string->input_y = input_y;
    string->string_len = screen[screennum].gadget_string_string_len;
    string->visible_len = string_visible_len;

    string->cursor_width = char_width;
    string->cursor_height = text_height;
    string->cursor_pos = -1;
    string->insert_pos = -1;

	string->passwd = 0;		// show input chars, can be set with "set_gadget_string_passwd" function

    printf ("set_gadget_string: status = %i\n", string->status);

    if (! draw_gadget_string (screennum, gadget_index, GADGET_NOT_SELECTED))
    {
        printf ("set_gadget_string: error can't draw gadget!\n");
        return (NULL);
    }
    else
    {
        return (sp);
    }
}

U1 *set_gadget_string_multiline (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// set a multi lines text edit gadget
    S2 text_len;
    Sint16 x2, y2, text_x, text_y, input_x, input_y;
    int text_height, text_width, char_width, dummy;
    struct gadget_string_multiline *string;
	S8 gadget_index ALIGN;
	S8 x ALIGN;
	S8 y ALIGN;
	S8 text_address ALIGN;
	S8 status ALIGN;
	S8 value_address ALIGN;
	S8 string_visible_len ALIGN;
	S8 text_lines ALIGN;
	U1 err = 0;

	// gadget number, x, y, textstr, valuestr, visible_len, status

	sp = stpopi ((U1 *) &status, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &text_lines, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &string_visible_len, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &value_address, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &text_address, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &y, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &x, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &gadget_index, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	if (err == 1)
    {
 	   printf ("set_gadget_string_multiline: ERROR, stack corrupt!\n");
 	   return (NULL);
    }

/*
    if (strlen_safe ((const char *) &data[value_address], MAXLINELEN) > screen[screennum].gadget_string_string_len)
    {
        printf ("set_gadget_string_multiline: error string value overflow!\n");
        return (NULL);
    }
*/

    text_len = strlen_safe ((const char *) &data[text_address], MAXLINELEN);

    if (! screen[screennum].gadget)
    {
        printf ("set_gadget_string_multiline: error gadget list not allocated!\n");
        return (NULL);
    }

    if (gadget_index >= screen[screennum].gadgets)
    {
        printf ("set_gadget_string_multiline: error gadget index out of range!\n");
        return (NULL);
    }

    if (! screen[screennum].font_ttf.font)
    {
        printf ("set_gadget_string_multiline: error no ttf font loaded!\n");
        return (NULL);
    }

    free_gadget (screennum, gadget_index);

    screen[screennum].gadget[gadget_index].gptr = (struct gadget_string_multiline *) malloc (sizeof (struct gadget_string_multiline));
    if (screen[screennum].gadget[gadget_index].gptr == NULL)
    {
        printf ("set_gadget_string_multiline: error can't allocate structure!\n");
        return (NULL);
    }

    string = (struct gadget_string_multiline *) screen[screennum].gadget[gadget_index].gptr;

    /* allocate space for gadget title */

    string->text = (U1 *) malloc ((text_len + 1) * sizeof (U1));
    if (string->text == NULL)
    {
        printf ("set_gadget_string_multiline: error can't allocate text!\n");
        return (NULL);
    }

    /* allocate space for input string */

    string->value = (U1 *) malloc (((string_visible_len + 1) * text_lines) * sizeof (U1));
    if (string->value == NULL)
    {
        printf ("set_gadget_string_multiline: error can't allocate value!\n");
        return (NULL);
    }

	// copy value string data to gadget string */
	{
		S8 i ALIGN;
		S8 max ALIGN;

		max = text_lines * string_visible_len;

		for (i = 0; i < max; i++)
		{
			string->value[i] = data[value_address + i];
		}
	}

    /* allocate space for display-buffer string */

    string->display = (U1 *) malloc (((string_visible_len + 1) * text_lines) * sizeof (U1));
    if (string->display == NULL)
    {
        printf ("set_gadget_string_multiline: error can't allocate display!\n");
        return (NULL);
    }


    /* get text width and height */

    text_height = TTF_FontHeight (screen[screennum].font_ttf.font);
    if (TTF_SizeText (screen[screennum].font_ttf.font, (const char *) &data[text_address], &text_width, &dummy) != 0)
    {
        printf ("set_gadget_string_multiline: error can't get text width!\n");
        return (NULL);
    }

    /* get width of one char */

    if (TTF_SizeText (screen[screennum].font_ttf.font, "W", &char_width, &dummy) != 0)
    {
        printf ("set_gadget_string_multiline: error can't get char width!\n");
        return (NULL);
    }

    /* layout string */

    text_x = x;
    text_y = y + (text_height / 4);

    x = text_x + text_width + text_height;
    x2 = (x + (string_visible_len + 1) * char_width) + (char_width * 2);
    y2 = y + (text_height * text_lines)  + (text_height / 2);

    input_x = x + char_width;
    input_y = text_y;

    screen[screennum].gadget[gadget_index].type = GADGET_STRING_MULTILINE;
    strcpy ((char *) string->text, (const char *) &data[text_address]);
    strcpy ((char *) string->value, (const char *) &data[value_address]);
    string->status = status;
    string->x = x;
    string->y = y;
    string->x2 = x2;
    string->y2 = y2;
    string->text_x = text_x;
    string->text_y = text_y;
    string->text_x2 = text_x + text_width;
    string->text_y2 = text_y + text_height;

    string->input_x = input_x;
    string->input_y = input_y;
    string->string_len = screen[screennum].gadget_string_string_len;
    string->visible_len = string_visible_len;
	string->text_lines = text_lines;

    string->cursor_width = char_width;
    string->cursor_height = text_height;
    string->cursor_pos_x = 0;
	string->cursor_pos_y = 0;
    string->insert_pos = 0;
	string->text_height = text_height;
	string->text_width = text_width;

	string->passwd = 0;		// show input chars, can be set with "set_gadget_string_multiline_passwd" function

    printf ("set_gadget_string_multiline: status = %i\n", string->status);

    if (! draw_gadget_string_multiline (screennum, gadget_index, GADGET_NOT_SELECTED))
    {
        printf ("set_gadget_string_multiline: error can't draw gadget!\n");
        return (NULL);
    }
    else
    {
        return (sp);
    }
}

U1 *set_gadget_box (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    /* set a gadget box without a drawed box, to set a active area */

    struct gadget_box *box;
    S8 gadget_index ALIGN;
	S8 x ALIGN;
	S8 y ALIGN;
	S8 x2 ALIGN;
	S8 y2 ALIGN;
	S8 status ALIGN;
	U1 err = 0;

	sp = stpopi ((U1 *) &status, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &y2, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &x2, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &y, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &x, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &gadget_index, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	if (err == 1)
    {
 	   printf ("set_gadget_box: ERROR, stack corrupt!\n");
 	   return (NULL);
   }

    if (! screen[screennum].gadget)
    {
        printf ("set_gadget_box: error gadget list not allocated!\n");
        return (NULL);
    }

    if (gadget_index >= screen[screennum].gadgets)
    {
        printf ("set_gadget_box: error gadget index out of range!\n");
        return (NULL);
    }

    free_gadget (screennum, gadget_index);

    screen[screennum].gadget[gadget_index].gptr = (struct gadget_box *) malloc (sizeof (struct gadget_box));
    if (screen[screennum].gadget[gadget_index].gptr == NULL)
    {
        printf ("set_gadget_box: error can't allocate structure!\n");
        return (NULL);
    }

    box = (struct gadget_box *) screen[screennum].gadget[gadget_index].gptr;

    screen[screennum].gadget[gadget_index].type = GADGET_BOX;
    box->status = status;
	box->selected = 0;
    box->x = x;
    box->y = y;
    box->x2 = x2;
    box->y2 = y2;

    return (sp);
}

// box grid gadgets ===========================================================
U1 *set_gadget_box_grid (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// draw a x * y user defined grid by given color

    struct gadget_box *box;
    S8 gadget_index ALIGN;
	S8 x ALIGN;
	S8 y ALIGN;
	S8 x_start ALIGN;
	S8 y_start ALIGN;
	S8 x2 ALIGN;
	S8 y2 ALIGN;
	S8 x_tile_width ALIGN;	// tile width and height in pixels
	S8 y_tile_height ALIGN;
	S8 x_tiles ALIGN;		// number of drawed tiles
	S8 y_tiles ALIGN;
	S8 x_tiles_ind ALIGN;
	S8 y_tiles_ind ALIGN;
	U1 r, g, b, alpha; 			// colors
	S8 status ALIGN;
	S8 selected ALIGN;
	U1 err = 0;

	sp = stpopi ((U1 *) &status, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopb (&alpha, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopb (&b, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopb (&g, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopb (&r, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &y_tile_height, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &x_tile_width, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &y_tiles, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &x_tiles, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &y_start, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &x_start, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &selected, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &gadget_index, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	if (err == 1)
    {
 	   printf ("set_gadget_box_grid: ERROR, stack corrupt!\n");
 	   return (NULL);
   	}

    if (! screen[screennum].gadget)
    {
        printf ("set_gadget_box_grid: error gadget list not allocated!\n");
        return (NULL);
    }

	// printf ("set_gadget_box_grid: gadget_index: %lli\n", gadget_index);

	y = y_start;
	for (y_tiles_ind = 0; y_tiles_ind < y_tiles; y_tiles_ind++)
	{
		x = x_start;
		for (x_tiles_ind = 0; x_tiles_ind < x_tiles; x_tiles_ind++)
		{
			x2 = x + x_tile_width;
			y2 = y + y_tile_height;
			rectangleRGBA (renderer, x, y, x2, y2, r, g, b, alpha);

			// save gadget box data using gadget_index as upper left corner, and set it horizontal to 1, 2, 3...

			if (gadget_index >= screen[screennum].gadgets)
			{
				printf ("set_gadget_box_grid: error gadget index out of range: %lli\n", gadget_index);
				return (NULL);
			}

			free_gadget (screennum, gadget_index);

			screen[screennum].gadget[gadget_index].gptr = (struct gadget_box *) malloc (sizeof (struct gadget_box));
			if (screen[screennum].gadget[gadget_index].gptr == NULL)
			{
				printf ("set_gadget_box_grid: error can't allocate structure!\n");
				return (NULL);
			}

			box = (struct gadget_box *) screen[screennum].gadget[gadget_index].gptr;

			screen[screennum].gadget[gadget_index].type = GADGET_BOX;
			box->status = status;
			box->selected = selected;
			box->x = x;
			box->y = y;
			box->x2 = x2;
			box->y2 = y2;

			gadget_index++;
			x = x + x_tile_width;
		}
		y = y + y_tile_height;
	}
	return (sp);
}

U1 *set_gadget_slider (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S2 text_len;
    Sint16 x2, y2, text_x, text_y;
    int text_height, text_width, dummy;
	U1 err = 0;
    S8 gadget_index ALIGN;
	S8 x ALIGN;
	S8 y ALIGN;
	S8 text_address ALIGN;
	S8 status ALIGN;
	S8 value ALIGN;
    S8 min ALIGN;
    S8 max ALIGN;
    struct gadget_slider *slider;

	// gadget number, x, y, text, value, min, max, status

	sp = stpopi ((U1 *) &status, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

    sp = stpopi ((U1 *) &max, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

    sp = stpopi ((U1 *) &min, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &value, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &text_address, sp, sp_top);
	if (sp == NULL)
	{
	   err = 1;
	}

	sp = stpopi ((U1 *) &y, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &x, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &gadget_index, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	if (err == 1)
    {
 	   printf ("set_gadget_slider: ERROR, stack corrupt!\n");
 	   return (NULL);
    }

    text_len = strlen_safe ((const char *) &data[text_address], MAXLINELEN);

    if (! screen[screennum].gadget)
    {
        printf ("set_gadget_slider: error gadget list not allocated!\n");
        return (NULL);
    }

    if (gadget_index >= screen[screennum].gadgets)
    {
        printf ("set_gadget_slider: error gadget index out of range!\n");
        return (NULL);
    }

    if (! screen[screennum].font_ttf.font)
    {
        printf ("set_gadget_slider: error no ttf font loaded!\n");
        return (NULL);
    }

    free_gadget (screennum, gadget_index);

    screen[screennum].gadget[gadget_index].gptr = (struct gadget_slider *) malloc (sizeof (struct gadget_slider));
    if (screen[screennum].gadget[gadget_index].gptr == NULL)
    {
        printf ("set_gadget_slider: error can't allocate structure!\n");
        return (NULL);
    }

    slider = (struct gadget_slider *) screen[screennum].gadget[gadget_index].gptr;

    slider->text = (U1 *) malloc ((text_len + 1) * sizeof (U1));
    if (slider->text == NULL)
    {
        printf ("set_gadget_slider: error can't allocate text!\n");
        return (NULL);
    }

    /* get text width and height */

    text_height = TTF_FontHeight (screen[screennum].font_ttf.font);
    if (TTF_SizeText (screen[screennum].font_ttf.font, (const char *) &data[text_address], &text_width, &dummy) != 0)
    {
        printf ("set_gadget_slider: error can't get text width!\n");
        return (NULL);
    }

    // sense check
    if (value < min || value > max)
    {
        printf ("set_gagdet_slider: error value not in limits!\n");
        return (NULL);
    }

    /* layout slider */

    x2 = x + text_width + text_height * 2;
    y2 = y + text_height + (text_height / 2);

    text_x = x + text_height;
    text_y = y + (text_height / 4);

    screen[screennum].gadget[gadget_index].type = GADGET_SLIDER;
    strcpy ((char *) slider->text, (const char *) &data[text_address]);
    slider->status = status;
    slider->x = x;
    slider->y = y;
    slider->x2 = x2;
    slider->y2 = y2;
    slider->text_x = text_x;
    slider->text_y = text_y;
    slider->value = value;
    slider->min = min;
    slider->max = max;

	printf ("slider value: %lli\n", slider->value);

    if (! draw_gadget_slider (screennum, gadget_index, GADGET_NOT_SELECTED))
    {
        printf ("set_gadget_slider: error can't draw gadget!\n");
        return (NULL);
    }
    else
    {
        return (sp);
    }
}

U1 *set_gadget_slider_vert (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    Sint16 x2, y2, text_x, text_y;
	U1 err = 0;
    S8 gadget_index ALIGN;
	S8 x ALIGN;
	S8 y ALIGN;
	S8 status ALIGN;
	S8 value ALIGN;
    S8 min ALIGN;
    S8 max ALIGN;
    S8 width ALIGN;
    S8 height ALIGN;
    struct gadget_slider *slider;

	// gadget number, x, y, width, height, value, min, max, status

	sp = stpopi ((U1 *) &status, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

    sp = stpopi ((U1 *) &max, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

    sp = stpopi ((U1 *) &min, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &value, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

    sp = stpopi ((U1 *) &height, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

    sp = stpopi ((U1 *) &width, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &y, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &x, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &gadget_index, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	if (err == 1)
    {
 	   printf ("set_gadget_slider_vert: ERROR, stack corrupt!\n");
 	   return (NULL);
    }

    if (! screen[screennum].gadget)
    {
        printf ("set_gadget_slider_vert: error gadget list not allocated!\n");
        return (NULL);
    }

    if (gadget_index >= screen[screennum].gadgets)
    {
        printf ("set_gadget_slider_vert: error gadget index out of range!\n");
        return (NULL);
    }

    if (! screen[screennum].font_ttf.font)
    {
        printf ("set_gadget_slider_vert: error no ttf font loaded!\n");
        return (NULL);
    }

    free_gadget (screennum, gadget_index);

    screen[screennum].gadget[gadget_index].gptr = (struct gadget_slider *) malloc (sizeof (struct gadget_slider));
    if (screen[screennum].gadget[gadget_index].gptr == NULL)
    {
        printf ("set_gadget_slider_vert: error can't allocate structure!\n");
        return (NULL);
    }

    slider = (struct gadget_slider *) screen[screennum].gadget[gadget_index].gptr;

    // sense check
    if (value < min || value > max)
    {
        printf ("set_gagdet_slider_vert: error value not in limits!\n");
        return (NULL);
    }

    /* layout slider */

    x2 = x + width;
    y2 = y + height;

    text_x = 0;
    text_y = 0;

    screen[screennum].gadget[gadget_index].type = GADGET_SLIDER_VERT;
    slider->status = status;
    slider->x = x;
    slider->y = y;
    slider->x2 = x2;
    slider->y2 = y2;
    slider->text_x = text_x;
    slider->text_y = text_y;
    slider->value = value;
    slider->min = min;
    slider->max = max;

    if (! draw_gadget_slider_vert (screennum, gadget_index, GADGET_NOT_SELECTED))
    {
        printf ("set_gadget_slider_vert: error can't draw gadget!\n");
        return (NULL);
    }
    else
    {
        return (sp);
    }
}

U1 *change_gadget_checkbox (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    /* change value and status (active / inactive) */

    S8 gadget_index ALIGN;
	S8 value ALIGN;
	S8 status ALIGN;
	U1 err = 0;

    struct gadget_checkbox *checkbox;

	sp = stpopi ((U1 *) &status, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &value, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &gadget_index, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	if (err == 1)
	{
	   printf ("change_gadget_checkbox: ERROR, stack corrupt!\n");
	   return (NULL);
	}

    if (! screen[screennum].gadget)
    {
        printf ("change_gadget_checkbox: error gadget list not allocated!\n");
        return (NULL);
    }

    if (screen[screennum].gadget[gadget_index].gptr == NULL)
    {
        printf ("change_gadget_checkbox: error gadget %lli not allocated!\n", gadget_index);
        return (NULL);
    }

    if (screen[screennum].gadget[gadget_index].type != GADGET_CHECKBOX)
    {
        printf ("change_gadget_checkbox: error %lli not a checkbox gadget!\n", gadget_index);
        return (NULL);
    }

    checkbox = (struct gadget_checkbox *) screen[screennum].gadget[gadget_index].gptr;

    checkbox->value = value;
    checkbox->status = status;

    if (! draw_gadget_checkbox (screennum, gadget_index, GADGET_NOT_SELECTED, screen[screennum].gadget_int_value))
    {
        printf ("change_gadget_checkbox: error can't draw gadget!\n");
        return (NULL);
    }
    else
    {
        return (sp);
    }
}

U1 *change_gadget_cycle (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    /* change value and status (active / inactive) */

 	S8 gadget_index ALIGN;
	S8 value ALIGN;
	S8 status ALIGN;
	U1 err = 0;

    struct gadget_cycle *cycle;

	sp = stpopi ((U1 *) &status, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &value, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &gadget_index, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	if (err == 1)
	{
	   printf ("change_gadget_cycle: ERROR, stack corrupt!\n");
	   return (NULL);
	}

    if (! screen[screennum].gadget)
    {
        printf ("change_gadget_cycle: error gadget list not allocated!\n");
        return (NULL);
    }

    if (screen[screennum].gadget[gadget_index].gptr == NULL)
    {
        printf ("change_gadget_cycle: error gadget %lli not allocated!\n", gadget_index);
        return (NULL);
    }

    if (screen[screennum].gadget[gadget_index].type != GADGET_CYCLE)
    {
        printf ("change_gadget_cycle: error %lli not a cycle gadget!\n", gadget_index);
        return (NULL);
    }

    cycle = (struct gadget_cycle *) screen[screennum].gadget[gadget_index].gptr;

    cycle->value = value;
    cycle->status = status;

    if (! draw_gadget_cycle (screennum, gadget_index, GADGET_NOT_SELECTED, screen[screennum].gadget_int_value))
    {
        printf ("change_gadget_cycle: error can't draw gadget!\n");
        return (NULL);
    }
    else
    {
        return (sp);
    }
}

U1 *change_gadget_string (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    /* change value and status (active / inactive) */

    S8 gadget_index ALIGN;
	S8 text_address ALIGN;
	S8 status ALIGN;
	U1 err = 0;

	sp = stpopi ((U1 *) &status, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &text_address, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &gadget_index, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	if (err == 1)
	{
	   printf ("change_gadget_string: ERROR, stack corrupt!\n");
	   return (NULL);
	}

    struct gadget_string *string;

    if (! screen[screennum].gadget)
    {
        printf ("change_gadget_string: error gadget list not allocated!\n");
        return (NULL);
    }

    if (screen[screennum].gadget[gadget_index].gptr == NULL)
    {
        printf ("change_gadget_string: error gadget %lli not allocated!\n", gadget_index);
        return (NULL);
    }

    if (screen[screennum].gadget[gadget_index].type != GADGET_STRING)
    {
        printf ("change_gadget_string: error %lli not a string gadget!\n", gadget_index);
        return (NULL);
    }

    string = (struct gadget_string *) screen[screennum].gadget[gadget_index].gptr;

    if (strlen_safe((const char *) &data[text_address], MAXLINELEN) > string->string_len)
    {
        printf ("change_gadget_string: error string value overflow!\n");
        return (NULL);
    }

    strcpy ((char *) string->value, (const char *) &data[text_address]);
    string->status = status;

    if (! draw_gadget_string (screennum, gadget_index, GADGET_NOT_SELECTED))
    {
        printf ("change_gadget_string: error can't draw gadget!\n");
        return (NULL);
    }
    else
    {
        return (sp);
    }
}

U1 *change_gadget_string_multiline (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    /* change value and status (active / inactive) */

    S8 gadget_index ALIGN;
	S8 text_address ALIGN;
	S8 status ALIGN;
	U1 err = 0;

	sp = stpopi ((U1 *) &status, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &text_address, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &gadget_index, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	if (err == 1)
	{
	   printf ("change_gadget_string_multiline: ERROR, stack corrupt!\n");
	   return (NULL);
	}

    struct gadget_string_multiline *string;

    if (! screen[screennum].gadget)
    {
        printf ("change_gadget_string_multiline: error gadget list not allocated!\n");
        return (NULL);
    }

    if (screen[screennum].gadget[gadget_index].gptr == NULL)
    {
        printf ("change_gadget_string_multiline: error gadget %lli not allocated!\n", gadget_index);
        return (NULL);
    }

    if (screen[screennum].gadget[gadget_index].type != GADGET_STRING_MULTILINE)
    {
        printf ("change_gadget_string_multiline: error %lli not a string gadget!\n", gadget_index);
        return (NULL);
    }

    string = (struct gadget_string_multiline *) screen[screennum].gadget[gadget_index].gptr;

	if (strlen_safe ((const char *) &data[text_address], (string->visible_len * string->text_lines)) > (string->visible_len * string->text_lines))
	{
        printf ("change_gadget_string_multiline: error string value overflow!\n");
        return (NULL);
    }

    strcpy ((char *) string->value, (const char *) &data[text_address]);
    string->status = status;

    if (! draw_gadget_string (screennum, gadget_index, GADGET_NOT_SELECTED))
    {
        printf ("change_gadget_string_multiline: error can't draw gadget!\n");
        return (NULL);
    }
    else
    {
        return (sp);
    }
}

U1 *get_gadget_string_multiline (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 gadget_index ALIGN;
	S8 text_address ALIGN;
	U1 err = 0;

	U1 ch;
    S8 string_y ALIGN;
    S8 string_x ALIGN;
    S8 string_x_end ALIGN;
	S8 ind ALIGN;
	S8 i ALIGN;

	struct gadget_string_multiline *string;

	sp = stpopi ((U1 *) &text_address, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &gadget_index, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	if (err == 1)
	{
	   printf ("get_gadget_string_multiline: ERROR, stack corrupt!\n");
	   return (NULL);
	}

	if (! screen[screennum].gadget)
    {
        printf ("get_gadget_string_multiline: error gadget list not allocated!\n");
        return (NULL);
    }

    if (screen[screennum].gadget[gadget_index].gptr == NULL)
    {
        printf ("get_gadget_string_multiline: error gadget %lli not allocated!\n", gadget_index);
        return (NULL);
    }

    if (screen[screennum].gadget[gadget_index].type != GADGET_STRING_MULTILINE)
    {
        printf ("get_gadget_string_multiline: error %lli not a string gadget!\n", gadget_index);
        return (NULL);
    }

	string = (struct gadget_string_multiline *) screen[screennum].gadget[gadget_index].gptr;

	ind = 0;
	for (string_y = 0; string_y < string->text_lines; string_y++)
    {
        string_x = string_y * string->visible_len;
        string_x_end = string_x + string->visible_len;

        // printf ("\nDEBUG: get_string_multiline string_x: %lli, string_x_end: %lli\n", string_x, string_x_end);

        for (i = string_x; i < string_x_end; i++)
        {
            ch = string->value[i];
            if (ch != '\0')
            {
                 data[text_address + ind] = string->value[i];
				 ind++;
            }
			else
			{
				// assign space char
				data[text_address + ind] = ' ';
				ind++;
			}
        }
    }
	return (sp);
}

U1 *set_gadget_string_passwd (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    // change status of string gadget and set if chars should be shown or be hiddden

    S8 gadget_index ALIGN;
	S8 status ALIGN;
	U1 err = 0;

	sp = stpopi ((U1 *) &status, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &gadget_index, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	if (err == 1)
	{
	   printf ("set_gadget_string_passwd: ERROR, stack corrupt!\n");
	   return (NULL);
	}

	struct gadget_string *string;

    if (! screen[screennum].gadget)
    {
        printf ("set_gadget_string_passwd: error gadget list not allocated!\n");
        return (NULL);
    }

    if (screen[screennum].gadget[gadget_index].gptr == NULL)
    {
        printf ("set_gadget_string_passwd: error gadget %lli not allocated!\n", gadget_index);
        return (NULL);
    }

    if (screen[screennum].gadget[gadget_index].type != GADGET_STRING)
    {
        printf ("set_gadget_string_passwd: error %lli not a string gadget!\n", gadget_index);
        return (NULL);
    }

    string = (struct gadget_string *) screen[screennum].gadget[gadget_index].gptr;

	string->passwd = status;

	return (sp);
}


U1 *change_gadget_box (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    /* change value and status (active / inactive) */
	/* change filled color */

    S8 gadget_index ALIGN;
	S8 status ALIGN;
	U1 err = 0;
    struct gadget_box *box;

	sp = stpopi ((U1 *) &status, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &gadget_index, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	if (err == 1)
	{
	   printf ("change_gadget_box: ERROR, stack corrupt!\n");
	   return (NULL);
	}

    if (! screen[screennum].gadget)
    {
        printf ("change_gadget_box: error gadget list not allocated!\n");
        return (NULL);
    }

    if (screen[screennum].gadget[gadget_index].gptr == NULL)
    {
        printf ("change_gadget_box: error gadget %lli not allocated!\n", gadget_index);
        return (NULL);
    }

    if (screen[screennum].gadget[gadget_index].type != GADGET_BOX)
    {
        printf ("change_gadget_box: error %lli not a box gadget!\n", gadget_index);
        return (NULL);
    }

    box = (struct gadget_box *) screen[screennum].gadget[gadget_index].gptr;
    box->status = status;

    return (sp);
}

U1 *change_gadget_box_grid (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    /* change value and status (active / inactive) */

    S8 gadget_index ALIGN;
	S8 status ALIGN;
	S8 selected ALIGN;
	U1 r, g, b, alpha;
	U1 err = 0;
    struct gadget_box *box;

	sp = stpopb (&alpha, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopb (&b, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopb (&g, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopb (&r, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &status, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1*) &selected, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &gadget_index, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	if (err == 1)
	{
	   printf ("change_gadget_box_grid: ERROR, stack corrupt!\n");
	   return (NULL);
	}

    if (! screen[screennum].gadget)
    {
        printf ("change_gadget_box_grid: error gadget list not allocated!\n");
        return (NULL);
    }

    if (screen[screennum].gadget[gadget_index].gptr == NULL)
    {
        printf ("change_gadget_box_grid: error gadget %lli not allocated!\n", gadget_index);
        return (NULL);
    }

    if (screen[screennum].gadget[gadget_index].type != GADGET_BOX)
    {
        printf ("change_gadget_box_grid: error %lli not a box gadget!\n", gadget_index);
        return (NULL);
    }

    box = (struct gadget_box *) screen[screennum].gadget[gadget_index].gptr;

    box->status = status;
	box->selected = selected;
	boxRGBA (renderer, box->x + 1, box->y + 1, box->x2 - 2, box->y2 - 2, r, g, b, alpha);

    return (sp);
}

U1 *get_gadget_box_grid (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 gadget_index ALIGN;
	S8 selected ALIGN;

	struct gadget_box *box;

	sp = stpopi ((U1 *) &gadget_index, sp, sp_top);
    if (sp == NULL)
    {
		printf ("get_gadget_box_grid: ERROR, stack corrupt!\n");
	  	return (NULL);
    }

	box = (struct gadget_box *) screen[screennum].gadget[gadget_index].gptr;
	selected = box->selected;

	sp = stpushi (selected, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("get_gadget_box_grid: ERROR: stack corrupt!\n");
		return (NULL);
	}

	return (sp);
}

U1 *change_gadget_progress_bar (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    /* change value and status (active / inactive) */

    S8 gadget_index ALIGN;
	S8 status ALIGN;
	S8 value ALIGN;
	U1 err = 0;

    struct gadget_progress_bar *progress;

	sp = stpopi ((U1 *) &status, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &value, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &gadget_index, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	if (err == 1)
	{
	   printf ("change_gadget_progress_bar: ERROR, stack corrupt!\n");
	   return (NULL);
	}

    if (! screen[screennum].gadget)
    {
        printf ("change_gadget_progress_bar: error gadget list not allocated!\n");
        return (NULL);
    }

    if (screen[screennum].gadget[gadget_index].gptr == NULL)
    {
        printf ("change_gadget_progress_bar: error gadget %lli not allocated!\n", gadget_index);
        return (NULL);
    }

    if (screen[screennum].gadget[gadget_index].type != GADGET_PROGRESS_BAR)
    {
        printf ("change_gadget_progress_bar: error %lli not a progress bar gadget!\n", gadget_index);
        return (NULL);
    }

    progress = (struct gadget_progress_bar *) screen[screennum].gadget[gadget_index].gptr;

    progress->status = status;
	progress->value = value;

	if (! draw_gadget_progress_bar (screennum, gadget_index, GADGET_NOT_SELECTED))
    {
        printf ("set_gadget_progress_bar: error can't draw gadget!\n");
        return (NULL);
    }
    else
    {
        return (sp);
    }
}

U1 *change_gadget_slider (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    /* change value and status (active / inactive) */

    S8 gadget_index ALIGN;
	S8 status ALIGN;
	S8 value ALIGN;
	U1 err = 0;

    struct gadget_slider *slider;

	sp = stpopi ((U1 *) &status, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &value, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	sp = stpopi ((U1 *) &gadget_index, sp, sp_top);
    if (sp == NULL)
    {
 	   err = 1;
    }

	if (err == 1)
	{
	   printf ("change_gadget_slider: ERROR, stack corrupt!\n");
	   return (NULL);
	}

    if (! screen[screennum].gadget)
    {
        printf ("change_gadget_slider: error gadget list not allocated!\n");
        return (NULL);
    }

    if (screen[screennum].gadget[gadget_index].gptr == NULL)
    {
        printf ("change_gadget_slider: error gadget %lli not allocated!\n", gadget_index);
        return (NULL);
    }

    if (screen[screennum].gadget[gadget_index].type != GADGET_SLIDER || screen[screennum].gadget[gadget_index].type != GADGET_SLIDER_VERT)
    {
        printf ("change_gadget_slider: error %lli not a slider bar gadget!\n", gadget_index);
        return (NULL);
    }

    slider = (struct gadget_slider *) screen[screennum].gadget[gadget_index].gptr;

    slider->status = status;
	slider->value = value;

    if (screen[screennum].gadget[gadget_index].type == GADGET_SLIDER)
    {
        if (! draw_gadget_slider (screennum, gadget_index, GADGET_NOT_SELECTED))
        {
            printf ("set_gadget_slider: error can't draw gadget!\n");
            return (NULL);
        }
        else
        {
            return (sp);
        }
    }
    else
    {
        if (! draw_gadget_slider_vert (screennum, gadget_index, GADGET_NOT_SELECTED))
        {
            printf ("set_gadget_slider: error can't draw gadget!\n");
            return (NULL);
        }
        else
        {
            return (sp);
        }
    }
}

U1 *get_gadget_x2y2 (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    /* return the right bottom corner of a gadget */
    /* for layouting gadgets */

    S8 gadget_index ALIGN;
    Sint16 x2, y2;

    struct gadget_button *button;
    struct gadget_checkbox *checkbox;
    struct gadget_cycle *cycle;
    struct gadget_string *string;
    struct gadget_box *box;
	struct gadget_progress_bar *progress;

	sp = stpopi ((U1 *) &gadget_index, sp, sp_top);
	if (sp == NULL)
	{
		printf ("get_gadget_x2y2: ERROR, stack corrupt!\n");
		return (NULL);
	}

    if (! screen[screennum].gadget)
    {
        printf ("get_gadget_x2y2: error gadget list not allocated!\n");
        return (NULL);
    }

    switch (screen[screennum].gadget[gadget_index].type)
    {
        case GADGET_BUTTON:
            button = (struct gadget_button *) screen[screennum].gadget[gadget_index].gptr;

            x2 = button->x2;
            y2 = button->y2;
            break;

        case GADGET_CHECKBOX:
            checkbox = (struct gadget_checkbox *) screen[screennum].gadget[gadget_index].gptr;

            x2 = checkbox->text_x2;
            y2 = checkbox->y2;
            break;

        case GADGET_CYCLE:
            cycle = (struct gadget_cycle *) screen[screennum].gadget[gadget_index].gptr;

            x2 = cycle->x2;
            y2 = cycle->y2;
            break;

        case GADGET_STRING:
            string = (struct gadget_string *) screen[screennum].gadget[gadget_index].gptr;

            x2 = string->x2;
            y2 = string->y2;
            break;

        case GADGET_BOX:
            box = (struct gadget_box *) screen[screennum].gadget[gadget_index].gptr;

            x2 = box->x2;
            y2 = box->y2;
            break;

		case GADGET_PROGRESS_BAR:
			progress = (struct gadget_progress_bar *) screen[screennum].gadget[gadget_index].gptr;

            x2 = progress->x2;
            y2 = progress->y2;
            break;
    }

	sp = stpushi (y2, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("get_gadget_x2y2: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpushi (x2, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("get_gadget_x2y2: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *get_mouse_state (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// get mouse position and button status

	S8 x ALIGN;
	S8 y ALIGN;
	S8 mouse_button_left ALIGN = 0;
	S8 mouse_button_middle ALIGN = 0;
	S8 mouse_button_right ALIGN = 0;

 	Uint8 buttonmask;

	SDL_PumpEvents ();

	// get mouse data
	buttonmask = SDL_GetMouseState ((int *) &x, (int *) &y);

	// set the buttons as pressed
	if (! (buttonmask & SDL_BUTTON (SDL_BUTTON_LEFT)))
	{
		mouse_button_left = 1;
	}

	if (! (buttonmask & SDL_BUTTON (SDL_BUTTON_MIDDLE)))
	{
		mouse_button_middle = 1;
	}

	if (! (buttonmask & SDL_BUTTON (SDL_BUTTON_RIGHT)))
	{
		mouse_button_right= 1;
	}

	sp = stpushi (mouse_button_right, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("get_mouse_state ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpushi (mouse_button_middle, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("get_mouse_state ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpushi (mouse_button_left, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("get_mouse_state ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpushi (y, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("get_mouse_state ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpushi (x, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("get_mouse_state ERROR: stack corrupt!\n");
		return (NULL);
	}

	// on stack: x, y, mouse_button_left, mouse_button_middle, mouse_button_right

	return (sp);
}


// joystick input handling ----------------------------------------------------
U1 *close_joystick (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	SDL_JoystickClose (joystick);
	return (sp);
}

U1 *get_joystick_x_axis (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// get x axis
	// get value in the range of: -32768 to 32767
	S8 x ALIGN;

	SDL_PumpEvents ();
	x = SDL_JoystickGetAxis (joystick, 0);

	sp = stpushi (x, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("get_joystick_x_axis: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *get_joystick_y_axis (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// get y axis
	// get value in the range of: -32768 to 32767
	S8 y ALIGN;

	SDL_PumpEvents ();
	y = SDL_JoystickGetAxis (joystick, 1);

	sp = stpushi (y, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("get_joystick_y_axis: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

// axis 2/3:
U1 *get_joystick_x2_axis (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// get x axis
	// get value in the range of: -32768 to 32767
	S8 x ALIGN;

	if (SDL_JoystickNumAxes (joystick) <= 2)
	{
		printf ("get_joystick_x2_axis: ERROR: only 2 axis!\n");
		return (NULL);
	}

	SDL_PumpEvents ();
	x = SDL_JoystickGetAxis (joystick, 2);

	sp = stpushi (x, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("get_joystick_x2_axis: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *get_joystick_y2_axis (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// get y axis
	// get value in the range of: -32768 to 32767
	S8 y ALIGN;

	if (SDL_JoystickNumAxes (joystick) <= 2)
	{
		printf ("get_joystick_y2_axis: ERROR: only 2 axis!\n");
		return (NULL);
	}

	SDL_PumpEvents ();
	y = SDL_JoystickGetAxis (joystick, 3);

	sp = stpushi (y, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("get_joystick_y2_axis: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *get_joystick_buttons (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// get number of joystick buttons
	S8 buttons ALIGN;

	buttons = SDL_JoystickNumButtons (joystick);
	if (buttons < 0)
	{
		// error
		printf ("get_joystick_buttons: ERROR: can't get number of joystick buttons!\n");
		return (NULL);
	}

	sp = stpushi (buttons, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("get_joystick_buttons: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *get_joystick_button (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// check if button is pressed or not
	S8 button ALIGN;
	S8 button_state ALIGN = 0;
	SDL_Event event;

	sp = stpopi ((U1 *) &button, sp, sp_top);
	if (sp == NULL)
	{
		printf ("get_joystick_button: ERROR stack corrupt!\n");
		return (NULL);
	}

	if (button > SDL_JoystickNumButtons (joystick) - 1)
	{
		printf ("get_joystick_button: ERROR no button: %lli !\n", button);
		return (NULL);
	}

	SDL_PumpEvents ();
	if (SDL_PollEvent (&event) == 1)
	{
		// there is some event, check if button is pressed
		switch (event.type)
		{
			case SDL_JOYBUTTONDOWN:  /* Handle Joystick Button Presses */
				if (event.jbutton.button == button)
				{
					// the checked button is pressed
					button_state = 1;
				}
				break;

			case SDL_JOYBUTTONUP:
				if (event.jbutton.button == button)
				{
					// the checked button is released
					button_state = 0;
				}
				break;
		}
	}


	/* old code, only working on button 0 !!!
	// 1 = pressed, 0 = not pressed
	SDL_PumpEvents ();
	button_state = SDL_JoystickGetButton (joystick, button);
	*/


	sp = stpushi (button_state, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("get_joystick_button: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *get_joystick_info (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// get joystick infos

	S8 name_len ALIGN;
	S8 name_address ALIGN;
	S8 name_real_len ALIGN;

	S8 joystick_axis_max ALIGN;
	S8 joystick_buttons_max ALIGN;

	sp = stpopi ((U1 *) &name_len, sp, sp_top);
	if (sp == NULL)
	{
		printf ("get_joystick_info: ERROR stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &name_address, sp, sp_top);
	if (sp == NULL)
	{
		printf ("get_joystick_info: ERROR stack corrupt!\n");
		return (NULL);
	}

	name_real_len = strlen_safe (SDL_JoystickNameForIndex (0), name_len);
	if (name_real_len > name_len)
	{
		printf ("get_joystick_info: ERROR name string overflow!\n");
		return (NULL);
	}

	// save joystick name in string variable
	strcpy ((char *) &data[name_address], SDL_JoystickNameForIndex (0));

	joystick_axis_max = SDL_JoystickNumAxes (joystick);
	joystick_buttons_max = SDL_JoystickNumButtons (joystick);

	// return values
	sp = stpushi (joystick_buttons_max, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("get_joystick_info: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpushi (joystick_axis_max, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("get_joystick_info: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}
