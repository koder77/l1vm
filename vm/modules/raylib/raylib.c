/*
 * This file raylib.c is part of L1vm.
 *
 * (c) Copyright Stefan Pietzonke (info@midnight-coding.de), 2026
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

// created with opencode Big Pickle AI

/*
 * L1vm raylib module: 2D/3D graphics using the raylib library.
 *
 * All exported functions have the module function signature:
 *
 *   U1 *raylib_xxx (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
 *
 * Integer/Double arguments are passed on the VM stack (stpopi/stpopd),
 * the top of the stack is the LAST function argument (push order in the
 * L1 program: first arg pushed first, last arg on top).
 *
 * Memory layout used for vector data (data memory, little endian):
 *
 *   Color        : B, 4   (r, g, b, a bytes)
 *   Vector2      : D, 2   (x, y)
 *   Vector3      : D, 3   (x, y, z)
 *   Vector4      : D, 4   (x, y, z, w)
 *   Rectangle    : D, 4   (x, y, width, height)
 *   Matrix       : D, 16  (column major, 16 floats as doubles)
 *   Camera3D     : D, 11  (position D,3, target D,3, up D,3, fovy, projection)
 *   BoundingBox  : D, 6   (min D,3, max D,3)
 *   Ray          : D, 6   (position D,3, direction D,3)
 *   RayCollision : D, 8   (hit, distance, point D,3, normal D,3)
 *
 * Textures, models, meshes, shaders, materials, fonts and images are
 * referenced by integer handles returned by the loading functions.
 */

#if __OpenBSD__
#undef __linux__
#endif

#include "../../../include/global.h"
#include "../../../include/stack.h"

#include <stdbool.h>
#include <pthread.h>

#include <raylib.h>

// header only math module, no linking required
#define RAYMATH_STATIC_INLINE
#include <raymath.h>

extern S2 memory_bounds (S8 start, S8 offset_access);
extern U1 get_sandbox_filename (U1 *filename, U1 *sandbox_filename, S2 max_name_len);

struct data_info data_info[MAXDATAINFO];
S8 data_info_ind;

S2 init_memory_bounds (struct data_info *data_info_orig, S8 data_info_ind_orig)
{
	memcpy (&data_info, &data_info_orig, sizeof (data_info_orig));
	data_info_ind = data_info_ind_orig;

	return (0);
}

// global object tables, referenced by handles -------------------------------
#define RAYLIB_MAX_MODELS		256
#define RAYLIB_MAX_TEXTURES		256
#define RAYLIB_MAX_MESHES		256
#define RAYLIB_MAX_SHADERS		64
#define RAYLIB_MAX_MATERIALS	256
#define RAYLIB_MAX_FONTS		64
#define RAYLIB_MAX_IMAGES		128
#define RAYLIB_MAX_RENDERTEX	64
#define RAYLIB_MAX_ANIMATIONS	64
#define RAYLIB_MAX_SOUNDS		64
#define RAYLIB_MAX_MUSICS		64

Model raylib_models[RAYLIB_MAX_MODELS];
S2 raylib_models_used[RAYLIB_MAX_MODELS];

Texture2D raylib_textures[RAYLIB_MAX_TEXTURES];
S2 raylib_textures_used[RAYLIB_MAX_TEXTURES];

Mesh raylib_meshes[RAYLIB_MAX_MESHES];
S2 raylib_meshes_used[RAYLIB_MAX_MESHES];

Shader raylib_shaders[RAYLIB_MAX_SHADERS];
S2 raylib_shaders_used[RAYLIB_MAX_SHADERS];

Material raylib_materials[RAYLIB_MAX_MATERIALS];
S2 raylib_materials_used[RAYLIB_MAX_MATERIALS];

Font raylib_fonts[RAYLIB_MAX_FONTS];
S2 raylib_fonts_used[RAYLIB_MAX_FONTS];

Image raylib_images[RAYLIB_MAX_IMAGES];
S2 raylib_images_used[RAYLIB_MAX_IMAGES];

RenderTexture2D raylib_rendertextures[RAYLIB_MAX_RENDERTEX];
S2 raylib_rendertextures_used[RAYLIB_MAX_RENDERTEX];

ModelAnimation raylib_animations[RAYLIB_MAX_ANIMATIONS];
S2 raylib_animations_used[RAYLIB_MAX_ANIMATIONS];

Sound raylib_sounds[RAYLIB_MAX_SOUNDS];
S2 raylib_sounds_used[RAYLIB_MAX_SOUNDS];

Music raylib_musics[RAYLIB_MAX_MUSICS];
S2 raylib_musics_used[RAYLIB_MAX_MUSICS];

// internal camera state
Camera3D raylib_camera;
int raylib_camera_mode = CAMERA_ORBITAL;

// default font handle (loaded lazy at first use)
S2 raylib_default_font_used = 0;

// thread safety: raylib must be called from a single thread, so all
// module function calls get serialized with this mutex
pthread_mutex_t rlib_mutex = PTHREAD_MUTEX_INITIALIZER;

// helper functions ----------------------------------------------------------

size_t strlen_safe (const char *str, S8 maxlen)
{
	S8 i ALIGN = 0;

	while (1)
	{
		if (str[i] != '\0')
		{
			if (i < maxlen)
			{
				i++;
			}
			else
			{
				// maximum length reached
				break;
			}
		}
		else
		{
			break;
		}
	}
	return (i);
}

// read a NUL terminated string from the VM data memory
S2 rlib_read_string (U1 *data, S8 addr, U1 *buf, S2 buflen)
{
	S8 slen ALIGN;

	if (addr < 0)
	{
		printf ("rlib_read_string: ERROR illegal address: %lli!\n", addr);
		return (1);
	}

	slen = strlen_safe ((const char *) &data[addr], buflen - 1);
	if (memory_bounds (addr, slen) != 0)
	{
		printf ("rlib_read_string: ERROR memory bounds check failed!\n");
		return (1);
	}

	memcpy (buf, &data[addr], slen);
	buf[slen] = '\0';
	return (0);
}

// read a Color struct from a B,4 byte array
S2 rlib_read_color (U1 *data, S8 addr, Color *col)
{
	if (memory_bounds (addr, 4) != 0)
	{
		printf ("rlib_read_color: ERROR memory bounds check failed!\n");
		return (1);
	}

	col->r = data[addr];
	col->g = data[addr + 1];
	col->b = data[addr + 2];
	col->a = data[addr + 3];
	return (0);
}

S2 rlib_read_vec2 (U1 *data, S8 addr, Vector2 *v)
{
	F8 x ALIGN, y ALIGN;

	if (memory_bounds (addr, 16) != 0)
	{
		printf ("rlib_read_vec2: ERROR memory bounds check failed!\n");
		return (1);
	}

	memcpy (&x, &data[addr], sizeof (F8));
	memcpy (&y, &data[addr + 8], sizeof (F8));
	v->x = (float) x;
	v->y = (float) y;
	return (0);
}

S2 rlib_read_vec3 (U1 *data, S8 addr, Vector3 *v)
{
	F8 x ALIGN, y ALIGN, z ALIGN;

	if (memory_bounds (addr, 24) != 0)
	{
		printf ("rlib_read_vec3: ERROR memory bounds check failed!\n");
		return (1);
	}

	memcpy (&x, &data[addr], sizeof (F8));
	memcpy (&y, &data[addr + 8], sizeof (F8));
	memcpy (&z, &data[addr + 16], sizeof (F8));
	v->x = (float) x;
	v->y = (float) y;
	v->z = (float) z;
	return (0);
}

S2 rlib_read_vec4 (U1 *data, S8 addr, Vector4 *v)
{
	F8 x ALIGN, y ALIGN, z ALIGN, w ALIGN;

	if (memory_bounds (addr, 32) != 0)
	{
		printf ("rlib_read_vec4: ERROR memory bounds check failed!\n");
		return (1);
	}

	memcpy (&x, &data[addr], sizeof (F8));
	memcpy (&y, &data[addr + 8], sizeof (F8));
	memcpy (&z, &data[addr + 16], sizeof (F8));
	memcpy (&w, &data[addr + 24], sizeof (F8));
	v->x = (float) x;
	v->y = (float) y;
	v->z = (float) z;
	v->w = (float) w;
	return (0);
}

S2 rlib_read_rect (U1 *data, S8 addr, Rectangle *r)
{
	F8 x ALIGN, y ALIGN, w ALIGN, h ALIGN;

	if (memory_bounds (addr, 32) != 0)
	{
		printf ("rlib_read_rect: ERROR memory bounds check failed!\n");
		return (1);
	}

	memcpy (&x, &data[addr], sizeof (F8));
	memcpy (&y, &data[addr + 8], sizeof (F8));
	memcpy (&w, &data[addr + 16], sizeof (F8));
	memcpy (&h, &data[addr + 24], sizeof (F8));
	r->x = (float) x;
	r->y = (float) y;
	r->width = (float) w;
	r->height = (float) h;
	return (0);
}

S2 rlib_read_matrix (U1 *data, S8 addr, Matrix *m)
{
	F8 t ALIGN;
	S8 i ALIGN;
	float *fm = (float *) m;

	if (memory_bounds (addr, 128) != 0)
	{
		printf ("rlib_read_matrix: ERROR memory bounds check failed!\n");
		return (1);
	}

	for (i = 0; i < 16; i++)
	{
		memcpy (&t, &data[addr + (i * 8)], sizeof (F8));
		fm[i] = (float) t;
	}
	return (0);
}

S2 rlib_write_vec2 (U1 *data, S8 addr, Vector2 v)
{
	F8 t ALIGN;

	if (memory_bounds (addr, 16) != 0)
	{
		printf ("rlib_write_vec2: ERROR memory bounds check failed!\n");
		return (1);
	}

	t = v.x; memcpy (&data[addr], &t, sizeof (F8));
	t = v.y; memcpy (&data[addr + 8], &t, sizeof (F8));
	return (0);
}

S2 rlib_write_vec3 (U1 *data, S8 addr, Vector3 v)
{
	F8 t ALIGN;

	if (memory_bounds (addr, 24) != 0)
	{
		printf ("rlib_write_vec3: ERROR memory bounds check failed!\n");
		return (1);
	}

	t = v.x; memcpy (&data[addr], &t, sizeof (F8));
	t = v.y; memcpy (&data[addr + 8], &t, sizeof (F8));
	t = v.z; memcpy (&data[addr + 16], &t, sizeof (F8));
	return (0);
}

S2 rlib_write_vec4 (U1 *data, S8 addr, Vector4 v)
{
	F8 t ALIGN;

	if (memory_bounds (addr, 32) != 0)
	{
		printf ("rlib_write_vec4: ERROR memory bounds check failed!\n");
		return (1);
	}

	t = v.x; memcpy (&data[addr], &t, sizeof (F8));
	t = v.y; memcpy (&data[addr + 8], &t, sizeof (F8));
	t = v.z; memcpy (&data[addr + 16], &t, sizeof (F8));
	t = v.w; memcpy (&data[addr + 24], &t, sizeof (F8));
	return (0);
}

S2 rlib_write_matrix (U1 *data, S8 addr, Matrix m)
{
	F8 t ALIGN;
	S8 i ALIGN;
	float *fm = (float *) &m;

	if (memory_bounds (addr, 128) != 0)
	{
		printf ("rlib_write_matrix: ERROR memory bounds check failed!\n");
		return (1);
	}

	for (i = 0; i < 16; i++)
	{
		t = fm[i];
		memcpy (&data[addr + (i * 8)], &t, sizeof (F8));
	}
	return (0);
}

S2 rlib_read_bbox (U1 *data, S8 addr, BoundingBox *b)
{
	if (rlib_read_vec3 (data, addr, &b->min) != 0) return (1);
	if (rlib_read_vec3 (data, addr + 24, &b->max) != 0) return (1);
	return (0);
}

S2 rlib_write_bbox (U1 *data, S8 addr, BoundingBox b)
{
	if (rlib_write_vec3 (data, addr, b.min) != 0) return (1);
	if (rlib_write_vec3 (data, addr + 24, b.max) != 0) return (1);
	return (0);
}

S2 rlib_read_ray (U1 *data, S8 addr, Ray *r)
{
	if (rlib_read_vec3 (data, addr, &r->position) != 0) return (1);
	if (rlib_read_vec3 (data, addr + 24, &r->direction) != 0) return (1);
	return (0);
}

S2 rlib_write_ray (U1 *data, S8 addr, Ray r)
{
	if (rlib_write_vec3 (data, addr, r.position) != 0) return (1);
	if (rlib_write_vec3 (data, addr + 24, r.direction) != 0) return (1);
	return (0);
}

S2 rlib_write_raycollision (U1 *data, S8 addr, RayCollision c)
{
	F8 t ALIGN;

	if (memory_bounds (addr, 64) != 0)
	{
		printf ("rlib_write_raycollision: ERROR memory bounds check failed!\n");
		return (1);
	}

	t = c.hit ? 1.0 : 0.0;
	memcpy (&data[addr], &t, sizeof (F8));
	t = c.distance;
	memcpy (&data[addr + 8], &t, sizeof (F8));
	if (rlib_write_vec3 (data, addr + 16, c.point) != 0) return (1);
	if (rlib_write_vec3 (data, addr + 40, c.normal) != 0) return (1);
	return (0);
}

// read camera struct from a D,11 memory array
S2 rlib_read_camera (U1 *data, S8 addr, Camera3D *c)
{
	F8 f ALIGN, p ALIGN;

	if (rlib_read_vec3 (data, addr, &c->position) != 0) return (1);
	if (rlib_read_vec3 (data, addr + 24, &c->target) != 0) return (1);
	if (rlib_read_vec3 (data, addr + 48, &c->up) != 0) return (1);

	if (memory_bounds (addr + 72, 16) != 0)
	{
		printf ("rlib_read_camera: ERROR memory bounds check failed!\n");
		return (1);
	}

	memcpy (&f, &data[addr + 72], sizeof (F8));
	memcpy (&p, &data[addr + 80], sizeof (F8));
	c->fovy = (float) f;
	c->projection = (int) p;
	return (0);
}

S2 rlib_write_camera (U1 *data, S8 addr, Camera3D c)
{
	F8 t ALIGN;

	if (rlib_write_vec3 (data, addr, c.position) != 0) return (1);
	if (rlib_write_vec3 (data, addr + 24, c.target) != 0) return (1);
	if (rlib_write_vec3 (data, addr + 48, c.up) != 0) return (1);

	if (memory_bounds (addr + 72, 16) != 0)
	{
		printf ("rlib_write_camera: ERROR memory bounds check failed!\n");
		return (1);
	}

	t = c.fovy;
	memcpy (&data[addr + 72], &t, sizeof (F8));
	t = (F8) c.projection;
	memcpy (&data[addr + 80], &t, sizeof (F8));
	return (0);
}

// load a filename from VM data memory for raylib file functions,
// using the L1vm sandbox file access
S2 rlib_load_filename (U1 *data, S8 addr, U1 *out, S2 outlen)
{
	U1 sandbox_filename[512];

#if SANDBOX
	if (get_sandbox_filename (&data[addr], sandbox_filename, 255) != 0)
	{
		printf ("rlib_load_filename: ERROR illegal filename!\n");
		return (1);
	}
	strncpy ((char *) out, (const char *) sandbox_filename, outlen - 1);
#else
	if (rlib_read_string (data, addr, out, outlen) != 0)
	{
		return (1);
	}
#endif
	return (0);
}

// find a free slot in a used-flag table, returns index or -1
S2 rlib_find_slot (S2 *used, S8 max)
{
	S8 i ALIGN;

	for (i = 0; i < max; i++)
	{
		if (used[i] == 0)
		{
			return ((S2) i);
		}
	}
	printf ("raylib: ERROR object table full! (max: %lli)\n", max);
	return (-1);
}

// fill default font handle, called lazy
S2 rlib_get_default_font (void)
{
	if (raylib_default_font_used == 0)
	{
		raylib_fonts[0] = GetFontDefault ();
		raylib_default_font_used = 1;
	}
	return (0);
}

// ---------------------------------------------------------------------------
// 3D shapes drawing functions
// ---------------------------------------------------------------------------

U1 *raylib_draw_line_3d (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args (push order): start_addr, end_addr, color_addr
	S8 start_addr ALIGN, end_addr ALIGN, color_addr ALIGN;
	Vector3 start, end;
	Color col;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &end_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &start_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec3 (data, start_addr, &start) != 0) return (NULL);
	if (rlib_read_vec3 (data, end_addr, &end) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &col) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawLine3D (start, end, col);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_point_3d (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: pos_addr, color_addr
	S8 pos_addr ALIGN, color_addr ALIGN;
	Vector3 pos;
	Color col;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &pos_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec3 (data, pos_addr, &pos) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &col) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawPoint3D (pos, col);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_circle_3d (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: center_addr, radius, axis_addr, angle, color_addr
	S8 center_addr ALIGN, axis_addr ALIGN, color_addr ALIGN;
	F8 radius ALIGN, angle ALIGN;
	Vector3 center, axis;
	Color col;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &angle, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &axis_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &radius, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &center_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec3 (data, center_addr, &center) != 0) return (NULL);
	if (rlib_read_vec3 (data, axis_addr, &axis) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &col) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawCircle3D (center, (float) radius, axis, (float) angle, col);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_triangle_3d (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: v1_addr, v2_addr, v3_addr, color_addr
	S8 v1_addr ALIGN, v2_addr ALIGN, v3_addr ALIGN, color_addr ALIGN;
	Vector3 v1, v2, v3;
	Color col;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &v3_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &v2_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &v1_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec3 (data, v1_addr, &v1) != 0) return (NULL);
	if (rlib_read_vec3 (data, v2_addr, &v2) != 0) return (NULL);
	if (rlib_read_vec3 (data, v3_addr, &v3) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &col) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawTriangle3D (v1, v2, v3, col);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_triangle_strip_3d (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: points_addr, point_count, color_addr
	// points: consecutive D,3 vectors
	S8 points_addr ALIGN, color_addr ALIGN;
	S8 point_count ALIGN;
	Vector3 *points = NULL;
	Color col;
	S8 i ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &point_count, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &points_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (point_count < 1)
	{
		return (sp);
	}

	if (memory_bounds (points_addr, point_count * 24) != 0)
	{
		printf ("raylib_draw_triangle_strip_3d: ERROR memory bounds check failed!\n");
		return (NULL);
	}

	points = calloc (point_count, sizeof (Vector3));
	if (points == NULL)
	{
		printf ("raylib_draw_triangle_strip_3d: ERROR out of memory!\n");
		return (NULL);
	}

	for (i = 0; i < point_count; i++)
	{
		points[i].x = (float) *(F8 *) &data[points_addr + (i * 24)];
		points[i].y = (float) *(F8 *) &data[points_addr + (i * 24) + 8];
		points[i].z = (float) *(F8 *) &data[points_addr + (i * 24) + 16];
	}

	if (rlib_read_color (data, color_addr, &col) != 0)
	{
		free (points);
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	DrawTriangleStrip3D (points, (int) point_count, col);
	pthread_mutex_unlock (&rlib_mutex);

	free (points);
	return (sp);
}

U1 *raylib_draw_cube (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: pos_addr, width, height, length, color_addr
	S8 pos_addr ALIGN, color_addr ALIGN;
	F8 width ALIGN, height ALIGN, length ALIGN;
	Vector3 pos;
	Color col;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &length, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &height, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &width, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &pos_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec3 (data, pos_addr, &pos) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &col) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawCube (pos, (float) width, (float) height, (float) length, col);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_cube_v (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: pos_addr, size_addr, color_addr
	S8 pos_addr ALIGN, size_addr ALIGN, color_addr ALIGN;
	Vector3 pos, size;
	Color col;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &size_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &pos_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec3 (data, pos_addr, &pos) != 0) return (NULL);
	if (rlib_read_vec3 (data, size_addr, &size) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &col) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawCubeV (pos, size, col);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_cube_wires (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: pos_addr, width, height, length, color_addr
	S8 pos_addr ALIGN, color_addr ALIGN;
	F8 width ALIGN, height ALIGN, length ALIGN;
	Vector3 pos;
	Color col;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &length, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &height, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &width, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &pos_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec3 (data, pos_addr, &pos) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &col) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawCubeWires (pos, (float) width, (float) height, (float) length, col);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_cube_wires_v (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: pos_addr, size_addr, color_addr
	S8 pos_addr ALIGN, size_addr ALIGN, color_addr ALIGN;
	Vector3 pos, size;
	Color col;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &size_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &pos_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec3 (data, pos_addr, &pos) != 0) return (NULL);
	if (rlib_read_vec3 (data, size_addr, &size) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &col) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawCubeWiresV (pos, size, col);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_sphere (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: center_addr, radius, color_addr
	S8 center_addr ALIGN, color_addr ALIGN;
	F8 radius ALIGN;
	Vector3 center;
	Color col;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &radius, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &center_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec3 (data, center_addr, &center) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &col) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawSphere (center, (float) radius, col);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_sphere_ex (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: center_addr, radius, rings, slices, color_addr
	S8 center_addr ALIGN, color_addr ALIGN;
	F8 radius ALIGN;
	S8 rings ALIGN, slices ALIGN;
	Vector3 center;
	Color col;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &slices, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &rings, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &radius, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &center_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec3 (data, center_addr, &center) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &col) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawSphereEx (center, (float) radius, (int) rings, (int) slices, col);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_sphere_wires (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: center_addr, radius, rings, slices, color_addr
	S8 center_addr ALIGN, color_addr ALIGN;
	F8 radius ALIGN;
	S8 rings ALIGN, slices ALIGN;
	Vector3 center;
	Color col;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &slices, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &rings, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &radius, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &center_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec3 (data, center_addr, &center) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &col) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawSphereWires (center, (float) radius, (int) rings, (int) slices, col);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_cylinder (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: pos_addr, radius_top, radius_bottom, height, sides, color_addr
	S8 pos_addr ALIGN, color_addr ALIGN;
	F8 radius_top ALIGN, radius_bottom ALIGN, height ALIGN;
	S8 sides ALIGN;
	Vector3 pos;
	Color col;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &sides, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &height, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &radius_bottom, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &radius_top, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &pos_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec3 (data, pos_addr, &pos) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &col) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawCylinder (pos, (float) radius_top, (float) radius_bottom, (float) height, (int) sides, col);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_cylinder_ex (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: start_addr, end_addr, start_radius, end_radius, sides, color_addr
	S8 start_addr ALIGN, end_addr ALIGN, color_addr ALIGN;
	F8 start_radius ALIGN, end_radius ALIGN;
	S8 sides ALIGN;
	Vector3 start, end;
	Color col;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &sides, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &end_radius, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &start_radius, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &end_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &start_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec3 (data, start_addr, &start) != 0) return (NULL);
	if (rlib_read_vec3 (data, end_addr, &end) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &col) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawCylinderEx (start, end, (float) start_radius, (float) end_radius, (int) sides, col);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_cylinder_wires (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: pos_addr, radius_top, radius_bottom, height, sides, color_addr
	S8 pos_addr ALIGN, color_addr ALIGN;
	F8 radius_top ALIGN, radius_bottom ALIGN, height ALIGN;
	S8 sides ALIGN;
	Vector3 pos;
	Color col;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &sides, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &height, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &radius_bottom, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &radius_top, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &pos_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec3 (data, pos_addr, &pos) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &col) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawCylinderWires (pos, (float) radius_top, (float) radius_bottom, (float) height, (int) sides, col);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_cylinder_wires_ex (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: start_addr, end_addr, start_radius, end_radius, sides, color_addr
	S8 start_addr ALIGN, end_addr ALIGN, color_addr ALIGN;
	F8 start_radius ALIGN, end_radius ALIGN;
	S8 sides ALIGN;
	Vector3 start, end;
	Color col;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &sides, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &end_radius, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &start_radius, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &end_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &start_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec3 (data, start_addr, &start) != 0) return (NULL);
	if (rlib_read_vec3 (data, end_addr, &end) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &col) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawCylinderWiresEx (start, end, (float) start_radius, (float) end_radius, (int) sides, col);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_capsule (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: start_addr, end_addr, radius, rings, slices, color_addr
	S8 start_addr ALIGN, end_addr ALIGN, color_addr ALIGN;
	F8 radius ALIGN;
	S8 rings ALIGN, slices ALIGN;
	Vector3 start, end;
	Color col;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &slices, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &rings, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &radius, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &end_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &start_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec3 (data, start_addr, &start) != 0) return (NULL);
	if (rlib_read_vec3 (data, end_addr, &end) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &col) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawCapsule (start, end, (float) radius, (int) rings, (int) slices, col);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_capsule_wires (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: start_addr, end_addr, radius, rings, slices, color_addr
	S8 start_addr ALIGN, end_addr ALIGN, color_addr ALIGN;
	F8 radius ALIGN;
	S8 rings ALIGN, slices ALIGN;
	Vector3 start, end;
	Color col;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &slices, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &rings, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &radius, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &end_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &start_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec3 (data, start_addr, &start) != 0) return (NULL);
	if (rlib_read_vec3 (data, end_addr, &end) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &col) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawCapsuleWires (start, end, (float) radius, (int) rings, (int) slices, col);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_plane (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: center_addr, size_addr, color_addr
	S8 center_addr ALIGN, size_addr ALIGN, color_addr ALIGN;
	Vector3 center;
	Vector2 size;
	Color col;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &size_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &center_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec3 (data, center_addr, &center) != 0) return (NULL);
	if (rlib_read_vec2 (data, size_addr, &size) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &col) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawPlane (center, size, col);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_ray (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: ray_addr, color_addr
	S8 ray_addr ALIGN, color_addr ALIGN;
	Ray ray;
	Color col;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &ray_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_ray (data, ray_addr, &ray) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &col) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawRay (ray, col);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_grid (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: slices, spacing
	S8 slices ALIGN;
	F8 spacing ALIGN;

	sp = stpopd ((U1 *) &spacing, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &slices, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawGrid ((int) slices, (float) spacing);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_bounding_box (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: bbox_addr, color_addr
	S8 bbox_addr ALIGN, color_addr ALIGN;
	BoundingBox bbox;
	Color col;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &bbox_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_bbox (data, bbox_addr, &bbox) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &col) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawBoundingBox (bbox, col);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_billboard (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: texture_handle, pos_addr, scale, tint_addr
	// uses internal camera
	S8 texture_handle ALIGN, pos_addr ALIGN, tint_addr ALIGN;
	F8 scale ALIGN;
	Vector3 pos;
	Color tint;

	sp = stpopi ((U1 *) &tint_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &scale, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &pos_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &texture_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (texture_handle < 0 || texture_handle >= RAYLIB_MAX_TEXTURES || raylib_textures_used[texture_handle] == 0)
	{
		printf ("raylib_draw_billboard: ERROR invalid texture handle!\n");
		return (NULL);
	}

	if (rlib_read_vec3 (data, pos_addr, &pos) != 0) return (NULL);
	if (rlib_read_color (data, tint_addr, &tint) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawBillboard (raylib_camera, raylib_textures[texture_handle], pos, (float) scale, tint);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_billboard_rec (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: texture_handle, rec_addr, pos_addr, size_addr, tint_addr
	S8 texture_handle ALIGN, rec_addr ALIGN, pos_addr ALIGN, size_addr ALIGN, tint_addr ALIGN;
	Rectangle rec;
	Vector3 pos;
	Vector2 size;
	Color tint;

	sp = stpopi ((U1 *) &tint_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &size_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &pos_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &rec_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &texture_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (texture_handle < 0 || texture_handle >= RAYLIB_MAX_TEXTURES || raylib_textures_used[texture_handle] == 0)
	{
		printf ("raylib_draw_billboard_rec: ERROR invalid texture handle!\n");
		return (NULL);
	}

	if (rlib_read_rect (data, rec_addr, &rec) != 0) return (NULL);
	if (rlib_read_vec3 (data, pos_addr, &pos) != 0) return (NULL);
	if (rlib_read_vec2 (data, size_addr, &size) != 0) return (NULL);
	if (rlib_read_color (data, tint_addr, &tint) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawBillboardRec (raylib_camera, raylib_textures[texture_handle], rec, pos, size, tint);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_billboard_pro (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: texture_handle, rec_addr, pos_addr, up_addr, size_addr, origin_addr, rotation, tint_addr
	S8 texture_handle ALIGN, rec_addr ALIGN, pos_addr ALIGN, up_addr ALIGN;
	S8 size_addr ALIGN, origin_addr ALIGN, tint_addr ALIGN;
	F8 rotation ALIGN;
	Rectangle rec;
	Vector3 pos, up;
	Vector2 size, origin;
	Color tint;

	sp = stpopi ((U1 *) &tint_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &rotation, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &origin_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &size_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &up_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &pos_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &rec_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &texture_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (texture_handle < 0 || texture_handle >= RAYLIB_MAX_TEXTURES || raylib_textures_used[texture_handle] == 0)
	{
		printf ("raylib_draw_billboard_pro: ERROR invalid texture handle!\n");
		return (NULL);
	}

	if (rlib_read_rect (data, rec_addr, &rec) != 0) return (NULL);
	if (rlib_read_vec3 (data, pos_addr, &pos) != 0) return (NULL);
	if (rlib_read_vec3 (data, up_addr, &up) != 0) return (NULL);
	if (rlib_read_vec2 (data, size_addr, &size) != 0) return (NULL);
	if (rlib_read_vec2 (data, origin_addr, &origin) != 0) return (NULL);
	if (rlib_read_color (data, tint_addr, &tint) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawBillboardPro (raylib_camera, raylib_textures[texture_handle], rec, pos, up, size, origin, (float) rotation, tint);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

// ---------------------------------------------------------------------------
// Models functions
// ---------------------------------------------------------------------------

U1 *raylib_load_model (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: file_addr
	// return: model handle or -1 on error
	S8 file_addr ALIGN;
	U1 filename[512];
	S8 ind ALIGN;

	sp = stpopi ((U1 *) &file_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_load_filename (data, file_addr, filename, sizeof (filename)) != 0) return (NULL);

	ind = rlib_find_slot (raylib_models_used, RAYLIB_MAX_MODELS);
	if (ind < 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	raylib_models[ind] = LoadModel ((const char *) filename);
	pthread_mutex_unlock (&rlib_mutex);

	if (raylib_models[ind].meshCount == 0)
	{
		printf ("raylib_load_model: ERROR loading model: %s!\n", filename);
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	raylib_models_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_load_model_from_mesh (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: mesh_handle
	// return: model handle or -1 on error
	S8 mesh_handle ALIGN;
	S8 ind ALIGN;

	sp = stpopi ((U1 *) &mesh_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (mesh_handle < 0 || mesh_handle >= RAYLIB_MAX_MESHES || raylib_meshes_used[mesh_handle] == 0)
	{
		printf ("raylib_load_model_from_mesh: ERROR invalid mesh handle!\n");
		return (NULL);
	}

	ind = rlib_find_slot (raylib_models_used, RAYLIB_MAX_MODELS);
	if (ind < 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	raylib_models[ind] = LoadModelFromMesh (raylib_meshes[mesh_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	raylib_models_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_unload_model (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: model_handle
	S8 model_handle ALIGN;

	sp = stpopi ((U1 *) &model_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (model_handle < 0 || model_handle >= RAYLIB_MAX_MODELS || raylib_models_used[model_handle] == 0)
	{
		printf ("raylib_unload_model: ERROR invalid model handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	UnloadModel (raylib_models[model_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	memset (&raylib_models[model_handle], 0, sizeof (Model));
	raylib_models_used[model_handle] = 0;

	return (sp);
}

U1 *raylib_is_model_valid (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: model_handle
	// return: 1 valid, 0 invalid
	S8 model_handle ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &model_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (model_handle < 0 || model_handle >= RAYLIB_MAX_MODELS || raylib_models_used[model_handle] == 0)
	{
		ret = 0;
	}
	else
	{
		pthread_mutex_lock (&rlib_mutex);
		ret = IsModelValid (raylib_models[model_handle]) ? 1 : 0;
		pthread_mutex_unlock (&rlib_mutex);
	}

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_draw_model (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: model_handle, pos_addr, scale, tint_addr
	S8 model_handle ALIGN, pos_addr ALIGN, tint_addr ALIGN;
	F8 scale ALIGN;
	Vector3 pos;
	Color tint;

	sp = stpopi ((U1 *) &tint_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &scale, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &pos_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &model_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (model_handle < 0 || model_handle >= RAYLIB_MAX_MODELS || raylib_models_used[model_handle] == 0)
	{
		printf ("raylib_draw_model: ERROR invalid model handle!\n");
		return (NULL);
	}

	if (rlib_read_vec3 (data, pos_addr, &pos) != 0) return (NULL);
	if (rlib_read_color (data, tint_addr, &tint) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawModel (raylib_models[model_handle], pos, (float) scale, tint);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_model_ex (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: model_handle, pos_addr, rot_axis_addr, rot_angle, scale_addr, tint_addr
	S8 model_handle ALIGN, pos_addr ALIGN, rot_axis_addr ALIGN, scale_addr ALIGN, tint_addr ALIGN;
	F8 rot_angle ALIGN;
	Vector3 pos, rot_axis, scale;
	Color tint;

	sp = stpopi ((U1 *) &tint_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &scale_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &rot_angle, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &rot_axis_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &pos_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &model_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (model_handle < 0 || model_handle >= RAYLIB_MAX_MODELS || raylib_models_used[model_handle] == 0)
	{
		printf ("raylib_draw_model_ex: ERROR invalid model handle!\n");
		return (NULL);
	}

	if (rlib_read_vec3 (data, pos_addr, &pos) != 0) return (NULL);
	if (rlib_read_vec3 (data, rot_axis_addr, &rot_axis) != 0) return (NULL);
	if (rlib_read_vec3 (data, scale_addr, &scale) != 0) return (NULL);
	if (rlib_read_color (data, tint_addr, &tint) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawModelEx (raylib_models[model_handle], pos, rot_axis, (float) rot_angle, scale, tint);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_model_wires (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: model_handle, pos_addr, scale, tint_addr
	S8 model_handle ALIGN, pos_addr ALIGN, tint_addr ALIGN;
	F8 scale ALIGN;
	Vector3 pos;
	Color tint;

	sp = stpopi ((U1 *) &tint_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &scale, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &pos_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &model_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (model_handle < 0 || model_handle >= RAYLIB_MAX_MODELS || raylib_models_used[model_handle] == 0)
	{
		printf ("raylib_draw_model_wires: ERROR invalid model handle!\n");
		return (NULL);
	}

	if (rlib_read_vec3 (data, pos_addr, &pos) != 0) return (NULL);
	if (rlib_read_color (data, tint_addr, &tint) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawModelWires (raylib_models[model_handle], pos, (float) scale, tint);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_model_wires_ex (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: model_handle, pos_addr, rot_axis_addr, rot_angle, scale_addr, tint_addr
	S8 model_handle ALIGN, pos_addr ALIGN, rot_axis_addr ALIGN, scale_addr ALIGN, tint_addr ALIGN;
	F8 rot_angle ALIGN;
	Vector3 pos, rot_axis, scale;
	Color tint;

	sp = stpopi ((U1 *) &tint_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &scale_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &rot_angle, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &rot_axis_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &pos_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &model_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (model_handle < 0 || model_handle >= RAYLIB_MAX_MODELS || raylib_models_used[model_handle] == 0)
	{
		printf ("raylib_draw_model_wires_ex: ERROR invalid model handle!\n");
		return (NULL);
	}

	if (rlib_read_vec3 (data, pos_addr, &pos) != 0) return (NULL);
	if (rlib_read_vec3 (data, rot_axis_addr, &rot_axis) != 0) return (NULL);
	if (rlib_read_vec3 (data, scale_addr, &scale) != 0) return (NULL);
	if (rlib_read_color (data, tint_addr, &tint) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawModelWiresEx (raylib_models[model_handle], pos, rot_axis, (float) rot_angle, scale, tint);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_get_model_bounding_box (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: model_handle, out_addr
	// out: BoundingBox (B,6 / D,6: min D,3 + max D,3)
	S8 model_handle ALIGN, out_addr ALIGN;
	BoundingBox bbox;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &model_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (model_handle < 0 || model_handle >= RAYLIB_MAX_MODELS || raylib_models_used[model_handle] == 0)
	{
		printf ("raylib_get_model_bounding_box: ERROR invalid model handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	bbox = GetModelBoundingBox (raylib_models[model_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	if (rlib_write_bbox (data, out_addr, bbox) != 0) return (NULL);

	return (sp);
}

U1 *raylib_set_model_mesh_material (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: model_handle, mesh_id, material_id
	S8 model_handle ALIGN, mesh_id ALIGN, material_id ALIGN;

	sp = stpopi ((U1 *) &material_id, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &mesh_id, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &model_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (model_handle < 0 || model_handle >= RAYLIB_MAX_MODELS || raylib_models_used[model_handle] == 0)
	{
		printf ("raylib_set_model_mesh_material: ERROR invalid model handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	SetModelMeshMaterial (&raylib_models[model_handle], (int) mesh_id, (int) material_id);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_set_model_transform (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: model_handle, matrix_addr
	S8 model_handle ALIGN, matrix_addr ALIGN;
	Matrix m;

	sp = stpopi ((U1 *) &matrix_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &model_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (model_handle < 0 || model_handle >= RAYLIB_MAX_MODELS || raylib_models_used[model_handle] == 0)
	{
		printf ("raylib_set_model_transform: ERROR invalid model handle!\n");
		return (NULL);
	}

	if (rlib_read_matrix (data, matrix_addr, &m) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	raylib_models[model_handle].transform = m;
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

// ---------------------------------------------------------------------------
// Mesh generation functions
// ---------------------------------------------------------------------------

U1 *raylib_gen_mesh_poly (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: sides, radius
	// return: mesh handle or -1
	S8 sides ALIGN;
	F8 radius ALIGN;
	S8 ind ALIGN;

	sp = stpopd ((U1 *) &radius, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &sides, sp, sp_top);
	if (sp == NULL) return (NULL);

	ind = rlib_find_slot (raylib_meshes_used, RAYLIB_MAX_MESHES);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_meshes[ind] = GenMeshPoly ((int) sides, (float) radius);
	pthread_mutex_unlock (&rlib_mutex);

	raylib_meshes_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_gen_mesh_plane (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: width, length, resx, resz
	// return: mesh handle or -1
	S8 resx ALIGN, resz ALIGN;
	F8 width ALIGN, length ALIGN;
	S8 ind ALIGN;

	sp = stpopi ((U1 *) &resz, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &resx, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &length, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &width, sp, sp_top);
	if (sp == NULL) return (NULL);

	ind = rlib_find_slot (raylib_meshes_used, RAYLIB_MAX_MESHES);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_meshes[ind] = GenMeshPlane ((float) width, (float) length, (int) resx, (int) resz);
	pthread_mutex_unlock (&rlib_mutex);

	raylib_meshes_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_gen_mesh_cube (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: width, height, length
	// return: mesh handle or -1
	F8 width ALIGN, height ALIGN, length ALIGN;
	S8 ind ALIGN;

	sp = stpopd ((U1 *) &length, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &height, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &width, sp, sp_top);
	if (sp == NULL) return (NULL);

	ind = rlib_find_slot (raylib_meshes_used, RAYLIB_MAX_MESHES);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_meshes[ind] = GenMeshCube ((float) width, (float) height, (float) length);
	pthread_mutex_unlock (&rlib_mutex);

	raylib_meshes_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_gen_mesh_sphere (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: radius, rings, slices
	// return: mesh handle or -1
	F8 radius ALIGN;
	S8 rings ALIGN, slices ALIGN;
	S8 ind ALIGN;

	sp = stpopi ((U1 *) &slices, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &rings, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &radius, sp, sp_top);
	if (sp == NULL) return (NULL);

	ind = rlib_find_slot (raylib_meshes_used, RAYLIB_MAX_MESHES);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_meshes[ind] = GenMeshSphere ((float) radius, (int) rings, (int) slices);
	pthread_mutex_unlock (&rlib_mutex);

	raylib_meshes_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_gen_mesh_hemisphere (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: radius, rings, slices
	// return: mesh handle or -1
	F8 radius ALIGN;
	S8 rings ALIGN, slices ALIGN;
	S8 ind ALIGN;

	sp = stpopi ((U1 *) &slices, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &rings, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &radius, sp, sp_top);
	if (sp == NULL) return (NULL);

	ind = rlib_find_slot (raylib_meshes_used, RAYLIB_MAX_MESHES);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_meshes[ind] = GenMeshHemiSphere ((float) radius, (int) rings, (int) slices);
	pthread_mutex_unlock (&rlib_mutex);

	raylib_meshes_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_gen_mesh_cylinder (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: radius, height, slices
	// return: mesh handle or -1
	F8 radius ALIGN, height ALIGN;
	S8 slices ALIGN;
	S8 ind ALIGN;

	sp = stpopi ((U1 *) &slices, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &height, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &radius, sp, sp_top);
	if (sp == NULL) return (NULL);

	ind = rlib_find_slot (raylib_meshes_used, RAYLIB_MAX_MESHES);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_meshes[ind] = GenMeshCylinder ((float) radius, (float) height, (int) slices);
	pthread_mutex_unlock (&rlib_mutex);

	raylib_meshes_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_gen_mesh_cone (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: radius, height, slices
	// return: mesh handle or -1
	F8 radius ALIGN, height ALIGN;
	S8 slices ALIGN;
	S8 ind ALIGN;

	sp = stpopi ((U1 *) &slices, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &height, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &radius, sp, sp_top);
	if (sp == NULL) return (NULL);

	ind = rlib_find_slot (raylib_meshes_used, RAYLIB_MAX_MESHES);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_meshes[ind] = GenMeshCone ((float) radius, (float) height, (int) slices);
	pthread_mutex_unlock (&rlib_mutex);

	raylib_meshes_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_gen_mesh_torus (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: radius, size, radseg, sides
	// return: mesh handle or -1
	F8 radius ALIGN, ring_size ALIGN;
	S8 radseg ALIGN, sides ALIGN;
	S8 ind ALIGN;

	sp = stpopi ((U1 *) &sides, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &radseg, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &ring_size, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &radius, sp, sp_top);
	if (sp == NULL) return (NULL);

	ind = rlib_find_slot (raylib_meshes_used, RAYLIB_MAX_MESHES);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_meshes[ind] = GenMeshTorus ((float) radius, (float) ring_size, (int) radseg, (int) sides);
	pthread_mutex_unlock (&rlib_mutex);

	raylib_meshes_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_gen_mesh_knot (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: radius, size, radseg, sides
	// return: mesh handle or -1
	F8 radius ALIGN, ring_size ALIGN;
	S8 radseg ALIGN, sides ALIGN;
	S8 ind ALIGN;

	sp = stpopi ((U1 *) &sides, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &radseg, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &ring_size, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &radius, sp, sp_top);
	if (sp == NULL) return (NULL);

	ind = rlib_find_slot (raylib_meshes_used, RAYLIB_MAX_MESHES);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_meshes[ind] = GenMeshKnot ((float) radius, (float) ring_size, (int) radseg, (int) sides);
	pthread_mutex_unlock (&rlib_mutex);

	raylib_meshes_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_gen_mesh_heightmap (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: image_handle, size_addr
	// return: mesh handle or -1
	S8 image_handle ALIGN, size_addr ALIGN;
	Vector3 size;
	S8 ind ALIGN;

	sp = stpopi ((U1 *) &size_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &image_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (image_handle < 0 || image_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[image_handle] == 0)
	{
		printf ("raylib_gen_mesh_heightmap: ERROR invalid image handle!\n");
		return (NULL);
	}

	if (rlib_read_vec3 (data, size_addr, &size) != 0) return (NULL);

	ind = rlib_find_slot (raylib_meshes_used, RAYLIB_MAX_MESHES);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_meshes[ind] = GenMeshHeightmap (raylib_images[image_handle], size);
	pthread_mutex_unlock (&rlib_mutex);

	raylib_meshes_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_gen_mesh_cubicmap (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: image_handle, cubesize_addr
	// return: mesh handle or -1
	S8 image_handle ALIGN, cubesize_addr ALIGN;
	Vector3 cube_size;
	S8 ind ALIGN;

	sp = stpopi ((U1 *) &cubesize_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &image_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (image_handle < 0 || image_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[image_handle] == 0)
	{
		printf ("raylib_gen_mesh_cubicmap: ERROR invalid image handle!\n");
		return (NULL);
	}

	if (rlib_read_vec3 (data, cubesize_addr, &cube_size) != 0) return (NULL);

	ind = rlib_find_slot (raylib_meshes_used, RAYLIB_MAX_MESHES);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_meshes[ind] = GenMeshCubicmap (raylib_images[image_handle], cube_size);
	pthread_mutex_unlock (&rlib_mutex);

	raylib_meshes_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_upload_mesh (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: mesh_handle
	S8 mesh_handle ALIGN;

	sp = stpopi ((U1 *) &mesh_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (mesh_handle < 0 || mesh_handle >= RAYLIB_MAX_MESHES || raylib_meshes_used[mesh_handle] == 0)
	{
		printf ("raylib_upload_mesh: ERROR invalid mesh handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	UploadMesh (&raylib_meshes[mesh_handle], false);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_unload_mesh (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: mesh_handle
	S8 mesh_handle ALIGN;

	sp = stpopi ((U1 *) &mesh_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (mesh_handle < 0 || mesh_handle >= RAYLIB_MAX_MESHES || raylib_meshes_used[mesh_handle] == 0)
	{
		printf ("raylib_unload_mesh: ERROR invalid mesh handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	UnloadMesh (raylib_meshes[mesh_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	memset (&raylib_meshes[mesh_handle], 0, sizeof (Mesh));
	raylib_meshes_used[mesh_handle] = 0;

	return (sp);
}

U1 *raylib_get_mesh_bounding_box (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: mesh_handle, out_addr
	S8 mesh_handle ALIGN, out_addr ALIGN;
	BoundingBox bbox;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &mesh_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (mesh_handle < 0 || mesh_handle >= RAYLIB_MAX_MESHES || raylib_meshes_used[mesh_handle] == 0)
	{
		printf ("raylib_get_mesh_bounding_box: ERROR invalid mesh handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	bbox = GetMeshBoundingBox (raylib_meshes[mesh_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	if (rlib_write_bbox (data, out_addr, bbox) != 0) return (NULL);

	return (sp);
}

U1 *raylib_get_mesh_vertex_count (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: mesh_handle
	// return: vertex count
	S8 mesh_handle ALIGN;

	sp = stpopi ((U1 *) &mesh_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (mesh_handle < 0 || mesh_handle >= RAYLIB_MAX_MESHES || raylib_meshes_used[mesh_handle] == 0)
	{
		printf ("raylib_get_mesh_vertex_count: ERROR invalid mesh handle!\n");
		return (NULL);
	}

	sp = stpushi (raylib_meshes[mesh_handle].vertexCount, sp, sp_bottom);
	return (sp);
}

U1 *raylib_draw_mesh (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: mesh_handle, material_handle, transform_addr
	S8 mesh_handle ALIGN, material_handle ALIGN, transform_addr ALIGN;
	Matrix m;

	sp = stpopi ((U1 *) &transform_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &material_handle, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &mesh_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (mesh_handle < 0 || mesh_handle >= RAYLIB_MAX_MESHES || raylib_meshes_used[mesh_handle] == 0)
	{
		printf ("raylib_draw_mesh: ERROR invalid mesh handle!\n");
		return (NULL);
	}

	if (material_handle < 0 || material_handle >= RAYLIB_MAX_MATERIALS || raylib_materials_used[material_handle] == 0)
	{
		printf ("raylib_draw_mesh: ERROR invalid material handle!\n");
		return (NULL);
	}

	if (rlib_read_matrix (data, transform_addr, &m) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawMesh (raylib_meshes[mesh_handle], raylib_materials[material_handle], m);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_mesh_instanced (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: mesh_handle, material_handle, transforms_addr, instances
	// transforms: consecutive D,16 matrices
	S8 mesh_handle ALIGN, material_handle ALIGN, transforms_addr ALIGN;
	S8 instances ALIGN;
	Matrix *mtrans = NULL;
	S8 i ALIGN;

	sp = stpopi ((U1 *) &instances, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &transforms_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &material_handle, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &mesh_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (mesh_handle < 0 || mesh_handle >= RAYLIB_MAX_MESHES || raylib_meshes_used[mesh_handle] == 0)
	{
		printf ("raylib_draw_mesh_instanced: ERROR invalid mesh handle!\n");
		return (NULL);
	}

	if (material_handle < 0 || material_handle >= RAYLIB_MAX_MATERIALS || raylib_materials_used[material_handle] == 0)
	{
		printf ("raylib_draw_mesh_instanced: ERROR invalid material handle!\n");
		return (NULL);
	}

	if (instances < 1) return (sp);

	if (memory_bounds (transforms_addr, instances * 128) != 0)
	{
		printf ("raylib_draw_mesh_instanced: ERROR memory bounds check failed!\n");
		return (NULL);
	}

	mtrans = calloc (instances, sizeof (Matrix));
	if (mtrans == NULL)
	{
		printf ("raylib_draw_mesh_instanced: ERROR out of memory!\n");
		return (NULL);
	}

	for (i = 0; i < instances; i++)
	{
		if (rlib_read_matrix (data, transforms_addr + (i * 128), &mtrans[i]) != 0)
		{
			free (mtrans);
			return (NULL);
		}
	}

	pthread_mutex_lock (&rlib_mutex);
	DrawMeshInstanced (raylib_meshes[mesh_handle], raylib_materials[material_handle], mtrans, (int) instances);
	pthread_mutex_unlock (&rlib_mutex);

	free (mtrans);
	return (sp);
}

// ---------------------------------------------------------------------------
// Materials functions
// ---------------------------------------------------------------------------

U1 *raylib_load_material_default (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// return: material handle or -1
	S8 ind ALIGN;

	ind = rlib_find_slot (raylib_materials_used, RAYLIB_MAX_MATERIALS);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_materials[ind] = LoadMaterialDefault ();
	pthread_mutex_unlock (&rlib_mutex);

	raylib_materials_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_unload_material (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: material_handle
	S8 material_handle ALIGN;

	sp = stpopi ((U1 *) &material_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (material_handle < 0 || material_handle >= RAYLIB_MAX_MATERIALS || raylib_materials_used[material_handle] == 0)
	{
		printf ("raylib_unload_material: ERROR invalid material handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	UnloadMaterial (raylib_materials[material_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	memset (&raylib_materials[material_handle], 0, sizeof (Material));
	raylib_materials_used[material_handle] = 0;

	return (sp);
}

U1 *raylib_is_material_valid (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: material_handle
	// return: 1 valid, 0 invalid
	S8 material_handle ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &material_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (material_handle < 0 || material_handle >= RAYLIB_MAX_MATERIALS || raylib_materials_used[material_handle] == 0)
	{
		ret = 0;
	}
	else
	{
		pthread_mutex_lock (&rlib_mutex);
		ret = IsMaterialValid (raylib_materials[material_handle]) ? 1 : 0;
		pthread_mutex_unlock (&rlib_mutex);
	}

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_set_material_texture (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: model_handle, material_id, map_index, texture_handle
	S8 model_handle ALIGN, material_id ALIGN, map_index ALIGN, texture_handle ALIGN;

	sp = stpopi ((U1 *) &texture_handle, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &map_index, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &material_id, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &model_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (model_handle < 0 || model_handle >= RAYLIB_MAX_MODELS || raylib_models_used[model_handle] == 0)
	{
		printf ("raylib_set_material_texture: ERROR invalid model handle!\n");
		return (NULL);
	}

	if (texture_handle < 0 || texture_handle >= RAYLIB_MAX_TEXTURES || raylib_textures_used[texture_handle] == 0)
	{
		printf ("raylib_set_material_texture: ERROR invalid texture handle!\n");
		return (NULL);
	}

	if (material_id < 0 || material_id >= raylib_models[model_handle].materialCount)
	{
		printf ("raylib_set_material_texture: ERROR invalid material id!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	SetMaterialTexture (&raylib_models[model_handle].materials[material_id], (int) map_index, raylib_textures[texture_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_set_material_color (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: model_handle, material_id, map_index, color_addr
	S8 model_handle ALIGN, material_id ALIGN, map_index ALIGN, color_addr ALIGN;
	Color col;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &map_index, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &material_id, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &model_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (model_handle < 0 || model_handle >= RAYLIB_MAX_MODELS || raylib_models_used[model_handle] == 0)
	{
		printf ("raylib_set_material_color: ERROR invalid model handle!\n");
		return (NULL);
	}

	if (material_id < 0 || material_id >= raylib_models[model_handle].materialCount)
	{
		printf ("raylib_set_material_color: ERROR invalid material id!\n");
		return (NULL);
	}

	if (rlib_read_color (data, color_addr, &col) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	raylib_models[model_handle].materials[material_id].maps[map_index].color = col;
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

// ---------------------------------------------------------------------------
// Shader functions
// ---------------------------------------------------------------------------

U1 *raylib_load_shader (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: vs_addr, fs_addr
	// return: shader handle or -1
	// empty strings (len < 2) are passed as NULL to raylib
	S8 vs_addr ALIGN, fs_addr ALIGN;
	U1 vs_buf[1024], fs_buf[1024];
	U1 vs_empty, fs_empty;
	S8 ind ALIGN;

	sp = stpopi ((U1 *) &fs_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &vs_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	vs_empty = 0;
	if (rlib_read_string (data, vs_addr, vs_buf, sizeof (vs_buf)) != 0)
	{
		vs_empty = 1;
		vs_buf[0] = '\0';
	}
	if (vs_buf[0] == '\0') vs_empty = 1;

	fs_empty = 0;
	if (rlib_read_string (data, fs_addr, fs_buf, sizeof (fs_buf)) != 0)
	{
		fs_empty = 1;
		fs_buf[0] = '\0';
	}
	if (fs_buf[0] == '\0') fs_empty = 1;

	ind = rlib_find_slot (raylib_shaders_used, RAYLIB_MAX_SHADERS);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_shaders[ind] = LoadShader (vs_empty ? NULL : (const char *) vs_buf, fs_empty ? NULL : (const char *) fs_buf);
	pthread_mutex_unlock (&rlib_mutex);

	if (raylib_shaders[ind].id == 0)
	{
		printf ("raylib_load_shader: ERROR loading shader!\n");
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	raylib_shaders_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_load_shader_from_memory (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: vs_addr, fs_addr (shader code strings)
	// return: shader handle or -1
	S8 vs_addr ALIGN, fs_addr ALIGN;
	U1 vs_buf[16384], fs_buf[16384];
	U1 vs_empty, fs_empty;
	S8 ind ALIGN;

	sp = stpopi ((U1 *) &fs_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &vs_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	vs_empty = 0;
	if (rlib_read_string (data, vs_addr, vs_buf, sizeof (vs_buf)) != 0) vs_empty = 1;
	if (vs_buf[0] == '\0') vs_empty = 1;

	fs_empty = 0;
	if (rlib_read_string (data, fs_addr, fs_buf, sizeof (fs_buf)) != 0) fs_empty = 1;
	if (fs_buf[0] == '\0') fs_empty = 1;

	ind = rlib_find_slot (raylib_shaders_used, RAYLIB_MAX_SHADERS);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_shaders[ind] = LoadShaderFromMemory (vs_empty ? NULL : (const char *) vs_buf, fs_empty ? NULL : (const char *) fs_buf);
	pthread_mutex_unlock (&rlib_mutex);

	if (raylib_shaders[ind].id == 0)
	{
		printf ("raylib_load_shader_from_memory: ERROR loading shader!\n");
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	raylib_shaders_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_unload_shader (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: shader_handle
	S8 shader_handle ALIGN;

	sp = stpopi ((U1 *) &shader_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (shader_handle < 0 || shader_handle >= RAYLIB_MAX_SHADERS || raylib_shaders_used[shader_handle] == 0)
	{
		printf ("raylib_unload_shader: ERROR invalid shader handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	UnloadShader (raylib_shaders[shader_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	memset (&raylib_shaders[shader_handle], 0, sizeof (Shader));
	raylib_shaders_used[shader_handle] = 0;

	return (sp);
}

U1 *raylib_is_shader_valid (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: shader_handle
	// return: 1 valid, 0 invalid
	S8 shader_handle ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &shader_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (shader_handle < 0 || shader_handle >= RAYLIB_MAX_SHADERS || raylib_shaders_used[shader_handle] == 0)
	{
		ret = 0;
	}
	else
	{
		pthread_mutex_lock (&rlib_mutex);
		ret = IsShaderValid (raylib_shaders[shader_handle]) ? 1 : 0;
		pthread_mutex_unlock (&rlib_mutex);
	}

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_get_shader_location (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: shader_handle, name_addr
	// return: uniform location or -1
	S8 shader_handle ALIGN, name_addr ALIGN;
	S8 loc ALIGN;
	U1 name_buf[256];

	sp = stpopi ((U1 *) &name_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &shader_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (shader_handle < 0 || shader_handle >= RAYLIB_MAX_SHADERS || raylib_shaders_used[shader_handle] == 0)
	{
		printf ("raylib_get_shader_location: ERROR invalid shader handle!\n");
		return (NULL);
	}

	if (rlib_read_string (data, name_addr, name_buf, sizeof (name_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	loc = GetShaderLocation (raylib_shaders[shader_handle], (const char *) name_buf);
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (loc, sp, sp_bottom);
	return (sp);
}

U1 *raylib_set_shader_value_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: shader_handle, loc, value
	S8 shader_handle ALIGN, loc ALIGN;
	F8 value ALIGN;

	sp = stpopd ((U1 *) &value, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &loc, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &shader_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (shader_handle < 0 || shader_handle >= RAYLIB_MAX_SHADERS || raylib_shaders_used[shader_handle] == 0)
	{
		printf ("raylib_set_shader_value_float: ERROR invalid shader handle!\n");
		return (NULL);
	}

	float f_val = (float) value;

	pthread_mutex_lock (&rlib_mutex);
	SetShaderValue (raylib_shaders[shader_handle], (int) loc, &f_val, SHADER_UNIFORM_FLOAT);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_set_shader_value_int (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: shader_handle, loc, value
	S8 shader_handle ALIGN, loc ALIGN, value ALIGN;

	sp = stpopi ((U1 *) &value, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &loc, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &shader_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (shader_handle < 0 || shader_handle >= RAYLIB_MAX_SHADERS || raylib_shaders_used[shader_handle] == 0)
	{
		printf ("raylib_set_shader_value_int: ERROR invalid shader handle!\n");
		return (NULL);
	}

	int i_val = (int) value;

	pthread_mutex_lock (&rlib_mutex);
	SetShaderValue (raylib_shaders[shader_handle], (int) loc, &i_val, SHADER_UNIFORM_INT);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_set_shader_value_vec2 (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: shader_handle, loc, vec_addr
	S8 shader_handle ALIGN, loc ALIGN, vec_addr ALIGN;
	Vector2 v;

	sp = stpopi ((U1 *) &vec_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &loc, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &shader_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (shader_handle < 0 || shader_handle >= RAYLIB_MAX_SHADERS || raylib_shaders_used[shader_handle] == 0)
	{
		printf ("raylib_set_shader_value_vec2: ERROR invalid shader handle!\n");
		return (NULL);
	}

	if (rlib_read_vec2 (data, vec_addr, &v) != 0) return (NULL);

	float arr[2] = { v.x, v.y };

	pthread_mutex_lock (&rlib_mutex);
	SetShaderValue (raylib_shaders[shader_handle], (int) loc, arr, SHADER_UNIFORM_VEC2);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_set_shader_value_vec3 (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: shader_handle, loc, vec_addr
	S8 shader_handle ALIGN, loc ALIGN, vec_addr ALIGN;
	Vector3 v;

	sp = stpopi ((U1 *) &vec_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &loc, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &shader_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (shader_handle < 0 || shader_handle >= RAYLIB_MAX_SHADERS || raylib_shaders_used[shader_handle] == 0)
	{
		printf ("raylib_set_shader_value_vec3: ERROR invalid shader handle!\n");
		return (NULL);
	}

	if (rlib_read_vec3 (data, vec_addr, &v) != 0) return (NULL);

	float arr[3] = { v.x, v.y, v.z };

	pthread_mutex_lock (&rlib_mutex);
	SetShaderValue (raylib_shaders[shader_handle], (int) loc, arr, SHADER_UNIFORM_VEC3);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_set_shader_value_vec4 (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: shader_handle, loc, vec_addr
	S8 shader_handle ALIGN, loc ALIGN, vec_addr ALIGN;
	Vector4 v;

	sp = stpopi ((U1 *) &vec_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &loc, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &shader_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (shader_handle < 0 || shader_handle >= RAYLIB_MAX_SHADERS || raylib_shaders_used[shader_handle] == 0)
	{
		printf ("raylib_set_shader_value_vec4: ERROR invalid shader handle!\n");
		return (NULL);
	}

	if (rlib_read_vec4 (data, vec_addr, &v) != 0) return (NULL);

	float arr[4] = { v.x, v.y, v.z, v.w };

	pthread_mutex_lock (&rlib_mutex);
	SetShaderValue (raylib_shaders[shader_handle], (int) loc, arr, SHADER_UNIFORM_VEC4);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_set_shader_value_matrix (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: shader_handle, loc, matrix_addr
	S8 shader_handle ALIGN, loc ALIGN, matrix_addr ALIGN;
	Matrix m;

	sp = stpopi ((U1 *) &matrix_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &loc, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &shader_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (shader_handle < 0 || shader_handle >= RAYLIB_MAX_SHADERS || raylib_shaders_used[shader_handle] == 0)
	{
		printf ("raylib_set_shader_value_matrix: ERROR invalid shader handle!\n");
		return (NULL);
	}

	if (rlib_read_matrix (data, matrix_addr, &m) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	SetShaderValueMatrix (raylib_shaders[shader_handle], (int) loc, m);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_set_shader_value_texture (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: shader_handle, loc, texture_handle
	S8 shader_handle ALIGN, loc ALIGN, texture_handle ALIGN;

	sp = stpopi ((U1 *) &texture_handle, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &loc, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &shader_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (shader_handle < 0 || shader_handle >= RAYLIB_MAX_SHADERS || raylib_shaders_used[shader_handle] == 0)
	{
		printf ("raylib_set_shader_value_texture: ERROR invalid shader handle!\n");
		return (NULL);
	}

	if (texture_handle < 0 || texture_handle >= RAYLIB_MAX_TEXTURES || raylib_textures_used[texture_handle] == 0)
	{
		printf ("raylib_set_shader_value_texture: ERROR invalid texture handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	SetShaderValueTexture (raylib_shaders[shader_handle], (int) loc, raylib_textures[texture_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

// ---------------------------------------------------------------------------
// Camera functions
// ---------------------------------------------------------------------------

U1 *raylib_set_camera (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: camera_addr (D,11 layout)
	S8 camera_addr ALIGN;
	Camera3D cam;

	sp = stpopi ((U1 *) &camera_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_camera (data, camera_addr, &cam) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	raylib_camera = cam;
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_get_camera (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: out_addr (D,11 layout)
	S8 out_addr ALIGN;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	if (rlib_write_camera (data, out_addr, raylib_camera) != 0)
	{
		pthread_mutex_unlock (&rlib_mutex);
		return (NULL);
	}
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_set_camera_mode (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: mode (CAMERA_ORBITAL, CAMERA_FREE, ...)
	S8 mode ALIGN;

	sp = stpopi ((U1 *) &mode, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	raylib_camera_mode = (int) mode;
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_update_camera (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// no args, updates the internal camera with the set camera mode
	pthread_mutex_lock (&rlib_mutex);
	UpdateCamera (&raylib_camera, raylib_camera_mode);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_update_camera_pro (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: movement_addr, rotation_addr, zoom
	S8 movement_addr ALIGN, rotation_addr ALIGN;
	F8 zoom ALIGN;
	Vector3 movement, rotation;

	sp = stpopd ((U1 *) &zoom, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &rotation_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &movement_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec3 (data, movement_addr, &movement) != 0) return (NULL);
	if (rlib_read_vec3 (data, rotation_addr, &rotation) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	UpdateCameraPro (&raylib_camera, movement, rotation, (float) zoom);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_get_world_to_screen (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: pos_addr, out_addr
	S8 pos_addr ALIGN, out_addr ALIGN;
	Vector3 pos;
	Vector2 screen;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &pos_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec3 (data, pos_addr, &pos) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	screen = GetWorldToScreen (pos, raylib_camera);
	pthread_mutex_unlock (&rlib_mutex);

	if (rlib_write_vec2 (data, out_addr, screen) != 0) return (NULL);

	return (sp);
}

U1 *raylib_get_screen_to_world_ray (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: pos2d_addr, out_addr
	S8 pos2d_addr ALIGN, out_addr ALIGN;
	Vector2 mouse;
	Ray ray;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &pos2d_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec2 (data, pos2d_addr, &mouse) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ray = GetScreenToWorldRay (mouse, raylib_camera);
	pthread_mutex_unlock (&rlib_mutex);

	if (rlib_write_ray (data, out_addr, ray) != 0) return (NULL);

	return (sp);
}

// ---------------------------------------------------------------------------
// Collision detection functions
// ---------------------------------------------------------------------------

U1 *raylib_check_collision_spheres (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: center1_addr, radius1, center2_addr, radius2
	// return: 1 collision, 0 no collision
	S8 center1_addr ALIGN, center2_addr ALIGN;
	F8 radius1 ALIGN, radius2 ALIGN;
	Vector3 c1, c2;
	S8 ret ALIGN;

	sp = stpopd ((U1 *) &radius2, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &center2_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &radius1, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &center1_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec3 (data, center1_addr, &c1) != 0) return (NULL);
	if (rlib_read_vec3 (data, center2_addr, &c2) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = CheckCollisionSpheres (c1, (float) radius1, c2, (float) radius2) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_check_collision_boxes (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: box1_addr, box2_addr
	// return: 1 collision, 0 no collision
	S8 box1_addr ALIGN, box2_addr ALIGN;
	BoundingBox b1, b2;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &box2_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &box1_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_bbox (data, box1_addr, &b1) != 0) return (NULL);
	if (rlib_read_bbox (data, box2_addr, &b2) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = CheckCollisionBoxes (b1, b2) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_check_collision_box_sphere (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: box_addr, center_addr, radius
	// return: 1 collision, 0 no collision
	S8 box_addr ALIGN, center_addr ALIGN;
	F8 radius ALIGN;
	BoundingBox box;
	Vector3 center;
	S8 ret ALIGN;

	sp = stpopd ((U1 *) &radius, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &center_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &box_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_bbox (data, box_addr, &box) != 0) return (NULL);
	if (rlib_read_vec3 (data, center_addr, &center) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = CheckCollisionBoxSphere (box, center, (float) radius) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_get_ray_collision_sphere (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: ray_addr, center_addr, radius, out_addr
	S8 ray_addr ALIGN, center_addr ALIGN, out_addr ALIGN;
	F8 radius ALIGN;
	Ray ray;
	Vector3 center;
	RayCollision col;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &radius, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &center_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &ray_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_ray (data, ray_addr, &ray) != 0) return (NULL);
	if (rlib_read_vec3 (data, center_addr, &center) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	col = GetRayCollisionSphere (ray, center, (float) radius);
	pthread_mutex_unlock (&rlib_mutex);

	if (rlib_write_raycollision (data, out_addr, col) != 0) return (NULL);

	return (sp);
}

U1 *raylib_get_ray_collision_box (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: ray_addr, box_addr, out_addr
	S8 ray_addr ALIGN, box_addr ALIGN, out_addr ALIGN;
	Ray ray;
	BoundingBox box;
	RayCollision col;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &box_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &ray_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_ray (data, ray_addr, &ray) != 0) return (NULL);
	if (rlib_read_bbox (data, box_addr, &box) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	col = GetRayCollisionBox (ray, box);
	pthread_mutex_unlock (&rlib_mutex);

	if (rlib_write_raycollision (data, out_addr, col) != 0) return (NULL);

	return (sp);
}

U1 *raylib_get_ray_collision_triangle (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: ray_addr, p1_addr, p2_addr, p3_addr, out_addr
	S8 ray_addr ALIGN, p1_addr ALIGN, p2_addr ALIGN, p3_addr ALIGN, out_addr ALIGN;
	Ray ray;
	Vector3 p1, p2, p3;
	RayCollision col;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &p3_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &p2_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &p1_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &ray_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_ray (data, ray_addr, &ray) != 0) return (NULL);
	if (rlib_read_vec3 (data, p1_addr, &p1) != 0) return (NULL);
	if (rlib_read_vec3 (data, p2_addr, &p2) != 0) return (NULL);
	if (rlib_read_vec3 (data, p3_addr, &p3) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	col = GetRayCollisionTriangle (ray, p1, p2, p3);
	pthread_mutex_unlock (&rlib_mutex);

	if (rlib_write_raycollision (data, out_addr, col) != 0) return (NULL);

	return (sp);
}

U1 *raylib_get_ray_collision_quad (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: ray_addr, p1_addr, p2_addr, p3_addr, p4_addr, out_addr
	S8 ray_addr ALIGN, p1_addr ALIGN, p2_addr ALIGN, p3_addr ALIGN, p4_addr ALIGN, out_addr ALIGN;
	Ray ray;
	Vector3 p1, p2, p3, p4;
	RayCollision col;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &p4_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &p3_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &p2_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &p1_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &ray_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_ray (data, ray_addr, &ray) != 0) return (NULL);
	if (rlib_read_vec3 (data, p1_addr, &p1) != 0) return (NULL);
	if (rlib_read_vec3 (data, p2_addr, &p2) != 0) return (NULL);
	if (rlib_read_vec3 (data, p3_addr, &p3) != 0) return (NULL);
	if (rlib_read_vec3 (data, p4_addr, &p4) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	col = GetRayCollisionQuad (ray, p1, p2, p3, p4);
	pthread_mutex_unlock (&rlib_mutex);

	if (rlib_write_raycollision (data, out_addr, col) != 0) return (NULL);

	return (sp);
}

// ---------------------------------------------------------------------------
// Vector math helper functions
// ---------------------------------------------------------------------------

U1 *raylib_vec2_add (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: v1_addr, v2_addr, out_addr
	S8 v1_addr ALIGN, v2_addr ALIGN, out_addr ALIGN;
	Vector2 a, b, r;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &v2_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &v1_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec2 (data, v1_addr, &a) != 0) return (NULL);
	if (rlib_read_vec2 (data, v2_addr, &b) != 0) return (NULL);

	r = Vector2Add (a, b);

	if (rlib_write_vec2 (data, out_addr, r) != 0) return (NULL);
	return (sp);
}

U1 *raylib_vec2_subtract (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: v1_addr, v2_addr, out_addr
	S8 v1_addr ALIGN, v2_addr ALIGN, out_addr ALIGN;
	Vector2 a, b, r;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &v2_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &v1_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec2 (data, v1_addr, &a) != 0) return (NULL);
	if (rlib_read_vec2 (data, v2_addr, &b) != 0) return (NULL);

	r = Vector2Subtract (a, b);

	if (rlib_write_vec2 (data, out_addr, r) != 0) return (NULL);
	return (sp);
}

U1 *raylib_vec2_scale (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: v_addr, scale, out_addr
	S8 v_addr ALIGN, out_addr ALIGN;
	F8 scale ALIGN;
	Vector2 a, r;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &scale, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &v_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec2 (data, v_addr, &a) != 0) return (NULL);

	r = Vector2Scale (a, (float) scale);

	if (rlib_write_vec2 (data, out_addr, r) != 0) return (NULL);
	return (sp);
}

U1 *raylib_vec3_add (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: v1_addr, v2_addr, out_addr
	S8 v1_addr ALIGN, v2_addr ALIGN, out_addr ALIGN;
	Vector3 a, b, r;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &v2_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &v1_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec3 (data, v1_addr, &a) != 0) return (NULL);
	if (rlib_read_vec3 (data, v2_addr, &b) != 0) return (NULL);

	r = Vector3Add (a, b);

	if (rlib_write_vec3 (data, out_addr, r) != 0) return (NULL);
	return (sp);
}

U1 *raylib_vec3_subtract (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: v1_addr, v2_addr, out_addr
	S8 v1_addr ALIGN, v2_addr ALIGN, out_addr ALIGN;
	Vector3 a, b, r;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &v2_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &v1_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec3 (data, v1_addr, &a) != 0) return (NULL);
	if (rlib_read_vec3 (data, v2_addr, &b) != 0) return (NULL);

	r = Vector3Subtract (a, b);

	if (rlib_write_vec3 (data, out_addr, r) != 0) return (NULL);
	return (sp);
}

U1 *raylib_vec3_scale (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: v_addr, scale, out_addr
	S8 v_addr ALIGN, out_addr ALIGN;
	F8 scale ALIGN;
	Vector3 a, r;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &scale, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &v_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec3 (data, v_addr, &a) != 0) return (NULL);

	r = Vector3Scale (a, (float) scale);

	if (rlib_write_vec3 (data, out_addr, r) != 0) return (NULL);
	return (sp);
}

U1 *raylib_vec3_normalize (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: v_addr, out_addr
	S8 v_addr ALIGN, out_addr ALIGN;
	Vector3 a, r;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &v_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec3 (data, v_addr, &a) != 0) return (NULL);

	r = Vector3Normalize (a);

	if (rlib_write_vec3 (data, out_addr, r) != 0) return (NULL);
	return (sp);
}

U1 *raylib_vec3_length (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: v_addr
	// return: length
	S8 v_addr ALIGN;
	Vector3 a;

	sp = stpopi ((U1 *) &v_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec3 (data, v_addr, &a) != 0) return (NULL);

	sp = stpushd ((F8) Vector3Length (a), sp, sp_bottom);
	return (sp);
}

U1 *raylib_vec3_dot_product (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: v1_addr, v2_addr
	// return: dot product
	S8 v1_addr ALIGN, v2_addr ALIGN;
	Vector3 a, b;

	sp = stpopi ((U1 *) &v2_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &v1_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec3 (data, v1_addr, &a) != 0) return (NULL);
	if (rlib_read_vec3 (data, v2_addr, &b) != 0) return (NULL);

	sp = stpushd ((F8) Vector3DotProduct (a, b), sp, sp_bottom);
	return (sp);
}

U1 *raylib_vec3_cross_product (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: v1_addr, v2_addr, out_addr
	S8 v1_addr ALIGN, v2_addr ALIGN, out_addr ALIGN;
	Vector3 a, b, r;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &v2_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &v1_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec3 (data, v1_addr, &a) != 0) return (NULL);
	if (rlib_read_vec3 (data, v2_addr, &b) != 0) return (NULL);

	r = Vector3CrossProduct (a, b);

	if (rlib_write_vec3 (data, out_addr, r) != 0) return (NULL);
	return (sp);
}

U1 *raylib_vec3_distance (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: v1_addr, v2_addr
	// return: distance
	S8 v1_addr ALIGN, v2_addr ALIGN;
	Vector3 a, b;

	sp = stpopi ((U1 *) &v2_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &v1_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec3 (data, v1_addr, &a) != 0) return (NULL);
	if (rlib_read_vec3 (data, v2_addr, &b) != 0) return (NULL);

	sp = stpushd ((F8) Vector3Distance (a, b), sp, sp_bottom);
	return (sp);
}

U1 *raylib_vec3_transform (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: v_addr, matrix_addr, out_addr
	S8 v_addr ALIGN, matrix_addr ALIGN, out_addr ALIGN;
	Vector3 v, r;
	Matrix m;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &matrix_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &v_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec3 (data, v_addr, &v) != 0) return (NULL);
	if (rlib_read_matrix (data, matrix_addr, &m) != 0) return (NULL);

	r = Vector3Transform (v, m);

	if (rlib_write_vec3 (data, out_addr, r) != 0) return (NULL);
	return (sp);
}

U1 *raylib_matrix_multiply (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: m1_addr, m2_addr, out_addr
	S8 m1_addr ALIGN, m2_addr ALIGN, out_addr ALIGN;
	Matrix a, b, r;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &m2_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &m1_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_matrix (data, m1_addr, &a) != 0) return (NULL);
	if (rlib_read_matrix (data, m2_addr, &b) != 0) return (NULL);

	r = MatrixMultiply (a, b);

	if (rlib_write_matrix (data, out_addr, r) != 0) return (NULL);
	return (sp);
}

U1 *raylib_matrix_translate (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: x, y, z, out_addr
	S8 out_addr ALIGN;
	F8 x ALIGN, y ALIGN, z ALIGN;
	Matrix r;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &z, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &y, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &x, sp, sp_top);
	if (sp == NULL) return (NULL);

	r = MatrixTranslate ((float) x, (float) y, (float) z);

	if (rlib_write_matrix (data, out_addr, r) != 0) return (NULL);
	return (sp);
}

U1 *raylib_matrix_rotate_xyz (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: angle_addr, out_addr
	// angle: Vector3 (x, y, z angles)
	S8 angle_addr ALIGN, out_addr ALIGN;
	Vector3 angles;
	Matrix r;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &angle_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec3 (data, angle_addr, &angles) != 0) return (NULL);

	r = MatrixRotateXYZ (angles);

	if (rlib_write_matrix (data, out_addr, r) != 0) return (NULL);
	return (sp);
}

U1 *raylib_matrix_scale (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: x, y, z, out_addr
	S8 out_addr ALIGN;
	F8 x ALIGN, y ALIGN, z ALIGN;
	Matrix r;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &z, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &y, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &x, sp, sp_top);
	if (sp == NULL) return (NULL);

	r = MatrixScale ((float) x, (float) y, (float) z);

	if (rlib_write_matrix (data, out_addr, r) != 0) return (NULL);
	return (sp);
}

U1 *raylib_matrix_identity (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: out_addr
	S8 out_addr ALIGN;
	Matrix r;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	r = MatrixIdentity ();

	if (rlib_write_matrix (data, out_addr, r) != 0) return (NULL);
	return (sp);
}

// Function collection ends here, following sections are appended.
// ---------------------------------------------------------------------------
// Window and OpenGL context functions
// ---------------------------------------------------------------------------

U1 *raylib_init_window (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: width, height, title_addr
	// return: 0 on success, 1 on error
	S8 width ALIGN, height ALIGN, title_addr ALIGN;
	U1 title[512];
	S8 ret ALIGN = 0;

	sp = stpopi ((U1 *) &title_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &height, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &width, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_string (data, title_addr, title, sizeof (title)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	InitWindow ((int) width, (int) height, (const char *) title);
	if (! IsWindowReady ())
	{
		ret = 1;
	}
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_close_window (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	pthread_mutex_lock (&rlib_mutex);
	CloseWindow ();
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_window_should_close (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 ret ALIGN;

	pthread_mutex_lock (&rlib_mutex);
	ret = WindowShouldClose () ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_is_window_ready (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 ret ALIGN;

	pthread_mutex_lock (&rlib_mutex);
	ret = IsWindowReady () ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_is_window_fullscreen (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 ret ALIGN;

	pthread_mutex_lock (&rlib_mutex);
	ret = IsWindowFullscreen () ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_is_window_hidden (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 ret ALIGN;

	pthread_mutex_lock (&rlib_mutex);
	ret = IsWindowHidden () ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_is_window_minimized (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 ret ALIGN;

	pthread_mutex_lock (&rlib_mutex);
	ret = IsWindowMinimized () ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_is_window_maximized (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 ret ALIGN;

	pthread_mutex_lock (&rlib_mutex);
	ret = IsWindowMaximized () ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_is_window_focused (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 ret ALIGN;

	pthread_mutex_lock (&rlib_mutex);
	ret = IsWindowFocused () ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_is_window_resized (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 ret ALIGN;

	pthread_mutex_lock (&rlib_mutex);
	ret = IsWindowResized () ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_is_window_state (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: flag
	S8 flag ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &flag, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = IsWindowState ((unsigned int) flag) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_set_window_state (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: flag
	S8 flag ALIGN;

	sp = stpopi ((U1 *) &flag, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	SetWindowState ((unsigned int) flag);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_clear_window_state (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: flag
	S8 flag ALIGN;

	sp = stpopi ((U1 *) &flag, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ClearWindowState ((unsigned int) flag);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_toggle_fullscreen (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	pthread_mutex_lock (&rlib_mutex);
	ToggleFullscreen ();
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_toggle_borderless_windowed (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	pthread_mutex_lock (&rlib_mutex);
	ToggleBorderlessWindowed ();
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_maximize_window (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	pthread_mutex_lock (&rlib_mutex);
	MaximizeWindow ();
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_minimize_window (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	pthread_mutex_lock (&rlib_mutex);
	MinimizeWindow ();
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_restore_window (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	pthread_mutex_lock (&rlib_mutex);
	RestoreWindow ();
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_set_window_title (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: title_addr
	S8 title_addr ALIGN;
	U1 title[512];

	sp = stpopi ((U1 *) &title_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_string (data, title_addr, title, sizeof (title)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	SetWindowTitle ((const char *) title);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_set_window_size (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: width, height
	S8 width ALIGN, height ALIGN;

	sp = stpopi ((U1 *) &height, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &width, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	SetWindowSize ((int) width, (int) height);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_set_window_min_size (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: width, height
	S8 width ALIGN, height ALIGN;

	sp = stpopi ((U1 *) &height, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &width, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	SetWindowMinSize ((int) width, (int) height);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_set_window_max_size (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: width, height
	S8 width ALIGN, height ALIGN;

	sp = stpopi ((U1 *) &height, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &width, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	SetWindowMaxSize ((int) width, (int) height);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_set_window_position (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: x, y
	S8 x ALIGN, y ALIGN;

	sp = stpopi ((U1 *) &y, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &x, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	SetWindowPosition ((int) x, (int) y);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_set_window_opacity (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: opacity
	F8 opacity ALIGN;

	sp = stpopd ((U1 *) &opacity, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	SetWindowOpacity ((float) opacity);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_get_screen_width (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 ret ALIGN;

	pthread_mutex_lock (&rlib_mutex);
	ret = GetScreenWidth ();
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_get_screen_height (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 ret ALIGN;

	pthread_mutex_lock (&rlib_mutex);
	ret = GetScreenHeight ();
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_get_render_width (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 ret ALIGN;

	pthread_mutex_lock (&rlib_mutex);
	ret = GetRenderWidth ();
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_get_render_height (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 ret ALIGN;

	pthread_mutex_lock (&rlib_mutex);
	ret = GetRenderHeight ();
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_get_fps (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 ret ALIGN;

	pthread_mutex_lock (&rlib_mutex);
	ret = GetFPS ();
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_get_frame_time (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	F8 ret ALIGN;

	pthread_mutex_lock (&rlib_mutex);
	ret = GetFrameTime ();
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushd (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_get_time (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	F8 ret ALIGN;

	pthread_mutex_lock (&rlib_mutex);
	ret = GetTime ();
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushd (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_set_target_fps (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: fps
	S8 fps ALIGN;

	sp = stpopi ((U1 *) &fps, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	SetTargetFPS ((int) fps);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_set_config_flags (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: flags
	S8 flags ALIGN;

	sp = stpopi ((U1 *) &flags, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	SetConfigFlags ((unsigned int) flags);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_begin_drawing (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	pthread_mutex_lock (&rlib_mutex);
	BeginDrawing ();
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_end_drawing (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	pthread_mutex_lock (&rlib_mutex);
	EndDrawing ();
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_clear_background (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: color_addr
	S8 color_addr ALIGN;
	Color col;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_color (data, color_addr, &col) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ClearBackground (col);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_begin_mode_3d (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// uses the internal camera set with raylib_set_camera()
	pthread_mutex_lock (&rlib_mutex);
	BeginMode3D (raylib_camera);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_end_mode_3d (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	pthread_mutex_lock (&rlib_mutex);
	EndMode3D ();
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_begin_mode_2d (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: cam2d_addr
	// cam2d layout: offset D,2, target D,2, rotation D, zoom D (6 doubles)
	S8 cam2d_addr ALIGN;
	Camera2D cam;
	F8 rotation ALIGN, zoom ALIGN;

	memset (&cam, 0, sizeof (Camera2D));

	sp = stpopi ((U1 *) &cam2d_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (memory_bounds (cam2d_addr, 48) != 0)
	{
		printf ("raylib_begin_mode_2d: ERROR memory bounds check failed!\n");
		return (NULL);
	}

	if (rlib_read_vec2 (data, cam2d_addr, &cam.offset) != 0) return (NULL);
	if (rlib_read_vec2 (data, cam2d_addr + 16, &cam.target) != 0) return (NULL);
	memcpy (&rotation, &data[cam2d_addr + 32], sizeof (F8));
	memcpy (&zoom, &data[cam2d_addr + 40], sizeof (F8));
	cam.rotation = (float) rotation;
	cam.zoom = (float) zoom;

	pthread_mutex_lock (&rlib_mutex);
	BeginMode2D (cam);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_end_mode_2d (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	pthread_mutex_lock (&rlib_mutex);
	EndMode2D ();
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_begin_blend_mode (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: mode
	S8 mode ALIGN;

	sp = stpopi ((U1 *) &mode, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	BeginBlendMode ((int) mode);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_end_blend_mode (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	pthread_mutex_lock (&rlib_mutex);
	EndBlendMode ();
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_begin_scissor_mode (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: x, y, width, height
	S8 x ALIGN, y ALIGN, width ALIGN, height ALIGN;

	sp = stpopi ((U1 *) &height, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &width, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &y, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &x, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	BeginScissorMode ((int) x, (int) y, (int) width, (int) height);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_end_scissor_mode (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	pthread_mutex_lock (&rlib_mutex);
	EndScissorMode ();
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_swap_screen_buffer (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	pthread_mutex_lock (&rlib_mutex);
	SwapScreenBuffer ();
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_poll_input_events (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	pthread_mutex_lock (&rlib_mutex);
	PollInputEvents ();
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_wait_time (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: seconds
	F8 seconds ALIGN;

	sp = stpopd ((U1 *) &seconds, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	WaitTime (seconds);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_take_screenshot (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: file_addr
	S8 file_addr ALIGN;
	U1 filename[512];

	sp = stpopi ((U1 *) &file_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_load_filename (data, file_addr, filename, sizeof (filename)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	TakeScreenshot ((const char *) filename);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_open_url (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: url_addr
	S8 url_addr ALIGN;
	U1 url[1024];

	sp = stpopi ((U1 *) &url_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_string (data, url_addr, url, sizeof (url)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	OpenURL ((const char *) url);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_set_random_seed (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: seed
	S8 seed ALIGN;

	sp = stpopi ((U1 *) &seed, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	SetRandomSeed ((unsigned int) seed);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_get_random_value (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: min, max
	// return: random value
	S8 min ALIGN, max ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &max, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &min, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = GetRandomValue ((int) min, (int) max);
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_set_trace_log_level (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: log_level
	S8 log_level ALIGN;

	sp = stpopi ((U1 *) &log_level, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	SetTraceLogLevel ((int) log_level);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_get_monitor_count (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 ret ALIGN;

	pthread_mutex_lock (&rlib_mutex);
	ret = GetMonitorCount ();
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_get_current_monitor (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 ret ALIGN;

	pthread_mutex_lock (&rlib_mutex);
	ret = GetCurrentMonitor ();
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_get_monitor_width (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: monitor
	S8 monitor ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &monitor, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = GetMonitorWidth ((int) monitor);
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_get_monitor_height (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: monitor
	S8 monitor ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &monitor, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = GetMonitorHeight ((int) monitor);
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_get_monitor_physical_width (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: monitor
	S8 monitor ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &monitor, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = GetMonitorPhysicalWidth ((int) monitor);
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_get_monitor_physical_height (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: monitor
	S8 monitor ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &monitor, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = GetMonitorPhysicalHeight ((int) monitor);
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_get_monitor_refresh_rate (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: monitor
	S8 monitor ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &monitor, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = GetMonitorRefreshRate ((int) monitor);
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_get_window_position (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: out_addr
	S8 out_addr ALIGN;
	Vector2 pos;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	pos = GetWindowPosition ();
	pthread_mutex_unlock (&rlib_mutex);

	if (rlib_write_vec2 (data, out_addr, pos) != 0) return (NULL);
	return (sp);
}

U1 *raylib_get_window_scale_dpi (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: out_addr
	S8 out_addr ALIGN;
	Vector2 scale;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	scale = GetWindowScaleDPI ();
	pthread_mutex_unlock (&rlib_mutex);

	if (rlib_write_vec2 (data, out_addr, scale) != 0) return (NULL);
	return (sp);
}

U1 *raylib_set_clipboard_text (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: text_addr
	S8 text_addr ALIGN;
	U1 text[4096];

	sp = stpopi ((U1 *) &text_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_string (data, text_addr, text, sizeof (text)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	SetClipboardText ((const char *) text);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_get_clipboard_text (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: out_addr
	S8 out_addr ALIGN;
	const char *text;
	S8 len ALIGN;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	text = GetClipboardText ();
	if (text != NULL)
	{
		len = strlen_safe (text, 4095);
	}
	else
	{
		len = 0;
	}
	pthread_mutex_unlock (&rlib_mutex);

	if (memory_bounds (out_addr, len) != 0)
	{
		printf ("raylib_get_clipboard_text: ERROR memory bounds check failed!\n");
		return (NULL);
	}

	if (len > 0) memcpy (&data[out_addr], text, len);
	data[out_addr + len] = '\0';

	return (sp);
}

// ---------------------------------------------------------------------------
// 2D basic shapes drawing functions
// ---------------------------------------------------------------------------

U1 *raylib_draw_pixel (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: x, y, color_addr
	S8 x ALIGN, y ALIGN, color_addr ALIGN;
	Color col;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &y, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &x, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_color (data, color_addr, &col) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawPixel ((int) x, (int) y, col);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_pixel_v (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: pos_addr, color_addr
	S8 pos_addr ALIGN, color_addr ALIGN;
	Vector2 pos;
	Color col;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &pos_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec2 (data, pos_addr, &pos) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &col) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawPixelV (pos, col);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_line (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: x1, y1, x2, y2, color_addr
	S8 x1 ALIGN, y1 ALIGN, x2 ALIGN, y2 ALIGN, color_addr ALIGN;
	Color col;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &y2, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &x2, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &y1, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &x1, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_color (data, color_addr, &col) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawLine ((int) x1, (int) y1, (int) x2, (int) y2, col);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_line_v (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: start_addr, end_addr, color_addr
	S8 start_addr ALIGN, end_addr ALIGN, color_addr ALIGN;
	Vector2 start, end;
	Color col;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &end_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &start_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec2 (data, start_addr, &start) != 0) return (NULL);
	if (rlib_read_vec2 (data, end_addr, &end) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &col) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawLineV (start, end, col);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_line_ex (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: start_addr, end_addr, thick, color_addr
	S8 start_addr ALIGN, end_addr ALIGN, color_addr ALIGN;
	F8 thick ALIGN;
	Vector2 start, end;
	Color col;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &thick, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &end_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &start_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec2 (data, start_addr, &start) != 0) return (NULL);
	if (rlib_read_vec2 (data, end_addr, &end) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &col) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawLineEx (start, end, (float) thick, col);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_line_strip (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: points_addr, point_count, color_addr
	// points: consecutive D,2 vectors
	S8 points_addr ALIGN, color_addr ALIGN;
	S8 point_count ALIGN;
	Vector2 *points = NULL;
	Color col;
	S8 i ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &point_count, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &points_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (point_count < 2) return (sp);

	if (memory_bounds (points_addr, point_count * 16) != 0)
	{
		printf ("raylib_draw_line_strip: ERROR memory bounds check failed!\n");
		return (NULL);
	}

	points = calloc (point_count, sizeof (Vector2));
	if (points == NULL)
	{
		printf ("raylib_draw_line_strip: ERROR out of memory!\n");
		return (NULL);
	}

	for (i = 0; i < point_count; i++)
	{
		points[i].x = (float) *(F8 *) &data[points_addr + (i * 16)];
		points[i].y = (float) *(F8 *) &data[points_addr + (i * 16) + 8];
	}

	if (rlib_read_color (data, color_addr, &col) != 0)
	{
		free (points);
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	DrawLineStrip (points, (int) point_count, col);
	pthread_mutex_unlock (&rlib_mutex);

	free (points);
	return (sp);
}

U1 *raylib_draw_circle (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: cx, cy, radius, color_addr
	S8 cx ALIGN, cy ALIGN, color_addr ALIGN;
	F8 radius ALIGN;
	Color col;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &radius, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &cy, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &cx, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_color (data, color_addr, &col) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawCircle ((int) cx, (int) cy, (float) radius, col);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_circle_v (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: center_addr, radius, color_addr
	S8 center_addr ALIGN, color_addr ALIGN;
	F8 radius ALIGN;
	Vector2 center;
	Color col;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &radius, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &center_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec2 (data, center_addr, &center) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &col) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawCircleV (center, (float) radius, col);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_circle_lines (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: cx, cy, radius, color_addr
	S8 cx ALIGN, cy ALIGN, color_addr ALIGN;
	F8 radius ALIGN;
	Color col;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &radius, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &cy, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &cx, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_color (data, color_addr, &col) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawCircleLines ((int) cx, (int) cy, (float) radius, col);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_circle_lines_v (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: center_addr, radius, color_addr
	S8 center_addr ALIGN, color_addr ALIGN;
	F8 radius ALIGN;
	Vector2 center;
	Color col;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &radius, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &center_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec2 (data, center_addr, &center) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &col) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawCircleLinesV (center, (float) radius, col);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_circle_sector (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: center_addr, radius, start_angle, end_angle, segments, color_addr
	S8 center_addr ALIGN, color_addr ALIGN;
	F8 radius ALIGN, start_angle ALIGN, end_angle ALIGN;
	S8 segments ALIGN;
	Vector2 center;
	Color col;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &segments, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &end_angle, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &start_angle, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &radius, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &center_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec2 (data, center_addr, &center) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &col) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawCircleSector (center, (float) radius, (float) start_angle, (float) end_angle, (int) segments, col);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_circle_gradient (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: center_addr, radius, inner_addr, outer_addr
	S8 center_addr ALIGN, inner_addr ALIGN, outer_addr ALIGN;
	F8 radius ALIGN;
	Vector2 center;
	Color inner, outer;

	sp = stpopi ((U1 *) &outer_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &inner_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &radius, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &center_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec2 (data, center_addr, &center) != 0) return (NULL);
	if (rlib_read_color (data, inner_addr, &inner) != 0) return (NULL);
	if (rlib_read_color (data, outer_addr, &outer) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawCircleGradient (center, (float) radius, inner, outer);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_rectangle (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: x, y, width, height, color_addr
	S8 x ALIGN, y ALIGN, width ALIGN, height ALIGN, color_addr ALIGN;
	Color col;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &height, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &width, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &y, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &x, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_color (data, color_addr, &col) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawRectangle ((int) x, (int) y, (int) width, (int) height, col);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_rectangle_v (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: pos_addr, size_addr, color_addr
	S8 pos_addr ALIGN, size_addr ALIGN, color_addr ALIGN;
	Vector2 pos, size;
	Color col;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &size_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &pos_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec2 (data, pos_addr, &pos) != 0) return (NULL);
	if (rlib_read_vec2 (data, size_addr, &size) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &col) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawRectangleV (pos, size, col);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_rectangle_rec (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: rec_addr, color_addr
	S8 rec_addr ALIGN, color_addr ALIGN;
	Rectangle rec;
	Color col;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &rec_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_rect (data, rec_addr, &rec) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &col) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawRectangleRec (rec, col);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_rectangle_pro (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: rec_addr, origin_addr, rotation, color_addr
	S8 rec_addr ALIGN, origin_addr ALIGN, color_addr ALIGN;
	F8 rotation ALIGN;
	Rectangle rec;
	Vector2 origin;
	Color col;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &rotation, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &origin_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &rec_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_rect (data, rec_addr, &rec) != 0) return (NULL);
	if (rlib_read_vec2 (data, origin_addr, &origin) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &col) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawRectanglePro (rec, origin, (float) rotation, col);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_rectangle_lines (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: x, y, width, height, color_addr
	S8 x ALIGN, y ALIGN, width ALIGN, height ALIGN, color_addr ALIGN;
	Color col;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &height, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &width, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &y, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &x, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_color (data, color_addr, &col) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawRectangleLines ((int) x, (int) y, (int) width, (int) height, col);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_rectangle_lines_ex (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: rec_addr, thick, color_addr
	S8 rec_addr ALIGN, color_addr ALIGN;
	F8 thick ALIGN;
	Rectangle rec;
	Color col;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &thick, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &rec_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_rect (data, rec_addr, &rec) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &col) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawRectangleLinesEx (rec, (float) thick, col);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_rectangle_rounded (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: rec_addr, roundness, segments, color_addr
	S8 rec_addr ALIGN, color_addr ALIGN;
	F8 roundness ALIGN;
	S8 segments ALIGN;
	Rectangle rec;
	Color col;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &segments, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &roundness, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &rec_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_rect (data, rec_addr, &rec) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &col) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawRectangleRounded (rec, (float) roundness, (int) segments, col);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_poly (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: center_addr, sides, radius, rotation, color_addr
	S8 center_addr ALIGN, sides ALIGN, color_addr ALIGN;
	F8 radius ALIGN, rotation ALIGN;
	Vector2 center;
	Color col;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &rotation, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &radius, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &sides, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &center_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec2 (data, center_addr, &center) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &col) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawPoly (center, (int) sides, (float) radius, (float) rotation, col);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_poly_lines (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: center_addr, sides, radius, rotation, color_addr
	S8 center_addr ALIGN, sides ALIGN, color_addr ALIGN;
	F8 radius ALIGN, rotation ALIGN;
	Vector2 center;
	Color col;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &rotation, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &radius, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &sides, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &center_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec2 (data, center_addr, &center) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &col) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawPolyLines (center, (int) sides, (float) radius, (float) rotation, col);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_triangle (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: v1_addr, v2_addr, v3_addr, color_addr
	S8 v1_addr ALIGN, v2_addr ALIGN, v3_addr ALIGN, color_addr ALIGN;
	Vector2 v1, v2, v3;
	Color col;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &v3_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &v2_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &v1_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec2 (data, v1_addr, &v1) != 0) return (NULL);
	if (rlib_read_vec2 (data, v2_addr, &v2) != 0) return (NULL);
	if (rlib_read_vec2 (data, v3_addr, &v3) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &col) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawTriangle (v1, v2, v3, col);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_triangle_lines (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: v1_addr, v2_addr, v3_addr, color_addr
	S8 v1_addr ALIGN, v2_addr ALIGN, v3_addr ALIGN, color_addr ALIGN;
	Vector2 v1, v2, v3;
	Color col;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &v3_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &v2_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &v1_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec2 (data, v1_addr, &v1) != 0) return (NULL);
	if (rlib_read_vec2 (data, v2_addr, &v2) != 0) return (NULL);
	if (rlib_read_vec2 (data, v3_addr, &v3) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &col) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawTriangleLines (v1, v2, v3, col);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_ellipse (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: cx, cy, rh, rv, color_addr
	S8 cx ALIGN, cy ALIGN, color_addr ALIGN;
	F8 rh ALIGN, rv ALIGN;
	Color col;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &rv, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &rh, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &cy, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &cx, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_color (data, color_addr, &col) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawEllipse ((int) cx, (int) cy, (float) rh, (float) rv, col);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_ellipse_lines (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: cx, cy, rh, rv, color_addr
	S8 cx ALIGN, cy ALIGN, color_addr ALIGN;
	F8 rh ALIGN, rv ALIGN;
	Color col;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &rv, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &rh, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &cy, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &cx, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_color (data, color_addr, &col) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawEllipseLines ((int) cx, (int) cy, (float) rh, (float) rv, col);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_ring (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: center_addr, inner_radius, outer_radius, start_angle, end_angle, segments, color_addr
	S8 center_addr ALIGN, color_addr ALIGN;
	F8 inner_radius ALIGN, outer_radius ALIGN, start_angle ALIGN, end_angle ALIGN;
	S8 segments ALIGN;
	Vector2 center;
	Color col;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &segments, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &end_angle, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &start_angle, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &outer_radius, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &inner_radius, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &center_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec2 (data, center_addr, &center) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &col) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawRing (center, (float) inner_radius, (float) outer_radius, (float) start_angle, (float) end_angle, (int) segments, col);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_ring_lines (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: center_addr, inner_radius, outer_radius, start_angle, end_angle, segments, color_addr
	S8 center_addr ALIGN, color_addr ALIGN;
	F8 inner_radius ALIGN, outer_radius ALIGN, start_angle ALIGN, end_angle ALIGN;
	S8 segments ALIGN;
	Vector2 center;
	Color col;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &segments, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &end_angle, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &start_angle, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &outer_radius, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &inner_radius, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &center_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec2 (data, center_addr, &center) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &col) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawRingLines (center, (float) inner_radius, (float) outer_radius, (float) start_angle, (float) end_angle, (int) segments, col);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_fps (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: x, y
	S8 x ALIGN, y ALIGN;

	sp = stpopi ((U1 *) &y, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &x, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawFPS ((int) x, (int) y);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_text (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: text_addr, x, y, font_size, color_addr
	S8 text_addr ALIGN, x ALIGN, y ALIGN, font_size ALIGN, color_addr ALIGN;
	U1 text[4096];
	Color col;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &font_size, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &y, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &x, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &text_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_string (data, text_addr, text, sizeof (text)) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &col) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawText ((const char *) text, (int) x, (int) y, (int) font_size, col);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_measure_text (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: text_addr, font_size
	// return: text width in pixels
	S8 text_addr ALIGN, font_size ALIGN;
	S8 ret ALIGN;
	U1 text[4096];

	sp = stpopi ((U1 *) &font_size, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &text_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_string (data, text_addr, text, sizeof (text)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = MeasureText ((const char *) text, (int) font_size);
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_draw_text_ex (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: font_handle, text_addr, pos_addr, font_size, spacing, tint_addr
	S8 font_handle ALIGN, text_addr ALIGN, pos_addr ALIGN, tint_addr ALIGN;
	F8 font_size ALIGN, spacing ALIGN;
	U1 text[4096];
	Vector2 pos;
	Color tint;

	sp = stpopi ((U1 *) &tint_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &spacing, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &font_size, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &pos_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &text_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &font_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (font_handle < 0 || font_handle >= RAYLIB_MAX_FONTS || raylib_fonts_used[font_handle] == 0)
	{
		printf ("raylib_draw_text_ex: ERROR invalid font handle!\n");
		return (NULL);
	}

	if (rlib_read_string (data, text_addr, text, sizeof (text)) != 0) return (NULL);
	if (rlib_read_vec2 (data, pos_addr, &pos) != 0) return (NULL);
	if (rlib_read_color (data, tint_addr, &tint) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawTextEx (raylib_fonts[font_handle], (const char *) text, pos, (float) font_size, (float) spacing, tint);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_measure_text_ex (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: font_handle, text_addr, font_size, spacing, out_addr
	S8 font_handle ALIGN, text_addr ALIGN, out_addr ALIGN;
	F8 font_size ALIGN, spacing ALIGN;
	U1 text[4096];
	Vector2 size;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &spacing, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &font_size, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &text_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &font_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (font_handle < 0 || font_handle >= RAYLIB_MAX_FONTS || raylib_fonts_used[font_handle] == 0)
	{
		printf ("raylib_measure_text_ex: ERROR invalid font handle!\n");
		return (NULL);
	}

	if (rlib_read_string (data, text_addr, text, sizeof (text)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	size = MeasureTextEx (raylib_fonts[font_handle], (const char *) text, (float) font_size, (float) spacing);
	pthread_mutex_unlock (&rlib_mutex);

	if (rlib_write_vec2 (data, out_addr, size) != 0) return (NULL);
	return (sp);
}

U1 *raylib_draw_text_pro (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: font_handle, text_addr, pos_addr, origin_addr, rotation, font_size, spacing, tint_addr
	S8 font_handle ALIGN, text_addr ALIGN, pos_addr ALIGN, origin_addr ALIGN, tint_addr ALIGN;
	F8 rotation ALIGN, font_size ALIGN, spacing ALIGN;
	U1 text[4096];
	Vector2 pos, origin;
	Color tint;

	sp = stpopi ((U1 *) &tint_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &spacing, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &font_size, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &rotation, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &origin_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &pos_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &text_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &font_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (font_handle < 0 || font_handle >= RAYLIB_MAX_FONTS || raylib_fonts_used[font_handle] == 0)
	{
		printf ("raylib_draw_text_pro: ERROR invalid font handle!\n");
		return (NULL);
	}

	if (rlib_read_string (data, text_addr, text, sizeof (text)) != 0) return (NULL);
	if (rlib_read_vec2 (data, pos_addr, &pos) != 0) return (NULL);
	if (rlib_read_vec2 (data, origin_addr, &origin) != 0) return (NULL);
	if (rlib_read_color (data, tint_addr, &tint) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawTextPro (raylib_fonts[font_handle], (const char *) text, pos, origin, (float) rotation, (float) font_size, (float) spacing, tint);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

// ---------------------------------------------------------------------------
// Texture functions
// ---------------------------------------------------------------------------

U1 *raylib_load_texture (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: file_addr
	// return: texture handle or -1
	S8 file_addr ALIGN;
	U1 filename[512];
	S8 ind ALIGN;

	sp = stpopi ((U1 *) &file_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_load_filename (data, file_addr, filename, sizeof (filename)) != 0) return (NULL);

	ind = rlib_find_slot (raylib_textures_used, RAYLIB_MAX_TEXTURES);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_textures[ind] = LoadTexture ((const char *) filename);
	pthread_mutex_unlock (&rlib_mutex);

	if (raylib_textures[ind].id == 0)
	{
		printf ("raylib_load_texture: ERROR loading texture: %s!\n", filename);
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	raylib_textures_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_load_texture_from_image (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: image_handle
	// return: texture handle or -1
	S8 image_handle ALIGN;
	S8 ind ALIGN;

	sp = stpopi ((U1 *) &image_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (image_handle < 0 || image_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[image_handle] == 0)
	{
		printf ("raylib_load_texture_from_image: ERROR invalid image handle!\n");
		return (NULL);
	}

	ind = rlib_find_slot (raylib_textures_used, RAYLIB_MAX_TEXTURES);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_textures[ind] = LoadTextureFromImage (raylib_images[image_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	raylib_textures_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_unload_texture (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: texture_handle
	S8 texture_handle ALIGN;

	sp = stpopi ((U1 *) &texture_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (texture_handle < 0 || texture_handle >= RAYLIB_MAX_TEXTURES || raylib_textures_used[texture_handle] == 0)
	{
		printf ("raylib_unload_texture: ERROR invalid texture handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	UnloadTexture (raylib_textures[texture_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	memset (&raylib_textures[texture_handle], 0, sizeof (Texture2D));
	raylib_textures_used[texture_handle] = 0;

	return (sp);
}

U1 *raylib_is_texture_valid (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: texture_handle
	// return: 1 valid, 0 invalid
	S8 texture_handle ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &texture_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (texture_handle < 0 || texture_handle >= RAYLIB_MAX_TEXTURES || raylib_textures_used[texture_handle] == 0)
	{
		ret = 0;
	}
	else
	{
		pthread_mutex_lock (&rlib_mutex);
		ret = IsTextureValid (raylib_textures[texture_handle]) ? 1 : 0;
		pthread_mutex_unlock (&rlib_mutex);
	}

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_draw_texture (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: texture_handle, x, y, tint_addr
	S8 texture_handle ALIGN, x ALIGN, y ALIGN, tint_addr ALIGN;
	Color tint;

	sp = stpopi ((U1 *) &tint_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &y, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &x, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &texture_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (texture_handle < 0 || texture_handle >= RAYLIB_MAX_TEXTURES || raylib_textures_used[texture_handle] == 0)
	{
		printf ("raylib_draw_texture: ERROR invalid texture handle!\n");
		return (NULL);
	}

	if (rlib_read_color (data, tint_addr, &tint) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawTexture (raylib_textures[texture_handle], (int) x, (int) y, tint);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_texture_v (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: texture_handle, pos_addr, tint_addr
	S8 texture_handle ALIGN, pos_addr ALIGN, tint_addr ALIGN;
	Vector2 pos;
	Color tint;

	sp = stpopi ((U1 *) &tint_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &pos_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &texture_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (texture_handle < 0 || texture_handle >= RAYLIB_MAX_TEXTURES || raylib_textures_used[texture_handle] == 0)
	{
		printf ("raylib_draw_texture_v: ERROR invalid texture handle!\n");
		return (NULL);
	}

	if (rlib_read_vec2 (data, pos_addr, &pos) != 0) return (NULL);
	if (rlib_read_color (data, tint_addr, &tint) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawTextureV (raylib_textures[texture_handle], pos, tint);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_texture_ex (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: texture_handle, pos_addr, rotation, scale, tint_addr
	S8 texture_handle ALIGN, pos_addr ALIGN, tint_addr ALIGN;
	F8 rotation ALIGN, scale ALIGN;
	Vector2 pos;
	Color tint;

	sp = stpopi ((U1 *) &tint_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &scale, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &rotation, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &pos_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &texture_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (texture_handle < 0 || texture_handle >= RAYLIB_MAX_TEXTURES || raylib_textures_used[texture_handle] == 0)
	{
		printf ("raylib_draw_texture_ex: ERROR invalid texture handle!\n");
		return (NULL);
	}

	if (rlib_read_vec2 (data, pos_addr, &pos) != 0) return (NULL);
	if (rlib_read_color (data, tint_addr, &tint) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawTextureEx (raylib_textures[texture_handle], pos, (float) rotation, (float) scale, tint);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_texture_rec (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: texture_handle, rec_addr, pos_addr, tint_addr
	S8 texture_handle ALIGN, rec_addr ALIGN, pos_addr ALIGN, tint_addr ALIGN;
	Rectangle rec;
	Vector2 pos;
	Color tint;

	sp = stpopi ((U1 *) &tint_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &pos_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &rec_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &texture_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (texture_handle < 0 || texture_handle >= RAYLIB_MAX_TEXTURES || raylib_textures_used[texture_handle] == 0)
	{
		printf ("raylib_draw_texture_rec: ERROR invalid texture handle!\n");
		return (NULL);
	}

	if (rlib_read_rect (data, rec_addr, &rec) != 0) return (NULL);
	if (rlib_read_vec2 (data, pos_addr, &pos) != 0) return (NULL);
	if (rlib_read_color (data, tint_addr, &tint) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawTextureRec (raylib_textures[texture_handle], rec, pos, tint);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_texture_pro (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: texture_handle, src_addr, dst_addr, origin_addr, rotation, tint_addr
	S8 texture_handle ALIGN, src_addr ALIGN, dst_addr ALIGN, origin_addr ALIGN, tint_addr ALIGN;
	F8 rotation ALIGN;
	Rectangle src, dst;
	Vector2 origin;
	Color tint;

	sp = stpopi ((U1 *) &tint_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &rotation, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &origin_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &dst_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &src_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &texture_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (texture_handle < 0 || texture_handle >= RAYLIB_MAX_TEXTURES || raylib_textures_used[texture_handle] == 0)
	{
		printf ("raylib_draw_texture_pro: ERROR invalid texture handle!\n");
		return (NULL);
	}

	if (rlib_read_rect (data, src_addr, &src) != 0) return (NULL);
	if (rlib_read_rect (data, dst_addr, &dst) != 0) return (NULL);
	if (rlib_read_vec2 (data, origin_addr, &origin) != 0) return (NULL);
	if (rlib_read_color (data, tint_addr, &tint) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawTexturePro (raylib_textures[texture_handle], src, dst, origin, (float) rotation, tint);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_set_texture_filter (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: texture_handle, filter
	S8 texture_handle ALIGN, filter ALIGN;

	sp = stpopi ((U1 *) &filter, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &texture_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (texture_handle < 0 || texture_handle >= RAYLIB_MAX_TEXTURES || raylib_textures_used[texture_handle] == 0)
	{
		printf ("raylib_set_texture_filter: ERROR invalid texture handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	SetTextureFilter (raylib_textures[texture_handle], (int) filter);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_set_texture_wrap (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: texture_handle, wrap
	S8 texture_handle ALIGN, wrap ALIGN;

	sp = stpopi ((U1 *) &wrap, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &texture_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (texture_handle < 0 || texture_handle >= RAYLIB_MAX_TEXTURES || raylib_textures_used[texture_handle] == 0)
	{
		printf ("raylib_set_texture_wrap: ERROR invalid texture handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	SetTextureWrap (raylib_textures[texture_handle], (int) wrap);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

// ---------------------------------------------------------------------------
// Image functions
// ---------------------------------------------------------------------------

U1 *raylib_load_image (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: file_addr
	// return: image handle or -1
	S8 file_addr ALIGN;
	U1 filename[512];
	S8 ind ALIGN;

	sp = stpopi ((U1 *) &file_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_load_filename (data, file_addr, filename, sizeof (filename)) != 0) return (NULL);

	ind = rlib_find_slot (raylib_images_used, RAYLIB_MAX_IMAGES);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_images[ind] = LoadImage ((const char *) filename);
	pthread_mutex_unlock (&rlib_mutex);

	if (raylib_images[ind].data == NULL)
	{
		printf ("raylib_load_image: ERROR loading image: %s!\n", filename);
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	raylib_images_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_load_image_from_texture (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: texture_handle
	// return: image handle or -1
	S8 texture_handle ALIGN;
	S8 ind ALIGN;

	sp = stpopi ((U1 *) &texture_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (texture_handle < 0 || texture_handle >= RAYLIB_MAX_TEXTURES || raylib_textures_used[texture_handle] == 0)
	{
		printf ("raylib_load_image_from_texture: ERROR invalid texture handle!\n");
		return (NULL);
	}

	ind = rlib_find_slot (raylib_images_used, RAYLIB_MAX_IMAGES);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_images[ind] = LoadImageFromTexture (raylib_textures[texture_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	raylib_images_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_unload_image (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: image_handle
	S8 image_handle ALIGN;

	sp = stpopi ((U1 *) &image_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (image_handle < 0 || image_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[image_handle] == 0)
	{
		printf ("raylib_unload_image: ERROR invalid image handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	UnloadImage (raylib_images[image_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	memset (&raylib_images[image_handle], 0, sizeof (Image));
	raylib_images_used[image_handle] = 0;

	return (sp);
}

U1 *raylib_is_image_valid (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: image_handle
	// return: 1 valid, 0 invalid
	S8 image_handle ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &image_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (image_handle < 0 || image_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[image_handle] == 0)
	{
		ret = 0;
	}
	else
	{
		pthread_mutex_lock (&rlib_mutex);
		ret = IsImageValid (raylib_images[image_handle]) ? 1 : 0;
		pthread_mutex_unlock (&rlib_mutex);
	}

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_gen_image_color (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: width, height, color_addr
	// return: image handle or -1
	S8 width ALIGN, height ALIGN, color_addr ALIGN;
	Color col;
	S8 ind ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &height, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &width, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_color (data, color_addr, &col) != 0) return (NULL);

	ind = rlib_find_slot (raylib_images_used, RAYLIB_MAX_IMAGES);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_images[ind] = GenImageColor ((int) width, (int) height, col);
	pthread_mutex_unlock (&rlib_mutex);

	raylib_images_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_gen_image_checked (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: width, height, checks_x, checks_y, color1_addr, color2_addr
	// return: image handle or -1
	S8 width ALIGN, height ALIGN, checks_x ALIGN, checks_y ALIGN;
	S8 color1_addr ALIGN, color2_addr ALIGN;
	Color c1, c2;
	S8 ind ALIGN;

	sp = stpopi ((U1 *) &color2_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &color1_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &checks_y, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &checks_x, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &height, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &width, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_color (data, color1_addr, &c1) != 0) return (NULL);
	if (rlib_read_color (data, color2_addr, &c2) != 0) return (NULL);

	ind = rlib_find_slot (raylib_images_used, RAYLIB_MAX_IMAGES);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_images[ind] = GenImageChecked ((int) width, (int) height, (int) checks_x, (int) checks_y, c1, c2);
	pthread_mutex_unlock (&rlib_mutex);

	raylib_images_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

// ---------------------------------------------------------------------------
// Font functions
// ---------------------------------------------------------------------------

U1 *raylib_get_font_default (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	pthread_mutex_lock (&rlib_mutex);
	rlib_get_default_font ();
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (0, sp, sp_bottom);
	return (sp);
}

U1 *raylib_load_font (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: file_addr
	// return: font handle or -1
	S8 file_addr ALIGN;
	U1 filename[512];
	S8 ind ALIGN;

	sp = stpopi ((U1 *) &file_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_load_filename (data, file_addr, filename, sizeof (filename)) != 0) return (NULL);

	ind = rlib_find_slot (raylib_fonts_used, RAYLIB_MAX_FONTS);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_fonts[ind] = LoadFont ((const char *) filename);
	pthread_mutex_unlock (&rlib_mutex);

	if (raylib_fonts[ind].glyphCount == 0)
	{
		printf ("raylib_load_font: ERROR loading font: %s!\n", filename);
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	raylib_fonts_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_load_font_ex (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: file_addr, font_size, codepoint_count
	// return: font handle or -1
	S8 file_addr ALIGN;
	S8 font_size ALIGN, codepoint_count ALIGN;
	U1 filename[512];
	S8 ind ALIGN;

	sp = stpopi ((U1 *) &codepoint_count, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &font_size, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &file_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_load_filename (data, file_addr, filename, sizeof (filename)) != 0) return (NULL);

	ind = rlib_find_slot (raylib_fonts_used, RAYLIB_MAX_FONTS);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_fonts[ind] = LoadFontEx ((const char *) filename, (int) font_size, NULL, (int) codepoint_count);
	pthread_mutex_unlock (&rlib_mutex);

	if (raylib_fonts[ind].glyphCount == 0)
	{
		printf ("raylib_load_font_ex: ERROR loading font: %s!\n", filename);
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	raylib_fonts_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_unload_font (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: font_handle
	S8 font_handle ALIGN;

	sp = stpopi ((U1 *) &font_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (font_handle < 0 || font_handle >= RAYLIB_MAX_FONTS || raylib_fonts_used[font_handle] == 0)
	{
		printf ("raylib_unload_font: ERROR invalid font handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	UnloadFont (raylib_fonts[font_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	memset (&raylib_fonts[font_handle], 0, sizeof (Font));
	raylib_fonts_used[font_handle] = 0;

	return (sp);
}

U1 *raylib_is_font_valid (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: font_handle
	// return: 1 valid, 0 invalid
	S8 font_handle ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &font_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (font_handle < 0 || font_handle >= RAYLIB_MAX_FONTS || raylib_fonts_used[font_handle] == 0)
	{
		ret = 0;
	}
	else
	{
		pthread_mutex_lock (&rlib_mutex);
		ret = IsFontValid (raylib_fonts[font_handle]) ? 1 : 0;
		pthread_mutex_unlock (&rlib_mutex);
	}

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

// ---------------------------------------------------------------------------
// Input functions: keyboard
// ---------------------------------------------------------------------------

U1 *raylib_is_key_pressed (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: key
	// return: 1 pressed, 0 not pressed
	S8 key ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &key, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = IsKeyPressed ((int) key) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_is_key_down (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: key
	// return: 1 down, 0 not down
	S8 key ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &key, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = IsKeyDown ((int) key) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_is_key_released (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: key
	// return: 1 released, 0 not released
	S8 key ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &key, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = IsKeyReleased ((int) key) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_is_key_up (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: key
	// return: 1 up, 0 not up
	S8 key ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &key, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = IsKeyUp ((int) key) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_get_key_pressed (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 ret ALIGN;

	pthread_mutex_lock (&rlib_mutex);
	ret = GetKeyPressed ();
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_get_char_pressed (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 ret ALIGN;

	pthread_mutex_lock (&rlib_mutex);
	ret = GetCharPressed ();
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_set_exit_key (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: key
	S8 key ALIGN;

	sp = stpopi ((U1 *) &key, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	SetExitKey ((int) key);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_get_key_name (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: key, out_addr
	S8 key ALIGN, out_addr ALIGN;
	const char *name;
	S8 len ALIGN;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &key, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	name = GetKeyName ((int) key);
	if (name != NULL)
	{
		len = strlen_safe (name, 255);
	}
	else
	{
		len = 0;
	}
	pthread_mutex_unlock (&rlib_mutex);

	if (memory_bounds (out_addr, len) != 0)
	{
		printf ("raylib_get_key_name: ERROR memory bounds check failed!\n");
		return (NULL);
	}

	if (len > 0) memcpy (&data[out_addr], name, len);
	data[out_addr + len] = '\0';

	return (sp);
}

// ---------------------------------------------------------------------------
// Input functions: mouse
// ---------------------------------------------------------------------------

U1 *raylib_is_mouse_button_pressed (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: button
	// return: 1 pressed, 0 not pressed
	S8 button ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &button, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = IsMouseButtonPressed ((int) button) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_is_mouse_button_down (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: button
	// return: 1 down, 0 not down
	S8 button ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &button, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = IsMouseButtonDown ((int) button) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_is_mouse_button_released (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: button
	// return: 1 released, 0 not released
	S8 button ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &button, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = IsMouseButtonReleased ((int) button) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_is_mouse_button_up (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: button
	// return: 1 up, 0 not up
	S8 button ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &button, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = IsMouseButtonUp ((int) button) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_get_mouse_x (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 ret ALIGN;

	pthread_mutex_lock (&rlib_mutex);
	ret = GetMouseX ();
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_get_mouse_y (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 ret ALIGN;

	pthread_mutex_lock (&rlib_mutex);
	ret = GetMouseY ();
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_get_mouse_position (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: out_addr
	S8 out_addr ALIGN;
	Vector2 pos;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	pos = GetMousePosition ();
	pthread_mutex_unlock (&rlib_mutex);

	if (rlib_write_vec2 (data, out_addr, pos) != 0) return (NULL);
	return (sp);
}

U1 *raylib_get_mouse_delta (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: out_addr
	S8 out_addr ALIGN;
	Vector2 delta;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	delta = GetMouseDelta ();
	pthread_mutex_unlock (&rlib_mutex);

	if (rlib_write_vec2 (data, out_addr, delta) != 0) return (NULL);
	return (sp);
}

U1 *raylib_get_mouse_wheel_move (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	F8 ret ALIGN;

	pthread_mutex_lock (&rlib_mutex);
	ret = GetMouseWheelMove ();
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushd (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_set_mouse_position (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: x, y
	S8 x ALIGN, y ALIGN;

	sp = stpopi ((U1 *) &y, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &x, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	SetMousePosition ((int) x, (int) y);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_set_mouse_cursor (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: cursor
	S8 cursor ALIGN;

	sp = stpopi ((U1 *) &cursor, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	SetMouseCursor ((int) cursor);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

// ---------------------------------------------------------------------------
// Input functions: gamepad
// ---------------------------------------------------------------------------

U1 *raylib_is_gamepad_available (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: gamepad
	// return: 1 available, 0 not available
	S8 gamepad ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &gamepad, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = IsGamepadAvailable ((int) gamepad) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_is_gamepad_button_pressed (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: gamepad, button
	// return: 1 pressed, 0 not pressed
	S8 gamepad ALIGN, button ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &button, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &gamepad, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = IsGamepadButtonPressed ((int) gamepad, (int) button) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_is_gamepad_button_down (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: gamepad, button
	// return: 1 down, 0 not down
	S8 gamepad ALIGN, button ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &button, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &gamepad, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = IsGamepadButtonDown ((int) gamepad, (int) button) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_is_gamepad_button_released (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: gamepad, button
	// return: 1 released, 0 not released
	S8 gamepad ALIGN, button ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &button, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &gamepad, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = IsGamepadButtonReleased ((int) gamepad, (int) button) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_is_gamepad_button_up (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: gamepad, button
	// return: 1 up, 0 not up
	S8 gamepad ALIGN, button ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &button, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &gamepad, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = IsGamepadButtonUp ((int) gamepad, (int) button) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_get_gamepad_button_pressed (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 ret ALIGN;

	pthread_mutex_lock (&rlib_mutex);
	ret = GetGamepadButtonPressed ();
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_get_gamepad_axis_count (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: gamepad
	// return: axis count
	S8 gamepad ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &gamepad, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = GetGamepadAxisCount ((int) gamepad);
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_get_gamepad_axis_movement (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: gamepad, axis
	// return: axis movement
	S8 gamepad ALIGN, axis ALIGN;
	F8 ret ALIGN;

	sp = stpopi ((U1 *) &axis, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &gamepad, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = GetGamepadAxisMovement ((int) gamepad, (int) axis);
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushd (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_get_gamepad_name (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: gamepad, out_addr
	S8 gamepad ALIGN, out_addr ALIGN;
	const char *name;
	S8 len ALIGN;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &gamepad, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	name = GetGamepadName ((int) gamepad);
	if (name != NULL)
	{
		len = strlen_safe (name, 255);
	}
	else
	{
		len = 0;
	}
	pthread_mutex_unlock (&rlib_mutex);

	if (memory_bounds (out_addr, len) != 0)
	{
		printf ("raylib_get_gamepad_name: ERROR memory bounds check failed!\n");
		return (NULL);
	}

	if (len > 0) memcpy (&data[out_addr], name, len);
	data[out_addr + len] = '\0';

	return (sp);
}

U1 *raylib_set_gamepad_vibration (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: gamepad, left_motor, right_motor, duration
	S8 gamepad ALIGN;
	F8 left_motor ALIGN, right_motor ALIGN, duration ALIGN;

	sp = stpopd ((U1 *) &duration, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &right_motor, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &left_motor, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &gamepad, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	SetGamepadVibration ((int) gamepad, (float) left_motor, (float) right_motor, (float) duration);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

// ---------------------------------------------------------------------------
// Input functions: touch
// ---------------------------------------------------------------------------

U1 *raylib_get_touch_x (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 ret ALIGN;

	pthread_mutex_lock (&rlib_mutex);
	ret = GetTouchX ();
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_get_touch_y (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 ret ALIGN;

	pthread_mutex_lock (&rlib_mutex);
	ret = GetTouchY ();
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_get_touch_position (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: index, out_addr
	S8 index ALIGN, out_addr ALIGN;
	Vector2 pos;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &index, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	pos = GetTouchPosition ((int) index);
	pthread_mutex_unlock (&rlib_mutex);

	if (rlib_write_vec2 (data, out_addr, pos) != 0) return (NULL);
	return (sp);
}

U1 *raylib_get_touch_point_count (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 ret ALIGN;

	pthread_mutex_lock (&rlib_mutex);
	ret = GetTouchPointCount ();
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

// ---------------------------------------------------------------------------
// Audio device functions
// ---------------------------------------------------------------------------

U1 *raylib_init_audio_device (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	pthread_mutex_lock (&rlib_mutex);
	InitAudioDevice ();
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_close_audio_device (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	pthread_mutex_lock (&rlib_mutex);
	CloseAudioDevice ();
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_is_audio_device_ready (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 ret ALIGN;

	pthread_mutex_lock (&rlib_mutex);
	ret = IsAudioDeviceReady () ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_set_master_volume (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: volume
	F8 volume ALIGN;

	sp = stpopd ((U1 *) &volume, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	SetMasterVolume ((float) volume);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

// ---------------------------------------------------------------------------
// Sound functions (short audio samples, fully loaded into memory)
// ---------------------------------------------------------------------------

U1 *raylib_load_sound (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: file_addr
	// return: sound handle or -1
	S8 file_addr ALIGN;
	U1 filename[512];
	S8 ind ALIGN;

	sp = stpopi ((U1 *) &file_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_load_filename (data, file_addr, filename, sizeof (filename)) != 0) return (NULL);

	ind = rlib_find_slot (raylib_sounds_used, RAYLIB_MAX_SOUNDS);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_sounds[ind] = LoadSound ((const char *) filename);
	pthread_mutex_unlock (&rlib_mutex);

	if (raylib_sounds[ind].stream.buffer == NULL)
	{
		printf ("raylib_load_sound: ERROR loading sound: %s!\n", filename);
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	raylib_sounds_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_unload_sound (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: sound_handle
	S8 sound_handle ALIGN;

	sp = stpopi ((U1 *) &sound_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (sound_handle < 0 || sound_handle >= RAYLIB_MAX_SOUNDS || raylib_sounds_used[sound_handle] == 0)
	{
		printf ("raylib_unload_sound: ERROR invalid sound handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	UnloadSound (raylib_sounds[sound_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	memset (&raylib_sounds[sound_handle], 0, sizeof (Sound));
	raylib_sounds_used[sound_handle] = 0;

	return (sp);
}

U1 *raylib_is_sound_valid (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: sound_handle
	// return: 1 valid, 0 invalid
	S8 sound_handle ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &sound_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (sound_handle < 0 || sound_handle >= RAYLIB_MAX_SOUNDS || raylib_sounds_used[sound_handle] == 0)
	{
		ret = 0;
	}
	else
	{
		pthread_mutex_lock (&rlib_mutex);
		ret = IsSoundValid (raylib_sounds[sound_handle]) ? 1 : 0;
		pthread_mutex_unlock (&rlib_mutex);
	}

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_play_sound (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: sound_handle
	S8 sound_handle ALIGN;

	sp = stpopi ((U1 *) &sound_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (sound_handle < 0 || sound_handle >= RAYLIB_MAX_SOUNDS || raylib_sounds_used[sound_handle] == 0)
	{
		printf ("raylib_play_sound: ERROR invalid sound handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	PlaySound (raylib_sounds[sound_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_stop_sound (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: sound_handle
	S8 sound_handle ALIGN;

	sp = stpopi ((U1 *) &sound_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (sound_handle < 0 || sound_handle >= RAYLIB_MAX_SOUNDS || raylib_sounds_used[sound_handle] == 0)
	{
		printf ("raylib_stop_sound: ERROR invalid sound handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	StopSound (raylib_sounds[sound_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_pause_sound (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: sound_handle
	S8 sound_handle ALIGN;

	sp = stpopi ((U1 *) &sound_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (sound_handle < 0 || sound_handle >= RAYLIB_MAX_SOUNDS || raylib_sounds_used[sound_handle] == 0)
	{
		printf ("raylib_pause_sound: ERROR invalid sound handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	PauseSound (raylib_sounds[sound_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_resume_sound (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: sound_handle
	S8 sound_handle ALIGN;

	sp = stpopi ((U1 *) &sound_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (sound_handle < 0 || sound_handle >= RAYLIB_MAX_SOUNDS || raylib_sounds_used[sound_handle] == 0)
	{
		printf ("raylib_resume_sound: ERROR invalid sound handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	ResumeSound (raylib_sounds[sound_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_is_sound_playing (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: sound_handle
	// return: 1 playing, 0 not playing
	S8 sound_handle ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &sound_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (sound_handle < 0 || sound_handle >= RAYLIB_MAX_SOUNDS || raylib_sounds_used[sound_handle] == 0)
	{
		printf ("raylib_is_sound_playing: ERROR invalid sound handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	ret = IsSoundPlaying (raylib_sounds[sound_handle]) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_set_sound_volume (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: sound_handle, volume
	S8 sound_handle ALIGN;
	F8 volume ALIGN;

	sp = stpopd ((U1 *) &volume, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &sound_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (sound_handle < 0 || sound_handle >= RAYLIB_MAX_SOUNDS || raylib_sounds_used[sound_handle] == 0)
	{
		printf ("raylib_set_sound_volume: ERROR invalid sound handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	SetSoundVolume (raylib_sounds[sound_handle], (float) volume);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_set_sound_pitch (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: sound_handle, pitch
	S8 sound_handle ALIGN;
	F8 pitch ALIGN;

	sp = stpopd ((U1 *) &pitch, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &sound_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (sound_handle < 0 || sound_handle >= RAYLIB_MAX_SOUNDS || raylib_sounds_used[sound_handle] == 0)
	{
		printf ("raylib_set_sound_pitch: ERROR invalid sound handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	SetSoundPitch (raylib_sounds[sound_handle], (float) pitch);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_set_sound_pan (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: sound_handle, pan
	S8 sound_handle ALIGN;
	F8 pan ALIGN;

	sp = stpopd ((U1 *) &pan, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &sound_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (sound_handle < 0 || sound_handle >= RAYLIB_MAX_SOUNDS || raylib_sounds_used[sound_handle] == 0)
	{
		printf ("raylib_set_sound_pan: ERROR invalid sound handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	SetSoundPan (raylib_sounds[sound_handle], (float) pan);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

// ---------------------------------------------------------------------------
// Music stream functions (longer audio, streamed from disk)
// ---------------------------------------------------------------------------

U1 *raylib_load_music_stream (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: file_addr
	// return: music handle or -1
	S8 file_addr ALIGN;
	U1 filename[512];
	S8 ind ALIGN;

	sp = stpopi ((U1 *) &file_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_load_filename (data, file_addr, filename, sizeof (filename)) != 0) return (NULL);

	ind = rlib_find_slot (raylib_musics_used, RAYLIB_MAX_MUSICS);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_musics[ind] = LoadMusicStream ((const char *) filename);
	pthread_mutex_unlock (&rlib_mutex);

	if (raylib_musics[ind].stream.buffer == NULL || raylib_musics[ind].ctxData == NULL)
	{
		printf ("raylib_load_music_stream: ERROR loading music: %s!\n", filename);
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	raylib_musics_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_unload_music_stream (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: music_handle
	S8 music_handle ALIGN;

	sp = stpopi ((U1 *) &music_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (music_handle < 0 || music_handle >= RAYLIB_MAX_MUSICS || raylib_musics_used[music_handle] == 0)
	{
		printf ("raylib_unload_music_stream: ERROR invalid music handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	UnloadMusicStream (raylib_musics[music_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	memset (&raylib_musics[music_handle], 0, sizeof (Music));
	raylib_musics_used[music_handle] = 0;

	return (sp);
}

U1 *raylib_play_music_stream (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: music_handle
	S8 music_handle ALIGN;

	sp = stpopi ((U1 *) &music_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (music_handle < 0 || music_handle >= RAYLIB_MAX_MUSICS || raylib_musics_used[music_handle] == 0)
	{
		printf ("raylib_play_music_stream: ERROR invalid music handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	PlayMusicStream (raylib_musics[music_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_stop_music_stream (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: music_handle
	S8 music_handle ALIGN;

	sp = stpopi ((U1 *) &music_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (music_handle < 0 || music_handle >= RAYLIB_MAX_MUSICS || raylib_musics_used[music_handle] == 0)
	{
		printf ("raylib_stop_music_stream: ERROR invalid music handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	StopMusicStream (raylib_musics[music_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_pause_music_stream (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: music_handle
	S8 music_handle ALIGN;

	sp = stpopi ((U1 *) &music_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (music_handle < 0 || music_handle >= RAYLIB_MAX_MUSICS || raylib_musics_used[music_handle] == 0)
	{
		printf ("raylib_pause_music_stream: ERROR invalid music handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	PauseMusicStream (raylib_musics[music_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_resume_music_stream (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: music_handle
	S8 music_handle ALIGN;

	sp = stpopi ((U1 *) &music_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (music_handle < 0 || music_handle >= RAYLIB_MAX_MUSICS || raylib_musics_used[music_handle] == 0)
	{
		printf ("raylib_resume_music_stream: ERROR invalid music handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	ResumeMusicStream (raylib_musics[music_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_is_music_stream_playing (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: music_handle
	// return: 1 playing, 0 not playing
	S8 music_handle ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &music_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (music_handle < 0 || music_handle >= RAYLIB_MAX_MUSICS || raylib_musics_used[music_handle] == 0)
	{
		printf ("raylib_is_music_stream_playing: ERROR invalid music handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	ret = IsMusicStreamPlaying (raylib_musics[music_handle]) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_update_music_stream (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: music_handle
	S8 music_handle ALIGN;

	sp = stpopi ((U1 *) &music_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (music_handle < 0 || music_handle >= RAYLIB_MAX_MUSICS || raylib_musics_used[music_handle] == 0)
	{
		printf ("raylib_update_music_stream: ERROR invalid music handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	UpdateMusicStream (raylib_musics[music_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_set_music_volume (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: music_handle, volume
	S8 music_handle ALIGN;
	F8 volume ALIGN;

	sp = stpopd ((U1 *) &volume, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &music_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (music_handle < 0 || music_handle >= RAYLIB_MAX_MUSICS || raylib_musics_used[music_handle] == 0)
	{
		printf ("raylib_set_music_volume: ERROR invalid music handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	SetMusicVolume (raylib_musics[music_handle], (float) volume);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_set_music_pitch (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: music_handle, pitch
	S8 music_handle ALIGN;
	F8 pitch ALIGN;

	sp = stpopd ((U1 *) &pitch, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &music_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (music_handle < 0 || music_handle >= RAYLIB_MAX_MUSICS || raylib_musics_used[music_handle] == 0)
	{
		printf ("raylib_set_music_pitch: ERROR invalid music handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	SetMusicPitch (raylib_musics[music_handle], (float) pitch);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_set_music_pan (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: music_handle, pan
	S8 music_handle ALIGN;
	F8 pan ALIGN;

	sp = stpopd ((U1 *) &pan, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &music_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (music_handle < 0 || music_handle >= RAYLIB_MAX_MUSICS || raylib_musics_used[music_handle] == 0)
	{
		printf ("raylib_set_music_pan: ERROR invalid music handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	SetMusicPan (raylib_musics[music_handle], (float) pan);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_get_music_time_length (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: music_handle
	// return: music length in seconds (double)
	S8 music_handle ALIGN;
	F8 ret ALIGN;

	sp = stpopi ((U1 *) &music_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (music_handle < 0 || music_handle >= RAYLIB_MAX_MUSICS || raylib_musics_used[music_handle] == 0)
	{
		printf ("raylib_get_music_time_length: ERROR invalid music handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	ret = GetMusicTimeLength (raylib_musics[music_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushd (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_get_music_time_played (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: music_handle
	// return: current music time played in seconds (double)
	S8 music_handle ALIGN;
	F8 ret ALIGN;

	sp = stpopi ((U1 *) &music_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (music_handle < 0 || music_handle >= RAYLIB_MAX_MUSICS || raylib_musics_used[music_handle] == 0)
	{
		printf ("raylib_get_music_time_played: ERROR invalid music handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	ret = GetMusicTimePlayed (raylib_musics[music_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushd (ret, sp, sp_bottom);
	return (sp);
}

// raylib module end marker
