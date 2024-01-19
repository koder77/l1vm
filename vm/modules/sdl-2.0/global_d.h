/*
 * This file global_d.h is part of L1vm.
 *
 * (c) Copyright Stefan Pietzonke (jay-t@gmx.net), 2020
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

/* set this to one if you want to use sound */
#define WITH_SOUND		1


#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#if __amigaos__ || __AROS__ || __linux__ || __ANDROID__
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define CLOSEWINSOCK();
#endif

#if _WIN32
	#include <winsock2.h>
    #include <ws2tcpip.h>

    #define CLOSEWINSOCK();             WSACleanup();
    #define SHUT_RDWR                   0x02
#endif



#include <SDL2/SDL.h>
#include <SDL2/SDL_endian.h>
#include <SDL2/video.h>
#include <SDL2/SDL_render.h>
// #include <SDL2/SDL_byteorder.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_events.h>



#if WITH_SOUND
	#include <SDL2/SDL_mixer.h>
#endif


#include <errno.h>
#include <string.h>

#define TRUE                1
#define FALSE               0
#define BINUL               '\0'


#define BACKLOG             1                       /* number of pending connections */


#define MAXSCREEN           8
#define BLITSCREEN          MAXSCREEN - 1
#define OPEN                1
#define CLOSED              0


#define MAXGADGET           256

#define OK                  0
#define ERROR               1


/* gadget event stuff */

#define GADGET_NOT_SELECTED 0
#define GADGET_SELECTED     1
#define GADGET_MENU_DOWN    2
#define GADGET_MENU_UP      3

/* gadget status */

#define GADGET_NOT_ACTIVE   0
#define GADGET_ACTIVE       1

/* checkbox value */

#define GADGET_CHECKBOX_FALSE   0
#define GADGET_CHECKBOX_TRUE    1


/* font path env variable */

#define FONT_DIR_SB                         "NANOGFXFONT"
#define SOUND_DIR_SB                        "NANOSFXSOUND"

#define NANOVM_ROOT_SB						"NANOVM_ROOT"

typedef unsigned char   U1;     /* UBYTE   */
typedef int16_t    		S2;     /* INT     */
typedef uint16_t 		U2;     /* UINT    */
typedef int32_t    		S4;     /* LONGINT */
typedef double          F8;     /* DOUBLE  */
typedef int             NINT;


/* fonts -------------------------------------------------- */

struct font_bitmap
{
    Sint16 width;
    Sint16 height;
    U1 *buf;                            /* buffer for font data */
};

struct font_ttf
{
    TTF_Font *font;
    U2 size;
    U1 style_normal;
    U1 style_bold;
    U1 style_italic;
    U1 style_underline;
};

/* gadgets ------------------------------------------------ */

struct gadget_button
{
    U1 *text;
	struct font_ttf font_ttf;
    U1 status;
    Sint16 x;
    Sint16 y;
    Sint16 x2;
    Sint16 y2;
    Sint16 text_x;
    Sint16 text_y;
};

struct gadget_progress_bar
{
    U1 *text;
	struct font_ttf font_ttf;
    U1 status;
    Sint16 x;
    Sint16 y;
    Sint16 x2;
    Sint16 y2;
    Sint16 text_x;
    Sint16 text_y;
	U2 value;
};

struct gadget_checkbox
{
    U1 *text;
	struct font_ttf font_ttf;
    U1 status;
    U1 value;
    Sint16 x;
    Sint16 y;
    Sint16 x2;
    Sint16 y2;
    Sint16 text_x;
    Sint16 text_y;
    Sint16 text_x2;
    Sint16 text_y2;
};

struct gadget_cycle
{
    U1 menu;
    U1 **text;
	struct font_ttf font_ttf;
    U1 status;
    U1 value;
    U2 menu_entries;
    Sint16 x;
    Sint16 y;
    Sint16 x2;
    Sint16 y2;
    Sint16 text_x;
    Sint16 text_y;
    Sint16 text_height;
    Sint16 menu_x;
    Sint16 menu_y;
    Sint16 menu_x2;
    Sint16 menu_y2;
    Sint16 arrow_x;
    Sint16 arrow_y;
};

struct gadget_string
{
    U1 *text;                           /* info text */
	struct font_ttf font_ttf;
    U1 *value;                          /* input string */
    U1 *display;                        /* display-buffer string */
    U1 status;
    U2 string_len;                      /* max input string length */
    U2 visible_len;                     /* visible chars of input string */
    Sint16 x;
    Sint16 y;
    Sint16 x2;
    Sint16 y2;
    Sint16 text_x;                      /* gadget info text */
    Sint16 text_y;
    Sint16 text_x2;
    Sint16 text_y2;
    Sint16 input_x;                     /* gadget entry text */
    Sint16 input_y;
    Sint16 cursor_width;
    Sint16 cursor_height;
    S2 cursor_pos;                      /* cursor position */
    S2 insert_pos;                      /* text insert position */
};

struct gadget_box
{
    U1 status;
    Sint16 x;
    Sint16 y;
    Sint16 x2;
    Sint16 y2;
};

struct gadget
{
    void *gptr;                         /* pointer to gadget structure */
    U1 type;                            /* gadget type */
};

struct color
{
    Uint8 r;                            /* colors */
    Uint8 g;
    Uint8 b;
};

struct gadget_color
{
    struct color border_light;
    struct color border_shadow;
    struct color backgr_light;
    struct color backgr_shadow;
    struct color text_light;
    struct color text_shadow;
};


struct sound
{
    U1 wav_filename[512];
    U1 music_filename[512];
    S2 channel;
    S2 loops;

#if WITH_SOUND
    Mix_Music *music;
#else
	int *music;
#endif
};

/* -------------------------------------------------------- */

struct screen
{
    SDL_Surface *bmap;
    Sint16 width;
    Sint16 height;
    Sint16 x;
    Sint16 y;
    Sint16 x2;
    Sint16 y2;
    Sint16 x3;
    Sint16 y3;
    Sint16 *vx;                         /* for polygon x coord */
    Sint16 *vy;                         /* for polygon y coord */
    U2 vectors;                         /* number of vectors (vx/vy pairs) */
    U2 vx_index;                        /* vector x index */
    U2 vy_index;                        /* vector y index */
    U2 steps;
    Sint16 radius;
    Sint16 xradius;
    Sint16 yradius;
    Sint16 startangle;
    Sint16 endangle;
    Uint8 r;                            /* normal colors */
    Uint8 g;
    Uint8 b;
    Uint8 r_bg;                         /* background colors (unused) */
    Uint8 g_bg;
    Uint8 b_bg;
    Uint8 alpha;
    Uint8 video_bpp;
    U1 status;
    U1 title[256];                      /* window title */
    U1 icon[256];                       /* icon name */
    U1 text[256];                       /* text to output */
    U1 fontname[512];                   /* font to load */
    U1 picture_name[512];               /* picture to load */


    struct sound sound;                 /* sound data */

    struct font_bitmap font_bitmap;
    struct font_ttf font_ttf;
    struct gadget *gadget;              /* gadget list */
    struct gadget_color gadget_color;   /* gadget colors */
    U2 gadgets;                         /* number of gadgets */
    U2 gadget_index;                    /* gadget index for setting gadget */
    U1 gadget_status;                   /* gadget status for new gadget */
    U1 gadget_string_value[256];        /* gadget string value */
    S4 gadget_int_value;                /* gadget integer value */
    U1 **gadget_cycle_text;             /* menu text */
    U2 gadget_cycle_menu_entries;       /* number of menu entries */
    U2 gadget_cycle_text_index;
    U2 gadget_string_string_len;        /* max input string length */
    U2 gadget_string_visible_len;       /* visible chars of input string */

	U2 gadget_progress_value;
};

struct rs232
{
	S4 comport;
	S4 baudrate;
	U1 buf[4096];
	S4 bufsize;
};

struct rpi
{
	S4 mode;
	S4 value;
	S4 pin;
};
