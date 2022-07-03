/*
 * This file gui.h is part of L1vm.
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
 */

#define MAXSCREEN 8
#define BLITSCREEN 7

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

// gadget numbers

#define GADGET_BUTTON       1
#define GADGET_CHECKBOX     2
#define GADGET_CYCLE        3
#define GADGET_STRING       4
#define GADGET_BOX          5
#define GADGET_PROGRESS_BAR 6
#define GADGET_SLIDER       7   // horizontal
#define GADGET_SLIDER_VERT  8   // vertical
#define GADGET_STRING_MULTILINE 9

// string gadget passwd stars output max
#define GADGET_STRING_PASSWD_STARS	512

// protos

U1 *stpushi (S8 data, U1 *sp, U1 *sp_bottom);
U1 *stpopi (U1 *data, U1 *sp, U1 *sp_top);

U1 *init_gadgets(U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data);
void free_gadget(S2 screennum, S2 gadget_index);
void free_gadgets(void);
U1 set_ttf_style(S2 screennum);
U1 draw_text_ttf(SDL_Surface *surface, S2 screennum, U1 *textstr, Sint16 x, Sint16 y, Uint8 r, Uint8 g, Uint8 b);
void set_gadget_color_border_light(S2 screennum);
void set_gadget_color_border_shadow(S2 screennum);
void set_gadget_color_backgr_light(S2 screennum);
void set_gadget_color_backgr_shadow(S2 screennum);
void set_gadget_color_text_light(S2 screennum);
void set_gadget_color_text_shadow(S2 screennum);
void draw_gadget_light(SDL_Renderer *renderer, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2);
void draw_gadget_shadow(SDL_Renderer *renderer, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2);
void draw_gadget_input(SDL_Renderer *renderer, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2);
void draw_ghost_gadget(SDL_Renderer *renderer, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2);
U1 draw_gadget_button(S2 screennum, U2 gadget_index, U1 selected);
void draw_checkmark_light(SDL_Renderer *renderer, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2);
void draw_checkmark_shadow(SDL_Renderer *renderer, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2);
U1 draw_gadget_checkbox(S2 screennum, U2 gadget_index, U1 selected, U1 value);
void draw_cycle_arrow_light(S2 screennum, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2);
void draw_cycle_arrow_shadow(S2 screennum, Sint16 x, Sint16 y, Sint16 x2, Sint16 y2);
int do_copy_surface (SDL_Renderer *dest_renderer, SDL_Surface *src, SDL_Rect *srcrect, SDL_Surface *dst, SDL_Rect *dstrect);
U1 draw_gadget_cycle(S2 screennum, U2 gadget_index, U1 selected, S4 value);
U1 draw_gadget_progress_bar(S2 screennum, U2 gadget_index, U1 selected);
U1 draw_gadget_slider_bar (S2 screennum, U2 gadget_index, U1 selected);
U1 draw_gadget_string (S2 screennum, U2 gadget_index, U1 selected);
U1 draw_gadget_string_multiline (S2 screennum, U2 gadget_index, U1 selected);
U1 event_gadget_string (S2 screennum, U2 gadget_index);
U1 event_gadget_string_multiline (S2 screennum, U2 gadget_index);
U1 *gadget_event(U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data);
U1 *set_gadget_button(U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data);
U1 *set_gadget_progress_bar(U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data);
U1 *set_gadget_checkbox(U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data);
U1 *set_gadget_cycle(U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data);
U1 *set_gadget_string(U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data);
U1 *set_gadget_string_multiline (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data);
U1 *set_gadget_box(U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data);
U1 *change_gadget_checkbox(U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data);
U1 *change_gadget_cycle (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data);
U1 *change_gadget_string (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data);
U1 *change_gadget_box (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data);
U1 *set_gadget_slider (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data);
U1 *change_gadget_progress_bar(U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data);
U1 *change_gadget_string_multiline (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data);
U1 *get_gadget_x2y2 (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data);
U1 *change_gadget_slider (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data);
Uint32 getpixel (SDL_Surface *surface, Sint16 x, Sint16 y);
U1 **alloc_array_U1 (S4 x, S4 y);
void dealloc_array_U1 (U1 **array, S4 x);
void sdl_do_delay (S8 delay);
U1 *set_gadget_string_passwd (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data);

U1 *get_mouse_state (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data);

// joystick input handling ----------------------------------------------------
U1 *close_joystick (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data);
U1 *get_joystick_x_axis (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data);
U1 *get_joystick_y_axis (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data);
U1 *get_joystick_x2_axis (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data);
U1 *get_joystick_y2_axis (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data);
U1 *get_joystick_buttons (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data);
U1 *get_joystick_button (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data);
U1 *get_joystick_info (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data);

U1 *get_key (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data);
U1 *free_all_gadgets (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data);

// gadget box grid
U1 *set_gadget_box (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data);
U1 *change_gadget_box (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data);

/* string.c */
int strright(U1 *dst, U1 *src, int chars);
int strleft(U1 *dst, U1 *src, int chars);
int strremoveleft(U1 *dst, U1 *src, int pos);
int strremoveright(U1 *dst, U1 *src, int pos);
int strinsertchar(U1 *dst, U1 *src, U1 chr, int pos);
U1 *my_strcpy (U1 *destination, const U1 *source);

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
	 struct font_ttf font;
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
	 struct font_ttf font;
     U1 status;
     Sint16 x;
     Sint16 y;
     Sint16 x2;
     Sint16 y2;
     Sint16 text_x;
     Sint16 text_y;
     S8 value;
 };

 struct gadget_checkbox
 {
     U1 *text;
	 struct font_ttf font;
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
	 struct font_ttf font;
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
	 struct font_ttf font;
     U1 *value;                          /* input string */
     U1 *display;                        /* display-buffer string */
     U1 status;
     U2 string_len;                      /* max input string length */
     U2 visible_len;                     /* visible chars of input string */
	 U1 passwd;						    /* is 1: don't show char input! */
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

// multi line text input gadget
 struct gadget_string_multiline
 {
 	U1 *text;                           /* info text */
 	struct font_ttf font;
 	U1 *value;                          /* input string */
 	U1 *display;                        /* display-buffer string */
 	U1 status;
 	U2 string_len;                      /* max input string length */
 	U2 visible_len;                     /* visible chars of input string */
	U2 text_lines;						/* number of text lines in gadget */
 	U1 passwd;						    /* is 1: don't show char input! */
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
 	S2 cursor_pos_x;                      /* cursor position */
	S2 cursor_pos_y;
 	S2 insert_pos;                      /* text insert position */
	S2 text_height;						// font height
	S2 text_width;						// font width
 };



 struct gadget_box
 {
     U1 status;
	 U1 selected;						// for grid box status selected or not
     Sint16 x;
     Sint16 y;
     Sint16 x2;
     Sint16 y2;
 };

 struct gadget_slider
 {
     U1 *text;
     struct font_ttf font;
     U1 status;
     Sint16 x;
     Sint16 y;
     Sint16 x2;
     Sint16 y2;
     Sint16 text_x;
     Sint16 text_y;
     S8 value ALIGN;      // current value
     S8 min ALIGN;        // minimal value
     S8 max ALIGN;        // maximum value
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

 /* -------------------------------------------------------- */
