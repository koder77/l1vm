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

/* Raspberry Pi GPIO support */
#define RPI_GPIO		0


/* set this to 1 on ANDROID: doubleclick to select */

#if __ANDROID__
#define GADGET_CYCLE_DOUBLECLICK	1
#else
#define GADGET_CYCLE_DOUBLECLICK	0
#endif



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

#if __ANDROID__

#include <SDL.h>
#include <SDL_byteorder.h>
#include <SDL_gfxPrimitives.h>
#include <SDL_ttf.h>
#include <SDL_image.h>

#if WITH_SOUND
	#include <SDL_mixer.h>
#endif

#else

#include <SDL2/SDL.h>
#include <SDL2/SDL_render.h>
// #include <SDL2/SDL_byteorder.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_events.h>


#if WITH_SOUND
	#include <SDL2/SDL_mixer.h>
#endif

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


/* commands */

#define END                                 1
#define END_SB                              "*end"                  /* end connection to server */

#define SHUTDOWN                            2
#define SHUTDOWN_SB                         "*shutdown"             /* shutdown server */

#define OPEN_SCREEN                         3
#define OPEN_SCREEN_SB                      "*openscreen"

#define CLOSE_SCREEN                        4
#define CLOSE_SCREEN_SB                     "*closescreen"

#define CLEAR_SCREEN                        5
#define CLEAR_SCREEN_SB                     "*clearscreen"

#define UPDATE_SCREEN                       6
#define UPDATE_SCREEN_SB                    "*updatescreen"

#define COLOR                               7
#define COLOR_SB                            "*color"

#define PIXEL                               8
#define PIXEL_SB                            "*pixel"

#define LINE                                9
#define LINE_SB                             "*line"

#define RECTANGLE                           10
#define RECTANGLE_SB                        "*rectangle"

#define RECTANGLE_FILL                      11
#define RECTANGLE_FILL_SB                   "*rectangle-f"

#define CIRCLE                              12
#define CIRCLE_SB                           "*circle"

#define CIRCLE_FILL                         13
#define CIRCLE_FILL_SB                      "*circle-f"

#define ELLIPSE                             14
#define ELLIPSE_SB                          "*ellipse"

#define ELLIPSE_FILL                        15
#define ELLIPSE_FILL_SB                     "*ellipse-f"

#define PIE                                 16
#define PIE_SB                              "*pie"

#define PIE_FILL                            17
#define PIE_FILL_SB                         "*pie-f"

#define TRIGON                              18
#define TRIGON_SB                           "*trigon"

#define TRIGON_FILL                         19
#define TRIGON_FILL_SB                      "*trigon-f"

#define POLYGON                             20
#define POLYGON_SB                          "*polygon"

#define POLYGON_FILL                        21
#define POLYGON_FILL_SB                     "*polygon-f"

#define BEZIER                              22
#define BEZIER_SB                           "*bezier"

#define TEXT_BMAP                           23
#define TEXT_BMAP_SB                        "*text-bmap"

#define LOADFONT_BMAP                       24
#define LOADFONT_BMAP_SB                    "*loadfont-bmap"

#define TEXT_TTF                            25
#define TEXT_TTF_SB                         "*text-ttf"

#define LOADFONT_TTF                        26
#define LOADFONT_TTF_SB                     "*loadfont-ttf"

#define LOAD_PICTURE                        27
#define LOAD_PICTURE_SB                     "*load-picture"

#define GETPIXEL                            28
#define GETPIXEL_SB                         "*getpixel"

#define GETMOUSE                            29
#define GETMOUSE_SB                         "*getmouse"

#define GADGET_COLOR_BORDER_LIGHT           30
#define GADGET_COLOR_BORDER_LIGHT_SB        "*gadget-color-border-light"

#define GADGET_COLOR_BORDER_SHADOW          31
#define GADGET_COLOR_BORDER_SHADOW_SB       "*gadget-color-border-shadow"

#define GADGET_COLOR_BACKGR_LIGHT           32
#define GADGET_COLOR_BACKGR_LIGHT_SB        "*gadget-color-backgr-light"

#define GADGET_COLOR_BACKGR_SHADOW          33
#define GADGET_COLOR_BACKGR_SHADOW_SB       "*gadget-color-backgr-shadow"

#define GADGET_COLOR_TEXT_LIGHT             34
#define GADGET_COLOR_TEXT_LIGHT_SB          "*gadget-color-text-light"

#define GADGET_COLOR_TEXT_SHADOW            35
#define GADGET_COLOR_TEXT_SHADOW_SB         "*gadget-color-text-shadow"

#define GADGET_BUTTON                       36
#define GADGET_BUTTON_SB                    "*gadget-button"

#define GADGET_CHECKBOX                     37
#define GADGET_CHECKBOX_SB                  "*gadget-checkbox"

#define GADGET_CYCLE                        38
#define GADGET_CYCLE_SB                     "*gadget-cycle"

#define GADGET_STRING                       39
#define GADGET_STRING_SB                    "*gadget-string"

#define GADGET_BOX                          40
#define GADGET_PROGRESS_BAR					41

#define GADGET_EVENT                        42
#define GADGET_EVENT_SB                     "*gadget-event"

#define GADGET_GET_X2Y2                     43
#define GADGET_GET_X2Y2_SB                  "*gadget-get-x2y2"

#define GADGET_CHECKBOX_CHANGE              44
#define GADGET_CHECKBOX_CHANGE_SB           "*gadget-checkbox-change"

#define GADGET_CYCLE_CHANGE					45

#define GADGET_STRING_CHANGE                46
#define GADGET_STRING_CHANGE_SB             "*gadget-string-change"

#define GADGET_BOX_CHANGE                   47
#define GADGET_PROGRESS_BAR_CHANGE			48

#define SOUND_PLAY_WAV                      49
#define SOUND_STOP_CHANNEL                  50

#define SOUND_PLAY_MUSIC                    51
#define SOUND_STOP_MUSIC                    52


#define RS232_OPEN_COMPORT					53
#define RS232_POLL_COMPORT					54
#define RS232_SEND_BYTE						55
#define RS232_SEND_BUF						56
#define RS232_CLOSE_COMPORT					57


#define RPI_GPIO_START						58
#define RPI_GPIO_MODE						59
#define RPI_GPIO_READ						60
#define RPI_GPIO_WRITE						61

#define SAVE_PICTURE						62

/* types */

#define SCREENNUM                           10001
#define WIDTH                               10002
#define HEIGHT                              10003
#define BIT                                 10004

#define X                                   10005
#define Y                                   10006
#define X2                                  10007
#define Y2                                  10008
#define X3                                  10009
#define Y3                                  10010

#define VECTORS                             10011
#define STEPS                               10012
#define RADIUS                              10013
#define XRADIUS                             10014
#define YRADIUS                             10015
#define STARTANGLE                          10016
#define ENDANGLE                            10017


/* normal colors */

#define R                                   10018
#define G                                   10019
#define B                                   10020


/* background colors (unused) */

#define R_BG                                10021
#define G_BG                                10022
#define B_BG                                10023


/* alpha channel */

#define ALPHA                               10024


#define FONTWIDTH                           10025
#define FONTHEIGHT                          10026
#define FONTSIZE                            10027
#define FONTSTYLE                           10028

/* true type font styles */

#define FONTSTYLE_NORMAL                    10029
#define FONTSTYLE_BOLD                      10030
#define FONTSTYLE_ITALIC                    10031
#define FONTSTYLE_UNDERLINE                 10032


#define GADGET                              10033
#define GADGETS                             10034
#define GADGET_STATUS                       10035
#define GADGET_INT_VALUE                    10036
#define GADGET_CYCLE_ENTRIES                10037
#define GADGET_STRING_STR_LEN               10038
#define GADGET_STRING_VIS_LEN               10039


/* vectors (x, y pairs) */

#define VX                                  10040
#define VY                                  10041

/* string data */

#define TEXT                                10042
#define SCREENTITLE                         10043
#define SCREENICON                          10044
#define PICTURENAME                         10045
#define FONTNAME                            10046
#define GADGET_STRING_VALUE                 10047
#define GADGET_CYCLE_TEXT                   10048

/* sound data */

#define SOUND_WAV_FILE                      10049
#define SOUND_CHANNEL                       10050
#define SOUND_LOOPS                         10051
#define SOUND_MUSIC_FILE                    10052

/* RS232 data */

#define RS232_COMPORT_NUMBER				10053
#define RS232_BAUDRATE						10054
#define RS232_BUF_SIZE						10055
#define RS232_BYTE							10056
#define RS232_BUF							10057

/* RPI GPIO data */

#define RPI_GPIO_PIN						10058
#define RPI_GPIO_VALUE						10059


/* progress bar */

#define GADGET_PROGRESS_BAR_VALUE			10060


#define OK                                  0
#define ERROR                               1


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
