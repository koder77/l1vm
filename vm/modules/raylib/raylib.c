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
 * IMPORTANT - how the memory_bounds () argument must be computed:
 *
 * The VM loader sets data_info.end = i - 1, i.e. end is the index of the
 * LAST valid byte, and memory_bounds () tests start + offset_access <= end.
 * The argument is therefore NOT a byte count: a byte count demands one byte
 * more than the data occupies and forces callers to declare struct arrays
 * with one element too many.
 *
 * It must also stay on the element boundary: for QUADWORD / DOUBLEFLOAT
 * arrays memory_bounds () rejects any argument that is not a multiple of
 * sizeof (S8). So for a type of N bytes per element the argument is the
 * offset of the LAST ELEMENT, i.e. (count - 1) * N, which is both <= end
 * and 8-byte aligned. For BYTE arrays any offset in range is accepted, so
 * the last byte index (count - 1) is used.
 *
 * In short: the argument is the last byte index rounded DOWN to the element
 * boundary, which for a correctly sized array is simply the start of its
 * last element. That is why an exactly sized array passes and an
 * undersized one fails, with no padding required.
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
 *   Camera2D     : D, 6   (offset D,2, target D,2, rotation, zoom)
 *   NPatchInfo   : D, 9   (source D,4, left, top, right, bottom, layout)
 *   VrDeviceInfo : D, 15  (hResolution, vResolution, hScreenSize, vScreenSize,
 *                          eyeToScreenDistance, lensSeparationDistance,
 *                          interpupillaryDistance, lensDistortionValues D,4,
 *                          chromaAbCorrection D,4)
 *   AutomationEvent: D, 6 (frame, type, params D,4)
 *
 * Textures, models, meshes, shaders, materials, fonts, images, waves,
 * audio streams, model animations, vr stereo configs, automation event
 * lists and file path lists are referenced by integer handles returned
 * by the loading functions.
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
#define RAYLIB_MAX_ANIMATIONS	256
#define RAYLIB_MAX_SOUNDS		64
#define RAYLIB_MAX_MUSICS		64
#define RAYLIB_MAX_WAVES		64
#define RAYLIB_MAX_AUDIOSTREAMS	64
#define RAYLIB_MAX_VRCONFIGS	16
#define RAYLIB_MAX_AUTOMATION	16
#define RAYLIB_MAX_FILEPATHLISTS 16

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

// one animation file is one array returned by raylib, the array pointer and
// its count are stored at the base slot, every animation in the array gets
// its own handle (base handle + index), the owner field of every slot points
// back to the base slot
ModelAnimation *raylib_animations_array[RAYLIB_MAX_ANIMATIONS];
S8 raylib_animations_owner[RAYLIB_MAX_ANIMATIONS];
S8 raylib_animations_count[RAYLIB_MAX_ANIMATIONS];

Sound raylib_sounds[RAYLIB_MAX_SOUNDS];
S2 raylib_sounds_used[RAYLIB_MAX_SOUNDS];

Music raylib_musics[RAYLIB_MAX_MUSICS];
S2 raylib_musics_used[RAYLIB_MAX_MUSICS];

Wave raylib_waves[RAYLIB_MAX_WAVES];
S2 raylib_waves_used[RAYLIB_MAX_WAVES];

AudioStream raylib_audiostreams[RAYLIB_MAX_AUDIOSTREAMS];
S2 raylib_audiostreams_used[RAYLIB_MAX_AUDIOSTREAMS];

VrStereoConfig raylib_vrconfigs[RAYLIB_MAX_VRCONFIGS];
S2 raylib_vrconfigs_used[RAYLIB_MAX_VRCONFIGS];

AutomationEventList raylib_automationlists[RAYLIB_MAX_AUTOMATION];
S2 raylib_automationlists_used[RAYLIB_MAX_AUTOMATION];

FilePathList raylib_filepathlists[RAYLIB_MAX_FILEPATHLISTS];
S2 raylib_filepathlists_used[RAYLIB_MAX_FILEPATHLISTS];

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
	if (slen > 0)
	{
		// data_info.end is the index of the LAST valid byte, so the offset
		// argument is the index of the last byte touched, not a byte count
		if (memory_bounds (addr, slen - 1) != 0)
		{
			printf ("rlib_read_string: ERROR memory bounds check failed!\n");
			return (1);
		}
	}

	memcpy (buf, &data[addr], slen);
	buf[slen] = '\0';
	return (0);
}

// read a Color struct from a B,4 byte array
S2 rlib_read_color (U1 *data, S8 addr, Color *col)
{
	if (memory_bounds (addr, 3) != 0)
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

S2 rlib_write_color (U1 *data, S8 addr, Color col)
{
	if (memory_bounds (addr, 3) != 0)
	{
		printf ("rlib_write_color: ERROR memory bounds check failed!\n");
		return (1);
	}

	data[addr] = col.r;
	data[addr + 1] = col.g;
	data[addr + 2] = col.b;
	data[addr + 3] = col.a;
	return (0);
}

S2 rlib_read_vec2 (U1 *data, S8 addr, Vector2 *v)
{
	F8 x ALIGN, y ALIGN;

	if (memory_bounds (addr, 8) != 0)
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

	if (memory_bounds (addr, 16) != 0)
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

	if (memory_bounds (addr, 24) != 0)
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

	if (memory_bounds (addr, 24) != 0)
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

S2 rlib_write_rect (U1 *data, S8 addr, Rectangle r)
{
	F8 t ALIGN;

	if (memory_bounds (addr, 24) != 0)
	{
		printf ("rlib_write_rect: ERROR memory bounds check failed!\n");
		return (1);
	}

	t = r.x; memcpy (&data[addr], &t, sizeof (F8));
	t = r.y; memcpy (&data[addr + 8], &t, sizeof (F8));
	t = r.width; memcpy (&data[addr + 16], &t, sizeof (F8));
	t = r.height; memcpy (&data[addr + 24], &t, sizeof (F8));
	return (0);
}

S2 rlib_read_matrix (U1 *data, S8 addr, Matrix *m)
{
	F8 t ALIGN;
	S8 i ALIGN;
	float *fm = (float *) m;

	if (memory_bounds (addr, 120) != 0)
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

	if (memory_bounds (addr, 8) != 0)
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

	if (memory_bounds (addr, 16) != 0)
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

	if (memory_bounds (addr, 24) != 0)
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

	if (memory_bounds (addr, 120) != 0)
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

	if (memory_bounds (addr, 56) != 0)
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

	if (memory_bounds (addr + 72, 8) != 0)
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

	if (memory_bounds (addr + 72, 8) != 0)
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

// read camera2d struct from a D,6 memory array
S2 rlib_read_camera2d (U1 *data, S8 addr, Camera2D *c)
{
	F8 rotation ALIGN, zoom ALIGN;

	if (memory_bounds (addr, 40) != 0)
	{
		printf ("rlib_read_camera2d: ERROR memory bounds check failed!\n");
		return (1);
	}

	if (rlib_read_vec2 (data, addr, &c->offset) != 0) return (1);
	if (rlib_read_vec2 (data, addr + 16, &c->target) != 0) return (1);
	memcpy (&rotation, &data[addr + 32], sizeof (F8));
	memcpy (&zoom, &data[addr + 40], sizeof (F8));
	c->rotation = (float) rotation;
	c->zoom = (float) zoom;
	return (0);
}

// read n-patch info from a D,9 memory array:
// source rectangle, left, top, right, bottom, layout
S2 rlib_read_npatchinfo (U1 *data, S8 addr, NPatchInfo *n)
{
	F8 l ALIGN, t ALIGN, r ALIGN, b ALIGN, layout ALIGN;

	if (memory_bounds (addr, 64) != 0)
	{
		printf ("rlib_read_npatchinfo: ERROR memory bounds check failed!\n");
		return (1);
	}

	if (rlib_read_rect (data, addr, &n->source) != 0) return (1);
	memcpy (&l, &data[addr + 32], sizeof (F8));
	memcpy (&t, &data[addr + 40], sizeof (F8));
	memcpy (&r, &data[addr + 48], sizeof (F8));
	memcpy (&b, &data[addr + 56], sizeof (F8));
	memcpy (&layout, &data[addr + 64], sizeof (F8));
	n->left = (int) l;
	n->top = (int) t;
	n->right = (int) r;
	n->bottom = (int) b;
	n->layout = (int) layout;
	return (0);
}

// read vr device info from a D,15 memory array:
// hResolution, vResolution, hScreenSize, vScreenSize, eyeToScreenDistance,
// lensSeparationDistance, interpupillaryDistance, lensDistortionValues[4],
// chromaAbCorrection[4]
S2 rlib_read_vrdeviceinfo (U1 *data, S8 addr, VrDeviceInfo *d)
{
	F8 t ALIGN;
	S8 i ALIGN;

	if (memory_bounds (addr, 112) != 0)
	{
		printf ("rlib_read_vrdeviceinfo: ERROR memory bounds check failed!\n");
		return (1);
	}

	memcpy (&t, &data[addr], sizeof (F8));
	d->hResolution = (int) t;
	memcpy (&t, &data[addr + 8], sizeof (F8));
	d->vResolution = (int) t;
	memcpy (&t, &data[addr + 16], sizeof (F8));
	d->hScreenSize = (float) t;
	memcpy (&t, &data[addr + 24], sizeof (F8));
	d->vScreenSize = (float) t;
	memcpy (&t, &data[addr + 32], sizeof (F8));
	d->eyeToScreenDistance = (float) t;
	memcpy (&t, &data[addr + 40], sizeof (F8));
	d->lensSeparationDistance = (float) t;
	memcpy (&t, &data[addr + 48], sizeof (F8));
	d->interpupillaryDistance = (float) t;

	for (i = 0; i < 4; i++)
	{
		memcpy (&t, &data[addr + 56 + (i * 8)], sizeof (F8));
		d->lensDistortionValues[i] = (float) t;
		memcpy (&t, &data[addr + 88 + (i * 8)], sizeof (F8));
		d->chromaAbCorrection[i] = (float) t;
	}
	return (0);
}

// read automation event from a D,6 memory array:
// frame, type, params[4]
S2 rlib_read_automationevent (U1 *data, S8 addr, AutomationEvent *e)
{
	F8 t ALIGN;
	S8 i ALIGN;

	if (memory_bounds (addr, 40) != 0)
	{
		printf ("rlib_read_automationevent: ERROR memory bounds check failed!\n");
		return (1);
	}

	memcpy (&t, &data[addr], sizeof (F8));
	e->frame = (unsigned int) t;
	memcpy (&t, &data[addr + 8], sizeof (F8));
	e->type = (unsigned int) t;

	for (i = 0; i < 4; i++)
	{
		memcpy (&t, &data[addr + 16 + (i * 8)], sizeof (F8));
		e->params[i] = (int) t;
	}
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

// find num free slots in a used-flag table, returns index of the first
// slot of the range or -1
S2 rlib_find_slots (S2 *used, S8 max, S8 num)
{
	S8 i ALIGN;
	S8 j ALIGN;

	if (num < 1)
	{
		return (-1);
	}

	for (i = 0; i <= max - num; i++)
	{
		for (j = 0; j < num; j++)
		{
			if (used[i + j] != 0)
			{
				break;
			}
		}
		if (j == num)
		{
			return ((S2) i);
		}
	}
	printf ("raylib: ERROR object table full! (need: %lli, max: %lli)\n", num, max);
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

	if (memory_bounds (points_addr, (point_count - 1) * 24) != 0)
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

	if (memory_bounds (transforms_addr, (instances - 1) * 128) != 0)
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

	if (memory_bounds (cam2d_addr, 40) != 0)
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

	if (memory_bounds (points_addr, (point_count - 1) * 16) != 0)
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

// ---------------------------------------------------------------------------
// raylib 6.2 API functions added on top of the base module
// ---------------------------------------------------------------------------

// All raylib 6.2 API functions that can be mapped onto the L1vm stack
// machine and its data memory are wrapped below. The following functions
// are not wrapped:
//
// The callback setters (SetTraceLogCallback, SetLoadFileDataCallback,
//   SetSaveFileDataCallback, SetLoadFileTextCallback, SetSaveFileTextCallback,
//   SetAudioStreamCallback, AttachAudioStreamProcessor, AttachAudioMixedProcessor)
//   are wrapped in the callback section at the end of this file; the detach
//   operations are covered by the enable flag arguments of those wrappers.
// variadic functions: TraceLog, TextFormat
// raw pointers: MemAlloc, MemFree, MemRealloc, GetWindowHandle, LoadFileData,
//   SetPixelColor, GetPixelColor, SetShaderValueV, UpdateTexture,
//   UpdateTextureRec, UpdateAudioStream, UpdateSound, LoadCodepoints,
//   UnloadCodepoints, LoadImageColors, LoadImagePalette, LoadTextLines,
//   UnloadTextLines, LoadWaveSamples, LoadRandomSequence,
//   UnloadRandomSequence, LoadMaterials, LoadFontData, UnloadFontData,
//   GenImageFontAtlas, ComputeMD5, ComputeSHA1, ComputeSHA256, UnloadUTF8
// text pointer arrays: TextSplit, TextJoin, TextAppend, TextCopy
// out parameters without a VM mapping: LoadImageAnim,
//   LoadImageAnimFromMemory, GetGlyphInfo
// other: SetWindowIcons (array of image handles), GetShapesTexture
//   (returns the internal shapes texture without a matching unload)
//
// Model animation arrays are handled by hand written wrappers, because an
// animation file is returned as one pointer to an array of animations.


U1 *raylib_begin_shader_mode (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: shader_handle
	S8 shader_handle ALIGN;

	sp = stpopi ((U1 *) &shader_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (shader_handle < 0 || shader_handle >= RAYLIB_MAX_SHADERS || raylib_shaders_used[shader_handle] == 0)
	{
		printf ("raylib_begin_shader_mode: ERROR invalid shader handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	BeginShaderMode (raylib_shaders[shader_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_begin_texture_mode (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: target_handle
	S8 target_handle ALIGN;

	sp = stpopi ((U1 *) &target_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (target_handle < 0 || target_handle >= RAYLIB_MAX_RENDERTEX || raylib_rendertextures_used[target_handle] == 0)
	{
		printf ("raylib_begin_texture_mode: ERROR invalid render texture handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	BeginTextureMode (raylib_rendertextures[target_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_begin_vr_stereo_mode (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: config_handle
	S8 config_handle ALIGN;

	sp = stpopi ((U1 *) &config_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (config_handle < 0 || config_handle >= RAYLIB_MAX_VRCONFIGS || raylib_vrconfigs_used[config_handle] == 0)
	{
		printf ("raylib_begin_vr_stereo_mode: ERROR invalid vr stereo config handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	BeginVrStereoMode (raylib_vrconfigs[config_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_change_directory (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: dir_path_addr
	// return: integer result
	S8 dir_path_addr ALIGN;
	U1 dir_path_buf[512];
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &dir_path_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_load_filename (data, dir_path_addr, dir_path_buf, sizeof (dir_path_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = ChangeDirectory ((const char *) dir_path_buf);
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_check_collision_circle_line (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: center_addr, radius, p1_addr, p2_addr
	// return: 1 true, 0 false
	Vector2 center;
	S8 center_addr ALIGN;
	F8 radius ALIGN;
	Vector2 p1;
	S8 p1_addr ALIGN;
	Vector2 p2;
	S8 p2_addr ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &p2_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &p1_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &radius, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &center_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec2 (data, center_addr, &center) != 0) return (NULL);
	if (rlib_read_vec2 (data, p1_addr, &p1) != 0) return (NULL);
	if (rlib_read_vec2 (data, p2_addr, &p2) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = CheckCollisionCircleLine (center, (float) radius, p1, p2) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_check_collision_circle_rec (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: center_addr, radius, rec_addr
	// return: 1 true, 0 false
	Vector2 center;
	S8 center_addr ALIGN;
	F8 radius ALIGN;
	Rectangle rec;
	S8 rec_addr ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &rec_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &radius, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &center_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec2 (data, center_addr, &center) != 0) return (NULL);
	if (rlib_read_rect (data, rec_addr, &rec) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = CheckCollisionCircleRec (center, (float) radius, rec) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_check_collision_circles (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: center1_addr, radius1, center2_addr, radius2
	// return: 1 true, 0 false
	Vector2 center1;
	S8 center1_addr ALIGN;
	F8 radius1 ALIGN;
	Vector2 center2;
	S8 center2_addr ALIGN;
	F8 radius2 ALIGN;
	S8 ret ALIGN;

	sp = stpopd ((U1 *) &radius2, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &center2_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &radius1, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &center1_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec2 (data, center1_addr, &center1) != 0) return (NULL);
	if (rlib_read_vec2 (data, center2_addr, &center2) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = CheckCollisionCircles (center1, (float) radius1, center2, (float) radius2) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_check_collision_lines (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: start_pos1_addr, end_pos1_addr, start_pos2_addr, end_pos2_addr, out_addr
	// return: 1 true, 0 false
	Vector2 start_pos1;
	S8 start_pos1_addr ALIGN;
	Vector2 end_pos1;
	S8 end_pos1_addr ALIGN;
	Vector2 start_pos2;
	S8 start_pos2_addr ALIGN;
	Vector2 end_pos2;
	S8 end_pos2_addr ALIGN;
	Vector2 collision_point;
	S8 out_addr ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &end_pos2_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &start_pos2_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &end_pos1_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &start_pos1_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec2 (data, start_pos1_addr, &start_pos1) != 0) return (NULL);
	if (rlib_read_vec2 (data, end_pos1_addr, &end_pos1) != 0) return (NULL);
	if (rlib_read_vec2 (data, start_pos2_addr, &start_pos2) != 0) return (NULL);
	if (rlib_read_vec2 (data, end_pos2_addr, &end_pos2) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = CheckCollisionLines (start_pos1, end_pos1, start_pos2, end_pos2, &collision_point) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	if (rlib_write_vec2 (data, out_addr, collision_point) != 0) return (NULL);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_check_collision_point_circle (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: point_addr, center_addr, radius
	// return: 1 true, 0 false
	Vector2 point;
	S8 point_addr ALIGN;
	Vector2 center;
	S8 center_addr ALIGN;
	F8 radius ALIGN;
	S8 ret ALIGN;

	sp = stpopd ((U1 *) &radius, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &center_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &point_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec2 (data, point_addr, &point) != 0) return (NULL);
	if (rlib_read_vec2 (data, center_addr, &center) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = CheckCollisionPointCircle (point, center, (float) radius) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_check_collision_point_line (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: point_addr, p1_addr, p2_addr, threshold
	// return: 1 true, 0 false
	Vector2 point;
	S8 point_addr ALIGN;
	Vector2 p1;
	S8 p1_addr ALIGN;
	Vector2 p2;
	S8 p2_addr ALIGN;
	S8 threshold ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &threshold, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &p2_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &p1_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &point_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec2 (data, point_addr, &point) != 0) return (NULL);
	if (rlib_read_vec2 (data, p1_addr, &p1) != 0) return (NULL);
	if (rlib_read_vec2 (data, p2_addr, &p2) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = CheckCollisionPointLine (point, p1, p2, (int) threshold) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_check_collision_point_poly (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: point_addr, points_addr, point_count
	// return: 1 true, 0 false
	Vector2 point;
	S8 point_addr ALIGN;
	S8 points_addr ALIGN;
	Vector2 *points_ptr = NULL;
	S8 point_count ALIGN;
	S8 ret ALIGN;
	S8 i ALIGN;

	sp = stpopi ((U1 *) &point_count, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &points_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &point_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec2 (data, point_addr, &point) != 0) return (NULL);
	if (point_count > 0)
	{
		if (memory_bounds (points_addr, (point_count - 1) * 16) != 0)
		{
			printf ("raylib_check_collision_point_poly: ERROR memory bounds check failed!\n");
			return (NULL);
		}
		points_ptr = calloc (point_count, sizeof (Vector2));
		if (points_ptr == NULL)
		{
			printf ("raylib_check_collision_point_poly: ERROR out of memory!\n");
			return (NULL);
		}
		for (i = 0; i < point_count; i++)
		{
			if (rlib_read_vec2 (data, points_addr + (i * 16), &points_ptr[i]) != 0)
			{
				free (points_ptr);
				return (NULL);
			}
		}
	}

	pthread_mutex_lock (&rlib_mutex);
	ret = CheckCollisionPointPoly (point, (const Vector2 *) points_ptr, (int) point_count) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	free (points_ptr);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_check_collision_point_rec (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: point_addr, rec_addr
	// return: 1 true, 0 false
	Vector2 point;
	S8 point_addr ALIGN;
	Rectangle rec;
	S8 rec_addr ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &rec_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &point_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec2 (data, point_addr, &point) != 0) return (NULL);
	if (rlib_read_rect (data, rec_addr, &rec) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = CheckCollisionPointRec (point, rec) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_check_collision_point_triangle (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: point_addr, p1_addr, p2_addr, p3_addr
	// return: 1 true, 0 false
	Vector2 point;
	S8 point_addr ALIGN;
	Vector2 p1;
	S8 p1_addr ALIGN;
	Vector2 p2;
	S8 p2_addr ALIGN;
	Vector2 p3;
	S8 p3_addr ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &p3_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &p2_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &p1_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &point_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec2 (data, point_addr, &point) != 0) return (NULL);
	if (rlib_read_vec2 (data, p1_addr, &p1) != 0) return (NULL);
	if (rlib_read_vec2 (data, p2_addr, &p2) != 0) return (NULL);
	if (rlib_read_vec2 (data, p3_addr, &p3) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = CheckCollisionPointTriangle (point, p1, p2, p3) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_check_collision_recs (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: rec1_addr, rec2_addr
	// return: 1 true, 0 false
	Rectangle rec1;
	S8 rec1_addr ALIGN;
	Rectangle rec2;
	S8 rec2_addr ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &rec2_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &rec1_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_rect (data, rec1_addr, &rec1) != 0) return (NULL);
	if (rlib_read_rect (data, rec2_addr, &rec2) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = CheckCollisionRecs (rec1, rec2) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_codepoint_to_utf8 (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: codepoint, out_addr
	// return: string written to out_addr
	S8 codepoint ALIGN;
	int utf_8size_out = 0;
	S8 out_addr ALIGN;
	const char *text;
	S8 len ALIGN;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &codepoint, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	text = CodepointToUTF8 ((int) codepoint, &utf_8size_out);
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
		printf ("raylib_codepoint_to_utf8: ERROR memory bounds check failed!\n");
		return (NULL);
	}
	if (len > 0) memcpy (&data[out_addr], text, len);
	data[out_addr + len] = '\0';

	return (sp);
}

U1 *raylib_color_alpha (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: color_addr, alpha, out_addr
	// return: struct written to out_addr
	Color color;
	S8 color_addr ALIGN;
	F8 alpha ALIGN;
	S8 out_addr ALIGN;
	Color ret;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &alpha, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = ColorAlpha (color, (float) alpha);
	pthread_mutex_unlock (&rlib_mutex);

	if (rlib_write_color (data, out_addr, ret) != 0) return (NULL);

	return (sp);
}

U1 *raylib_color_alpha_blend (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: dst_addr, src_addr, tint_addr, out_addr
	// return: struct written to out_addr
	Color dst;
	S8 dst_addr ALIGN;
	Color src;
	S8 src_addr ALIGN;
	Color tint;
	S8 tint_addr ALIGN;
	S8 out_addr ALIGN;
	Color ret;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &tint_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &src_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &dst_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_color (data, dst_addr, &dst) != 0) return (NULL);
	if (rlib_read_color (data, src_addr, &src) != 0) return (NULL);
	if (rlib_read_color (data, tint_addr, &tint) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = ColorAlphaBlend (dst, src, tint);
	pthread_mutex_unlock (&rlib_mutex);

	if (rlib_write_color (data, out_addr, ret) != 0) return (NULL);

	return (sp);
}

U1 *raylib_color_brightness (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: color_addr, factor, out_addr
	// return: struct written to out_addr
	Color color;
	S8 color_addr ALIGN;
	F8 factor ALIGN;
	S8 out_addr ALIGN;
	Color ret;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &factor, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = ColorBrightness (color, (float) factor);
	pthread_mutex_unlock (&rlib_mutex);

	if (rlib_write_color (data, out_addr, ret) != 0) return (NULL);

	return (sp);
}

U1 *raylib_color_contrast (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: color_addr, contrast, out_addr
	// return: struct written to out_addr
	Color color;
	S8 color_addr ALIGN;
	F8 contrast ALIGN;
	S8 out_addr ALIGN;
	Color ret;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &contrast, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = ColorContrast (color, (float) contrast);
	pthread_mutex_unlock (&rlib_mutex);

	if (rlib_write_color (data, out_addr, ret) != 0) return (NULL);

	return (sp);
}

U1 *raylib_color_from_hsv (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: hue, saturation, value, out_addr
	// return: struct written to out_addr
	F8 hue ALIGN;
	F8 saturation ALIGN;
	F8 value ALIGN;
	S8 out_addr ALIGN;
	Color ret;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &value, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &saturation, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &hue, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = ColorFromHSV ((float) hue, (float) saturation, (float) value);
	pthread_mutex_unlock (&rlib_mutex);

	if (rlib_write_color (data, out_addr, ret) != 0) return (NULL);

	return (sp);
}

U1 *raylib_color_from_normalized (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: normalized_addr, out_addr
	// return: struct written to out_addr
	Vector4 normalized;
	S8 normalized_addr ALIGN;
	S8 out_addr ALIGN;
	Color ret;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &normalized_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec4 (data, normalized_addr, &normalized) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = ColorFromNormalized (normalized);
	pthread_mutex_unlock (&rlib_mutex);

	if (rlib_write_color (data, out_addr, ret) != 0) return (NULL);

	return (sp);
}

U1 *raylib_color_is_equal (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: col1_addr, col2_addr
	// return: 1 true, 0 false
	Color col1;
	S8 col1_addr ALIGN;
	Color col2;
	S8 col2_addr ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &col2_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &col1_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_color (data, col1_addr, &col1) != 0) return (NULL);
	if (rlib_read_color (data, col2_addr, &col2) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = ColorIsEqual (col1, col2) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_color_lerp (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: color1_addr, color2_addr, factor, out_addr
	// return: struct written to out_addr
	Color color1;
	S8 color1_addr ALIGN;
	Color color2;
	S8 color2_addr ALIGN;
	F8 factor ALIGN;
	S8 out_addr ALIGN;
	Color ret;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &factor, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &color2_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &color1_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_color (data, color1_addr, &color1) != 0) return (NULL);
	if (rlib_read_color (data, color2_addr, &color2) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = ColorLerp (color1, color2, (float) factor);
	pthread_mutex_unlock (&rlib_mutex);

	if (rlib_write_color (data, out_addr, ret) != 0) return (NULL);

	return (sp);
}

U1 *raylib_color_normalize (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: color_addr, out_addr
	// return: struct written to out_addr
	Color color;
	S8 color_addr ALIGN;
	S8 out_addr ALIGN;
	Vector4 ret;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = ColorNormalize (color);
	pthread_mutex_unlock (&rlib_mutex);

	if (rlib_write_vec4 (data, out_addr, ret) != 0) return (NULL);

	return (sp);
}

U1 *raylib_color_tint (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: color_addr, tint_addr, out_addr
	// return: struct written to out_addr
	Color color;
	S8 color_addr ALIGN;
	Color tint;
	S8 tint_addr ALIGN;
	S8 out_addr ALIGN;
	Color ret;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &tint_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);
	if (rlib_read_color (data, tint_addr, &tint) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = ColorTint (color, tint);
	pthread_mutex_unlock (&rlib_mutex);

	if (rlib_write_color (data, out_addr, ret) != 0) return (NULL);

	return (sp);
}

U1 *raylib_color_to_hsv (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: color_addr, out_addr
	// return: struct written to out_addr
	Color color;
	S8 color_addr ALIGN;
	S8 out_addr ALIGN;
	Vector3 ret;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = ColorToHSV (color);
	pthread_mutex_unlock (&rlib_mutex);

	if (rlib_write_vec3 (data, out_addr, ret) != 0) return (NULL);

	return (sp);
}

U1 *raylib_color_to_int (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: color_addr
	// return: integer result
	Color color;
	S8 color_addr ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = ColorToInt (color);
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_compute_crc32 (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: data_addr, data_size
	// return: integer result
	S8 data_addr ALIGN;
	S8 data_size ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &data_size, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &data_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (data_size > 0)
	{
		if (memory_bounds (data_addr, data_size - 1) != 0)
		{
			printf ("raylib_compute_crc32: ERROR memory bounds check failed!\n");
			return (NULL);
		}
	}

	pthread_mutex_lock (&rlib_mutex);
	ret = ComputeCRC32 ((const unsigned char *) &data[data_addr], (int) data_size);
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_directory_exists (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: dir_path_addr
	// return: 1 true, 0 false
	S8 dir_path_addr ALIGN;
	U1 dir_path_buf[512];
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &dir_path_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_load_filename (data, dir_path_addr, dir_path_buf, sizeof (dir_path_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = DirectoryExists ((const char *) dir_path_buf) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_disable_cursor (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{

	pthread_mutex_lock (&rlib_mutex);
	DisableCursor ();
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_circle_lines_ex (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: center_addr, radius, thick, color_addr
	Vector2 center;
	S8 center_addr ALIGN;
	F8 radius ALIGN;
	F8 thick ALIGN;
	Color color;
	S8 color_addr ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &thick, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &radius, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &center_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec2 (data, center_addr, &center) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawCircleLinesEx (center, (float) radius, (float) thick, color);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_circle_sector_lines (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: center_addr, radius, start_angle, end_angle, segments, color_addr
	Vector2 center;
	S8 center_addr ALIGN;
	F8 radius ALIGN;
	F8 start_angle ALIGN;
	F8 end_angle ALIGN;
	S8 segments ALIGN;
	Color color;
	S8 color_addr ALIGN;

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
	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawCircleSectorLines (center, (float) radius, (float) start_angle, (float) end_angle, (int) segments, color);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_circle_sector_lines_ex (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: center_addr, radius, start_angle, end_angle, segments, thick, color_addr
	Vector2 center;
	S8 center_addr ALIGN;
	F8 radius ALIGN;
	F8 start_angle ALIGN;
	F8 end_angle ALIGN;
	S8 segments ALIGN;
	F8 thick ALIGN;
	Color color;
	S8 color_addr ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &thick, sp, sp_top);
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
	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawCircleSectorLinesEx (center, (float) radius, (float) start_angle, (float) end_angle, (int) segments, (float) thick, color);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_ellipse_lines_ex (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: center_addr, radius_h, radius_v, thick, color_addr
	Vector2 center;
	S8 center_addr ALIGN;
	F8 radius_h ALIGN;
	F8 radius_v ALIGN;
	F8 thick ALIGN;
	Color color;
	S8 color_addr ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &thick, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &radius_v, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &radius_h, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &center_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec2 (data, center_addr, &center) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawEllipseLinesEx (center, (float) radius_h, (float) radius_v, (float) thick, color);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_ellipse_lines_v (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: center_addr, radius_h, radius_v, color_addr
	Vector2 center;
	S8 center_addr ALIGN;
	F8 radius_h ALIGN;
	F8 radius_v ALIGN;
	Color color;
	S8 color_addr ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &radius_v, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &radius_h, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &center_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec2 (data, center_addr, &center) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawEllipseLinesV (center, (float) radius_h, (float) radius_v, color);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_ellipse_v (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: center_addr, radius_h, radius_v, color_addr
	Vector2 center;
	S8 center_addr ALIGN;
	F8 radius_h ALIGN;
	F8 radius_v ALIGN;
	Color color;
	S8 color_addr ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &radius_v, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &radius_h, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &center_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec2 (data, center_addr, &center) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawEllipseV (center, (float) radius_h, (float) radius_v, color);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_line_bezier (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: start_pos_addr, end_pos_addr, thick, color_addr
	Vector2 start_pos;
	S8 start_pos_addr ALIGN;
	Vector2 end_pos;
	S8 end_pos_addr ALIGN;
	F8 thick ALIGN;
	Color color;
	S8 color_addr ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &thick, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &end_pos_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &start_pos_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec2 (data, start_pos_addr, &start_pos) != 0) return (NULL);
	if (rlib_read_vec2 (data, end_pos_addr, &end_pos) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawLineBezier (start_pos, end_pos, (float) thick, color);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_line_dashed (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: start_pos_addr, end_pos_addr, dash_size, space_size, color_addr
	Vector2 start_pos;
	S8 start_pos_addr ALIGN;
	Vector2 end_pos;
	S8 end_pos_addr ALIGN;
	S8 dash_size ALIGN;
	S8 space_size ALIGN;
	Color color;
	S8 color_addr ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &space_size, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &dash_size, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &end_pos_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &start_pos_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec2 (data, start_pos_addr, &start_pos) != 0) return (NULL);
	if (rlib_read_vec2 (data, end_pos_addr, &end_pos) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawLineDashed (start_pos, end_pos, (int) dash_size, (int) space_size, color);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_poly_lines_ex (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: center_addr, sides, radius, rotation, thick, color_addr
	Vector2 center;
	S8 center_addr ALIGN;
	S8 sides ALIGN;
	F8 radius ALIGN;
	F8 rotation ALIGN;
	F8 thick ALIGN;
	Color color;
	S8 color_addr ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &thick, sp, sp_top);
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
	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawPolyLinesEx (center, (int) sides, (float) radius, (float) rotation, (float) thick, color);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_rectangle_gradient_ex (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: rec_addr, col1_addr, col2_addr, col3_addr, col4_addr
	Rectangle rec;
	S8 rec_addr ALIGN;
	Color col1;
	S8 col1_addr ALIGN;
	Color col2;
	S8 col2_addr ALIGN;
	Color col3;
	S8 col3_addr ALIGN;
	Color col4;
	S8 col4_addr ALIGN;

	sp = stpopi ((U1 *) &col4_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &col3_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &col2_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &col1_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &rec_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_rect (data, rec_addr, &rec) != 0) return (NULL);
	if (rlib_read_color (data, col1_addr, &col1) != 0) return (NULL);
	if (rlib_read_color (data, col2_addr, &col2) != 0) return (NULL);
	if (rlib_read_color (data, col3_addr, &col3) != 0) return (NULL);
	if (rlib_read_color (data, col4_addr, &col4) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawRectangleGradientEx (rec, col1, col2, col3, col4);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_rectangle_gradient_h (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: pos_x, pos_y, width, height, left_addr, right_addr
	S8 pos_x ALIGN;
	S8 pos_y ALIGN;
	S8 width ALIGN;
	S8 height ALIGN;
	Color left;
	S8 left_addr ALIGN;
	Color right;
	S8 right_addr ALIGN;

	sp = stpopi ((U1 *) &right_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &left_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &height, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &width, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &pos_y, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &pos_x, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_color (data, left_addr, &left) != 0) return (NULL);
	if (rlib_read_color (data, right_addr, &right) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawRectangleGradientH ((int) pos_x, (int) pos_y, (int) width, (int) height, left, right);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_rectangle_gradient_v (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: pos_x, pos_y, width, height, top_addr, bottom_addr
	S8 pos_x ALIGN;
	S8 pos_y ALIGN;
	S8 width ALIGN;
	S8 height ALIGN;
	Color top;
	S8 top_addr ALIGN;
	Color bottom;
	S8 bottom_addr ALIGN;

	sp = stpopi ((U1 *) &bottom_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &top_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &height, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &width, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &pos_y, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &pos_x, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_color (data, top_addr, &top) != 0) return (NULL);
	if (rlib_read_color (data, bottom_addr, &bottom) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawRectangleGradientV ((int) pos_x, (int) pos_y, (int) width, (int) height, top, bottom);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_rectangle_rounded_lines (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: rec_addr, roundness, segments, color_addr
	Rectangle rec;
	S8 rec_addr ALIGN;
	F8 roundness ALIGN;
	S8 segments ALIGN;
	Color color;
	S8 color_addr ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &segments, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &roundness, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &rec_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_rect (data, rec_addr, &rec) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawRectangleRoundedLines (rec, (float) roundness, (int) segments, color);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_rectangle_rounded_lines_ex (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: rec_addr, roundness, segments, thick, color_addr
	Rectangle rec;
	S8 rec_addr ALIGN;
	F8 roundness ALIGN;
	S8 segments ALIGN;
	F8 thick ALIGN;
	Color color;
	S8 color_addr ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &thick, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &segments, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &roundness, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &rec_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_rect (data, rec_addr, &rec) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawRectangleRoundedLinesEx (rec, (float) roundness, (int) segments, (float) thick, color);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_ring_lines_ex (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: center_addr, inner_radius, outer_radius, start_angle, end_angle, segments, thick, color_addr
	Vector2 center;
	S8 center_addr ALIGN;
	F8 inner_radius ALIGN;
	F8 outer_radius ALIGN;
	F8 start_angle ALIGN;
	F8 end_angle ALIGN;
	S8 segments ALIGN;
	F8 thick ALIGN;
	Color color;
	S8 color_addr ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &thick, sp, sp_top);
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
	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawRingLinesEx (center, (float) inner_radius, (float) outer_radius, (float) start_angle, (float) end_angle, (int) segments, (float) thick, color);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_spline_basis (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: points_addr, point_count, thick, color_addr
	S8 points_addr ALIGN;
	Vector2 *points_ptr = NULL;
	S8 point_count ALIGN;
	F8 thick ALIGN;
	Color color;
	S8 color_addr ALIGN;
	S8 i ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &thick, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &point_count, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &points_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (point_count > 0)
	{
		if (memory_bounds (points_addr, (point_count - 1) * 16) != 0)
		{
			printf ("raylib_draw_spline_basis: ERROR memory bounds check failed!\n");
			return (NULL);
		}
		points_ptr = calloc (point_count, sizeof (Vector2));
		if (points_ptr == NULL)
		{
			printf ("raylib_draw_spline_basis: ERROR out of memory!\n");
			return (NULL);
		}
		for (i = 0; i < point_count; i++)
		{
			if (rlib_read_vec2 (data, points_addr + (i * 16), &points_ptr[i]) != 0)
			{
				free (points_ptr);
				return (NULL);
			}
		}
	}
	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawSplineBasis ((const Vector2 *) points_ptr, (int) point_count, (float) thick, color);
	pthread_mutex_unlock (&rlib_mutex);

	free (points_ptr);

	return (sp);
}

U1 *raylib_draw_spline_bezier_cubic (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: points_addr, point_count, thick, color_addr
	S8 points_addr ALIGN;
	Vector2 *points_ptr = NULL;
	S8 point_count ALIGN;
	F8 thick ALIGN;
	Color color;
	S8 color_addr ALIGN;
	S8 i ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &thick, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &point_count, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &points_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (point_count > 0)
	{
		if (memory_bounds (points_addr, (point_count - 1) * 16) != 0)
		{
			printf ("raylib_draw_spline_bezier_cubic: ERROR memory bounds check failed!\n");
			return (NULL);
		}
		points_ptr = calloc (point_count, sizeof (Vector2));
		if (points_ptr == NULL)
		{
			printf ("raylib_draw_spline_bezier_cubic: ERROR out of memory!\n");
			return (NULL);
		}
		for (i = 0; i < point_count; i++)
		{
			if (rlib_read_vec2 (data, points_addr + (i * 16), &points_ptr[i]) != 0)
			{
				free (points_ptr);
				return (NULL);
			}
		}
	}
	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawSplineBezierCubic ((const Vector2 *) points_ptr, (int) point_count, (float) thick, color);
	pthread_mutex_unlock (&rlib_mutex);

	free (points_ptr);

	return (sp);
}

U1 *raylib_draw_spline_bezier_quadratic (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: points_addr, point_count, thick, color_addr
	S8 points_addr ALIGN;
	Vector2 *points_ptr = NULL;
	S8 point_count ALIGN;
	F8 thick ALIGN;
	Color color;
	S8 color_addr ALIGN;
	S8 i ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &thick, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &point_count, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &points_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (point_count > 0)
	{
		if (memory_bounds (points_addr, (point_count - 1) * 16) != 0)
		{
			printf ("raylib_draw_spline_bezier_quadratic: ERROR memory bounds check failed!\n");
			return (NULL);
		}
		points_ptr = calloc (point_count, sizeof (Vector2));
		if (points_ptr == NULL)
		{
			printf ("raylib_draw_spline_bezier_quadratic: ERROR out of memory!\n");
			return (NULL);
		}
		for (i = 0; i < point_count; i++)
		{
			if (rlib_read_vec2 (data, points_addr + (i * 16), &points_ptr[i]) != 0)
			{
				free (points_ptr);
				return (NULL);
			}
		}
	}
	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawSplineBezierQuadratic ((const Vector2 *) points_ptr, (int) point_count, (float) thick, color);
	pthread_mutex_unlock (&rlib_mutex);

	free (points_ptr);

	return (sp);
}

U1 *raylib_draw_spline_catmull_rom (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: points_addr, point_count, thick, color_addr
	S8 points_addr ALIGN;
	Vector2 *points_ptr = NULL;
	S8 point_count ALIGN;
	F8 thick ALIGN;
	Color color;
	S8 color_addr ALIGN;
	S8 i ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &thick, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &point_count, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &points_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (point_count > 0)
	{
		if (memory_bounds (points_addr, (point_count - 1) * 16) != 0)
		{
			printf ("raylib_draw_spline_catmull_rom: ERROR memory bounds check failed!\n");
			return (NULL);
		}
		points_ptr = calloc (point_count, sizeof (Vector2));
		if (points_ptr == NULL)
		{
			printf ("raylib_draw_spline_catmull_rom: ERROR out of memory!\n");
			return (NULL);
		}
		for (i = 0; i < point_count; i++)
		{
			if (rlib_read_vec2 (data, points_addr + (i * 16), &points_ptr[i]) != 0)
			{
				free (points_ptr);
				return (NULL);
			}
		}
	}
	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawSplineCatmullRom ((const Vector2 *) points_ptr, (int) point_count, (float) thick, color);
	pthread_mutex_unlock (&rlib_mutex);

	free (points_ptr);

	return (sp);
}

U1 *raylib_draw_spline_linear (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: points_addr, point_count, thick, color_addr
	S8 points_addr ALIGN;
	Vector2 *points_ptr = NULL;
	S8 point_count ALIGN;
	F8 thick ALIGN;
	Color color;
	S8 color_addr ALIGN;
	S8 i ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &thick, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &point_count, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &points_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (point_count > 0)
	{
		if (memory_bounds (points_addr, (point_count - 1) * 16) != 0)
		{
			printf ("raylib_draw_spline_linear: ERROR memory bounds check failed!\n");
			return (NULL);
		}
		points_ptr = calloc (point_count, sizeof (Vector2));
		if (points_ptr == NULL)
		{
			printf ("raylib_draw_spline_linear: ERROR out of memory!\n");
			return (NULL);
		}
		for (i = 0; i < point_count; i++)
		{
			if (rlib_read_vec2 (data, points_addr + (i * 16), &points_ptr[i]) != 0)
			{
				free (points_ptr);
				return (NULL);
			}
		}
	}
	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawSplineLinear ((const Vector2 *) points_ptr, (int) point_count, (float) thick, color);
	pthread_mutex_unlock (&rlib_mutex);

	free (points_ptr);

	return (sp);
}

U1 *raylib_draw_spline_segment_basis (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: p1_addr, p2_addr, p3_addr, p4_addr, thick, color_addr
	Vector2 p1;
	S8 p1_addr ALIGN;
	Vector2 p2;
	S8 p2_addr ALIGN;
	Vector2 p3;
	S8 p3_addr ALIGN;
	Vector2 p4;
	S8 p4_addr ALIGN;
	F8 thick ALIGN;
	Color color;
	S8 color_addr ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &thick, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &p4_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &p3_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &p2_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &p1_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec2 (data, p1_addr, &p1) != 0) return (NULL);
	if (rlib_read_vec2 (data, p2_addr, &p2) != 0) return (NULL);
	if (rlib_read_vec2 (data, p3_addr, &p3) != 0) return (NULL);
	if (rlib_read_vec2 (data, p4_addr, &p4) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawSplineSegmentBasis (p1, p2, p3, p4, (float) thick, color);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_spline_segment_bezier_cubic (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: p1_addr, c2_addr, c3_addr, p4_addr, thick, color_addr
	Vector2 p1;
	S8 p1_addr ALIGN;
	Vector2 c2;
	S8 c2_addr ALIGN;
	Vector2 c3;
	S8 c3_addr ALIGN;
	Vector2 p4;
	S8 p4_addr ALIGN;
	F8 thick ALIGN;
	Color color;
	S8 color_addr ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &thick, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &p4_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &c3_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &c2_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &p1_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec2 (data, p1_addr, &p1) != 0) return (NULL);
	if (rlib_read_vec2 (data, c2_addr, &c2) != 0) return (NULL);
	if (rlib_read_vec2 (data, c3_addr, &c3) != 0) return (NULL);
	if (rlib_read_vec2 (data, p4_addr, &p4) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawSplineSegmentBezierCubic (p1, c2, c3, p4, (float) thick, color);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_spline_segment_bezier_quadratic (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: p1_addr, c2_addr, p3_addr, thick, color_addr
	Vector2 p1;
	S8 p1_addr ALIGN;
	Vector2 c2;
	S8 c2_addr ALIGN;
	Vector2 p3;
	S8 p3_addr ALIGN;
	F8 thick ALIGN;
	Color color;
	S8 color_addr ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &thick, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &p3_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &c2_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &p1_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec2 (data, p1_addr, &p1) != 0) return (NULL);
	if (rlib_read_vec2 (data, c2_addr, &c2) != 0) return (NULL);
	if (rlib_read_vec2 (data, p3_addr, &p3) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawSplineSegmentBezierQuadratic (p1, c2, p3, (float) thick, color);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_spline_segment_catmull_rom (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: p1_addr, p2_addr, p3_addr, p4_addr, thick, color_addr
	Vector2 p1;
	S8 p1_addr ALIGN;
	Vector2 p2;
	S8 p2_addr ALIGN;
	Vector2 p3;
	S8 p3_addr ALIGN;
	Vector2 p4;
	S8 p4_addr ALIGN;
	F8 thick ALIGN;
	Color color;
	S8 color_addr ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &thick, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &p4_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &p3_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &p2_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &p1_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec2 (data, p1_addr, &p1) != 0) return (NULL);
	if (rlib_read_vec2 (data, p2_addr, &p2) != 0) return (NULL);
	if (rlib_read_vec2 (data, p3_addr, &p3) != 0) return (NULL);
	if (rlib_read_vec2 (data, p4_addr, &p4) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawSplineSegmentCatmullRom (p1, p2, p3, p4, (float) thick, color);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_spline_segment_linear (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: p1_addr, p2_addr, thick, color_addr
	Vector2 p1;
	S8 p1_addr ALIGN;
	Vector2 p2;
	S8 p2_addr ALIGN;
	F8 thick ALIGN;
	Color color;
	S8 color_addr ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &thick, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &p2_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &p1_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec2 (data, p1_addr, &p1) != 0) return (NULL);
	if (rlib_read_vec2 (data, p2_addr, &p2) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawSplineSegmentLinear (p1, p2, (float) thick, color);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_text_codepoint (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: font_handle, codepoint, position_addr, font_size, tint_addr
	S8 font_handle ALIGN;
	S8 codepoint ALIGN;
	Vector2 position;
	S8 position_addr ALIGN;
	F8 font_size ALIGN;
	Color tint;
	S8 tint_addr ALIGN;

	sp = stpopi ((U1 *) &tint_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &font_size, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &position_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &codepoint, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &font_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (font_handle < 0 || font_handle >= RAYLIB_MAX_FONTS || raylib_fonts_used[font_handle] == 0)
	{
		printf ("raylib_draw_text_codepoint: ERROR invalid font handle!\n");
		return (NULL);
	}

	if (rlib_read_vec2 (data, position_addr, &position) != 0) return (NULL);
	if (rlib_read_color (data, tint_addr, &tint) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawTextCodepoint (raylib_fonts[font_handle], (int) codepoint, position, (float) font_size, tint);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_text_codepoints (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: font_handle, codepoints_addr, codepoint_count, position_addr, font_size, spacing, tint_addr
	S8 font_handle ALIGN;
	S8 codepoints_addr ALIGN;
	int *codepoints_ptr = NULL;
	S8 codepoint_count ALIGN;
	Vector2 position;
	S8 position_addr ALIGN;
	F8 font_size ALIGN;
	F8 spacing ALIGN;
	Color tint;
	S8 tint_addr ALIGN;
	S8 i ALIGN;
	F8 t ALIGN;

	sp = stpopi ((U1 *) &tint_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &spacing, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &font_size, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &position_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &codepoint_count, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &codepoints_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &font_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (font_handle < 0 || font_handle >= RAYLIB_MAX_FONTS || raylib_fonts_used[font_handle] == 0)
	{
		printf ("raylib_draw_text_codepoints: ERROR invalid font handle!\n");
		return (NULL);
	}

	if (codepoint_count > 0)
	{
		if (memory_bounds (codepoints_addr, (codepoint_count - 1) * 8) != 0)
		{
			printf ("raylib_draw_text_codepoints: ERROR memory bounds check failed!\n");
			return (NULL);
		}
		codepoints_ptr = calloc (codepoint_count, sizeof (int));
		if (codepoints_ptr == NULL)
		{
			printf ("raylib_draw_text_codepoints: ERROR out of memory!\n");
			return (NULL);
		}
		for (i = 0; i < codepoint_count; i++)
		{
			memcpy (&t, &data[codepoints_addr + (i * 8)], sizeof (F8));
			codepoints_ptr[i] = (int) t;
		}
	}
	if (rlib_read_vec2 (data, position_addr, &position) != 0) return (NULL);
	if (rlib_read_color (data, tint_addr, &tint) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawTextCodepoints (raylib_fonts[font_handle], (const int *) codepoints_ptr, (int) codepoint_count, position, (float) font_size, (float) spacing, tint);
	pthread_mutex_unlock (&rlib_mutex);

	free (codepoints_ptr);

	return (sp);
}

U1 *raylib_draw_texture_n_patch (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: texture_handle, n_patch_info_addr, dstrec_addr, origin_addr, rotation, tint_addr
	S8 texture_handle ALIGN;
	NPatchInfo n_patch_info;
	S8 n_patch_info_addr ALIGN;
	Rectangle dstrec;
	S8 dstrec_addr ALIGN;
	Vector2 origin;
	S8 origin_addr ALIGN;
	F8 rotation ALIGN;
	Color tint;
	S8 tint_addr ALIGN;

	sp = stpopi ((U1 *) &tint_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &rotation, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &origin_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &dstrec_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &n_patch_info_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &texture_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (texture_handle < 0 || texture_handle >= RAYLIB_MAX_TEXTURES || raylib_textures_used[texture_handle] == 0)
	{
		printf ("raylib_draw_texture_n_patch: ERROR invalid texture handle!\n");
		return (NULL);
	}

	if (rlib_read_npatchinfo (data, n_patch_info_addr, &n_patch_info) != 0) return (NULL);
	if (rlib_read_rect (data, dstrec_addr, &dstrec) != 0) return (NULL);
	if (rlib_read_vec2 (data, origin_addr, &origin) != 0) return (NULL);
	if (rlib_read_color (data, tint_addr, &tint) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawTextureNPatch (raylib_textures[texture_handle], n_patch_info, dstrec, origin, (float) rotation, tint);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_triangle_fan (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: points_addr, point_count, color_addr
	S8 points_addr ALIGN;
	Vector2 *points_ptr = NULL;
	S8 point_count ALIGN;
	Color color;
	S8 color_addr ALIGN;
	S8 i ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &point_count, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &points_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (point_count > 0)
	{
		if (memory_bounds (points_addr, (point_count - 1) * 16) != 0)
		{
			printf ("raylib_draw_triangle_fan: ERROR memory bounds check failed!\n");
			return (NULL);
		}
		points_ptr = calloc (point_count, sizeof (Vector2));
		if (points_ptr == NULL)
		{
			printf ("raylib_draw_triangle_fan: ERROR out of memory!\n");
			return (NULL);
		}
		for (i = 0; i < point_count; i++)
		{
			if (rlib_read_vec2 (data, points_addr + (i * 16), &points_ptr[i]) != 0)
			{
				free (points_ptr);
				return (NULL);
			}
		}
	}
	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawTriangleFan ((const Vector2 *) points_ptr, (int) point_count, color);
	pthread_mutex_unlock (&rlib_mutex);

	free (points_ptr);

	return (sp);
}

U1 *raylib_draw_triangle_gradient (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: v1_addr, v2_addr, v3_addr, c1_addr, c2_addr, c3_addr
	Vector2 v1;
	S8 v1_addr ALIGN;
	Vector2 v2;
	S8 v2_addr ALIGN;
	Vector2 v3;
	S8 v3_addr ALIGN;
	Color c1;
	S8 c1_addr ALIGN;
	Color c2;
	S8 c2_addr ALIGN;
	Color c3;
	S8 c3_addr ALIGN;

	sp = stpopi ((U1 *) &c3_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &c2_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &c1_addr, sp, sp_top);
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
	if (rlib_read_color (data, c1_addr, &c1) != 0) return (NULL);
	if (rlib_read_color (data, c2_addr, &c2) != 0) return (NULL);
	if (rlib_read_color (data, c3_addr, &c3) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawTriangleGradient (v1, v2, v3, c1, c2, c3);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_triangle_lines_ex (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: v1_addr, v2_addr, v3_addr, thick, color_addr
	Vector2 v1;
	S8 v1_addr ALIGN;
	Vector2 v2;
	S8 v2_addr ALIGN;
	Vector2 v3;
	S8 v3_addr ALIGN;
	F8 thick ALIGN;
	Color color;
	S8 color_addr ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &thick, sp, sp_top);
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
	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawTriangleLinesEx (v1, v2, v3, (float) thick, color);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_draw_triangle_strip (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: points_addr, point_count, color_addr
	S8 points_addr ALIGN;
	Vector2 *points_ptr = NULL;
	S8 point_count ALIGN;
	Color color;
	S8 color_addr ALIGN;
	S8 i ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &point_count, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &points_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (point_count > 0)
	{
		if (memory_bounds (points_addr, (point_count - 1) * 16) != 0)
		{
			printf ("raylib_draw_triangle_strip: ERROR memory bounds check failed!\n");
			return (NULL);
		}
		points_ptr = calloc (point_count, sizeof (Vector2));
		if (points_ptr == NULL)
		{
			printf ("raylib_draw_triangle_strip: ERROR out of memory!\n");
			return (NULL);
		}
		for (i = 0; i < point_count; i++)
		{
			if (rlib_read_vec2 (data, points_addr + (i * 16), &points_ptr[i]) != 0)
			{
				free (points_ptr);
				return (NULL);
			}
		}
	}
	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	DrawTriangleStrip ((const Vector2 *) points_ptr, (int) point_count, color);
	pthread_mutex_unlock (&rlib_mutex);

	free (points_ptr);

	return (sp);
}

U1 *raylib_enable_cursor (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{

	pthread_mutex_lock (&rlib_mutex);
	EnableCursor ();
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_end_shader_mode (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{

	pthread_mutex_lock (&rlib_mutex);
	EndShaderMode ();
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_end_texture_mode (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{

	pthread_mutex_lock (&rlib_mutex);
	EndTextureMode ();
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_end_vr_stereo_mode (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{

	pthread_mutex_lock (&rlib_mutex);
	EndVrStereoMode ();
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_export_automation_event_list (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: list_handle, file_name_addr
	// return: 1 true, 0 false
	S8 list_handle ALIGN;
	S8 file_name_addr ALIGN;
	U1 file_name_buf[512];
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &file_name_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &list_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (list_handle < 0 || list_handle >= RAYLIB_MAX_AUTOMATION || raylib_automationlists_used[list_handle] == 0)
	{
		printf ("raylib_export_automation_event_list: ERROR invalid automation list handle!\n");
		return (NULL);
	}

	if (rlib_load_filename (data, file_name_addr, file_name_buf, sizeof (file_name_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = ExportAutomationEventList (raylib_automationlists[list_handle], (const char *) file_name_buf) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_export_data_as_code (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: data_addr, data_size, file_name_addr
	// return: 1 true, 0 false
	S8 data_addr ALIGN;
	S8 data_size ALIGN;
	S8 file_name_addr ALIGN;
	U1 file_name_buf[512];
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &file_name_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &data_size, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &data_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (data_size > 0)
	{
		if (memory_bounds (data_addr, data_size - 1) != 0)
		{
			printf ("raylib_export_data_as_code: ERROR memory bounds check failed!\n");
			return (NULL);
		}
	}
	if (rlib_load_filename (data, file_name_addr, file_name_buf, sizeof (file_name_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = ExportDataAsCode ((const unsigned char *) &data[data_addr], (int) data_size, (const char *) file_name_buf) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_export_font_as_code (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: font_handle, file_name_addr
	// return: 1 true, 0 false
	S8 font_handle ALIGN;
	S8 file_name_addr ALIGN;
	U1 file_name_buf[512];
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &file_name_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &font_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (font_handle < 0 || font_handle >= RAYLIB_MAX_FONTS || raylib_fonts_used[font_handle] == 0)
	{
		printf ("raylib_export_font_as_code: ERROR invalid font handle!\n");
		return (NULL);
	}

	if (rlib_load_filename (data, file_name_addr, file_name_buf, sizeof (file_name_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = ExportFontAsCode (raylib_fonts[font_handle], (const char *) file_name_buf) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_export_image (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: image_handle, file_name_addr
	// return: 1 true, 0 false
	S8 image_handle ALIGN;
	S8 file_name_addr ALIGN;
	U1 file_name_buf[512];
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &file_name_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &image_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (image_handle < 0 || image_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[image_handle] == 0)
	{
		printf ("raylib_export_image: ERROR invalid image handle!\n");
		return (NULL);
	}

	if (rlib_load_filename (data, file_name_addr, file_name_buf, sizeof (file_name_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = ExportImage (raylib_images[image_handle], (const char *) file_name_buf) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_export_image_as_code (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: image_handle, file_name_addr
	// return: 1 true, 0 false
	S8 image_handle ALIGN;
	S8 file_name_addr ALIGN;
	U1 file_name_buf[512];
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &file_name_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &image_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (image_handle < 0 || image_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[image_handle] == 0)
	{
		printf ("raylib_export_image_as_code: ERROR invalid image handle!\n");
		return (NULL);
	}

	if (rlib_load_filename (data, file_name_addr, file_name_buf, sizeof (file_name_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = ExportImageAsCode (raylib_images[image_handle], (const char *) file_name_buf) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_export_mesh (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: mesh_handle, file_name_addr
	// return: 1 true, 0 false
	S8 mesh_handle ALIGN;
	S8 file_name_addr ALIGN;
	U1 file_name_buf[512];
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &file_name_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &mesh_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (mesh_handle < 0 || mesh_handle >= RAYLIB_MAX_MESHES || raylib_meshes_used[mesh_handle] == 0)
	{
		printf ("raylib_export_mesh: ERROR invalid mesh handle!\n");
		return (NULL);
	}

	if (rlib_load_filename (data, file_name_addr, file_name_buf, sizeof (file_name_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = ExportMesh (raylib_meshes[mesh_handle], (const char *) file_name_buf) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_export_mesh_as_code (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: mesh_handle, file_name_addr
	// return: 1 true, 0 false
	S8 mesh_handle ALIGN;
	S8 file_name_addr ALIGN;
	U1 file_name_buf[512];
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &file_name_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &mesh_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (mesh_handle < 0 || mesh_handle >= RAYLIB_MAX_MESHES || raylib_meshes_used[mesh_handle] == 0)
	{
		printf ("raylib_export_mesh_as_code: ERROR invalid mesh handle!\n");
		return (NULL);
	}

	if (rlib_load_filename (data, file_name_addr, file_name_buf, sizeof (file_name_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = ExportMeshAsCode (raylib_meshes[mesh_handle], (const char *) file_name_buf) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_export_wave (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: wave_handle, file_name_addr
	// return: 1 true, 0 false
	S8 wave_handle ALIGN;
	S8 file_name_addr ALIGN;
	U1 file_name_buf[512];
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &file_name_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &wave_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (wave_handle < 0 || wave_handle >= RAYLIB_MAX_WAVES || raylib_waves_used[wave_handle] == 0)
	{
		printf ("raylib_export_wave: ERROR invalid wave handle!\n");
		return (NULL);
	}

	if (rlib_load_filename (data, file_name_addr, file_name_buf, sizeof (file_name_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = ExportWave (raylib_waves[wave_handle], (const char *) file_name_buf) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_export_wave_as_code (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: wave_handle, file_name_addr
	// return: 1 true, 0 false
	S8 wave_handle ALIGN;
	S8 file_name_addr ALIGN;
	U1 file_name_buf[512];
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &file_name_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &wave_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (wave_handle < 0 || wave_handle >= RAYLIB_MAX_WAVES || raylib_waves_used[wave_handle] == 0)
	{
		printf ("raylib_export_wave_as_code: ERROR invalid wave handle!\n");
		return (NULL);
	}

	if (rlib_load_filename (data, file_name_addr, file_name_buf, sizeof (file_name_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = ExportWaveAsCode (raylib_waves[wave_handle], (const char *) file_name_buf) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_fade (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: color_addr, alpha, out_addr
	// return: struct written to out_addr
	Color color;
	S8 color_addr ALIGN;
	F8 alpha ALIGN;
	S8 out_addr ALIGN;
	Color ret;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &alpha, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = Fade (color, (float) alpha);
	pthread_mutex_unlock (&rlib_mutex);

	if (rlib_write_color (data, out_addr, ret) != 0) return (NULL);

	return (sp);
}

U1 *raylib_file_copy (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: src_path_addr, dst_path_addr
	// return: integer result
	S8 src_path_addr ALIGN;
	U1 src_path_buf[512];
	S8 dst_path_addr ALIGN;
	U1 dst_path_buf[512];
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &dst_path_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &src_path_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_load_filename (data, src_path_addr, src_path_buf, sizeof (src_path_buf)) != 0) return (NULL);
	if (rlib_load_filename (data, dst_path_addr, dst_path_buf, sizeof (dst_path_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = FileCopy ((const char *) src_path_buf, (const char *) dst_path_buf);
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_file_exists (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: file_name_addr
	// return: 1 true, 0 false
	S8 file_name_addr ALIGN;
	U1 file_name_buf[512];
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &file_name_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_load_filename (data, file_name_addr, file_name_buf, sizeof (file_name_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = FileExists ((const char *) file_name_buf) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_file_move (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: src_path_addr, dst_path_addr
	// return: integer result
	S8 src_path_addr ALIGN;
	U1 src_path_buf[512];
	S8 dst_path_addr ALIGN;
	U1 dst_path_buf[512];
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &dst_path_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &src_path_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_load_filename (data, src_path_addr, src_path_buf, sizeof (src_path_buf)) != 0) return (NULL);
	if (rlib_load_filename (data, dst_path_addr, dst_path_buf, sizeof (dst_path_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = FileMove ((const char *) src_path_buf, (const char *) dst_path_buf);
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_file_remove (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: file_name_addr
	// return: integer result
	S8 file_name_addr ALIGN;
	U1 file_name_buf[512];
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &file_name_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_load_filename (data, file_name_addr, file_name_buf, sizeof (file_name_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = FileRemove ((const char *) file_name_buf);
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_file_rename (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: file_name_addr, file_rename_addr
	// return: integer result
	S8 file_name_addr ALIGN;
	U1 file_name_buf[512];
	S8 file_rename_addr ALIGN;
	U1 file_rename_buf[512];
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &file_rename_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &file_name_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_load_filename (data, file_name_addr, file_name_buf, sizeof (file_name_buf)) != 0) return (NULL);
	if (rlib_read_string (data, file_rename_addr, file_rename_buf, sizeof (file_rename_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = FileRename ((const char *) file_name_buf, (const char *) file_rename_buf);
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_file_text_find_index (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: file_name_addr, search_addr
	// return: integer result
	S8 file_name_addr ALIGN;
	U1 file_name_buf[512];
	S8 search_addr ALIGN;
	U1 search_buf[512];
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &search_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &file_name_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_load_filename (data, file_name_addr, file_name_buf, sizeof (file_name_buf)) != 0) return (NULL);
	if (rlib_read_string (data, search_addr, search_buf, sizeof (search_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = FileTextFindIndex ((const char *) file_name_buf, (const char *) search_buf);
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_file_text_replace (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: file_name_addr, search_addr, replacement_addr
	// return: integer result
	S8 file_name_addr ALIGN;
	U1 file_name_buf[512];
	S8 search_addr ALIGN;
	U1 search_buf[512];
	S8 replacement_addr ALIGN;
	U1 replacement_buf[512];
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &replacement_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &search_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &file_name_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_load_filename (data, file_name_addr, file_name_buf, sizeof (file_name_buf)) != 0) return (NULL);
	if (rlib_read_string (data, search_addr, search_buf, sizeof (search_buf)) != 0) return (NULL);
	if (rlib_read_string (data, replacement_addr, replacement_buf, sizeof (replacement_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = FileTextReplace ((const char *) file_name_buf, (const char *) search_buf, (const char *) replacement_buf);
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_gen_image_cellular (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: width, height, tile_size
	// return: image handle or -1 on error
	S8 width ALIGN;
	S8 height ALIGN;
	S8 tile_size ALIGN;
	S8 ind ALIGN;

	sp = stpopi ((U1 *) &tile_size, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &height, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &width, sp, sp_top);
	if (sp == NULL) return (NULL);

	ind = rlib_find_slot (raylib_images_used, RAYLIB_MAX_IMAGES);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_images[ind] = GenImageCellular ((int) width, (int) height, (int) tile_size);
	pthread_mutex_unlock (&rlib_mutex);

	if (raylib_images[ind].data == NULL)
	{
		printf ("raylib_gen_image_cellular: ERROR operation failed!\n");
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	raylib_images_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_gen_image_gradient_linear (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: width, height, direction, start_addr, end_addr
	// return: image handle or -1 on error
	S8 width ALIGN;
	S8 height ALIGN;
	S8 direction ALIGN;
	Color start;
	S8 start_addr ALIGN;
	Color end;
	S8 end_addr ALIGN;
	S8 ind ALIGN;

	sp = stpopi ((U1 *) &end_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &start_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &direction, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &height, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &width, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_color (data, start_addr, &start) != 0) return (NULL);
	if (rlib_read_color (data, end_addr, &end) != 0) return (NULL);

	ind = rlib_find_slot (raylib_images_used, RAYLIB_MAX_IMAGES);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_images[ind] = GenImageGradientLinear ((int) width, (int) height, (int) direction, start, end);
	pthread_mutex_unlock (&rlib_mutex);

	if (raylib_images[ind].data == NULL)
	{
		printf ("raylib_gen_image_gradient_linear: ERROR operation failed!\n");
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	raylib_images_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_gen_image_gradient_radial (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: width, height, density, inner_addr, outer_addr
	// return: image handle or -1 on error
	S8 width ALIGN;
	S8 height ALIGN;
	F8 density ALIGN;
	Color inner;
	S8 inner_addr ALIGN;
	Color outer;
	S8 outer_addr ALIGN;
	S8 ind ALIGN;

	sp = stpopi ((U1 *) &outer_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &inner_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &density, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &height, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &width, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_color (data, inner_addr, &inner) != 0) return (NULL);
	if (rlib_read_color (data, outer_addr, &outer) != 0) return (NULL);

	ind = rlib_find_slot (raylib_images_used, RAYLIB_MAX_IMAGES);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_images[ind] = GenImageGradientRadial ((int) width, (int) height, (float) density, inner, outer);
	pthread_mutex_unlock (&rlib_mutex);

	if (raylib_images[ind].data == NULL)
	{
		printf ("raylib_gen_image_gradient_radial: ERROR operation failed!\n");
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	raylib_images_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_gen_image_gradient_square (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: width, height, density, inner_addr, outer_addr
	// return: image handle or -1 on error
	S8 width ALIGN;
	S8 height ALIGN;
	F8 density ALIGN;
	Color inner;
	S8 inner_addr ALIGN;
	Color outer;
	S8 outer_addr ALIGN;
	S8 ind ALIGN;

	sp = stpopi ((U1 *) &outer_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &inner_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &density, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &height, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &width, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_color (data, inner_addr, &inner) != 0) return (NULL);
	if (rlib_read_color (data, outer_addr, &outer) != 0) return (NULL);

	ind = rlib_find_slot (raylib_images_used, RAYLIB_MAX_IMAGES);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_images[ind] = GenImageGradientSquare ((int) width, (int) height, (float) density, inner, outer);
	pthread_mutex_unlock (&rlib_mutex);

	if (raylib_images[ind].data == NULL)
	{
		printf ("raylib_gen_image_gradient_square: ERROR operation failed!\n");
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	raylib_images_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_gen_image_perlin_noise (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: width, height, offset_x, offset_y, scale
	// return: image handle or -1 on error
	S8 width ALIGN;
	S8 height ALIGN;
	S8 offset_x ALIGN;
	S8 offset_y ALIGN;
	F8 scale ALIGN;
	S8 ind ALIGN;

	sp = stpopd ((U1 *) &scale, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &offset_y, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &offset_x, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &height, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &width, sp, sp_top);
	if (sp == NULL) return (NULL);

	ind = rlib_find_slot (raylib_images_used, RAYLIB_MAX_IMAGES);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_images[ind] = GenImagePerlinNoise ((int) width, (int) height, (int) offset_x, (int) offset_y, (float) scale);
	pthread_mutex_unlock (&rlib_mutex);

	if (raylib_images[ind].data == NULL)
	{
		printf ("raylib_gen_image_perlin_noise: ERROR operation failed!\n");
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	raylib_images_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_gen_image_text (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: width, height, text_addr
	// return: image handle or -1 on error
	S8 width ALIGN;
	S8 height ALIGN;
	S8 text_addr ALIGN;
	U1 text_buf[4096];
	S8 ind ALIGN;

	sp = stpopi ((U1 *) &text_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &height, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &width, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_string (data, text_addr, text_buf, sizeof (text_buf)) != 0) return (NULL);

	ind = rlib_find_slot (raylib_images_used, RAYLIB_MAX_IMAGES);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_images[ind] = GenImageText ((int) width, (int) height, (const char *) text_buf);
	pthread_mutex_unlock (&rlib_mutex);

	if (raylib_images[ind].data == NULL)
	{
		printf ("raylib_gen_image_text: ERROR operation failed!\n");
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	raylib_images_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_gen_image_white_noise (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: width, height, factor
	// return: image handle or -1 on error
	S8 width ALIGN;
	S8 height ALIGN;
	F8 factor ALIGN;
	S8 ind ALIGN;

	sp = stpopd ((U1 *) &factor, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &height, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &width, sp, sp_top);
	if (sp == NULL) return (NULL);

	ind = rlib_find_slot (raylib_images_used, RAYLIB_MAX_IMAGES);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_images[ind] = GenImageWhiteNoise ((int) width, (int) height, (float) factor);
	pthread_mutex_unlock (&rlib_mutex);

	if (raylib_images[ind].data == NULL)
	{
		printf ("raylib_gen_image_white_noise: ERROR operation failed!\n");
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	raylib_images_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_gen_mesh_tangents (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: mesh_handle
	S8 mesh_handle ALIGN;

	sp = stpopi ((U1 *) &mesh_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (mesh_handle < 0 || mesh_handle >= RAYLIB_MAX_MESHES || raylib_meshes_used[mesh_handle] == 0)
	{
		printf ("raylib_gen_mesh_tangents: ERROR invalid mesh handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	GenMeshTangents (&raylib_meshes[mesh_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_gen_texture_mipmaps (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: texture_handle
	S8 texture_handle ALIGN;

	sp = stpopi ((U1 *) &texture_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (texture_handle < 0 || texture_handle >= RAYLIB_MAX_TEXTURES || raylib_textures_used[texture_handle] == 0)
	{
		printf ("raylib_gen_texture_mipmaps: ERROR invalid texture handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	GenTextureMipmaps (&raylib_textures[texture_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_get_application_directory (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: out_addr
	// return: string written to out_addr
	S8 out_addr ALIGN;
	const char *text;
	S8 len ALIGN;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	text = GetApplicationDirectory ();
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
		printf ("raylib_get_application_directory: ERROR memory bounds check failed!\n");
		return (NULL);
	}
	if (len > 0) memcpy (&data[out_addr], text, len);
	data[out_addr + len] = '\0';

	return (sp);
}

U1 *raylib_get_camera_matrix (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: camera_addr, out_addr
	// return: struct written to out_addr
	Camera camera;
	S8 camera_addr ALIGN;
	S8 out_addr ALIGN;
	Matrix ret;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &camera_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_camera (data, camera_addr, &camera) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = GetCameraMatrix (camera);
	pthread_mutex_unlock (&rlib_mutex);

	if (rlib_write_matrix (data, out_addr, ret) != 0) return (NULL);

	return (sp);
}

U1 *raylib_get_camera_matrix_2d (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: camera_addr, out_addr
	// return: struct written to out_addr
	Camera2D camera;
	S8 camera_addr ALIGN;
	S8 out_addr ALIGN;
	Matrix ret;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &camera_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_camera2d (data, camera_addr, &camera) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = GetCameraMatrix2D (camera);
	pthread_mutex_unlock (&rlib_mutex);

	if (rlib_write_matrix (data, out_addr, ret) != 0) return (NULL);

	return (sp);
}

U1 *raylib_get_clipboard_image (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// return: image handle or -1 on error
	S8 ind ALIGN;

	ind = rlib_find_slot (raylib_images_used, RAYLIB_MAX_IMAGES);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_images[ind] = GetClipboardImage ();
	pthread_mutex_unlock (&rlib_mutex);

	if (raylib_images[ind].data == NULL)
	{
		printf ("raylib_get_clipboard_image: ERROR operation failed!\n");
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	raylib_images_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_get_codepoint (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: text_addr
	// return: integer result
	S8 text_addr ALIGN;
	U1 text_buf[4096];
	int codepoint_size_out = 0;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &text_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_string (data, text_addr, text_buf, sizeof (text_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = GetCodepoint ((const char *) text_buf, &codepoint_size_out);
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_get_codepoint_count (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: text_addr
	// return: integer result
	S8 text_addr ALIGN;
	U1 text_buf[4096];
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &text_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_string (data, text_addr, text_buf, sizeof (text_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = GetCodepointCount ((const char *) text_buf);
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_get_codepoint_next (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: text_addr
	// return: integer result
	S8 text_addr ALIGN;
	U1 text_buf[4096];
	int codepoint_size_out = 0;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &text_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_string (data, text_addr, text_buf, sizeof (text_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = GetCodepointNext ((const char *) text_buf, &codepoint_size_out);
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_get_codepoint_previous (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: text_addr
	// return: integer result
	S8 text_addr ALIGN;
	U1 text_buf[4096];
	int codepoint_size_out = 0;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &text_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_string (data, text_addr, text_buf, sizeof (text_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = GetCodepointPrevious ((const char *) text_buf, &codepoint_size_out);
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_get_collision_rec (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: rec1_addr, rec2_addr, out_addr
	// return: struct written to out_addr
	Rectangle rec1;
	S8 rec1_addr ALIGN;
	Rectangle rec2;
	S8 rec2_addr ALIGN;
	S8 out_addr ALIGN;
	Rectangle ret;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &rec2_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &rec1_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_rect (data, rec1_addr, &rec1) != 0) return (NULL);
	if (rlib_read_rect (data, rec2_addr, &rec2) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = GetCollisionRec (rec1, rec2);
	pthread_mutex_unlock (&rlib_mutex);

	if (rlib_write_rect (data, out_addr, ret) != 0) return (NULL);

	return (sp);
}

U1 *raylib_get_color (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: hex_value, out_addr
	// return: struct written to out_addr
	S8 hex_value ALIGN;
	S8 out_addr ALIGN;
	Color ret;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &hex_value, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = GetColor ((unsigned int) hex_value);
	pthread_mutex_unlock (&rlib_mutex);

	if (rlib_write_color (data, out_addr, ret) != 0) return (NULL);

	return (sp);
}

U1 *raylib_get_directory_file_count (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: dir_path_addr
	// return: integer result
	S8 dir_path_addr ALIGN;
	U1 dir_path_buf[512];
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &dir_path_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_load_filename (data, dir_path_addr, dir_path_buf, sizeof (dir_path_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = GetDirectoryFileCount ((const char *) dir_path_buf);
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_get_directory_file_count_ex (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: base_path_addr, filter_addr, scan_subdirs
	// return: integer result
	S8 base_path_addr ALIGN;
	U1 base_path_buf[512];
	S8 filter_addr ALIGN;
	U1 filter_buf[512];
	S8 scan_subdirs ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &scan_subdirs, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &filter_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &base_path_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_load_filename (data, base_path_addr, base_path_buf, sizeof (base_path_buf)) != 0) return (NULL);
	if (rlib_read_string (data, filter_addr, filter_buf, sizeof (filter_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = GetDirectoryFileCountEx ((const char *) base_path_buf, (const char *) filter_buf, (bool) scan_subdirs);
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_get_directory_path (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: file_path_addr, out_addr
	// return: string written to out_addr
	S8 file_path_addr ALIGN;
	U1 file_path_buf[512];
	S8 out_addr ALIGN;
	const char *text;
	S8 len ALIGN;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &file_path_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_load_filename (data, file_path_addr, file_path_buf, sizeof (file_path_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	text = GetDirectoryPath ((const char *) file_path_buf);
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
		printf ("raylib_get_directory_path: ERROR memory bounds check failed!\n");
		return (NULL);
	}
	if (len > 0) memcpy (&data[out_addr], text, len);
	data[out_addr + len] = '\0';

	return (sp);
}

U1 *raylib_get_file_extension (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: file_name_addr, out_addr
	// return: string written to out_addr
	S8 file_name_addr ALIGN;
	U1 file_name_buf[512];
	S8 out_addr ALIGN;
	const char *text;
	S8 len ALIGN;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &file_name_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_load_filename (data, file_name_addr, file_name_buf, sizeof (file_name_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	text = GetFileExtension ((const char *) file_name_buf);
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
		printf ("raylib_get_file_extension: ERROR memory bounds check failed!\n");
		return (NULL);
	}
	if (len > 0) memcpy (&data[out_addr], text, len);
	data[out_addr + len] = '\0';

	return (sp);
}

U1 *raylib_get_file_mod_time (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: file_name_addr
	// return: integer result
	S8 file_name_addr ALIGN;
	U1 file_name_buf[512];
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &file_name_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_load_filename (data, file_name_addr, file_name_buf, sizeof (file_name_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = GetFileModTime ((const char *) file_name_buf);
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_get_file_name (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: file_path_addr, out_addr
	// return: string written to out_addr
	S8 file_path_addr ALIGN;
	U1 file_path_buf[512];
	S8 out_addr ALIGN;
	const char *text;
	S8 len ALIGN;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &file_path_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_load_filename (data, file_path_addr, file_path_buf, sizeof (file_path_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	text = GetFileName ((const char *) file_path_buf);
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
		printf ("raylib_get_file_name: ERROR memory bounds check failed!\n");
		return (NULL);
	}
	if (len > 0) memcpy (&data[out_addr], text, len);
	data[out_addr + len] = '\0';

	return (sp);
}

U1 *raylib_get_file_name_without_ext (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: file_path_addr, out_addr
	// return: string written to out_addr
	S8 file_path_addr ALIGN;
	U1 file_path_buf[512];
	S8 out_addr ALIGN;
	const char *text;
	S8 len ALIGN;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &file_path_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_load_filename (data, file_path_addr, file_path_buf, sizeof (file_path_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	text = GetFileNameWithoutExt ((const char *) file_path_buf);
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
		printf ("raylib_get_file_name_without_ext: ERROR memory bounds check failed!\n");
		return (NULL);
	}
	if (len > 0) memcpy (&data[out_addr], text, len);
	data[out_addr + len] = '\0';

	return (sp);
}

U1 *raylib_get_file_length (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: file_name_addr
	// return: integer result
	S8 file_name_addr ALIGN;
	U1 file_name_buf[512];
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &file_name_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_load_filename (data, file_name_addr, file_name_buf, sizeof (file_name_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = GetFileLength ((const char *) file_name_buf);
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_get_gesture_detected (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// return: integer result
	S8 ret ALIGN;

	pthread_mutex_lock (&rlib_mutex);
	ret = GetGestureDetected ();
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_get_gesture_drag_angle (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// return: double value
	F8 ret ALIGN;

	pthread_mutex_lock (&rlib_mutex);
	ret = GetGestureDragAngle ();
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushd (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_get_gesture_drag_vector (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: out_addr
	// return: struct written to out_addr
	S8 out_addr ALIGN;
	Vector2 ret;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = GetGestureDragVector ();
	pthread_mutex_unlock (&rlib_mutex);

	if (rlib_write_vec2 (data, out_addr, ret) != 0) return (NULL);

	return (sp);
}

U1 *raylib_get_gesture_hold_duration (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// return: double value
	F8 ret ALIGN;

	pthread_mutex_lock (&rlib_mutex);
	ret = GetGestureHoldDuration ();
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushd (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_get_gesture_pinch_angle (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// return: double value
	F8 ret ALIGN;

	pthread_mutex_lock (&rlib_mutex);
	ret = GetGesturePinchAngle ();
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushd (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_get_gesture_pinch_vector (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: out_addr
	// return: struct written to out_addr
	S8 out_addr ALIGN;
	Vector2 ret;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = GetGesturePinchVector ();
	pthread_mutex_unlock (&rlib_mutex);

	if (rlib_write_vec2 (data, out_addr, ret) != 0) return (NULL);

	return (sp);
}

U1 *raylib_get_glyph_atlas_rec (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: font_handle, codepoint, out_addr
	// return: struct written to out_addr
	S8 font_handle ALIGN;
	S8 codepoint ALIGN;
	S8 out_addr ALIGN;
	Rectangle ret;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &codepoint, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &font_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (font_handle < 0 || font_handle >= RAYLIB_MAX_FONTS || raylib_fonts_used[font_handle] == 0)
	{
		printf ("raylib_get_glyph_atlas_rec: ERROR invalid font handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	ret = GetGlyphAtlasRec (raylib_fonts[font_handle], (int) codepoint);
	pthread_mutex_unlock (&rlib_mutex);

	if (rlib_write_rect (data, out_addr, ret) != 0) return (NULL);

	return (sp);
}

U1 *raylib_get_glyph_index (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: font_handle, codepoint
	// return: integer result
	S8 font_handle ALIGN;
	S8 codepoint ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &codepoint, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &font_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (font_handle < 0 || font_handle >= RAYLIB_MAX_FONTS || raylib_fonts_used[font_handle] == 0)
	{
		printf ("raylib_get_glyph_index: ERROR invalid font handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	ret = GetGlyphIndex (raylib_fonts[font_handle], (int) codepoint);
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_get_image_alpha_border (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: image_handle, threshold, out_addr
	// return: struct written to out_addr
	S8 image_handle ALIGN;
	F8 threshold ALIGN;
	S8 out_addr ALIGN;
	Rectangle ret;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &threshold, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &image_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (image_handle < 0 || image_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[image_handle] == 0)
	{
		printf ("raylib_get_image_alpha_border: ERROR invalid image handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	ret = GetImageAlphaBorder (raylib_images[image_handle], (float) threshold);
	pthread_mutex_unlock (&rlib_mutex);

	if (rlib_write_rect (data, out_addr, ret) != 0) return (NULL);

	return (sp);
}

U1 *raylib_get_image_color (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: image_handle, x, y, out_addr
	// return: struct written to out_addr
	S8 image_handle ALIGN;
	S8 x ALIGN;
	S8 y ALIGN;
	S8 out_addr ALIGN;
	Color ret;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &y, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &x, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &image_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (image_handle < 0 || image_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[image_handle] == 0)
	{
		printf ("raylib_get_image_color: ERROR invalid image handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	ret = GetImageColor (raylib_images[image_handle], (int) x, (int) y);
	pthread_mutex_unlock (&rlib_mutex);

	if (rlib_write_color (data, out_addr, ret) != 0) return (NULL);

	return (sp);
}

U1 *raylib_get_master_volume (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// return: double value
	F8 ret ALIGN;

	pthread_mutex_lock (&rlib_mutex);
	ret = GetMasterVolume ();
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushd (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_get_monitor_name (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: monitor, out_addr
	// return: string written to out_addr
	S8 monitor ALIGN;
	S8 out_addr ALIGN;
	const char *text;
	S8 len ALIGN;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &monitor, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	text = GetMonitorName ((int) monitor);
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
		printf ("raylib_get_monitor_name: ERROR memory bounds check failed!\n");
		return (NULL);
	}
	if (len > 0) memcpy (&data[out_addr], text, len);
	data[out_addr + len] = '\0';

	return (sp);
}

U1 *raylib_get_monitor_position (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: monitor, out_addr
	// return: struct written to out_addr
	S8 monitor ALIGN;
	S8 out_addr ALIGN;
	Vector2 ret;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &monitor, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = GetMonitorPosition ((int) monitor);
	pthread_mutex_unlock (&rlib_mutex);

	if (rlib_write_vec2 (data, out_addr, ret) != 0) return (NULL);

	return (sp);
}

U1 *raylib_get_mouse_wheel_move_v (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: out_addr
	// return: struct written to out_addr
	S8 out_addr ALIGN;
	Vector2 ret;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = GetMouseWheelMoveV ();
	pthread_mutex_unlock (&rlib_mutex);

	if (rlib_write_vec2 (data, out_addr, ret) != 0) return (NULL);

	return (sp);
}

U1 *raylib_get_pixel_data_size (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: width, height, format
	// return: integer result
	S8 width ALIGN;
	S8 height ALIGN;
	S8 format ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &format, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &height, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &width, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = GetPixelDataSize ((int) width, (int) height, (int) format);
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_get_prev_directory_path (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: dir_path_addr, out_addr
	// return: string written to out_addr
	S8 dir_path_addr ALIGN;
	U1 dir_path_buf[512];
	S8 out_addr ALIGN;
	const char *text;
	S8 len ALIGN;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &dir_path_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_load_filename (data, dir_path_addr, dir_path_buf, sizeof (dir_path_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	text = GetPrevDirectoryPath ((const char *) dir_path_buf);
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
		printf ("raylib_get_prev_directory_path: ERROR memory bounds check failed!\n");
		return (NULL);
	}
	if (len > 0) memcpy (&data[out_addr], text, len);
	data[out_addr + len] = '\0';

	return (sp);
}

U1 *raylib_get_ray_collision_mesh (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: ray_addr, mesh_handle, transform_addr, out_addr
	// return: struct written to out_addr
	Ray ray;
	S8 ray_addr ALIGN;
	S8 mesh_handle ALIGN;
	Matrix transform;
	S8 transform_addr ALIGN;
	S8 out_addr ALIGN;
	RayCollision ret;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &transform_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &mesh_handle, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &ray_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_ray (data, ray_addr, &ray) != 0) return (NULL);

	if (mesh_handle < 0 || mesh_handle >= RAYLIB_MAX_MESHES || raylib_meshes_used[mesh_handle] == 0)
	{
		printf ("raylib_get_ray_collision_mesh: ERROR invalid mesh handle!\n");
		return (NULL);
	}

	if (rlib_read_matrix (data, transform_addr, &transform) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = GetRayCollisionMesh (ray, raylib_meshes[mesh_handle], transform);
	pthread_mutex_unlock (&rlib_mutex);

	if (rlib_write_raycollision (data, out_addr, ret) != 0) return (NULL);

	return (sp);
}

U1 *raylib_get_screen_to_world_2d (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: position_addr, camera_addr, out_addr
	// return: struct written to out_addr
	Vector2 position;
	S8 position_addr ALIGN;
	Camera2D camera;
	S8 camera_addr ALIGN;
	S8 out_addr ALIGN;
	Vector2 ret;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &camera_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &position_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec2 (data, position_addr, &position) != 0) return (NULL);
	if (rlib_read_camera2d (data, camera_addr, &camera) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = GetScreenToWorld2D (position, camera);
	pthread_mutex_unlock (&rlib_mutex);

	if (rlib_write_vec2 (data, out_addr, ret) != 0) return (NULL);

	return (sp);
}

U1 *raylib_get_screen_to_world_ray_ex (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: position_addr, camera_addr, width, height, out_addr
	// return: struct written to out_addr
	Vector2 position;
	S8 position_addr ALIGN;
	Camera camera;
	S8 camera_addr ALIGN;
	S8 width ALIGN;
	S8 height ALIGN;
	S8 out_addr ALIGN;
	Ray ret;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &height, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &width, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &camera_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &position_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec2 (data, position_addr, &position) != 0) return (NULL);
	if (rlib_read_camera (data, camera_addr, &camera) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = GetScreenToWorldRayEx (position, camera, (int) width, (int) height);
	pthread_mutex_unlock (&rlib_mutex);

	if (rlib_write_ray (data, out_addr, ret) != 0) return (NULL);

	return (sp);
}

U1 *raylib_get_shader_location_attrib (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: shader_handle, attrib_name_addr
	// return: integer result
	S8 shader_handle ALIGN;
	S8 attrib_name_addr ALIGN;
	U1 attrib_name_buf[512];
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &attrib_name_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &shader_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (shader_handle < 0 || shader_handle >= RAYLIB_MAX_SHADERS || raylib_shaders_used[shader_handle] == 0)
	{
		printf ("raylib_get_shader_location_attrib: ERROR invalid shader handle!\n");
		return (NULL);
	}

	if (rlib_read_string (data, attrib_name_addr, attrib_name_buf, sizeof (attrib_name_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = GetShaderLocationAttrib (raylib_shaders[shader_handle], (const char *) attrib_name_buf);
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_get_shapes_texture_rectangle (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: out_addr
	// return: struct written to out_addr
	S8 out_addr ALIGN;
	Rectangle ret;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = GetShapesTextureRectangle ();
	pthread_mutex_unlock (&rlib_mutex);

	if (rlib_write_rect (data, out_addr, ret) != 0) return (NULL);

	return (sp);
}

U1 *raylib_get_spline_point_basis (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: p1_addr, p2_addr, p3_addr, p4_addr, t, out_addr
	// return: struct written to out_addr
	Vector2 p1;
	S8 p1_addr ALIGN;
	Vector2 p2;
	S8 p2_addr ALIGN;
	Vector2 p3;
	S8 p3_addr ALIGN;
	Vector2 p4;
	S8 p4_addr ALIGN;
	F8 t ALIGN;
	S8 out_addr ALIGN;
	Vector2 ret;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &t, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &p4_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &p3_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &p2_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &p1_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec2 (data, p1_addr, &p1) != 0) return (NULL);
	if (rlib_read_vec2 (data, p2_addr, &p2) != 0) return (NULL);
	if (rlib_read_vec2 (data, p3_addr, &p3) != 0) return (NULL);
	if (rlib_read_vec2 (data, p4_addr, &p4) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = GetSplinePointBasis (p1, p2, p3, p4, (float) t);
	pthread_mutex_unlock (&rlib_mutex);

	if (rlib_write_vec2 (data, out_addr, ret) != 0) return (NULL);

	return (sp);
}

U1 *raylib_get_spline_point_bezier_cubic (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: p1_addr, c2_addr, c3_addr, p4_addr, t, out_addr
	// return: struct written to out_addr
	Vector2 p1;
	S8 p1_addr ALIGN;
	Vector2 c2;
	S8 c2_addr ALIGN;
	Vector2 c3;
	S8 c3_addr ALIGN;
	Vector2 p4;
	S8 p4_addr ALIGN;
	F8 t ALIGN;
	S8 out_addr ALIGN;
	Vector2 ret;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &t, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &p4_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &c3_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &c2_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &p1_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec2 (data, p1_addr, &p1) != 0) return (NULL);
	if (rlib_read_vec2 (data, c2_addr, &c2) != 0) return (NULL);
	if (rlib_read_vec2 (data, c3_addr, &c3) != 0) return (NULL);
	if (rlib_read_vec2 (data, p4_addr, &p4) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = GetSplinePointBezierCubic (p1, c2, c3, p4, (float) t);
	pthread_mutex_unlock (&rlib_mutex);

	if (rlib_write_vec2 (data, out_addr, ret) != 0) return (NULL);

	return (sp);
}

U1 *raylib_get_spline_point_bezier_quadratic (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: p1_addr, c2_addr, p3_addr, t, out_addr
	// return: struct written to out_addr
	Vector2 p1;
	S8 p1_addr ALIGN;
	Vector2 c2;
	S8 c2_addr ALIGN;
	Vector2 p3;
	S8 p3_addr ALIGN;
	F8 t ALIGN;
	S8 out_addr ALIGN;
	Vector2 ret;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &t, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &p3_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &c2_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &p1_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec2 (data, p1_addr, &p1) != 0) return (NULL);
	if (rlib_read_vec2 (data, c2_addr, &c2) != 0) return (NULL);
	if (rlib_read_vec2 (data, p3_addr, &p3) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = GetSplinePointBezierQuadratic (p1, c2, p3, (float) t);
	pthread_mutex_unlock (&rlib_mutex);

	if (rlib_write_vec2 (data, out_addr, ret) != 0) return (NULL);

	return (sp);
}

U1 *raylib_get_spline_point_catmull_rom (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: p1_addr, p2_addr, p3_addr, p4_addr, t, out_addr
	// return: struct written to out_addr
	Vector2 p1;
	S8 p1_addr ALIGN;
	Vector2 p2;
	S8 p2_addr ALIGN;
	Vector2 p3;
	S8 p3_addr ALIGN;
	Vector2 p4;
	S8 p4_addr ALIGN;
	F8 t ALIGN;
	S8 out_addr ALIGN;
	Vector2 ret;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &t, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &p4_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &p3_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &p2_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &p1_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec2 (data, p1_addr, &p1) != 0) return (NULL);
	if (rlib_read_vec2 (data, p2_addr, &p2) != 0) return (NULL);
	if (rlib_read_vec2 (data, p3_addr, &p3) != 0) return (NULL);
	if (rlib_read_vec2 (data, p4_addr, &p4) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = GetSplinePointCatmullRom (p1, p2, p3, p4, (float) t);
	pthread_mutex_unlock (&rlib_mutex);

	if (rlib_write_vec2 (data, out_addr, ret) != 0) return (NULL);

	return (sp);
}

U1 *raylib_get_spline_point_linear (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: start_pos_addr, end_pos_addr, t, out_addr
	// return: struct written to out_addr
	Vector2 start_pos;
	S8 start_pos_addr ALIGN;
	Vector2 end_pos;
	S8 end_pos_addr ALIGN;
	F8 t ALIGN;
	S8 out_addr ALIGN;
	Vector2 ret;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &t, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &end_pos_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &start_pos_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec2 (data, start_pos_addr, &start_pos) != 0) return (NULL);
	if (rlib_read_vec2 (data, end_pos_addr, &end_pos) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = GetSplinePointLinear (start_pos, end_pos, (float) t);
	pthread_mutex_unlock (&rlib_mutex);

	if (rlib_write_vec2 (data, out_addr, ret) != 0) return (NULL);

	return (sp);
}

U1 *raylib_get_text_between (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: text_addr, begin_addr, end_addr, out_addr
	// return: string written to out_addr
	S8 text_addr ALIGN;
	U1 text_buf[4096];
	S8 begin_addr ALIGN;
	U1 begin_buf[512];
	S8 end_addr ALIGN;
	U1 end_buf[512];
	S8 out_addr ALIGN;
	const char *text;
	S8 len ALIGN;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &end_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &begin_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &text_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_string (data, text_addr, text_buf, sizeof (text_buf)) != 0) return (NULL);
	if (rlib_read_string (data, begin_addr, begin_buf, sizeof (begin_buf)) != 0) return (NULL);
	if (rlib_read_string (data, end_addr, end_buf, sizeof (end_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	text = GetTextBetween ((const char *) text_buf, (const char *) begin_buf, (const char *) end_buf);
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
		printf ("raylib_get_text_between: ERROR memory bounds check failed!\n");
		return (NULL);
	}
	if (len > 0) memcpy (&data[out_addr], text, len);
	data[out_addr + len] = '\0';

	return (sp);
}

U1 *raylib_get_touch_point_id (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: index
	// return: integer result
	S8 index ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &index, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = GetTouchPointId ((int) index);
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_get_working_directory (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: out_addr
	// return: string written to out_addr
	S8 out_addr ALIGN;
	const char *text;
	S8 len ALIGN;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	text = GetWorkingDirectory ();
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
		printf ("raylib_get_working_directory: ERROR memory bounds check failed!\n");
		return (NULL);
	}
	if (len > 0) memcpy (&data[out_addr], text, len);
	data[out_addr + len] = '\0';

	return (sp);
}

U1 *raylib_get_world_to_screen_2d (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: position_addr, camera_addr, out_addr
	// return: struct written to out_addr
	Vector2 position;
	S8 position_addr ALIGN;
	Camera2D camera;
	S8 camera_addr ALIGN;
	S8 out_addr ALIGN;
	Vector2 ret;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &camera_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &position_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec2 (data, position_addr, &position) != 0) return (NULL);
	if (rlib_read_camera2d (data, camera_addr, &camera) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = GetWorldToScreen2D (position, camera);
	pthread_mutex_unlock (&rlib_mutex);

	if (rlib_write_vec2 (data, out_addr, ret) != 0) return (NULL);

	return (sp);
}

U1 *raylib_get_world_to_screen_ex (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: position_addr, camera_addr, width, height, out_addr
	// return: struct written to out_addr
	Vector3 position;
	S8 position_addr ALIGN;
	Camera camera;
	S8 camera_addr ALIGN;
	S8 width ALIGN;
	S8 height ALIGN;
	S8 out_addr ALIGN;
	Vector2 ret;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &height, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &width, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &camera_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &position_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vec3 (data, position_addr, &position) != 0) return (NULL);
	if (rlib_read_camera (data, camera_addr, &camera) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = GetWorldToScreenEx (position, camera, (int) width, (int) height);
	pthread_mutex_unlock (&rlib_mutex);

	if (rlib_write_vec2 (data, out_addr, ret) != 0) return (NULL);

	return (sp);
}

U1 *raylib_hide_cursor (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{

	pthread_mutex_lock (&rlib_mutex);
	HideCursor ();
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_alpha_clear (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: image_handle, color_addr, threshold
	S8 image_handle ALIGN;
	Color color;
	S8 color_addr ALIGN;
	F8 threshold ALIGN;

	sp = stpopd ((U1 *) &threshold, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &image_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (image_handle < 0 || image_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[image_handle] == 0)
	{
		printf ("raylib_image_alpha_clear: ERROR invalid image handle!\n");
		return (NULL);
	}

	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ImageAlphaClear (&raylib_images[image_handle], color, (float) threshold);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_alpha_crop (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: image_handle, threshold
	S8 image_handle ALIGN;
	F8 threshold ALIGN;

	sp = stpopd ((U1 *) &threshold, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &image_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (image_handle < 0 || image_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[image_handle] == 0)
	{
		printf ("raylib_image_alpha_crop: ERROR invalid image handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	ImageAlphaCrop (&raylib_images[image_handle], (float) threshold);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_alpha_mask (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: image_handle, alpha_mask_handle
	S8 image_handle ALIGN;
	S8 alpha_mask_handle ALIGN;

	sp = stpopi ((U1 *) &alpha_mask_handle, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &image_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (image_handle < 0 || image_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[image_handle] == 0)
	{
		printf ("raylib_image_alpha_mask: ERROR invalid image handle!\n");
		return (NULL);
	}
	if (alpha_mask_handle < 0 || alpha_mask_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[alpha_mask_handle] == 0)
	{
		printf ("raylib_image_alpha_mask: ERROR invalid image handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	ImageAlphaMask (&raylib_images[image_handle], raylib_images[alpha_mask_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_alpha_premultiply (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: image_handle
	S8 image_handle ALIGN;

	sp = stpopi ((U1 *) &image_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (image_handle < 0 || image_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[image_handle] == 0)
	{
		printf ("raylib_image_alpha_premultiply: ERROR invalid image handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	ImageAlphaPremultiply (&raylib_images[image_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_blur_gaussian (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: image_handle, blur_size
	S8 image_handle ALIGN;
	S8 blur_size ALIGN;

	sp = stpopi ((U1 *) &blur_size, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &image_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (image_handle < 0 || image_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[image_handle] == 0)
	{
		printf ("raylib_image_blur_gaussian: ERROR invalid image handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	ImageBlurGaussian (&raylib_images[image_handle], (int) blur_size);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_clear_background (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: dst_handle, color_addr
	S8 dst_handle ALIGN;
	Color color;
	S8 color_addr ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &dst_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (dst_handle < 0 || dst_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[dst_handle] == 0)
	{
		printf ("raylib_image_clear_background: ERROR invalid image handle!\n");
		return (NULL);
	}

	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ImageClearBackground (&raylib_images[dst_handle], color);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_color_brightness (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: image_handle, brightness
	S8 image_handle ALIGN;
	S8 brightness ALIGN;

	sp = stpopi ((U1 *) &brightness, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &image_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (image_handle < 0 || image_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[image_handle] == 0)
	{
		printf ("raylib_image_color_brightness: ERROR invalid image handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	ImageColorBrightness (&raylib_images[image_handle], (int) brightness);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_color_contrast (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: image_handle, contrast
	S8 image_handle ALIGN;
	S8 contrast ALIGN;

	sp = stpopi ((U1 *) &contrast, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &image_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (image_handle < 0 || image_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[image_handle] == 0)
	{
		printf ("raylib_image_color_contrast: ERROR invalid image handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	ImageColorContrast (&raylib_images[image_handle], (int) contrast);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_color_grayscale (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: image_handle
	S8 image_handle ALIGN;

	sp = stpopi ((U1 *) &image_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (image_handle < 0 || image_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[image_handle] == 0)
	{
		printf ("raylib_image_color_grayscale: ERROR invalid image handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	ImageColorGrayscale (&raylib_images[image_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_color_invert (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: image_handle
	S8 image_handle ALIGN;

	sp = stpopi ((U1 *) &image_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (image_handle < 0 || image_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[image_handle] == 0)
	{
		printf ("raylib_image_color_invert: ERROR invalid image handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	ImageColorInvert (&raylib_images[image_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_color_replace (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: image_handle, color_addr, replace_addr
	S8 image_handle ALIGN;
	Color color;
	S8 color_addr ALIGN;
	Color replace;
	S8 replace_addr ALIGN;

	sp = stpopi ((U1 *) &replace_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &image_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (image_handle < 0 || image_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[image_handle] == 0)
	{
		printf ("raylib_image_color_replace: ERROR invalid image handle!\n");
		return (NULL);
	}

	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);
	if (rlib_read_color (data, replace_addr, &replace) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ImageColorReplace (&raylib_images[image_handle], color, replace);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_color_tint (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: image_handle, color_addr
	S8 image_handle ALIGN;
	Color color;
	S8 color_addr ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &image_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (image_handle < 0 || image_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[image_handle] == 0)
	{
		printf ("raylib_image_color_tint: ERROR invalid image handle!\n");
		return (NULL);
	}

	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ImageColorTint (&raylib_images[image_handle], color);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_copy (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: image_handle
	// return: image handle or -1 on error
	S8 image_handle ALIGN;
	S8 ind ALIGN;

	sp = stpopi ((U1 *) &image_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (image_handle < 0 || image_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[image_handle] == 0)
	{
		printf ("raylib_image_copy: ERROR invalid image handle!\n");
		return (NULL);
	}

	ind = rlib_find_slot (raylib_images_used, RAYLIB_MAX_IMAGES);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_images[ind] = ImageCopy (raylib_images[image_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	if (raylib_images[ind].data == NULL)
	{
		printf ("raylib_image_copy: ERROR operation failed!\n");
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	raylib_images_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_image_crop (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: image_handle, crop_addr
	S8 image_handle ALIGN;
	Rectangle crop;
	S8 crop_addr ALIGN;

	sp = stpopi ((U1 *) &crop_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &image_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (image_handle < 0 || image_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[image_handle] == 0)
	{
		printf ("raylib_image_crop: ERROR invalid image handle!\n");
		return (NULL);
	}

	if (rlib_read_rect (data, crop_addr, &crop) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ImageCrop (&raylib_images[image_handle], crop);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_dither (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: image_handle, r_bpp, g_bpp, b_bpp, a_bpp
	S8 image_handle ALIGN;
	S8 r_bpp ALIGN;
	S8 g_bpp ALIGN;
	S8 b_bpp ALIGN;
	S8 a_bpp ALIGN;

	sp = stpopi ((U1 *) &a_bpp, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &b_bpp, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &g_bpp, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &r_bpp, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &image_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (image_handle < 0 || image_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[image_handle] == 0)
	{
		printf ("raylib_image_dither: ERROR invalid image handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	ImageDither (&raylib_images[image_handle], (int) r_bpp, (int) g_bpp, (int) b_bpp, (int) a_bpp);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_draw_circle (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: dst_handle, center_x, center_y, radius, color_addr
	S8 dst_handle ALIGN;
	S8 center_x ALIGN;
	S8 center_y ALIGN;
	S8 radius ALIGN;
	Color color;
	S8 color_addr ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &radius, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &center_y, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &center_x, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &dst_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (dst_handle < 0 || dst_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[dst_handle] == 0)
	{
		printf ("raylib_image_draw_circle: ERROR invalid image handle!\n");
		return (NULL);
	}

	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ImageDrawCircle (&raylib_images[dst_handle], (int) center_x, (int) center_y, (int) radius, color);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_draw_circle_gradient (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: dst_handle, center_addr, radius, inner_addr, outer_addr
	S8 dst_handle ALIGN;
	Vector2 center;
	S8 center_addr ALIGN;
	F8 radius ALIGN;
	Color inner;
	S8 inner_addr ALIGN;
	Color outer;
	S8 outer_addr ALIGN;

	sp = stpopi ((U1 *) &outer_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &inner_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &radius, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &center_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &dst_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (dst_handle < 0 || dst_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[dst_handle] == 0)
	{
		printf ("raylib_image_draw_circle_gradient: ERROR invalid image handle!\n");
		return (NULL);
	}

	if (rlib_read_vec2 (data, center_addr, &center) != 0) return (NULL);
	if (rlib_read_color (data, inner_addr, &inner) != 0) return (NULL);
	if (rlib_read_color (data, outer_addr, &outer) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ImageDrawCircleGradient (&raylib_images[dst_handle], center, (float) radius, inner, outer);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_draw_circle_lines (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: dst_handle, center_x, center_y, radius, color_addr
	S8 dst_handle ALIGN;
	S8 center_x ALIGN;
	S8 center_y ALIGN;
	S8 radius ALIGN;
	Color color;
	S8 color_addr ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &radius, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &center_y, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &center_x, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &dst_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (dst_handle < 0 || dst_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[dst_handle] == 0)
	{
		printf ("raylib_image_draw_circle_lines: ERROR invalid image handle!\n");
		return (NULL);
	}

	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ImageDrawCircleLines (&raylib_images[dst_handle], (int) center_x, (int) center_y, (int) radius, color);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_draw_circle_lines_v (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: dst_handle, center_addr, radius, color_addr
	S8 dst_handle ALIGN;
	Vector2 center;
	S8 center_addr ALIGN;
	S8 radius ALIGN;
	Color color;
	S8 color_addr ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &radius, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &center_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &dst_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (dst_handle < 0 || dst_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[dst_handle] == 0)
	{
		printf ("raylib_image_draw_circle_lines_v: ERROR invalid image handle!\n");
		return (NULL);
	}

	if (rlib_read_vec2 (data, center_addr, &center) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ImageDrawCircleLinesV (&raylib_images[dst_handle], center, (int) radius, color);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_draw_circle_v (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: dst_handle, center_addr, radius, color_addr
	S8 dst_handle ALIGN;
	Vector2 center;
	S8 center_addr ALIGN;
	S8 radius ALIGN;
	Color color;
	S8 color_addr ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &radius, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &center_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &dst_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (dst_handle < 0 || dst_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[dst_handle] == 0)
	{
		printf ("raylib_image_draw_circle_v: ERROR invalid image handle!\n");
		return (NULL);
	}

	if (rlib_read_vec2 (data, center_addr, &center) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ImageDrawCircleV (&raylib_images[dst_handle], center, (int) radius, color);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_draw_image (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: dst_handle, src_handle, pos_x, pos_y, tint_addr
	S8 dst_handle ALIGN;
	S8 src_handle ALIGN;
	S8 pos_x ALIGN;
	S8 pos_y ALIGN;
	Color tint;
	S8 tint_addr ALIGN;

	sp = stpopi ((U1 *) &tint_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &pos_y, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &pos_x, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &src_handle, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &dst_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (dst_handle < 0 || dst_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[dst_handle] == 0)
	{
		printf ("raylib_image_draw_image: ERROR invalid image handle!\n");
		return (NULL);
	}
	if (src_handle < 0 || src_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[src_handle] == 0)
	{
		printf ("raylib_image_draw_image: ERROR invalid image handle!\n");
		return (NULL);
	}

	if (rlib_read_color (data, tint_addr, &tint) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ImageDrawImage (&raylib_images[dst_handle], raylib_images[src_handle], (int) pos_x, (int) pos_y, tint);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_draw_image_ex (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: dst_handle, src_handle, position_addr, rotation, scale, tint_addr
	S8 dst_handle ALIGN;
	S8 src_handle ALIGN;
	Vector2 position;
	S8 position_addr ALIGN;
	F8 rotation ALIGN;
	F8 scale ALIGN;
	Color tint;
	S8 tint_addr ALIGN;

	sp = stpopi ((U1 *) &tint_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &scale, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &rotation, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &position_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &src_handle, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &dst_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (dst_handle < 0 || dst_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[dst_handle] == 0)
	{
		printf ("raylib_image_draw_image_ex: ERROR invalid image handle!\n");
		return (NULL);
	}
	if (src_handle < 0 || src_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[src_handle] == 0)
	{
		printf ("raylib_image_draw_image_ex: ERROR invalid image handle!\n");
		return (NULL);
	}

	if (rlib_read_vec2 (data, position_addr, &position) != 0) return (NULL);
	if (rlib_read_color (data, tint_addr, &tint) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ImageDrawImageEx (&raylib_images[dst_handle], raylib_images[src_handle], position, (float) rotation, (float) scale, tint);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_draw_image_pro (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: dst_handle, src_handle, src_rec_addr, dst_rec_addr, origin_addr, rotation, tint_addr
	S8 dst_handle ALIGN;
	S8 src_handle ALIGN;
	Rectangle src_rec;
	S8 src_rec_addr ALIGN;
	Rectangle dst_rec;
	S8 dst_rec_addr ALIGN;
	Vector2 origin;
	S8 origin_addr ALIGN;
	F8 rotation ALIGN;
	Color tint;
	S8 tint_addr ALIGN;

	sp = stpopi ((U1 *) &tint_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &rotation, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &origin_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &dst_rec_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &src_rec_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &src_handle, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &dst_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (dst_handle < 0 || dst_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[dst_handle] == 0)
	{
		printf ("raylib_image_draw_image_pro: ERROR invalid image handle!\n");
		return (NULL);
	}
	if (src_handle < 0 || src_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[src_handle] == 0)
	{
		printf ("raylib_image_draw_image_pro: ERROR invalid image handle!\n");
		return (NULL);
	}

	if (rlib_read_rect (data, src_rec_addr, &src_rec) != 0) return (NULL);
	if (rlib_read_rect (data, dst_rec_addr, &dst_rec) != 0) return (NULL);
	if (rlib_read_vec2 (data, origin_addr, &origin) != 0) return (NULL);
	if (rlib_read_color (data, tint_addr, &tint) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ImageDrawImagePro (&raylib_images[dst_handle], raylib_images[src_handle], src_rec, dst_rec, origin, (float) rotation, tint);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_draw_image_rec (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: dst_handle, src_handle, src_rec_addr, position_addr, tint_addr
	S8 dst_handle ALIGN;
	S8 src_handle ALIGN;
	Rectangle src_rec;
	S8 src_rec_addr ALIGN;
	Vector2 position;
	S8 position_addr ALIGN;
	Color tint;
	S8 tint_addr ALIGN;

	sp = stpopi ((U1 *) &tint_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &position_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &src_rec_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &src_handle, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &dst_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (dst_handle < 0 || dst_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[dst_handle] == 0)
	{
		printf ("raylib_image_draw_image_rec: ERROR invalid image handle!\n");
		return (NULL);
	}
	if (src_handle < 0 || src_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[src_handle] == 0)
	{
		printf ("raylib_image_draw_image_rec: ERROR invalid image handle!\n");
		return (NULL);
	}

	if (rlib_read_rect (data, src_rec_addr, &src_rec) != 0) return (NULL);
	if (rlib_read_vec2 (data, position_addr, &position) != 0) return (NULL);
	if (rlib_read_color (data, tint_addr, &tint) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ImageDrawImageRec (&raylib_images[dst_handle], raylib_images[src_handle], src_rec, position, tint);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_draw_line (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: dst_handle, start_pos_x, start_pos_y, end_pos_x, end_pos_y, color_addr
	S8 dst_handle ALIGN;
	S8 start_pos_x ALIGN;
	S8 start_pos_y ALIGN;
	S8 end_pos_x ALIGN;
	S8 end_pos_y ALIGN;
	Color color;
	S8 color_addr ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &end_pos_y, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &end_pos_x, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &start_pos_y, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &start_pos_x, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &dst_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (dst_handle < 0 || dst_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[dst_handle] == 0)
	{
		printf ("raylib_image_draw_line: ERROR invalid image handle!\n");
		return (NULL);
	}

	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ImageDrawLine (&raylib_images[dst_handle], (int) start_pos_x, (int) start_pos_y, (int) end_pos_x, (int) end_pos_y, color);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_draw_line_ex (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: dst_handle, start_addr, end_addr, thick, color_addr
	S8 dst_handle ALIGN;
	Vector2 start;
	S8 start_addr ALIGN;
	Vector2 end;
	S8 end_addr ALIGN;
	S8 thick ALIGN;
	Color color;
	S8 color_addr ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &thick, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &end_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &start_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &dst_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (dst_handle < 0 || dst_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[dst_handle] == 0)
	{
		printf ("raylib_image_draw_line_ex: ERROR invalid image handle!\n");
		return (NULL);
	}

	if (rlib_read_vec2 (data, start_addr, &start) != 0) return (NULL);
	if (rlib_read_vec2 (data, end_addr, &end) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ImageDrawLineEx (&raylib_images[dst_handle], start, end, (int) thick, color);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_draw_line_strip (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: dst_handle, points_addr, point_count, color_addr
	S8 dst_handle ALIGN;
	S8 points_addr ALIGN;
	Vector2 *points_ptr = NULL;
	S8 point_count ALIGN;
	Color color;
	S8 color_addr ALIGN;
	S8 i ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &point_count, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &points_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &dst_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (dst_handle < 0 || dst_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[dst_handle] == 0)
	{
		printf ("raylib_image_draw_line_strip: ERROR invalid image handle!\n");
		return (NULL);
	}

	if (point_count > 0)
	{
		if (memory_bounds (points_addr, (point_count - 1) * 16) != 0)
		{
			printf ("raylib_image_draw_line_strip: ERROR memory bounds check failed!\n");
			return (NULL);
		}
		points_ptr = calloc (point_count, sizeof (Vector2));
		if (points_ptr == NULL)
		{
			printf ("raylib_image_draw_line_strip: ERROR out of memory!\n");
			return (NULL);
		}
		for (i = 0; i < point_count; i++)
		{
			if (rlib_read_vec2 (data, points_addr + (i * 16), &points_ptr[i]) != 0)
			{
				free (points_ptr);
				return (NULL);
			}
		}
	}
	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ImageDrawLineStrip (&raylib_images[dst_handle], (const Vector2 *) points_ptr, (int) point_count, color);
	pthread_mutex_unlock (&rlib_mutex);

	free (points_ptr);

	return (sp);
}

U1 *raylib_image_draw_line_v (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: dst_handle, start_addr, end_addr, color_addr
	S8 dst_handle ALIGN;
	Vector2 start;
	S8 start_addr ALIGN;
	Vector2 end;
	S8 end_addr ALIGN;
	Color color;
	S8 color_addr ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &end_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &start_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &dst_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (dst_handle < 0 || dst_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[dst_handle] == 0)
	{
		printf ("raylib_image_draw_line_v: ERROR invalid image handle!\n");
		return (NULL);
	}

	if (rlib_read_vec2 (data, start_addr, &start) != 0) return (NULL);
	if (rlib_read_vec2 (data, end_addr, &end) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ImageDrawLineV (&raylib_images[dst_handle], start, end, color);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_draw_pixel (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: dst_handle, pos_x, pos_y, color_addr
	S8 dst_handle ALIGN;
	S8 pos_x ALIGN;
	S8 pos_y ALIGN;
	Color color;
	S8 color_addr ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &pos_y, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &pos_x, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &dst_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (dst_handle < 0 || dst_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[dst_handle] == 0)
	{
		printf ("raylib_image_draw_pixel: ERROR invalid image handle!\n");
		return (NULL);
	}

	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ImageDrawPixel (&raylib_images[dst_handle], (int) pos_x, (int) pos_y, color);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_draw_pixel_v (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: dst_handle, position_addr, color_addr
	S8 dst_handle ALIGN;
	Vector2 position;
	S8 position_addr ALIGN;
	Color color;
	S8 color_addr ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &position_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &dst_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (dst_handle < 0 || dst_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[dst_handle] == 0)
	{
		printf ("raylib_image_draw_pixel_v: ERROR invalid image handle!\n");
		return (NULL);
	}

	if (rlib_read_vec2 (data, position_addr, &position) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ImageDrawPixelV (&raylib_images[dst_handle], position, color);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_draw_rectangle (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: dst_handle, pos_x, pos_y, width, height, color_addr
	S8 dst_handle ALIGN;
	S8 pos_x ALIGN;
	S8 pos_y ALIGN;
	S8 width ALIGN;
	S8 height ALIGN;
	Color color;
	S8 color_addr ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &height, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &width, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &pos_y, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &pos_x, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &dst_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (dst_handle < 0 || dst_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[dst_handle] == 0)
	{
		printf ("raylib_image_draw_rectangle: ERROR invalid image handle!\n");
		return (NULL);
	}

	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ImageDrawRectangle (&raylib_images[dst_handle], (int) pos_x, (int) pos_y, (int) width, (int) height, color);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_draw_rectangle_gradient_ex (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: dst_handle, rec_addr, col1_addr, col2_addr, col3_addr, col4_addr
	S8 dst_handle ALIGN;
	Rectangle rec;
	S8 rec_addr ALIGN;
	Color col1;
	S8 col1_addr ALIGN;
	Color col2;
	S8 col2_addr ALIGN;
	Color col3;
	S8 col3_addr ALIGN;
	Color col4;
	S8 col4_addr ALIGN;

	sp = stpopi ((U1 *) &col4_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &col3_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &col2_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &col1_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &rec_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &dst_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (dst_handle < 0 || dst_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[dst_handle] == 0)
	{
		printf ("raylib_image_draw_rectangle_gradient_ex: ERROR invalid image handle!\n");
		return (NULL);
	}

	if (rlib_read_rect (data, rec_addr, &rec) != 0) return (NULL);
	if (rlib_read_color (data, col1_addr, &col1) != 0) return (NULL);
	if (rlib_read_color (data, col2_addr, &col2) != 0) return (NULL);
	if (rlib_read_color (data, col3_addr, &col3) != 0) return (NULL);
	if (rlib_read_color (data, col4_addr, &col4) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ImageDrawRectangleGradientEx (&raylib_images[dst_handle], rec, col1, col2, col3, col4);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_draw_rectangle_lines (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: dst_handle, pos_x, pos_y, width, height, color_addr
	S8 dst_handle ALIGN;
	S8 pos_x ALIGN;
	S8 pos_y ALIGN;
	S8 width ALIGN;
	S8 height ALIGN;
	Color color;
	S8 color_addr ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &height, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &width, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &pos_y, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &pos_x, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &dst_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (dst_handle < 0 || dst_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[dst_handle] == 0)
	{
		printf ("raylib_image_draw_rectangle_lines: ERROR invalid image handle!\n");
		return (NULL);
	}

	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ImageDrawRectangleLines (&raylib_images[dst_handle], (int) pos_x, (int) pos_y, (int) width, (int) height, color);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_draw_rectangle_lines_ex (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: dst_handle, rec_addr, thick, color_addr
	S8 dst_handle ALIGN;
	Rectangle rec;
	S8 rec_addr ALIGN;
	S8 thick ALIGN;
	Color color;
	S8 color_addr ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &thick, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &rec_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &dst_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (dst_handle < 0 || dst_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[dst_handle] == 0)
	{
		printf ("raylib_image_draw_rectangle_lines_ex: ERROR invalid image handle!\n");
		return (NULL);
	}

	if (rlib_read_rect (data, rec_addr, &rec) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ImageDrawRectangleLinesEx (&raylib_images[dst_handle], rec, (int) thick, color);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_draw_rectangle_pro (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: dst_handle, rec_addr, origin_addr, rotation, color_addr
	S8 dst_handle ALIGN;
	Rectangle rec;
	S8 rec_addr ALIGN;
	Vector2 origin;
	S8 origin_addr ALIGN;
	F8 rotation ALIGN;
	Color color;
	S8 color_addr ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &rotation, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &origin_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &rec_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &dst_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (dst_handle < 0 || dst_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[dst_handle] == 0)
	{
		printf ("raylib_image_draw_rectangle_pro: ERROR invalid image handle!\n");
		return (NULL);
	}

	if (rlib_read_rect (data, rec_addr, &rec) != 0) return (NULL);
	if (rlib_read_vec2 (data, origin_addr, &origin) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ImageDrawRectanglePro (&raylib_images[dst_handle], rec, origin, (float) rotation, color);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_draw_rectangle_rec (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: dst_handle, rec_addr, color_addr
	S8 dst_handle ALIGN;
	Rectangle rec;
	S8 rec_addr ALIGN;
	Color color;
	S8 color_addr ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &rec_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &dst_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (dst_handle < 0 || dst_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[dst_handle] == 0)
	{
		printf ("raylib_image_draw_rectangle_rec: ERROR invalid image handle!\n");
		return (NULL);
	}

	if (rlib_read_rect (data, rec_addr, &rec) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ImageDrawRectangleRec (&raylib_images[dst_handle], rec, color);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_draw_rectangle_v (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: dst_handle, position_addr, size_addr, color_addr
	S8 dst_handle ALIGN;
	Vector2 position;
	S8 position_addr ALIGN;
	Vector2 size;
	S8 size_addr ALIGN;
	Color color;
	S8 color_addr ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &size_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &position_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &dst_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (dst_handle < 0 || dst_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[dst_handle] == 0)
	{
		printf ("raylib_image_draw_rectangle_v: ERROR invalid image handle!\n");
		return (NULL);
	}

	if (rlib_read_vec2 (data, position_addr, &position) != 0) return (NULL);
	if (rlib_read_vec2 (data, size_addr, &size) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ImageDrawRectangleV (&raylib_images[dst_handle], position, size, color);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_draw_text (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: dst_handle, text_addr, pos_x, pos_y, font_size, color_addr
	S8 dst_handle ALIGN;
	S8 text_addr ALIGN;
	U1 text_buf[4096];
	S8 pos_x ALIGN;
	S8 pos_y ALIGN;
	S8 font_size ALIGN;
	Color color;
	S8 color_addr ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &font_size, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &pos_y, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &pos_x, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &text_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &dst_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (dst_handle < 0 || dst_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[dst_handle] == 0)
	{
		printf ("raylib_image_draw_text: ERROR invalid image handle!\n");
		return (NULL);
	}

	if (rlib_read_string (data, text_addr, text_buf, sizeof (text_buf)) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ImageDrawText (&raylib_images[dst_handle], (const char *) text_buf, (int) pos_x, (int) pos_y, (int) font_size, color);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_draw_text_ex (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: dst_handle, font_handle, text_addr, position_addr, font_size, spacing, tint_addr
	S8 dst_handle ALIGN;
	S8 font_handle ALIGN;
	S8 text_addr ALIGN;
	U1 text_buf[4096];
	Vector2 position;
	S8 position_addr ALIGN;
	F8 font_size ALIGN;
	F8 spacing ALIGN;
	Color tint;
	S8 tint_addr ALIGN;

	sp = stpopi ((U1 *) &tint_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &spacing, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &font_size, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &position_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &text_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &font_handle, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &dst_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (dst_handle < 0 || dst_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[dst_handle] == 0)
	{
		printf ("raylib_image_draw_text_ex: ERROR invalid image handle!\n");
		return (NULL);
	}
	if (font_handle < 0 || font_handle >= RAYLIB_MAX_FONTS || raylib_fonts_used[font_handle] == 0)
	{
		printf ("raylib_image_draw_text_ex: ERROR invalid font handle!\n");
		return (NULL);
	}

	if (rlib_read_string (data, text_addr, text_buf, sizeof (text_buf)) != 0) return (NULL);
	if (rlib_read_vec2 (data, position_addr, &position) != 0) return (NULL);
	if (rlib_read_color (data, tint_addr, &tint) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ImageDrawTextEx (&raylib_images[dst_handle], raylib_fonts[font_handle], (const char *) text_buf, position, (float) font_size, (float) spacing, tint);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_draw_text_pro (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: dst_handle, font_handle, text_addr, position_addr, origin_addr, rotation, font_size, spacing, tint_addr
	S8 dst_handle ALIGN;
	S8 font_handle ALIGN;
	S8 text_addr ALIGN;
	U1 text_buf[4096];
	Vector2 position;
	S8 position_addr ALIGN;
	Vector2 origin;
	S8 origin_addr ALIGN;
	F8 rotation ALIGN;
	F8 font_size ALIGN;
	F8 spacing ALIGN;
	Color tint;
	S8 tint_addr ALIGN;

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
	sp = stpopi ((U1 *) &position_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &text_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &font_handle, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &dst_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (dst_handle < 0 || dst_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[dst_handle] == 0)
	{
		printf ("raylib_image_draw_text_pro: ERROR invalid image handle!\n");
		return (NULL);
	}
	if (font_handle < 0 || font_handle >= RAYLIB_MAX_FONTS || raylib_fonts_used[font_handle] == 0)
	{
		printf ("raylib_image_draw_text_pro: ERROR invalid font handle!\n");
		return (NULL);
	}

	if (rlib_read_string (data, text_addr, text_buf, sizeof (text_buf)) != 0) return (NULL);
	if (rlib_read_vec2 (data, position_addr, &position) != 0) return (NULL);
	if (rlib_read_vec2 (data, origin_addr, &origin) != 0) return (NULL);
	if (rlib_read_color (data, tint_addr, &tint) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ImageDrawTextPro (&raylib_images[dst_handle], raylib_fonts[font_handle], (const char *) text_buf, position, origin, (float) rotation, (float) font_size, (float) spacing, tint);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_draw_triangle (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: dst_handle, v1_addr, v2_addr, v3_addr, color_addr
	S8 dst_handle ALIGN;
	Vector2 v1;
	S8 v1_addr ALIGN;
	Vector2 v2;
	S8 v2_addr ALIGN;
	Vector2 v3;
	S8 v3_addr ALIGN;
	Color color;
	S8 color_addr ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &v3_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &v2_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &v1_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &dst_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (dst_handle < 0 || dst_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[dst_handle] == 0)
	{
		printf ("raylib_image_draw_triangle: ERROR invalid image handle!\n");
		return (NULL);
	}

	if (rlib_read_vec2 (data, v1_addr, &v1) != 0) return (NULL);
	if (rlib_read_vec2 (data, v2_addr, &v2) != 0) return (NULL);
	if (rlib_read_vec2 (data, v3_addr, &v3) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ImageDrawTriangle (&raylib_images[dst_handle], v1, v2, v3, color);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_draw_triangle_fan (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: dst_handle, points_addr, point_count, color_addr
	S8 dst_handle ALIGN;
	S8 points_addr ALIGN;
	Vector2 *points_ptr = NULL;
	S8 point_count ALIGN;
	Color color;
	S8 color_addr ALIGN;
	S8 i ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &point_count, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &points_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &dst_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (dst_handle < 0 || dst_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[dst_handle] == 0)
	{
		printf ("raylib_image_draw_triangle_fan: ERROR invalid image handle!\n");
		return (NULL);
	}

	if (point_count > 0)
	{
		if (memory_bounds (points_addr, (point_count - 1) * 16) != 0)
		{
			printf ("raylib_image_draw_triangle_fan: ERROR memory bounds check failed!\n");
			return (NULL);
		}
		points_ptr = calloc (point_count, sizeof (Vector2));
		if (points_ptr == NULL)
		{
			printf ("raylib_image_draw_triangle_fan: ERROR out of memory!\n");
			return (NULL);
		}
		for (i = 0; i < point_count; i++)
		{
			if (rlib_read_vec2 (data, points_addr + (i * 16), &points_ptr[i]) != 0)
			{
				free (points_ptr);
				return (NULL);
			}
		}
	}
	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ImageDrawTriangleFan (&raylib_images[dst_handle], (const Vector2 *) points_ptr, (int) point_count, color);
	pthread_mutex_unlock (&rlib_mutex);

	free (points_ptr);

	return (sp);
}

U1 *raylib_image_draw_triangle_gradient (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: dst_handle, v1_addr, v2_addr, v3_addr, c1_addr, c2_addr, c3_addr
	S8 dst_handle ALIGN;
	Vector2 v1;
	S8 v1_addr ALIGN;
	Vector2 v2;
	S8 v2_addr ALIGN;
	Vector2 v3;
	S8 v3_addr ALIGN;
	Color c1;
	S8 c1_addr ALIGN;
	Color c2;
	S8 c2_addr ALIGN;
	Color c3;
	S8 c3_addr ALIGN;

	sp = stpopi ((U1 *) &c3_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &c2_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &c1_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &v3_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &v2_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &v1_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &dst_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (dst_handle < 0 || dst_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[dst_handle] == 0)
	{
		printf ("raylib_image_draw_triangle_gradient: ERROR invalid image handle!\n");
		return (NULL);
	}

	if (rlib_read_vec2 (data, v1_addr, &v1) != 0) return (NULL);
	if (rlib_read_vec2 (data, v2_addr, &v2) != 0) return (NULL);
	if (rlib_read_vec2 (data, v3_addr, &v3) != 0) return (NULL);
	if (rlib_read_color (data, c1_addr, &c1) != 0) return (NULL);
	if (rlib_read_color (data, c2_addr, &c2) != 0) return (NULL);
	if (rlib_read_color (data, c3_addr, &c3) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ImageDrawTriangleGradient (&raylib_images[dst_handle], v1, v2, v3, c1, c2, c3);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_draw_triangle_lines (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: dst_handle, v1_addr, v2_addr, v3_addr, color_addr
	S8 dst_handle ALIGN;
	Vector2 v1;
	S8 v1_addr ALIGN;
	Vector2 v2;
	S8 v2_addr ALIGN;
	Vector2 v3;
	S8 v3_addr ALIGN;
	Color color;
	S8 color_addr ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &v3_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &v2_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &v1_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &dst_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (dst_handle < 0 || dst_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[dst_handle] == 0)
	{
		printf ("raylib_image_draw_triangle_lines: ERROR invalid image handle!\n");
		return (NULL);
	}

	if (rlib_read_vec2 (data, v1_addr, &v1) != 0) return (NULL);
	if (rlib_read_vec2 (data, v2_addr, &v2) != 0) return (NULL);
	if (rlib_read_vec2 (data, v3_addr, &v3) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ImageDrawTriangleLines (&raylib_images[dst_handle], v1, v2, v3, color);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_draw_triangle_strip (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: dst_handle, points_addr, point_count, color_addr
	S8 dst_handle ALIGN;
	S8 points_addr ALIGN;
	Vector2 *points_ptr = NULL;
	S8 point_count ALIGN;
	Color color;
	S8 color_addr ALIGN;
	S8 i ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &point_count, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &points_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &dst_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (dst_handle < 0 || dst_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[dst_handle] == 0)
	{
		printf ("raylib_image_draw_triangle_strip: ERROR invalid image handle!\n");
		return (NULL);
	}

	if (point_count > 0)
	{
		if (memory_bounds (points_addr, (point_count - 1) * 16) != 0)
		{
			printf ("raylib_image_draw_triangle_strip: ERROR memory bounds check failed!\n");
			return (NULL);
		}
		points_ptr = calloc (point_count, sizeof (Vector2));
		if (points_ptr == NULL)
		{
			printf ("raylib_image_draw_triangle_strip: ERROR out of memory!\n");
			return (NULL);
		}
		for (i = 0; i < point_count; i++)
		{
			if (rlib_read_vec2 (data, points_addr + (i * 16), &points_ptr[i]) != 0)
			{
				free (points_ptr);
				return (NULL);
			}
		}
	}
	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ImageDrawTriangleStrip (&raylib_images[dst_handle], (const Vector2 *) points_ptr, (int) point_count, color);
	pthread_mutex_unlock (&rlib_mutex);

	free (points_ptr);

	return (sp);
}

U1 *raylib_image_flip_horizontal (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: image_handle
	S8 image_handle ALIGN;

	sp = stpopi ((U1 *) &image_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (image_handle < 0 || image_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[image_handle] == 0)
	{
		printf ("raylib_image_flip_horizontal: ERROR invalid image handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	ImageFlipHorizontal (&raylib_images[image_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_flip_vertical (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: image_handle
	S8 image_handle ALIGN;

	sp = stpopi ((U1 *) &image_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (image_handle < 0 || image_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[image_handle] == 0)
	{
		printf ("raylib_image_flip_vertical: ERROR invalid image handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	ImageFlipVertical (&raylib_images[image_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_format (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: image_handle, new_format
	S8 image_handle ALIGN;
	S8 new_format ALIGN;

	sp = stpopi ((U1 *) &new_format, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &image_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (image_handle < 0 || image_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[image_handle] == 0)
	{
		printf ("raylib_image_format: ERROR invalid image handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	ImageFormat (&raylib_images[image_handle], (int) new_format);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_from_channel (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: image_handle, selected_channel
	// return: image handle or -1 on error
	S8 image_handle ALIGN;
	S8 selected_channel ALIGN;
	S8 ind ALIGN;

	sp = stpopi ((U1 *) &selected_channel, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &image_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (image_handle < 0 || image_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[image_handle] == 0)
	{
		printf ("raylib_image_from_channel: ERROR invalid image handle!\n");
		return (NULL);
	}

	ind = rlib_find_slot (raylib_images_used, RAYLIB_MAX_IMAGES);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_images[ind] = ImageFromChannel (raylib_images[image_handle], (int) selected_channel);
	pthread_mutex_unlock (&rlib_mutex);

	if (raylib_images[ind].data == NULL)
	{
		printf ("raylib_image_from_channel: ERROR operation failed!\n");
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	raylib_images_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_image_from_image (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: image_handle, rec_addr
	// return: image handle or -1 on error
	S8 image_handle ALIGN;
	Rectangle rec;
	S8 rec_addr ALIGN;
	S8 ind ALIGN;

	sp = stpopi ((U1 *) &rec_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &image_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (image_handle < 0 || image_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[image_handle] == 0)
	{
		printf ("raylib_image_from_image: ERROR invalid image handle!\n");
		return (NULL);
	}

	if (rlib_read_rect (data, rec_addr, &rec) != 0) return (NULL);

	ind = rlib_find_slot (raylib_images_used, RAYLIB_MAX_IMAGES);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_images[ind] = ImageFromImage (raylib_images[image_handle], rec);
	pthread_mutex_unlock (&rlib_mutex);

	if (raylib_images[ind].data == NULL)
	{
		printf ("raylib_image_from_image: ERROR operation failed!\n");
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	raylib_images_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_image_kernel_convolution (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: image_handle, kernel_addr, kernel_size
	S8 image_handle ALIGN;
	S8 kernel_addr ALIGN;
	float *kernel_ptr = NULL;
	S8 kernel_size ALIGN;
	S8 i ALIGN;
	F8 t ALIGN;

	sp = stpopi ((U1 *) &kernel_size, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &kernel_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &image_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (image_handle < 0 || image_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[image_handle] == 0)
	{
		printf ("raylib_image_kernel_convolution: ERROR invalid image handle!\n");
		return (NULL);
	}

	if (kernel_size > 0)
	{
		if (memory_bounds (kernel_addr, (kernel_size - 1) * 8) != 0)
		{
			printf ("raylib_image_kernel_convolution: ERROR memory bounds check failed!\n");
			return (NULL);
		}
		kernel_ptr = calloc (kernel_size, sizeof (float));
		if (kernel_ptr == NULL)
		{
			printf ("raylib_image_kernel_convolution: ERROR out of memory!\n");
			return (NULL);
		}
		for (i = 0; i < kernel_size; i++)
		{
			memcpy (&t, &data[kernel_addr + (i * 8)], sizeof (F8));
			kernel_ptr[i] = (float) t;
		}
	}

	pthread_mutex_lock (&rlib_mutex);
	ImageKernelConvolution (&raylib_images[image_handle], (const float *) kernel_ptr, (int) kernel_size);
	pthread_mutex_unlock (&rlib_mutex);

	free (kernel_ptr);

	return (sp);
}

U1 *raylib_image_mipmaps (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: image_handle
	S8 image_handle ALIGN;

	sp = stpopi ((U1 *) &image_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (image_handle < 0 || image_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[image_handle] == 0)
	{
		printf ("raylib_image_mipmaps: ERROR invalid image handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	ImageMipmaps (&raylib_images[image_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_resize (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: image_handle, new_width, new_height
	S8 image_handle ALIGN;
	S8 new_width ALIGN;
	S8 new_height ALIGN;

	sp = stpopi ((U1 *) &new_height, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &new_width, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &image_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (image_handle < 0 || image_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[image_handle] == 0)
	{
		printf ("raylib_image_resize: ERROR invalid image handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	ImageResize (&raylib_images[image_handle], (int) new_width, (int) new_height);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_resize_canvas (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: image_handle, new_width, new_height, offset_x, offset_y, fill_addr
	S8 image_handle ALIGN;
	S8 new_width ALIGN;
	S8 new_height ALIGN;
	S8 offset_x ALIGN;
	S8 offset_y ALIGN;
	Color fill;
	S8 fill_addr ALIGN;

	sp = stpopi ((U1 *) &fill_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &offset_y, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &offset_x, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &new_height, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &new_width, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &image_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (image_handle < 0 || image_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[image_handle] == 0)
	{
		printf ("raylib_image_resize_canvas: ERROR invalid image handle!\n");
		return (NULL);
	}

	if (rlib_read_color (data, fill_addr, &fill) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ImageResizeCanvas (&raylib_images[image_handle], (int) new_width, (int) new_height, (int) offset_x, (int) offset_y, fill);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_resize_nn (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: image_handle, new_width, new_height
	S8 image_handle ALIGN;
	S8 new_width ALIGN;
	S8 new_height ALIGN;

	sp = stpopi ((U1 *) &new_height, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &new_width, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &image_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (image_handle < 0 || image_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[image_handle] == 0)
	{
		printf ("raylib_image_resize_nn: ERROR invalid image handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	ImageResizeNN (&raylib_images[image_handle], (int) new_width, (int) new_height);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_rotate (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: image_handle, degrees
	S8 image_handle ALIGN;
	S8 degrees ALIGN;

	sp = stpopi ((U1 *) &degrees, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &image_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (image_handle < 0 || image_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[image_handle] == 0)
	{
		printf ("raylib_image_rotate: ERROR invalid image handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	ImageRotate (&raylib_images[image_handle], (int) degrees);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_rotate_ccw (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: image_handle
	S8 image_handle ALIGN;

	sp = stpopi ((U1 *) &image_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (image_handle < 0 || image_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[image_handle] == 0)
	{
		printf ("raylib_image_rotate_ccw: ERROR invalid image handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	ImageRotateCCW (&raylib_images[image_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_rotate_cw (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: image_handle
	S8 image_handle ALIGN;

	sp = stpopi ((U1 *) &image_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (image_handle < 0 || image_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[image_handle] == 0)
	{
		printf ("raylib_image_rotate_cw: ERROR invalid image handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	ImageRotateCW (&raylib_images[image_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_image_text (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: text_addr, font_size, color_addr
	// return: image handle or -1 on error
	S8 text_addr ALIGN;
	U1 text_buf[4096];
	S8 font_size ALIGN;
	Color color;
	S8 color_addr ALIGN;
	S8 ind ALIGN;

	sp = stpopi ((U1 *) &color_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &font_size, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &text_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_string (data, text_addr, text_buf, sizeof (text_buf)) != 0) return (NULL);
	if (rlib_read_color (data, color_addr, &color) != 0) return (NULL);

	ind = rlib_find_slot (raylib_images_used, RAYLIB_MAX_IMAGES);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_images[ind] = ImageText ((const char *) text_buf, (int) font_size, color);
	pthread_mutex_unlock (&rlib_mutex);

	if (raylib_images[ind].data == NULL)
	{
		printf ("raylib_image_text: ERROR operation failed!\n");
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	raylib_images_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_image_text_ex (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: font_handle, text_addr, font_size, spacing, tint_addr
	// return: image handle or -1 on error
	S8 font_handle ALIGN;
	S8 text_addr ALIGN;
	U1 text_buf[4096];
	F8 font_size ALIGN;
	F8 spacing ALIGN;
	Color tint;
	S8 tint_addr ALIGN;
	S8 ind ALIGN;

	sp = stpopi ((U1 *) &tint_addr, sp, sp_top);
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
		printf ("raylib_image_text_ex: ERROR invalid font handle!\n");
		return (NULL);
	}

	if (rlib_read_string (data, text_addr, text_buf, sizeof (text_buf)) != 0) return (NULL);
	if (rlib_read_color (data, tint_addr, &tint) != 0) return (NULL);

	ind = rlib_find_slot (raylib_images_used, RAYLIB_MAX_IMAGES);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_images[ind] = ImageTextEx (raylib_fonts[font_handle], (const char *) text_buf, (float) font_size, (float) spacing, tint);
	pthread_mutex_unlock (&rlib_mutex);

	if (raylib_images[ind].data == NULL)
	{
		printf ("raylib_image_text_ex: ERROR operation failed!\n");
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	raylib_images_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_image_to_pot (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: image_handle, fill_addr
	S8 image_handle ALIGN;
	Color fill;
	S8 fill_addr ALIGN;

	sp = stpopi ((U1 *) &fill_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &image_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (image_handle < 0 || image_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[image_handle] == 0)
	{
		printf ("raylib_image_to_pot: ERROR invalid image handle!\n");
		return (NULL);
	}

	if (rlib_read_color (data, fill_addr, &fill) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ImageToPOT (&raylib_images[image_handle], fill);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_is_audio_stream_playing (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: stream_handle
	// return: 1 true, 0 false
	S8 stream_handle ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &stream_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (stream_handle < 0 || stream_handle >= RAYLIB_MAX_AUDIOSTREAMS || raylib_audiostreams_used[stream_handle] == 0)
	{
		printf ("raylib_is_audio_stream_playing: ERROR invalid audio stream handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	ret = IsAudioStreamPlaying (raylib_audiostreams[stream_handle]) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_is_audio_stream_processed (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: stream_handle
	// return: 1 true, 0 false
	S8 stream_handle ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &stream_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (stream_handle < 0 || stream_handle >= RAYLIB_MAX_AUDIOSTREAMS || raylib_audiostreams_used[stream_handle] == 0)
	{
		printf ("raylib_is_audio_stream_processed: ERROR invalid audio stream handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	ret = IsAudioStreamProcessed (raylib_audiostreams[stream_handle]) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_is_audio_stream_valid (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: stream_handle
	// return: 1 true, 0 false
	S8 stream_handle ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &stream_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (stream_handle < 0 || stream_handle >= RAYLIB_MAX_AUDIOSTREAMS || raylib_audiostreams_used[stream_handle] == 0)
	{
		printf ("raylib_is_audio_stream_valid: ERROR invalid audio stream handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	ret = IsAudioStreamValid (raylib_audiostreams[stream_handle]) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_is_cursor_hidden (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// return: 1 true, 0 false
	S8 ret ALIGN;

	pthread_mutex_lock (&rlib_mutex);
	ret = IsCursorHidden () ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_is_cursor_on_screen (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// return: 1 true, 0 false
	S8 ret ALIGN;

	pthread_mutex_lock (&rlib_mutex);
	ret = IsCursorOnScreen () ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_is_file_dropped (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// return: 1 true, 0 false
	S8 ret ALIGN;

	pthread_mutex_lock (&rlib_mutex);
	ret = IsFileDropped () ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_is_file_extension (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: file_name_addr, ext_addr
	// return: 1 true, 0 false
	S8 file_name_addr ALIGN;
	U1 file_name_buf[512];
	S8 ext_addr ALIGN;
	U1 ext_buf[512];
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &ext_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &file_name_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_load_filename (data, file_name_addr, file_name_buf, sizeof (file_name_buf)) != 0) return (NULL);
	if (rlib_read_string (data, ext_addr, ext_buf, sizeof (ext_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = IsFileExtension ((const char *) file_name_buf, (const char *) ext_buf) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_is_file_hidden (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: file_path_addr
	// return: 1 true, 0 false
	S8 file_path_addr ALIGN;
	U1 file_path_buf[512];
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &file_path_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_load_filename (data, file_path_addr, file_path_buf, sizeof (file_path_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = IsFileHidden ((const char *) file_path_buf) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_is_file_name_valid (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: file_name_addr
	// return: 1 true, 0 false
	S8 file_name_addr ALIGN;
	U1 file_name_buf[512];
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &file_name_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_load_filename (data, file_name_addr, file_name_buf, sizeof (file_name_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = IsFileNameValid ((const char *) file_name_buf) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_is_gesture_detected (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: gesture
	// return: 1 true, 0 false
	S8 gesture ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &gesture, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = IsGestureDetected ((unsigned int) gesture) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_is_key_pressed_repeat (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: key
	// return: 1 true, 0 false
	S8 key ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &key, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = IsKeyPressedRepeat ((int) key) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_is_music_valid (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: music_handle
	// return: 1 true, 0 false
	S8 music_handle ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &music_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (music_handle < 0 || music_handle >= RAYLIB_MAX_MUSICS || raylib_musics_used[music_handle] == 0)
	{
		printf ("raylib_is_music_valid: ERROR invalid music handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	ret = IsMusicValid (raylib_musics[music_handle]) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_is_path_absolute (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: path_addr
	// return: 1 true, 0 false
	S8 path_addr ALIGN;
	U1 path_buf[512];
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &path_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_load_filename (data, path_addr, path_buf, sizeof (path_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = IsPathAbsolute ((const char *) path_buf) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_is_path_directory (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: path_addr
	// return: 1 true, 0 false
	S8 path_addr ALIGN;
	U1 path_buf[512];
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &path_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_load_filename (data, path_addr, path_buf, sizeof (path_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = IsPathDirectory ((const char *) path_buf) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_is_path_file (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: path_addr
	// return: 1 true, 0 false
	S8 path_addr ALIGN;
	U1 path_buf[512];
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &path_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_load_filename (data, path_addr, path_buf, sizeof (path_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = IsPathFile ((const char *) path_buf) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_is_render_texture_valid (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: target_handle
	// return: 1 true, 0 false
	S8 target_handle ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &target_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (target_handle < 0 || target_handle >= RAYLIB_MAX_RENDERTEX || raylib_rendertextures_used[target_handle] == 0)
	{
		printf ("raylib_is_render_texture_valid: ERROR invalid render texture handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	ret = IsRenderTextureValid (raylib_rendertextures[target_handle]) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_is_wave_valid (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: wave_handle
	// return: 1 true, 0 false
	S8 wave_handle ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &wave_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (wave_handle < 0 || wave_handle >= RAYLIB_MAX_WAVES || raylib_waves_used[wave_handle] == 0)
	{
		printf ("raylib_is_wave_valid: ERROR invalid wave handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	ret = IsWaveValid (raylib_waves[wave_handle]) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_load_audio_stream (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: sample_rate, sample_size, channels
	// return: audio stream handle or -1 on error
	S8 sample_rate ALIGN;
	S8 sample_size ALIGN;
	S8 channels ALIGN;
	S8 ind ALIGN;

	sp = stpopi ((U1 *) &channels, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &sample_size, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &sample_rate, sp, sp_top);
	if (sp == NULL) return (NULL);

	ind = rlib_find_slot (raylib_audiostreams_used, RAYLIB_MAX_AUDIOSTREAMS);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_audiostreams[ind] = LoadAudioStream ((unsigned int) sample_rate, (unsigned int) sample_size, (unsigned int) channels);
	pthread_mutex_unlock (&rlib_mutex);

	if (raylib_audiostreams[ind].buffer == NULL)
	{
		printf ("raylib_load_audio_stream: ERROR operation failed!\n");
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	raylib_audiostreams_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_load_automation_event_list (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: file_name_addr
	// return: automation list handle or -1 on error
	S8 file_name_addr ALIGN;
	U1 file_name_buf[512];
	S8 ind ALIGN;

	sp = stpopi ((U1 *) &file_name_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_load_filename (data, file_name_addr, file_name_buf, sizeof (file_name_buf)) != 0) return (NULL);

	ind = rlib_find_slot (raylib_automationlists_used, RAYLIB_MAX_AUTOMATION);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_automationlists[ind] = LoadAutomationEventList ((const char *) file_name_buf);
	pthread_mutex_unlock (&rlib_mutex);

	if (raylib_automationlists[ind].events == NULL)
	{
		printf ("raylib_load_automation_event_list: ERROR operation failed!\n");
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	raylib_automationlists_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_load_directory_files (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: dir_path_addr
	// return: file path list handle or -1 on error
	S8 dir_path_addr ALIGN;
	U1 dir_path_buf[512];
	S8 ind ALIGN;

	sp = stpopi ((U1 *) &dir_path_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_load_filename (data, dir_path_addr, dir_path_buf, sizeof (dir_path_buf)) != 0) return (NULL);

	ind = rlib_find_slot (raylib_filepathlists_used, RAYLIB_MAX_FILEPATHLISTS);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_filepathlists[ind] = LoadDirectoryFiles ((const char *) dir_path_buf);
	pthread_mutex_unlock (&rlib_mutex);

	raylib_filepathlists_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_load_directory_files_ex (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: base_path_addr, filter_addr, scan_subdirs
	// return: file path list handle or -1 on error
	S8 base_path_addr ALIGN;
	U1 base_path_buf[512];
	S8 filter_addr ALIGN;
	U1 filter_buf[512];
	S8 scan_subdirs ALIGN;
	S8 ind ALIGN;

	sp = stpopi ((U1 *) &scan_subdirs, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &filter_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &base_path_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_load_filename (data, base_path_addr, base_path_buf, sizeof (base_path_buf)) != 0) return (NULL);
	if (rlib_read_string (data, filter_addr, filter_buf, sizeof (filter_buf)) != 0) return (NULL);

	ind = rlib_find_slot (raylib_filepathlists_used, RAYLIB_MAX_FILEPATHLISTS);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_filepathlists[ind] = LoadDirectoryFilesEx ((const char *) base_path_buf, (const char *) filter_buf, (bool) scan_subdirs);
	pthread_mutex_unlock (&rlib_mutex);

	raylib_filepathlists_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_load_dropped_files (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// return: file path list handle or -1 on error
	S8 ind ALIGN;

	ind = rlib_find_slot (raylib_filepathlists_used, RAYLIB_MAX_FILEPATHLISTS);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_filepathlists[ind] = LoadDroppedFiles ();
	pthread_mutex_unlock (&rlib_mutex);

	raylib_filepathlists_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_load_file_text (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: file_name_addr, out_addr
	// return: string written to out_addr
	S8 file_name_addr ALIGN;
	U1 file_name_buf[512];
	S8 out_addr ALIGN;
	const char *text;
	S8 len ALIGN;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &file_name_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_load_filename (data, file_name_addr, file_name_buf, sizeof (file_name_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	text = LoadFileText ((const char *) file_name_buf);
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
		printf ("raylib_load_file_text: ERROR memory bounds check failed!\n");
		return (NULL);
	}
	if (len > 0) memcpy (&data[out_addr], text, len);
	data[out_addr + len] = '\0';

	UnloadFileText ((char *) text);

	return (sp);
}

U1 *raylib_load_font_from_image (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: image_handle, key_addr, first_char
	// return: font handle or -1 on error
	S8 image_handle ALIGN;
	Color key;
	S8 key_addr ALIGN;
	S8 first_char ALIGN;
	S8 ind ALIGN;

	sp = stpopi ((U1 *) &first_char, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &key_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &image_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (image_handle < 0 || image_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[image_handle] == 0)
	{
		printf ("raylib_load_font_from_image: ERROR invalid image handle!\n");
		return (NULL);
	}

	if (rlib_read_color (data, key_addr, &key) != 0) return (NULL);

	ind = rlib_find_slot (raylib_fonts_used, RAYLIB_MAX_FONTS);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_fonts[ind] = LoadFontFromImage (raylib_images[image_handle], key, (int) first_char);
	pthread_mutex_unlock (&rlib_mutex);

	if (raylib_fonts[ind].glyphCount == 0)
	{
		printf ("raylib_load_font_from_image: ERROR operation failed!\n");
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	raylib_fonts_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_load_font_from_memory (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: file_type_addr, file_data_addr, data_size, font_size, codepoints_addr, codepoint_count
	// return: font handle or -1 on error
	S8 file_type_addr ALIGN;
	U1 file_type_buf[512];
	S8 file_data_addr ALIGN;
	S8 data_size ALIGN;
	S8 font_size ALIGN;
	S8 codepoints_addr ALIGN;
	int *codepoints_ptr = NULL;
	S8 codepoint_count ALIGN;
	S8 ind ALIGN;
	S8 i ALIGN;
	F8 t ALIGN;

	sp = stpopi ((U1 *) &codepoint_count, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &codepoints_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &font_size, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &data_size, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &file_data_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &file_type_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_string (data, file_type_addr, file_type_buf, sizeof (file_type_buf)) != 0) return (NULL);
	if (data_size > 0)
	{
		if (memory_bounds (file_data_addr, data_size - 1) != 0)
		{
			printf ("raylib_load_font_from_memory: ERROR memory bounds check failed!\n");
			return (NULL);
		}
	}
	if (codepoint_count > 0)
	{
		if (memory_bounds (codepoints_addr, (codepoint_count - 1) * 8) != 0)
		{
			printf ("raylib_load_font_from_memory: ERROR memory bounds check failed!\n");
			return (NULL);
		}
		codepoints_ptr = calloc (codepoint_count, sizeof (int));
		if (codepoints_ptr == NULL)
		{
			printf ("raylib_load_font_from_memory: ERROR out of memory!\n");
			return (NULL);
		}
		for (i = 0; i < codepoint_count; i++)
		{
			memcpy (&t, &data[codepoints_addr + (i * 8)], sizeof (F8));
			codepoints_ptr[i] = (int) t;
		}
	}

	ind = rlib_find_slot (raylib_fonts_used, RAYLIB_MAX_FONTS);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_fonts[ind] = LoadFontFromMemory ((const char *) file_type_buf, (const unsigned char *) &data[file_data_addr], (int) data_size, (int) font_size, (const int *) codepoints_ptr, (int) codepoint_count);
	pthread_mutex_unlock (&rlib_mutex);

	free (codepoints_ptr);

	if (raylib_fonts[ind].glyphCount == 0)
	{
		printf ("raylib_load_font_from_memory: ERROR operation failed!\n");
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	raylib_fonts_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_load_image_from_memory (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: file_type_addr, file_data_addr, data_size
	// return: image handle or -1 on error
	S8 file_type_addr ALIGN;
	U1 file_type_buf[512];
	S8 file_data_addr ALIGN;
	S8 data_size ALIGN;
	S8 ind ALIGN;

	sp = stpopi ((U1 *) &data_size, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &file_data_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &file_type_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_string (data, file_type_addr, file_type_buf, sizeof (file_type_buf)) != 0) return (NULL);
	if (data_size > 0)
	{
		if (memory_bounds (file_data_addr, data_size - 1) != 0)
		{
			printf ("raylib_load_image_from_memory: ERROR memory bounds check failed!\n");
			return (NULL);
		}
	}

	ind = rlib_find_slot (raylib_images_used, RAYLIB_MAX_IMAGES);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_images[ind] = LoadImageFromMemory ((const char *) file_type_buf, (const unsigned char *) &data[file_data_addr], (int) data_size);
	pthread_mutex_unlock (&rlib_mutex);

	if (raylib_images[ind].data == NULL)
	{
		printf ("raylib_load_image_from_memory: ERROR operation failed!\n");
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	raylib_images_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_load_image_from_screen (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// return: image handle or -1 on error
	S8 ind ALIGN;

	ind = rlib_find_slot (raylib_images_used, RAYLIB_MAX_IMAGES);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_images[ind] = LoadImageFromScreen ();
	pthread_mutex_unlock (&rlib_mutex);

	if (raylib_images[ind].data == NULL)
	{
		printf ("raylib_load_image_from_screen: ERROR operation failed!\n");
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	raylib_images_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_load_image_raw (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: file_name_addr, width, height, format, header_size
	// return: image handle or -1 on error
	S8 file_name_addr ALIGN;
	U1 file_name_buf[512];
	S8 width ALIGN;
	S8 height ALIGN;
	S8 format ALIGN;
	S8 header_size ALIGN;
	S8 ind ALIGN;

	sp = stpopi ((U1 *) &header_size, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &format, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &height, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &width, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &file_name_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_load_filename (data, file_name_addr, file_name_buf, sizeof (file_name_buf)) != 0) return (NULL);

	ind = rlib_find_slot (raylib_images_used, RAYLIB_MAX_IMAGES);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_images[ind] = LoadImageRaw ((const char *) file_name_buf, (int) width, (int) height, (int) format, (int) header_size);
	pthread_mutex_unlock (&rlib_mutex);

	if (raylib_images[ind].data == NULL)
	{
		printf ("raylib_load_image_raw: ERROR operation failed!\n");
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	raylib_images_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_load_music_stream_from_memory (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: file_type_addr, data_addr, data_size
	// return: music handle or -1 on error
	S8 file_type_addr ALIGN;
	U1 file_type_buf[512];
	S8 data_addr ALIGN;
	S8 data_size ALIGN;
	S8 ind ALIGN;

	sp = stpopi ((U1 *) &data_size, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &data_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &file_type_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_string (data, file_type_addr, file_type_buf, sizeof (file_type_buf)) != 0) return (NULL);
	if (data_size > 0)
	{
		if (memory_bounds (data_addr, data_size - 1) != 0)
		{
			printf ("raylib_load_music_stream_from_memory: ERROR memory bounds check failed!\n");
			return (NULL);
		}
	}

	ind = rlib_find_slot (raylib_musics_used, RAYLIB_MAX_MUSICS);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_musics[ind] = LoadMusicStreamFromMemory ((const char *) file_type_buf, (const unsigned char *) &data[data_addr], (int) data_size);
	pthread_mutex_unlock (&rlib_mutex);

	if (raylib_musics[ind].stream.buffer == NULL || raylib_musics[ind].ctxData == NULL)
	{
		printf ("raylib_load_music_stream_from_memory: ERROR operation failed!\n");
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	raylib_musics_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_load_render_texture (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: width, height
	// return: render texture handle or -1 on error
	S8 width ALIGN;
	S8 height ALIGN;
	S8 ind ALIGN;

	sp = stpopi ((U1 *) &height, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &width, sp, sp_top);
	if (sp == NULL) return (NULL);

	ind = rlib_find_slot (raylib_rendertextures_used, RAYLIB_MAX_RENDERTEX);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_rendertextures[ind] = LoadRenderTexture ((int) width, (int) height);
	pthread_mutex_unlock (&rlib_mutex);

	if (raylib_rendertextures[ind].texture.id == 0)
	{
		printf ("raylib_load_render_texture: ERROR operation failed!\n");
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	raylib_rendertextures_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_load_render_texture_ex (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: width, height, format
	// return: render texture handle or -1 on error
	S8 width ALIGN;
	S8 height ALIGN;
	S8 format ALIGN;
	S8 ind ALIGN;

	sp = stpopi ((U1 *) &format, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &height, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &width, sp, sp_top);
	if (sp == NULL) return (NULL);

	ind = rlib_find_slot (raylib_rendertextures_used, RAYLIB_MAX_RENDERTEX);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_rendertextures[ind] = LoadRenderTextureEx ((int) width, (int) height, (int) format);
	pthread_mutex_unlock (&rlib_mutex);

	if (raylib_rendertextures[ind].texture.id == 0)
	{
		printf ("raylib_load_render_texture_ex: ERROR operation failed!\n");
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	raylib_rendertextures_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_load_sound_alias (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: source_handle
	// return: sound handle or -1 on error
	S8 source_handle ALIGN;
	S8 ind ALIGN;

	sp = stpopi ((U1 *) &source_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (source_handle < 0 || source_handle >= RAYLIB_MAX_SOUNDS || raylib_sounds_used[source_handle] == 0)
	{
		printf ("raylib_load_sound_alias: ERROR invalid sound handle!\n");
		return (NULL);
	}

	ind = rlib_find_slot (raylib_sounds_used, RAYLIB_MAX_SOUNDS);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_sounds[ind] = LoadSoundAlias (raylib_sounds[source_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	if (raylib_sounds[ind].stream.buffer == NULL)
	{
		printf ("raylib_load_sound_alias: ERROR operation failed!\n");
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	raylib_sounds_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_load_sound_from_wave (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: wave_handle
	// return: sound handle or -1 on error
	S8 wave_handle ALIGN;
	S8 ind ALIGN;

	sp = stpopi ((U1 *) &wave_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (wave_handle < 0 || wave_handle >= RAYLIB_MAX_WAVES || raylib_waves_used[wave_handle] == 0)
	{
		printf ("raylib_load_sound_from_wave: ERROR invalid wave handle!\n");
		return (NULL);
	}

	ind = rlib_find_slot (raylib_sounds_used, RAYLIB_MAX_SOUNDS);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_sounds[ind] = LoadSoundFromWave (raylib_waves[wave_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	if (raylib_sounds[ind].stream.buffer == NULL)
	{
		printf ("raylib_load_sound_from_wave: ERROR operation failed!\n");
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	raylib_sounds_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_load_texture_cubemap (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: image_handle, layout
	// return: texture handle or -1 on error
	S8 image_handle ALIGN;
	S8 layout ALIGN;
	S8 ind ALIGN;

	sp = stpopi ((U1 *) &layout, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &image_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (image_handle < 0 || image_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[image_handle] == 0)
	{
		printf ("raylib_load_texture_cubemap: ERROR invalid image handle!\n");
		return (NULL);
	}

	ind = rlib_find_slot (raylib_textures_used, RAYLIB_MAX_TEXTURES);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_textures[ind] = LoadTextureCubemap (raylib_images[image_handle], (int) layout);
	pthread_mutex_unlock (&rlib_mutex);

	if (raylib_textures[ind].id == 0)
	{
		printf ("raylib_load_texture_cubemap: ERROR operation failed!\n");
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	raylib_textures_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_load_utf8 (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: codepoints_addr, length, out_addr
	// return: string written to out_addr
	S8 codepoints_addr ALIGN;
	int *codepoints_ptr = NULL;
	S8 length ALIGN;
	S8 out_addr ALIGN;
	const char *text;
	S8 len ALIGN;
	S8 i ALIGN;
	F8 t ALIGN;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &length, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &codepoints_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (length > 0)
	{
		if (memory_bounds (codepoints_addr, (length - 1) * 8) != 0)
		{
			printf ("raylib_load_utf8: ERROR memory bounds check failed!\n");
			return (NULL);
		}
		codepoints_ptr = calloc (length, sizeof (int));
		if (codepoints_ptr == NULL)
		{
			printf ("raylib_load_utf8: ERROR out of memory!\n");
			return (NULL);
		}
		for (i = 0; i < length; i++)
		{
			memcpy (&t, &data[codepoints_addr + (i * 8)], sizeof (F8));
			codepoints_ptr[i] = (int) t;
		}
	}

	pthread_mutex_lock (&rlib_mutex);
	text = LoadUTF8 ((const int *) codepoints_ptr, (int) length);
	if (text != NULL)
	{
		len = strlen_safe (text, 4095);
	}
	else
	{
		len = 0;
	}
	pthread_mutex_unlock (&rlib_mutex);

	free (codepoints_ptr);

	if (memory_bounds (out_addr, len) != 0)
	{
		printf ("raylib_load_utf8: ERROR memory bounds check failed!\n");
		return (NULL);
	}
	if (len > 0) memcpy (&data[out_addr], text, len);
	data[out_addr + len] = '\0';

	UnloadUTF8 ((char *) text);

	return (sp);
}

U1 *raylib_load_vr_stereo_config (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: device_addr
	// return: vr stereo config handle or -1 on error
	VrDeviceInfo device;
	S8 device_addr ALIGN;
	S8 ind ALIGN;

	sp = stpopi ((U1 *) &device_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_vrdeviceinfo (data, device_addr, &device) != 0) return (NULL);

	ind = rlib_find_slot (raylib_vrconfigs_used, RAYLIB_MAX_VRCONFIGS);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_vrconfigs[ind] = LoadVrStereoConfig (device);
	pthread_mutex_unlock (&rlib_mutex);

	raylib_vrconfigs_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_load_wave (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: file_name_addr
	// return: wave handle or -1 on error
	S8 file_name_addr ALIGN;
	U1 file_name_buf[512];
	S8 ind ALIGN;

	sp = stpopi ((U1 *) &file_name_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_load_filename (data, file_name_addr, file_name_buf, sizeof (file_name_buf)) != 0) return (NULL);

	ind = rlib_find_slot (raylib_waves_used, RAYLIB_MAX_WAVES);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_waves[ind] = LoadWave ((const char *) file_name_buf);
	pthread_mutex_unlock (&rlib_mutex);

	if (raylib_waves[ind].frameCount == 0)
	{
		printf ("raylib_load_wave: ERROR operation failed!\n");
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	raylib_waves_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_load_wave_from_memory (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: file_type_addr, file_data_addr, data_size
	// return: wave handle or -1 on error
	S8 file_type_addr ALIGN;
	U1 file_type_buf[512];
	S8 file_data_addr ALIGN;
	S8 data_size ALIGN;
	S8 ind ALIGN;

	sp = stpopi ((U1 *) &data_size, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &file_data_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &file_type_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_string (data, file_type_addr, file_type_buf, sizeof (file_type_buf)) != 0) return (NULL);
	if (data_size > 0)
	{
		if (memory_bounds (file_data_addr, data_size - 1) != 0)
		{
			printf ("raylib_load_wave_from_memory: ERROR memory bounds check failed!\n");
			return (NULL);
		}
	}

	ind = rlib_find_slot (raylib_waves_used, RAYLIB_MAX_WAVES);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_waves[ind] = LoadWaveFromMemory ((const char *) file_type_buf, (const unsigned char *) &data[file_data_addr], (int) data_size);
	pthread_mutex_unlock (&rlib_mutex);

	if (raylib_waves[ind].frameCount == 0)
	{
		printf ("raylib_load_wave_from_memory: ERROR operation failed!\n");
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	raylib_waves_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_make_directory (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: dir_path_addr
	// return: integer result
	S8 dir_path_addr ALIGN;
	U1 dir_path_buf[512];
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &dir_path_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_load_filename (data, dir_path_addr, dir_path_buf, sizeof (dir_path_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = MakeDirectory ((const char *) dir_path_buf);
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_measure_text_codepoints (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: font_handle, codepoints_addr, length, font_size, spacing, out_addr
	// return: struct written to out_addr
	S8 font_handle ALIGN;
	S8 codepoints_addr ALIGN;
	int *codepoints_ptr = NULL;
	S8 length ALIGN;
	F8 font_size ALIGN;
	F8 spacing ALIGN;
	S8 out_addr ALIGN;
	Vector2 ret;
	S8 i ALIGN;
	F8 t ALIGN;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &spacing, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &font_size, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &length, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &codepoints_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &font_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (font_handle < 0 || font_handle >= RAYLIB_MAX_FONTS || raylib_fonts_used[font_handle] == 0)
	{
		printf ("raylib_measure_text_codepoints: ERROR invalid font handle!\n");
		return (NULL);
	}

	if (length > 0)
	{
		if (memory_bounds (codepoints_addr, (length - 1) * 8) != 0)
		{
			printf ("raylib_measure_text_codepoints: ERROR memory bounds check failed!\n");
			return (NULL);
		}
		codepoints_ptr = calloc (length, sizeof (int));
		if (codepoints_ptr == NULL)
		{
			printf ("raylib_measure_text_codepoints: ERROR out of memory!\n");
			return (NULL);
		}
		for (i = 0; i < length; i++)
		{
			memcpy (&t, &data[codepoints_addr + (i * 8)], sizeof (F8));
			codepoints_ptr[i] = (int) t;
		}
	}

	pthread_mutex_lock (&rlib_mutex);
	ret = MeasureTextCodepoints (raylib_fonts[font_handle], (const int *) codepoints_ptr, (int) length, (float) font_size, (float) spacing);
	pthread_mutex_unlock (&rlib_mutex);

	free (codepoints_ptr);

	if (rlib_write_vec2 (data, out_addr, ret) != 0) return (NULL);

	return (sp);
}

U1 *raylib_pause_audio_stream (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: stream_handle
	S8 stream_handle ALIGN;

	sp = stpopi ((U1 *) &stream_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (stream_handle < 0 || stream_handle >= RAYLIB_MAX_AUDIOSTREAMS || raylib_audiostreams_used[stream_handle] == 0)
	{
		printf ("raylib_pause_audio_stream: ERROR invalid audio stream handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	PauseAudioStream (raylib_audiostreams[stream_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_play_audio_stream (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: stream_handle
	S8 stream_handle ALIGN;

	sp = stpopi ((U1 *) &stream_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (stream_handle < 0 || stream_handle >= RAYLIB_MAX_AUDIOSTREAMS || raylib_audiostreams_used[stream_handle] == 0)
	{
		printf ("raylib_play_audio_stream: ERROR invalid audio stream handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	PlayAudioStream (raylib_audiostreams[stream_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_play_automation_event (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: event_addr
	AutomationEvent event;
	S8 event_addr ALIGN;

	sp = stpopi ((U1 *) &event_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_automationevent (data, event_addr, &event) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	PlayAutomationEvent (event);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_resume_audio_stream (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: stream_handle
	S8 stream_handle ALIGN;

	sp = stpopi ((U1 *) &stream_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (stream_handle < 0 || stream_handle >= RAYLIB_MAX_AUDIOSTREAMS || raylib_audiostreams_used[stream_handle] == 0)
	{
		printf ("raylib_resume_audio_stream: ERROR invalid audio stream handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	ResumeAudioStream (raylib_audiostreams[stream_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_save_file_data (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: file_name_addr, data_addr, data_size
	// return: 1 true, 0 false
	S8 file_name_addr ALIGN;
	U1 file_name_buf[512];
	S8 data_addr ALIGN;
	S8 data_size ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &data_size, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &data_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &file_name_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_load_filename (data, file_name_addr, file_name_buf, sizeof (file_name_buf)) != 0) return (NULL);
	if (data_size > 0)
	{
		if (memory_bounds (data_addr, data_size - 1) != 0)
		{
			printf ("raylib_save_file_data: ERROR memory bounds check failed!\n");
			return (NULL);
		}
	}

	pthread_mutex_lock (&rlib_mutex);
	ret = SaveFileData ((const char *) file_name_buf, (const void *) &data[data_addr], (int) data_size) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_save_file_text (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: file_name_addr, text_addr
	// return: 1 true, 0 false
	S8 file_name_addr ALIGN;
	U1 file_name_buf[512];
	S8 text_addr ALIGN;
	U1 text_buf[4096];
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &text_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &file_name_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_load_filename (data, file_name_addr, file_name_buf, sizeof (file_name_buf)) != 0) return (NULL);
	if (rlib_read_string (data, text_addr, text_buf, sizeof (text_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = SaveFileText ((const char *) file_name_buf, (const char *) text_buf) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_seek_music_stream (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: music_handle, position
	S8 music_handle ALIGN;
	F8 position ALIGN;

	sp = stpopd ((U1 *) &position, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &music_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (music_handle < 0 || music_handle >= RAYLIB_MAX_MUSICS || raylib_musics_used[music_handle] == 0)
	{
		printf ("raylib_seek_music_stream: ERROR invalid music handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	SeekMusicStream (raylib_musics[music_handle], (float) position);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_set_audio_stream_buffer_size_default (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: size
	S8 size ALIGN;

	sp = stpopi ((U1 *) &size, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	SetAudioStreamBufferSizeDefault ((int) size);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_set_audio_stream_pan (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: stream_handle, pan
	S8 stream_handle ALIGN;
	F8 pan ALIGN;

	sp = stpopd ((U1 *) &pan, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &stream_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (stream_handle < 0 || stream_handle >= RAYLIB_MAX_AUDIOSTREAMS || raylib_audiostreams_used[stream_handle] == 0)
	{
		printf ("raylib_set_audio_stream_pan: ERROR invalid audio stream handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	SetAudioStreamPan (raylib_audiostreams[stream_handle], (float) pan);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_set_audio_stream_pitch (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: stream_handle, pitch
	S8 stream_handle ALIGN;
	F8 pitch ALIGN;

	sp = stpopd ((U1 *) &pitch, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &stream_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (stream_handle < 0 || stream_handle >= RAYLIB_MAX_AUDIOSTREAMS || raylib_audiostreams_used[stream_handle] == 0)
	{
		printf ("raylib_set_audio_stream_pitch: ERROR invalid audio stream handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	SetAudioStreamPitch (raylib_audiostreams[stream_handle], (float) pitch);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_set_audio_stream_volume (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: stream_handle, volume
	S8 stream_handle ALIGN;
	F8 volume ALIGN;

	sp = stpopd ((U1 *) &volume, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &stream_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (stream_handle < 0 || stream_handle >= RAYLIB_MAX_AUDIOSTREAMS || raylib_audiostreams_used[stream_handle] == 0)
	{
		printf ("raylib_set_audio_stream_volume: ERROR invalid audio stream handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	SetAudioStreamVolume (raylib_audiostreams[stream_handle], (float) volume);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_set_automation_event_base_frame (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: frame
	S8 frame ALIGN;

	sp = stpopi ((U1 *) &frame, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	SetAutomationEventBaseFrame ((int) frame);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_set_automation_event_list (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: list_handle
	S8 list_handle ALIGN;

	sp = stpopi ((U1 *) &list_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (list_handle < 0 || list_handle >= RAYLIB_MAX_AUTOMATION || raylib_automationlists_used[list_handle] == 0)
	{
		printf ("raylib_set_automation_event_list: ERROR invalid automation list handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	SetAutomationEventList (&raylib_automationlists[list_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_set_gamepad_mappings (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: mappings_addr
	// return: integer result
	S8 mappings_addr ALIGN;
	U1 mappings_buf[512];
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &mappings_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_string (data, mappings_addr, mappings_buf, sizeof (mappings_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = SetGamepadMappings ((const char *) mappings_buf);
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_set_gestures_enabled (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: flags
	S8 flags ALIGN;

	sp = stpopi ((U1 *) &flags, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	SetGesturesEnabled ((unsigned int) flags);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_set_mouse_offset (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: offset_x, offset_y
	S8 offset_x ALIGN;
	S8 offset_y ALIGN;

	sp = stpopi ((U1 *) &offset_y, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &offset_x, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	SetMouseOffset ((int) offset_x, (int) offset_y);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_set_mouse_scale (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: scale_x, scale_y
	F8 scale_x ALIGN;
	F8 scale_y ALIGN;

	sp = stpopd ((U1 *) &scale_y, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &scale_x, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	SetMouseScale ((float) scale_x, (float) scale_y);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_set_shapes_texture (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: texture_handle, rec_addr
	S8 texture_handle ALIGN;
	Rectangle rec;
	S8 rec_addr ALIGN;

	sp = stpopi ((U1 *) &rec_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &texture_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (texture_handle < 0 || texture_handle >= RAYLIB_MAX_TEXTURES || raylib_textures_used[texture_handle] == 0)
	{
		printf ("raylib_set_shapes_texture: ERROR invalid texture handle!\n");
		return (NULL);
	}

	if (rlib_read_rect (data, rec_addr, &rec) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	SetShapesTexture (raylib_textures[texture_handle], rec);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_set_text_line_spacing (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: spacing
	S8 spacing ALIGN;

	sp = stpopi ((U1 *) &spacing, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	SetTextLineSpacing ((int) spacing);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_set_window_focused (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{

	pthread_mutex_lock (&rlib_mutex);
	SetWindowFocused ();
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_set_window_icon (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: image_handle
	S8 image_handle ALIGN;

	sp = stpopi ((U1 *) &image_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (image_handle < 0 || image_handle >= RAYLIB_MAX_IMAGES || raylib_images_used[image_handle] == 0)
	{
		printf ("raylib_set_window_icon: ERROR invalid image handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	SetWindowIcon (raylib_images[image_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_set_window_monitor (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: monitor
	S8 monitor ALIGN;

	sp = stpopi ((U1 *) &monitor, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	SetWindowMonitor ((int) monitor);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_show_cursor (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{

	pthread_mutex_lock (&rlib_mutex);
	ShowCursor ();
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_start_automation_event_recording (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{

	pthread_mutex_lock (&rlib_mutex);
	StartAutomationEventRecording ();
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_stop_audio_stream (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: stream_handle
	S8 stream_handle ALIGN;

	sp = stpopi ((U1 *) &stream_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (stream_handle < 0 || stream_handle >= RAYLIB_MAX_AUDIOSTREAMS || raylib_audiostreams_used[stream_handle] == 0)
	{
		printf ("raylib_stop_audio_stream: ERROR invalid audio stream handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	StopAudioStream (raylib_audiostreams[stream_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_stop_automation_event_recording (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{

	pthread_mutex_lock (&rlib_mutex);
	StopAutomationEventRecording ();
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_text_find_index (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: text_addr, search_addr
	// return: integer result
	S8 text_addr ALIGN;
	U1 text_buf[4096];
	S8 search_addr ALIGN;
	U1 search_buf[512];
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &search_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &text_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_string (data, text_addr, text_buf, sizeof (text_buf)) != 0) return (NULL);
	if (rlib_read_string (data, search_addr, search_buf, sizeof (search_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = TextFindIndex ((const char *) text_buf, (const char *) search_buf);
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_text_insert (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: text_addr, insert_addr, position, out_addr
	// return: string written to out_addr
	S8 text_addr ALIGN;
	U1 text_buf[4096];
	S8 insert_addr ALIGN;
	U1 insert_buf[512];
	S8 position ALIGN;
	S8 out_addr ALIGN;
	const char *text;
	S8 len ALIGN;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &position, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &insert_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &text_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_string (data, text_addr, text_buf, sizeof (text_buf)) != 0) return (NULL);
	if (rlib_read_string (data, insert_addr, insert_buf, sizeof (insert_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	text = TextInsert ((const char *) text_buf, (const char *) insert_buf, (int) position);
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
		printf ("raylib_text_insert: ERROR memory bounds check failed!\n");
		return (NULL);
	}
	if (len > 0) memcpy (&data[out_addr], text, len);
	data[out_addr + len] = '\0';

	return (sp);
}

U1 *raylib_text_is_equal (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: text1_addr, text2_addr
	// return: 1 true, 0 false
	S8 text1_addr ALIGN;
	U1 text1_buf[4096];
	S8 text2_addr ALIGN;
	U1 text2_buf[4096];
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &text2_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &text1_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_string (data, text1_addr, text1_buf, sizeof (text1_buf)) != 0) return (NULL);
	if (rlib_read_string (data, text2_addr, text2_buf, sizeof (text2_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = TextIsEqual ((const char *) text1_buf, (const char *) text2_buf) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_text_length (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: text_addr
	// return: integer result
	S8 text_addr ALIGN;
	U1 text_buf[4096];
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &text_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_string (data, text_addr, text_buf, sizeof (text_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = TextLength ((const char *) text_buf);
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_text_remove_spaces (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: text_addr, out_addr
	// return: string written to out_addr
	S8 text_addr ALIGN;
	U1 text_buf[4096];
	S8 out_addr ALIGN;
	const char *text;
	S8 len ALIGN;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &text_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_string (data, text_addr, text_buf, sizeof (text_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	text = TextRemoveSpaces ((const char *) text_buf);
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
		printf ("raylib_text_remove_spaces: ERROR memory bounds check failed!\n");
		return (NULL);
	}
	if (len > 0) memcpy (&data[out_addr], text, len);
	data[out_addr + len] = '\0';

	return (sp);
}

U1 *raylib_text_replace (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: text_addr, search_addr, replacement_addr, out_addr
	// return: string written to out_addr
	S8 text_addr ALIGN;
	U1 text_buf[4096];
	S8 search_addr ALIGN;
	U1 search_buf[512];
	S8 replacement_addr ALIGN;
	U1 replacement_buf[512];
	S8 out_addr ALIGN;
	const char *text;
	S8 len ALIGN;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &replacement_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &search_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &text_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_string (data, text_addr, text_buf, sizeof (text_buf)) != 0) return (NULL);
	if (rlib_read_string (data, search_addr, search_buf, sizeof (search_buf)) != 0) return (NULL);
	if (rlib_read_string (data, replacement_addr, replacement_buf, sizeof (replacement_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	text = TextReplace ((const char *) text_buf, (const char *) search_buf, (const char *) replacement_buf);
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
		printf ("raylib_text_replace: ERROR memory bounds check failed!\n");
		return (NULL);
	}
	if (len > 0) memcpy (&data[out_addr], text, len);
	data[out_addr + len] = '\0';

	return (sp);
}

U1 *raylib_text_replace_between (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: text_addr, begin_addr, end_addr, replacement_addr, out_addr
	// return: string written to out_addr
	S8 text_addr ALIGN;
	U1 text_buf[4096];
	S8 begin_addr ALIGN;
	U1 begin_buf[512];
	S8 end_addr ALIGN;
	U1 end_buf[512];
	S8 replacement_addr ALIGN;
	U1 replacement_buf[512];
	S8 out_addr ALIGN;
	const char *text;
	S8 len ALIGN;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &replacement_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &end_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &begin_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &text_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_string (data, text_addr, text_buf, sizeof (text_buf)) != 0) return (NULL);
	if (rlib_read_string (data, begin_addr, begin_buf, sizeof (begin_buf)) != 0) return (NULL);
	if (rlib_read_string (data, end_addr, end_buf, sizeof (end_buf)) != 0) return (NULL);
	if (rlib_read_string (data, replacement_addr, replacement_buf, sizeof (replacement_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	text = TextReplaceBetween ((const char *) text_buf, (const char *) begin_buf, (const char *) end_buf, (const char *) replacement_buf);
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
		printf ("raylib_text_replace_between: ERROR memory bounds check failed!\n");
		return (NULL);
	}
	if (len > 0) memcpy (&data[out_addr], text, len);
	data[out_addr + len] = '\0';

	return (sp);
}

U1 *raylib_text_subtext (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: text_addr, position, length, out_addr
	// return: string written to out_addr
	S8 text_addr ALIGN;
	U1 text_buf[4096];
	S8 position ALIGN;
	S8 length ALIGN;
	S8 out_addr ALIGN;
	const char *text;
	S8 len ALIGN;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &length, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &position, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &text_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_string (data, text_addr, text_buf, sizeof (text_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	text = TextSubtext ((const char *) text_buf, (int) position, (int) length);
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
		printf ("raylib_text_subtext: ERROR memory bounds check failed!\n");
		return (NULL);
	}
	if (len > 0) memcpy (&data[out_addr], text, len);
	data[out_addr + len] = '\0';

	return (sp);
}

U1 *raylib_text_to_camel (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: text_addr, out_addr
	// return: string written to out_addr
	S8 text_addr ALIGN;
	U1 text_buf[4096];
	S8 out_addr ALIGN;
	const char *text;
	S8 len ALIGN;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &text_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_string (data, text_addr, text_buf, sizeof (text_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	text = TextToCamel ((const char *) text_buf);
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
		printf ("raylib_text_to_camel: ERROR memory bounds check failed!\n");
		return (NULL);
	}
	if (len > 0) memcpy (&data[out_addr], text, len);
	data[out_addr + len] = '\0';

	return (sp);
}

U1 *raylib_text_to_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: text_addr
	// return: double value
	S8 text_addr ALIGN;
	U1 text_buf[4096];
	F8 ret ALIGN;

	sp = stpopi ((U1 *) &text_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_string (data, text_addr, text_buf, sizeof (text_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = TextToFloat ((const char *) text_buf);
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushd (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_text_to_integer (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: text_addr
	// return: integer result
	S8 text_addr ALIGN;
	U1 text_buf[4096];
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &text_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_string (data, text_addr, text_buf, sizeof (text_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	ret = TextToInteger ((const char *) text_buf);
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_text_to_lower (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: text_addr, out_addr
	// return: string written to out_addr
	S8 text_addr ALIGN;
	U1 text_buf[4096];
	S8 out_addr ALIGN;
	const char *text;
	S8 len ALIGN;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &text_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_string (data, text_addr, text_buf, sizeof (text_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	text = TextToLower ((const char *) text_buf);
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
		printf ("raylib_text_to_lower: ERROR memory bounds check failed!\n");
		return (NULL);
	}
	if (len > 0) memcpy (&data[out_addr], text, len);
	data[out_addr + len] = '\0';

	return (sp);
}

U1 *raylib_text_to_pascal (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: text_addr, out_addr
	// return: string written to out_addr
	S8 text_addr ALIGN;
	U1 text_buf[4096];
	S8 out_addr ALIGN;
	const char *text;
	S8 len ALIGN;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &text_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_string (data, text_addr, text_buf, sizeof (text_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	text = TextToPascal ((const char *) text_buf);
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
		printf ("raylib_text_to_pascal: ERROR memory bounds check failed!\n");
		return (NULL);
	}
	if (len > 0) memcpy (&data[out_addr], text, len);
	data[out_addr + len] = '\0';

	return (sp);
}

U1 *raylib_text_to_snake (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: text_addr, out_addr
	// return: string written to out_addr
	S8 text_addr ALIGN;
	U1 text_buf[4096];
	S8 out_addr ALIGN;
	const char *text;
	S8 len ALIGN;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &text_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_string (data, text_addr, text_buf, sizeof (text_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	text = TextToSnake ((const char *) text_buf);
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
		printf ("raylib_text_to_snake: ERROR memory bounds check failed!\n");
		return (NULL);
	}
	if (len > 0) memcpy (&data[out_addr], text, len);
	data[out_addr + len] = '\0';

	return (sp);
}

U1 *raylib_text_to_upper (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: text_addr, out_addr
	// return: string written to out_addr
	S8 text_addr ALIGN;
	U1 text_buf[4096];
	S8 out_addr ALIGN;
	const char *text;
	S8 len ALIGN;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &text_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_read_string (data, text_addr, text_buf, sizeof (text_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	text = TextToUpper ((const char *) text_buf);
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
		printf ("raylib_text_to_upper: ERROR memory bounds check failed!\n");
		return (NULL);
	}
	if (len > 0) memcpy (&data[out_addr], text, len);
	data[out_addr + len] = '\0';

	return (sp);
}

U1 *raylib_unload_audio_stream (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: stream_handle
	S8 stream_handle ALIGN;

	sp = stpopi ((U1 *) &stream_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (stream_handle < 0 || stream_handle >= RAYLIB_MAX_AUDIOSTREAMS || raylib_audiostreams_used[stream_handle] == 0)
	{
		printf ("raylib_unload_audio_stream: ERROR invalid audio stream handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	UnloadAudioStream (raylib_audiostreams[stream_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	memset (&raylib_audiostreams[stream_handle], 0, sizeof (raylib_audiostreams[0]));
	raylib_audiostreams_used[stream_handle] = 0;

	return (sp);
}

U1 *raylib_unload_automation_event_list (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: list_handle
	S8 list_handle ALIGN;

	sp = stpopi ((U1 *) &list_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (list_handle < 0 || list_handle >= RAYLIB_MAX_AUTOMATION || raylib_automationlists_used[list_handle] == 0)
	{
		printf ("raylib_unload_automation_event_list: ERROR invalid automation list handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	UnloadAutomationEventList (raylib_automationlists[list_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	memset (&raylib_automationlists[list_handle], 0, sizeof (raylib_automationlists[0]));
	raylib_automationlists_used[list_handle] = 0;

	return (sp);
}

U1 *raylib_unload_directory_files (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: files_handle
	S8 files_handle ALIGN;

	sp = stpopi ((U1 *) &files_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (files_handle < 0 || files_handle >= RAYLIB_MAX_FILEPATHLISTS || raylib_filepathlists_used[files_handle] == 0)
	{
		printf ("raylib_unload_directory_files: ERROR invalid file path list handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	UnloadDirectoryFiles (raylib_filepathlists[files_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	memset (&raylib_filepathlists[files_handle], 0, sizeof (raylib_filepathlists[0]));
	raylib_filepathlists_used[files_handle] = 0;

	return (sp);
}

U1 *raylib_unload_dropped_files (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: files_handle
	S8 files_handle ALIGN;

	sp = stpopi ((U1 *) &files_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (files_handle < 0 || files_handle >= RAYLIB_MAX_FILEPATHLISTS || raylib_filepathlists_used[files_handle] == 0)
	{
		printf ("raylib_unload_dropped_files: ERROR invalid file path list handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	UnloadDroppedFiles (raylib_filepathlists[files_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	memset (&raylib_filepathlists[files_handle], 0, sizeof (raylib_filepathlists[0]));
	raylib_filepathlists_used[files_handle] = 0;

	return (sp);
}

U1 *raylib_unload_render_texture (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: target_handle
	S8 target_handle ALIGN;

	sp = stpopi ((U1 *) &target_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (target_handle < 0 || target_handle >= RAYLIB_MAX_RENDERTEX || raylib_rendertextures_used[target_handle] == 0)
	{
		printf ("raylib_unload_render_texture: ERROR invalid render texture handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	UnloadRenderTexture (raylib_rendertextures[target_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	memset (&raylib_rendertextures[target_handle], 0, sizeof (raylib_rendertextures[0]));
	raylib_rendertextures_used[target_handle] = 0;

	return (sp);
}

U1 *raylib_unload_sound_alias (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: alias_handle
	S8 alias_handle ALIGN;

	sp = stpopi ((U1 *) &alias_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (alias_handle < 0 || alias_handle >= RAYLIB_MAX_SOUNDS || raylib_sounds_used[alias_handle] == 0)
	{
		printf ("raylib_unload_sound_alias: ERROR invalid sound handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	UnloadSoundAlias (raylib_sounds[alias_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	memset (&raylib_sounds[alias_handle], 0, sizeof (raylib_sounds[0]));
	raylib_sounds_used[alias_handle] = 0;

	return (sp);
}

U1 *raylib_unload_vr_stereo_config (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: config_handle
	S8 config_handle ALIGN;

	sp = stpopi ((U1 *) &config_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (config_handle < 0 || config_handle >= RAYLIB_MAX_VRCONFIGS || raylib_vrconfigs_used[config_handle] == 0)
	{
		printf ("raylib_unload_vr_stereo_config: ERROR invalid vr stereo config handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	UnloadVrStereoConfig (raylib_vrconfigs[config_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	memset (&raylib_vrconfigs[config_handle], 0, sizeof (raylib_vrconfigs[0]));
	raylib_vrconfigs_used[config_handle] = 0;

	return (sp);
}

U1 *raylib_unload_wave (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: wave_handle
	S8 wave_handle ALIGN;

	sp = stpopi ((U1 *) &wave_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (wave_handle < 0 || wave_handle >= RAYLIB_MAX_WAVES || raylib_waves_used[wave_handle] == 0)
	{
		printf ("raylib_unload_wave: ERROR invalid wave handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	UnloadWave (raylib_waves[wave_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	memset (&raylib_waves[wave_handle], 0, sizeof (raylib_waves[0]));
	raylib_waves_used[wave_handle] = 0;

	return (sp);
}

U1 *raylib_update_mesh_buffer (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: mesh_handle, index, data_addr, data_size, offset
	S8 mesh_handle ALIGN;
	S8 index ALIGN;
	S8 data_addr ALIGN;
	S8 data_size ALIGN;
	S8 offset ALIGN;

	sp = stpopi ((U1 *) &offset, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &data_size, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &data_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &index, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &mesh_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (mesh_handle < 0 || mesh_handle >= RAYLIB_MAX_MESHES || raylib_meshes_used[mesh_handle] == 0)
	{
		printf ("raylib_update_mesh_buffer: ERROR invalid mesh handle!\n");
		return (NULL);
	}

	if (data_size > 0)
	{
		if (memory_bounds (data_addr, data_size - 1) != 0)
		{
			printf ("raylib_update_mesh_buffer: ERROR memory bounds check failed!\n");
			return (NULL);
		}
	}

	pthread_mutex_lock (&rlib_mutex);
	UpdateMeshBuffer (raylib_meshes[mesh_handle], (int) index, (const void *) &data[data_addr], (int) data_size, (int) offset);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_wave_copy (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: wave_handle
	// return: wave handle or -1 on error
	S8 wave_handle ALIGN;
	S8 ind ALIGN;

	sp = stpopi ((U1 *) &wave_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (wave_handle < 0 || wave_handle >= RAYLIB_MAX_WAVES || raylib_waves_used[wave_handle] == 0)
	{
		printf ("raylib_wave_copy: ERROR invalid wave handle!\n");
		return (NULL);
	}

	ind = rlib_find_slot (raylib_waves_used, RAYLIB_MAX_WAVES);
	if (ind < 0)
	{
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	pthread_mutex_lock (&rlib_mutex);
	raylib_waves[ind] = WaveCopy (raylib_waves[wave_handle]);
	pthread_mutex_unlock (&rlib_mutex);

	if (raylib_waves[ind].frameCount == 0)
	{
		printf ("raylib_wave_copy: ERROR operation failed!\n");
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	raylib_waves_used[ind] = 1;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_wave_crop (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: wave_handle, init_frame, final_frame
	S8 wave_handle ALIGN;
	S8 init_frame ALIGN;
	S8 final_frame ALIGN;

	sp = stpopi ((U1 *) &final_frame, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &init_frame, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &wave_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (wave_handle < 0 || wave_handle >= RAYLIB_MAX_WAVES || raylib_waves_used[wave_handle] == 0)
	{
		printf ("raylib_wave_crop: ERROR invalid wave handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	WaveCrop (&raylib_waves[wave_handle], (int) init_frame, (int) final_frame);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_wave_format (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: wave_handle, sample_rate, sample_size, channels
	S8 wave_handle ALIGN;
	S8 sample_rate ALIGN;
	S8 sample_size ALIGN;
	S8 channels ALIGN;

	sp = stpopi ((U1 *) &channels, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &sample_size, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &sample_rate, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &wave_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (wave_handle < 0 || wave_handle >= RAYLIB_MAX_WAVES || raylib_waves_used[wave_handle] == 0)
	{
		printf ("raylib_wave_format: ERROR invalid wave handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	WaveFormat (&raylib_waves[wave_handle], (int) sample_rate, (int) sample_size, (int) channels);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

// ---------------------------------------------------------------------------
// model animation functions
// ---------------------------------------------------------------------------

U1 *raylib_load_model_animations (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: file_name_addr
	// return: base animation handle or -1 on error
	S8 file_name_addr ALIGN;
	U1 file_name_buf[512];
	ModelAnimation *anims = NULL;
	int anim_count = 0;
	S8 ind ALIGN;
	S8 i ALIGN;

	sp = stpopi ((U1 *) &file_name_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (rlib_load_filename (data, file_name_addr, file_name_buf, sizeof (file_name_buf)) != 0) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	anims = LoadModelAnimations ((const char *) file_name_buf, &anim_count);
	pthread_mutex_unlock (&rlib_mutex);

	if (anims == NULL || anim_count <= 0)
	{
		printf ("raylib_load_model_animations: ERROR loading model animations: %s!\n", file_name_buf);
		sp = stpushi (-1, sp, sp_bottom);
		return (sp);
	}

	ind = rlib_find_slots (raylib_animations_used, RAYLIB_MAX_ANIMATIONS, anim_count);
	if (ind < 0)
	{
		pthread_mutex_lock (&rlib_mutex);
		UnloadModelAnimations (anims, anim_count);
		pthread_mutex_unlock (&rlib_mutex);
		return (NULL);
	}

	for (i = 0; i < anim_count; i++)
	{
		raylib_animations[ind + i] = anims[i];
		raylib_animations_used[ind + i] = 1;
		raylib_animations_owner[ind + i] = ind;
		raylib_animations_count[ind + i] = anim_count;
	}
	raylib_animations_array[ind] = anims;

	sp = stpushi (ind, sp, sp_bottom);
	return (sp);
}

U1 *raylib_unload_model_animations (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: animation_handle
	// takes the base handle returned by raylib_load_model_animations and
	// frees the whole animation array
	S8 animation_handle ALIGN;
	S8 base ALIGN;
	S8 count ALIGN;
	S8 i ALIGN;

	sp = stpopi ((U1 *) &animation_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (animation_handle < 0 || animation_handle >= RAYLIB_MAX_ANIMATIONS || raylib_animations_used[animation_handle] == 0)
	{
		printf ("raylib_unload_model_animations: ERROR invalid model animation handle!\n");
		return (NULL);
	}

	base = raylib_animations_owner[animation_handle];
	if (base < 0 || base >= RAYLIB_MAX_ANIMATIONS || raylib_animations_array[base] == NULL)
	{
		printf ("raylib_unload_model_animations: ERROR invalid animation array!\n");
		return (NULL);
	}
	count = raylib_animations_count[base];

	pthread_mutex_lock (&rlib_mutex);
	UnloadModelAnimations (raylib_animations_array[base], (int) count);
	pthread_mutex_unlock (&rlib_mutex);

	for (i = 0; i < count; i++)
	{
		memset (&raylib_animations[base + i], 0, sizeof (ModelAnimation));
		raylib_animations_used[base + i] = 0;
		raylib_animations_owner[base + i] = 0;
		raylib_animations_count[base + i] = 0;
	}
	raylib_animations_array[base] = NULL;

	return (sp);
}

U1 *raylib_get_model_animation_count (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: animation_handle
	// return: number of animations in the loaded animation array
	S8 animation_handle ALIGN;
	S8 count ALIGN;

	sp = stpopi ((U1 *) &animation_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (animation_handle < 0 || animation_handle >= RAYLIB_MAX_ANIMATIONS || raylib_animations_used[animation_handle] == 0)
	{
		printf ("raylib_get_model_animation_count: ERROR invalid model animation handle!\n");
		return (NULL);
	}

	count = raylib_animations_count[animation_handle];

	sp = stpushi (count, sp, sp_bottom);
	return (sp);
}

U1 *raylib_update_model_animation (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: model_handle, animation_handle, frame
	S8 model_handle ALIGN;
	S8 animation_handle ALIGN;
	F8 frame ALIGN;

	sp = stpopd ((U1 *) &frame, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &animation_handle, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &model_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (model_handle < 0 || model_handle >= RAYLIB_MAX_MODELS || raylib_models_used[model_handle] == 0)
	{
		printf ("raylib_update_model_animation: ERROR invalid model handle!\n");
		return (NULL);
	}

	if (animation_handle < 0 || animation_handle >= RAYLIB_MAX_ANIMATIONS || raylib_animations_used[animation_handle] == 0)
	{
		printf ("raylib_update_model_animation: ERROR invalid model animation handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	UpdateModelAnimation (raylib_models[model_handle], raylib_animations[animation_handle], (float) frame);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_update_model_animation_ex (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: model_handle, anim_a_handle, frame_a, anim_b_handle, frame_b, blend
	S8 model_handle ALIGN;
	S8 anim_a_handle ALIGN;
	F8 frame_a ALIGN;
	S8 anim_b_handle ALIGN;
	F8 frame_b ALIGN;
	F8 blend ALIGN;

	sp = stpopd ((U1 *) &blend, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &frame_b, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &anim_b_handle, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopd ((U1 *) &frame_a, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &anim_a_handle, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &model_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (model_handle < 0 || model_handle >= RAYLIB_MAX_MODELS || raylib_models_used[model_handle] == 0)
	{
		printf ("raylib_update_model_animation_ex: ERROR invalid model handle!\n");
		return (NULL);
	}

	if (anim_a_handle < 0 || anim_a_handle >= RAYLIB_MAX_ANIMATIONS || raylib_animations_used[anim_a_handle] == 0)
	{
		printf ("raylib_update_model_animation_ex: ERROR invalid model animation handle!\n");
		return (NULL);
	}

	if (anim_b_handle < 0 || anim_b_handle >= RAYLIB_MAX_ANIMATIONS || raylib_animations_used[anim_b_handle] == 0)
	{
		printf ("raylib_update_model_animation_ex: ERROR invalid model animation handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	UpdateModelAnimationEx (raylib_models[model_handle], raylib_animations[anim_a_handle], (float) frame_a,
		raylib_animations[anim_b_handle], (float) frame_b, (float) blend);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_is_model_animation_valid (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: model_handle, animation_handle
	// return: 1 true, 0 false
	S8 model_handle ALIGN;
	S8 animation_handle ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &animation_handle, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &model_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (model_handle < 0 || model_handle >= RAYLIB_MAX_MODELS || raylib_models_used[model_handle] == 0)
	{
		printf ("raylib_is_model_animation_valid: ERROR invalid model handle!\n");
		return (NULL);
	}

	if (animation_handle < 0 || animation_handle >= RAYLIB_MAX_ANIMATIONS || raylib_animations_used[animation_handle] == 0)
	{
		printf ("raylib_is_model_animation_valid: ERROR invalid model animation handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	ret = IsModelAnimationValid (raylib_models[model_handle], raylib_animations[animation_handle]) ? 1 : 0;
	pthread_mutex_unlock (&rlib_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

// ---------------------------------------------------------------------------
// raylib 6.2 callback wrappers
// ---------------------------------------------------------------------------
//
// The raylib callback API has two halves: raylib invokes the C callback itself
// (either from inside a raylib call made by the VM, or from the audio thread),
// and the VM function enables/disables the callback and polls the data it
// captured. All state the callbacks need lives in the static "rlib_cb_*"
// variables below.
//
// Locking rules:
//
//   rlib_mutex     - serializes the raylib calls made by the VM (module rule)
//   rlib_cb_mutex  - protects the static callback storage
//
// The two mutexes are NEVER nested. The C callbacks only take rlib_cb_mutex,
// because they run while the VM thread already holds rlib_mutex (trace log,
// file IO callbacks fire inside the raylib call that triggers them) or on the
// audio thread (audio callbacks). VM poll functions only take rlib_cb_mutex
// and never call raylib while holding it, so no lock order cycle exists.

// callback storage limits ----------------------------------------------------
#define RLIB_CB_LOG_MAX			64				// queued trace log messages
#define RLIB_CB_LOG_MSG_LEN		1024			// max length of one log message
#define RLIB_CB_PATH_LEN		512				// max file name length
#define RLIB_CB_FILEDATA_MAX	(1024*1024)		// staged/captured binary data
#define RLIB_CB_FILETEXT_MAX	(256*1024)		// staged/captured file text
#define RLIB_CB_AUDIO_STAGE		8192			// sample stage, in stereo frames
#define RLIB_CB_AUDIO_CAP		16384			// processor capture, in samples

static pthread_mutex_t rlib_cb_mutex = PTHREAD_MUTEX_INITIALIZER;

// SetTraceLogCallback: ring of formatted messages, oldest at head - count
static char rlib_cb_log[RLIB_CB_LOG_MAX][RLIB_CB_LOG_MSG_LEN];
static S2 rlib_cb_log_level[RLIB_CB_LOG_MAX];
static S8 rlib_cb_log_head;
static S8 rlib_cb_log_count;

// SetLoadFileDataCallback: last request + staged response data
static U1 rlib_cb_load_data_name[RLIB_CB_PATH_LEN];
static S2 rlib_cb_load_data_req;
static U1 rlib_cb_load_data_resp[RLIB_CB_FILEDATA_MAX];
static S8 rlib_cb_load_data_resp_size;
static S2 rlib_cb_load_data_resp_staged;

// SetSaveFileDataCallback: last request + captured binary data
static U1 rlib_cb_save_data_name[RLIB_CB_PATH_LEN];
static U1 rlib_cb_save_data_buf[RLIB_CB_FILEDATA_MAX];
static S8 rlib_cb_save_data_size;
static S2 rlib_cb_save_data_req;

// SetLoadFileTextCallback: last request + staged text
static U1 rlib_cb_load_text_name[RLIB_CB_PATH_LEN];
static S2 rlib_cb_load_text_req;
static char rlib_cb_load_text_resp[RLIB_CB_FILETEXT_MAX];
static S2 rlib_cb_load_text_resp_staged;

// SetSaveFileTextCallback: last request + captured text
static U1 rlib_cb_save_text_name[RLIB_CB_PATH_LEN];
static char rlib_cb_save_text_buf[RLIB_CB_FILETEXT_MAX];
static S2 rlib_cb_save_text_req;

// SetAudioStreamCallback: sample stage ring (interleaved L,R float pairs)
static float rlib_cb_audio_stage[RLIB_CB_AUDIO_STAGE * 2];
static S8 rlib_cb_audio_stage_head;
static S8 rlib_cb_audio_stage_count;
static S2 rlib_cb_stream_cb_set[RAYLIB_MAX_AUDIOSTREAMS];
static S8 rlib_cb_stream_fmt[RAYLIB_MAX_AUDIOSTREAMS];	// sampleSize | channels << 8

// audio processors: capture ring (interleaved stereo float) + call counter
static float rlib_cb_audio_cap[RLIB_CB_AUDIO_CAP];
static S8 rlib_cb_audio_cap_head;
static S8 rlib_cb_audio_cap_count;
static S8 rlib_cb_audio_proc_calls;
static S2 rlib_cb_stream_proc_set[RAYLIB_MAX_AUDIOSTREAMS];
static S2 rlib_cb_mixed_proc_set;

// callback helpers -----------------------------------------------------------

// copy a C string into a fixed buffer, always NUL terminated
static void rlib_cb_copy_name (U1 *dst, const char *src, S8 dstlen)
{
	S8 i ALIGN = 0;

	if (dstlen < 1) return;
	if (src == NULL)
	{
		dst[0] = '\0';
		return;
	}
	while ((i < dstlen - 1) && (src[i] != '\0'))
	{
		dst[i] = (U1) src[i];
		i++;
	}
	dst[i] = '\0';
}

static float rlib_audio_clamp (float v)
{
	if (v < -1.0f) return (-1.0f);
	if (v > 1.0f) return (1.0f);
	return (v);
}

// trace log callback ---------------------------------------------------------
// formats the message (the callback gets the format string and the va_list)
// and queues it for raylib_get_trace_log_message ()

static void rlib_trace_log_cb (int log_level, const char *text, va_list args)
{
	S8 slot ALIGN;

	if (text == NULL) return;

	pthread_mutex_lock (&rlib_cb_mutex);
	slot = rlib_cb_log_head;
	vsnprintf (rlib_cb_log[slot], RLIB_CB_LOG_MSG_LEN, text, args);
	rlib_cb_log_level[slot] = (S2) log_level;
	rlib_cb_log_head = (slot + 1) % RLIB_CB_LOG_MAX;
	if (rlib_cb_log_count < RLIB_CB_LOG_MAX) rlib_cb_log_count++;
	// when the ring is full the write slot IS the oldest entry, so the ring
	// keeps the newest RLIB_CB_LOG_MAX messages
	pthread_mutex_unlock (&rlib_cb_mutex);
}

// file IO callbacks ----------------------------------------------------------
// LoadFileDataCallback: records the request and returns the staged response.
// The buffer is allocated with MemAlloc (), because raylib releases it with
// UnloadFileData () (RL_FREE). Without staged data NULL is returned, which
// makes the raylib load function fail for this request.

static unsigned char *rlib_load_file_data_cb (const char *file_name, int *data_size)
{
	unsigned char *out = NULL;
	S8 size ALIGN = 0;

	pthread_mutex_lock (&rlib_cb_mutex);
	rlib_cb_copy_name (rlib_cb_load_data_name, file_name, RLIB_CB_PATH_LEN);
	rlib_cb_load_data_req = 1;

	if (rlib_cb_load_data_resp_staged && (rlib_cb_load_data_resp_size > 0))
	{
		size = rlib_cb_load_data_resp_size;
		out = MemAlloc ((unsigned int) size);
		if (out != NULL)
		{
			memcpy (out, rlib_cb_load_data_resp, size);
		}
		else
		{
			size = 0;
		}
	}
	pthread_mutex_unlock (&rlib_cb_mutex);

	if (data_size != NULL) *data_size = (int) size;
	return (out);
}

// SaveFileDataCallback: records the request (name + data copy) and reports
// the data as saved. Data bigger than RLIB_CB_FILEDATA_MAX is truncated.

static bool rlib_save_file_data_cb (const char *file_name, const void *data, int data_size)
{
	S8 size ALIGN;

	if (data_size < 0) data_size = 0;
	size = data_size;
	if (size > RLIB_CB_FILEDATA_MAX) size = RLIB_CB_FILEDATA_MAX;

	pthread_mutex_lock (&rlib_cb_mutex);
	rlib_cb_copy_name (rlib_cb_save_data_name, file_name, RLIB_CB_PATH_LEN);
	if ((data != NULL) && (size > 0))
	{
		memcpy (rlib_cb_save_data_buf, data, size);
	}
	rlib_cb_save_data_size = size;
	rlib_cb_save_data_req = 1;
	pthread_mutex_unlock (&rlib_cb_mutex);

	return (true);
}

// LoadFileTextCallback: records the request and returns the staged text.
// The buffer is allocated with MemAlloc (), because raylib releases it with
// UnloadFileText () (RL_FREE). Without staged text NULL is returned.

static char *rlib_load_file_text_cb (const char *file_name)
{
	char *out = NULL;
	S8 len ALIGN = 0;

	pthread_mutex_lock (&rlib_cb_mutex);
	rlib_cb_copy_name (rlib_cb_load_text_name, file_name, RLIB_CB_PATH_LEN);
	rlib_cb_load_text_req = 1;

	if (rlib_cb_load_text_resp_staged)
	{
		len = strlen_safe (rlib_cb_load_text_resp, RLIB_CB_FILETEXT_MAX - 1);
		out = MemAlloc ((unsigned int) len + 1);
		if (out != NULL)
		{
			memcpy (out, rlib_cb_load_text_resp, len + 1);
		}
	}
	pthread_mutex_unlock (&rlib_cb_mutex);

	return (out);
}

// SaveFileTextCallback: records the request (name + text copy) and reports
// the text as saved. Text bigger than RLIB_CB_FILETEXT_MAX is truncated.

static bool rlib_save_file_text_cb (const char *file_name, const char *text)
{
	S8 len ALIGN;

	pthread_mutex_lock (&rlib_cb_mutex);
	rlib_cb_copy_name (rlib_cb_save_text_name, file_name, RLIB_CB_PATH_LEN);
	if (text != NULL)
	{
		len = strlen_safe (text, RLIB_CB_FILETEXT_MAX - 1);
		memcpy (rlib_cb_save_text_buf, text, len);
		rlib_cb_save_text_buf[len] = '\0';
	}
	else
	{
		rlib_cb_save_text_buf[0] = '\0';
	}
	rlib_cb_save_text_req = 1;
	pthread_mutex_unlock (&rlib_cb_mutex);

	return (true);
}

// audio callbacks ------------------------------------------------------------
// SetAudioStreamCallback: fills the stream buffer from the sample stage.
// The AudioCallback signature carries no user data, so every audio stream
// slot gets its own trampoline that knows which stream is calling. The
// stream format (sample size and channels) is cached when the callback is
// installed, so the audio thread never touches the live AudioStream struct.

static void rlib_audio_stream_fill (S8 handle, void *buffer, unsigned int frames)
{
	unsigned int sample_size, channels, i;
	float l, r;
	S8 slot ALIGN;

	if (handle < 0 || handle >= RAYLIB_MAX_AUDIOSTREAMS) return;

	sample_size = (unsigned int) (rlib_cb_stream_fmt[handle] & 0xff);
	channels = (unsigned int) ((rlib_cb_stream_fmt[handle] >> 8) & 0xff);

	if ((sample_size != 8) && (sample_size != 16) && (sample_size != 32)) return;
	if (channels < 1) return;

	// unknown channel positions start silent (mono only uses channel 0)
	memset (buffer, 0, (size_t) frames * channels * (sample_size / 8));

	pthread_mutex_lock (&rlib_cb_mutex);
	for (i = 0; i < frames; i++)
	{
		// pop the next stereo frame, silence on underrun
		if (rlib_cb_audio_stage_count > 0)
		{
			slot = (rlib_cb_audio_stage_head - rlib_cb_audio_stage_count + RLIB_CB_AUDIO_STAGE) % RLIB_CB_AUDIO_STAGE;
			l = rlib_cb_audio_stage[slot * 2];
			r = rlib_cb_audio_stage[slot * 2 + 1];
			rlib_cb_audio_stage_count--;
		}
		else
		{
			l = 0.0f;
			r = 0.0f;
		}

		switch (sample_size)
		{
			case 8:
			{
				unsigned char *out = (unsigned char *) buffer;
				out[i * channels] = (unsigned char) ((rlib_audio_clamp (l) + 1.0f) * 127.5f);
				if (channels > 1) out[i * channels + 1] = (unsigned char) ((rlib_audio_clamp (r) + 1.0f) * 127.5f);
				break;
			}
			case 16:
			{
				short *out = (short *) buffer;
				out[i * channels] = (short) (rlib_audio_clamp (l) * 32767.0f);
				if (channels > 1) out[i * channels + 1] = (short) (rlib_audio_clamp (r) * 32767.0f);
				break;
			}
			case 32:
			{
				float *out = (float *) buffer;
				out[i * channels] = l;
				if (channels > 1) out[i * channels + 1] = r;
				break;
			}
		}
	}
	pthread_mutex_unlock (&rlib_cb_mutex);
}

#define RLIB_STREAM_CB(N) \
	static void rlib_stream_cb_##N (void *buffer, unsigned int frames) \
	{ \
		rlib_audio_stream_fill (N, buffer, frames); \
	}

RLIB_STREAM_CB (0)
RLIB_STREAM_CB (1)
RLIB_STREAM_CB (2)
RLIB_STREAM_CB (3)
RLIB_STREAM_CB (4)
RLIB_STREAM_CB (5)
RLIB_STREAM_CB (6)
RLIB_STREAM_CB (7)
RLIB_STREAM_CB (8)
RLIB_STREAM_CB (9)
RLIB_STREAM_CB (10)
RLIB_STREAM_CB (11)
RLIB_STREAM_CB (12)
RLIB_STREAM_CB (13)
RLIB_STREAM_CB (14)
RLIB_STREAM_CB (15)
RLIB_STREAM_CB (16)
RLIB_STREAM_CB (17)
RLIB_STREAM_CB (18)
RLIB_STREAM_CB (19)
RLIB_STREAM_CB (20)
RLIB_STREAM_CB (21)
RLIB_STREAM_CB (22)
RLIB_STREAM_CB (23)
RLIB_STREAM_CB (24)
RLIB_STREAM_CB (25)
RLIB_STREAM_CB (26)
RLIB_STREAM_CB (27)
RLIB_STREAM_CB (28)
RLIB_STREAM_CB (29)
RLIB_STREAM_CB (30)
RLIB_STREAM_CB (31)
RLIB_STREAM_CB (32)
RLIB_STREAM_CB (33)
RLIB_STREAM_CB (34)
RLIB_STREAM_CB (35)
RLIB_STREAM_CB (36)
RLIB_STREAM_CB (37)
RLIB_STREAM_CB (38)
RLIB_STREAM_CB (39)
RLIB_STREAM_CB (40)
RLIB_STREAM_CB (41)
RLIB_STREAM_CB (42)
RLIB_STREAM_CB (43)
RLIB_STREAM_CB (44)
RLIB_STREAM_CB (45)
RLIB_STREAM_CB (46)
RLIB_STREAM_CB (47)
RLIB_STREAM_CB (48)
RLIB_STREAM_CB (49)
RLIB_STREAM_CB (50)
RLIB_STREAM_CB (51)
RLIB_STREAM_CB (52)
RLIB_STREAM_CB (53)
RLIB_STREAM_CB (54)
RLIB_STREAM_CB (55)
RLIB_STREAM_CB (56)
RLIB_STREAM_CB (57)
RLIB_STREAM_CB (58)
RLIB_STREAM_CB (59)
RLIB_STREAM_CB (60)
RLIB_STREAM_CB (61)
RLIB_STREAM_CB (62)
RLIB_STREAM_CB (63)

static AudioCallback rlib_stream_cbs[RAYLIB_MAX_AUDIOSTREAMS] =
{
	rlib_stream_cb_0, rlib_stream_cb_1, rlib_stream_cb_2, rlib_stream_cb_3,
	rlib_stream_cb_4, rlib_stream_cb_5, rlib_stream_cb_6, rlib_stream_cb_7,
	rlib_stream_cb_8, rlib_stream_cb_9, rlib_stream_cb_10, rlib_stream_cb_11,
	rlib_stream_cb_12, rlib_stream_cb_13, rlib_stream_cb_14, rlib_stream_cb_15,
	rlib_stream_cb_16, rlib_stream_cb_17, rlib_stream_cb_18, rlib_stream_cb_19,
	rlib_stream_cb_20, rlib_stream_cb_21, rlib_stream_cb_22, rlib_stream_cb_23,
	rlib_stream_cb_24, rlib_stream_cb_25, rlib_stream_cb_26, rlib_stream_cb_27,
	rlib_stream_cb_28, rlib_stream_cb_29, rlib_stream_cb_30, rlib_stream_cb_31,
	rlib_stream_cb_32, rlib_stream_cb_33, rlib_stream_cb_34, rlib_stream_cb_35,
	rlib_stream_cb_36, rlib_stream_cb_37, rlib_stream_cb_38, rlib_stream_cb_39,
	rlib_stream_cb_40, rlib_stream_cb_41, rlib_stream_cb_42, rlib_stream_cb_43,
	rlib_stream_cb_44, rlib_stream_cb_45, rlib_stream_cb_46, rlib_stream_cb_47,
	rlib_stream_cb_48, rlib_stream_cb_49, rlib_stream_cb_50, rlib_stream_cb_51,
	rlib_stream_cb_52, rlib_stream_cb_53, rlib_stream_cb_54, rlib_stream_cb_55,
	rlib_stream_cb_56, rlib_stream_cb_57, rlib_stream_cb_58, rlib_stream_cb_59,
	rlib_stream_cb_60, rlib_stream_cb_61, rlib_stream_cb_62, rlib_stream_cb_63
};

// AudioProcessCallback (AttachAudioStreamProcessor, AttachAudioMixedProcessor):
// copies the float stereo frames it receives into the capture ring for
// raylib_get_audio_processor_capture (). One shared processor function is
// used for every stream and for the mixed pipeline, so it can be detached
// again by passing the same function to the detach wrappers.

static void rlib_audio_capture_cb (void *buffer, unsigned int frames)
{
	float *in = (float *) buffer;
	S8 i, n ALIGN;

	if (in == NULL) return;

	n = (S8) frames * 2;
	if (n > RLIB_CB_AUDIO_CAP)
	{
		// only the newest part fits into the ring
		in += n - RLIB_CB_AUDIO_CAP;
		n = RLIB_CB_AUDIO_CAP;
	}

	pthread_mutex_lock (&rlib_cb_mutex);
	for (i = 0; i < n; i++)
	{
		rlib_cb_audio_cap[rlib_cb_audio_cap_head] = in[i];
		rlib_cb_audio_cap_head = (rlib_cb_audio_cap_head + 1) % RLIB_CB_AUDIO_CAP;
		if (rlib_cb_audio_cap_count < RLIB_CB_AUDIO_CAP) rlib_cb_audio_cap_count++;
	}
	rlib_cb_audio_proc_calls++;
	pthread_mutex_unlock (&rlib_cb_mutex);
}

// ---------------------------------------------------------------------------
// trace log wrappers
// ---------------------------------------------------------------------------

U1 *raylib_set_trace_log_callback (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: enable (1 = install, 0 = remove)
	// return: nothing
	S8 enable ALIGN;

	sp = stpopi ((U1 *) &enable, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	SetTraceLogCallback (enable ? rlib_trace_log_cb : NULL);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_get_trace_log_count (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// return: number of queued trace log messages
	S8 ret ALIGN;

	pthread_mutex_lock (&rlib_cb_mutex);
	ret = rlib_cb_log_count;
	pthread_mutex_unlock (&rlib_cb_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_get_trace_log_message (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: msg_index (0 = oldest), out_addr
	// return: message string written to out_addr
	S8 msg_index ALIGN;
	S8 out_addr ALIGN;
	char msg_buf[RLIB_CB_LOG_MSG_LEN];
	S8 slot ALIGN;
	S8 len ALIGN;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &msg_index, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_cb_mutex);
	if ((msg_index < 0) || (msg_index >= rlib_cb_log_count))
	{
		pthread_mutex_unlock (&rlib_cb_mutex);
		printf ("raylib_get_trace_log_message: ERROR message index out of range!\n");
		return (NULL);
	}
	slot = (rlib_cb_log_head - rlib_cb_log_count + msg_index + RLIB_CB_LOG_MAX) % RLIB_CB_LOG_MAX;
	strcpy (msg_buf, rlib_cb_log[slot]);
	len = strlen_safe (msg_buf, RLIB_CB_LOG_MSG_LEN - 1);
	pthread_mutex_unlock (&rlib_cb_mutex);

	if (memory_bounds (out_addr, len) != 0)
	{
		printf ("raylib_get_trace_log_message: ERROR memory bounds check failed!\n");
		return (NULL);
	}
	if (len > 0) memcpy (&data[out_addr], msg_buf, len);
	data[out_addr + len] = '\0';

	return (sp);
}

U1 *raylib_get_trace_log_level (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: msg_index (0 = oldest)
	// return: log level (LOG_ALL 0 ... LOG_NONE 7)
	S8 msg_index ALIGN;
	S8 ret ALIGN;
	S8 slot ALIGN;

	sp = stpopi ((U1 *) &msg_index, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_cb_mutex);
	if ((msg_index < 0) || (msg_index >= rlib_cb_log_count))
	{
		pthread_mutex_unlock (&rlib_cb_mutex);
		printf ("raylib_get_trace_log_level: ERROR message index out of range!\n");
		return (NULL);
	}
	slot = (rlib_cb_log_head - rlib_cb_log_count + msg_index + RLIB_CB_LOG_MAX) % RLIB_CB_LOG_MAX;
	ret = rlib_cb_log_level[slot];
	pthread_mutex_unlock (&rlib_cb_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_clear_trace_log (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: none
	// return: nothing (drops all queued trace log messages)
	pthread_mutex_lock (&rlib_cb_mutex);
	rlib_cb_log_head = 0;
	rlib_cb_log_count = 0;
	pthread_mutex_unlock (&rlib_cb_mutex);

	return (sp);
}

// ---------------------------------------------------------------------------
// file data callback wrappers
// ---------------------------------------------------------------------------

U1 *raylib_set_load_file_data_callback (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: enable (1 = install, 0 = remove)
	// return: nothing
	S8 enable ALIGN;

	sp = stpopi ((U1 *) &enable, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	SetLoadFileDataCallback (enable ? rlib_load_file_data_cb : NULL);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_set_load_file_data_response (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: data_addr, data_size
	// return: 1 ok, 0 error
	//
	// Stages the binary data the load callback hands to raylib. The staging
	// stays active until it is replaced, data_size 0 clears it. LoadFileData
	// is synchronous, so the response has to be staged BEFORE the raylib call
	// that triggers the load request.
	S8 data_addr ALIGN;
	S8 data_size ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &data_size, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &data_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if ((data_size < 0) || (data_size > RLIB_CB_FILEDATA_MAX))
	{
		printf ("raylib_set_load_file_data_response: ERROR data size out of range!\n");
		sp = stpushi (0, sp, sp_bottom);
		return (sp);
	}
	if (data_size > 0)
	{
		if (memory_bounds (data_addr, data_size - 1) != 0)
		{
			printf ("raylib_set_load_file_data_response: ERROR memory bounds check failed!\n");
			return (NULL);
		}
	}

	pthread_mutex_lock (&rlib_cb_mutex);
	if (data_size > 0)
	{
		memcpy (rlib_cb_load_data_resp, &data[data_addr], data_size);
		rlib_cb_load_data_resp_size = data_size;
		rlib_cb_load_data_resp_staged = 1;
	}
	else
	{
		rlib_cb_load_data_resp_size = 0;
		rlib_cb_load_data_resp_staged = 0;
	}
	pthread_mutex_unlock (&rlib_cb_mutex);

	ret = 1;
	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_get_load_file_data_request (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: out_addr
	// return: 1 if a load request is pending (file name written to out_addr),
	//         0 otherwise; the request flag is cleared by the call
	S8 out_addr ALIGN;
	char name_buf[RLIB_CB_PATH_LEN];
	S8 len ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_cb_mutex);
	if (rlib_cb_load_data_req == 0)
	{
		pthread_mutex_unlock (&rlib_cb_mutex);
		ret = 0;
		sp = stpushi (ret, sp, sp_bottom);
		return (sp);
	}
	rlib_cb_copy_name ((U1 *) name_buf, (const char *) rlib_cb_load_data_name, RLIB_CB_PATH_LEN);
	rlib_cb_load_data_req = 0;
	len = strlen_safe (name_buf, RLIB_CB_PATH_LEN - 1);
	pthread_mutex_unlock (&rlib_cb_mutex);

	if (memory_bounds (out_addr, len) != 0)
	{
		printf ("raylib_get_load_file_data_request: ERROR memory bounds check failed!\n");
		return (NULL);
	}
	if (len > 0) memcpy (&data[out_addr], name_buf, len);
	data[out_addr + len] = '\0';

	ret = 1;
	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_set_save_file_data_callback (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: enable (1 = install, 0 = remove)
	// return: nothing
	S8 enable ALIGN;

	sp = stpopi ((U1 *) &enable, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	SetSaveFileDataCallback (enable ? rlib_save_file_data_cb : NULL);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_get_save_file_data_request (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: out_name_addr, out_data_addr
	// return: data size written to out_data_addr (bytes), -1 if no save
	//         request is pending; the request flag is cleared by the call
	S8 out_name_addr ALIGN;
	S8 out_data_addr ALIGN;
	S8 name_len ALIGN;
	S8 size ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &out_data_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &out_name_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_cb_mutex);
	if (rlib_cb_save_data_req == 0)
	{
		pthread_mutex_unlock (&rlib_cb_mutex);
		ret = -1;
		sp = stpushi (ret, sp, sp_bottom);
		return (sp);
	}
	name_len = strlen_safe ((const char *) rlib_cb_save_data_name, RLIB_CB_PATH_LEN - 1);
	size = rlib_cb_save_data_size;

	if (memory_bounds (out_name_addr, name_len) != 0)
	{
		pthread_mutex_unlock (&rlib_cb_mutex);
		printf ("raylib_get_save_file_data_request: ERROR memory bounds check failed (name)!\n");
		return (NULL);
	}
	if (size > 0)
	{
		if (memory_bounds (out_data_addr, size - 1) != 0)
		{
			pthread_mutex_unlock (&rlib_cb_mutex);
			printf ("raylib_get_save_file_data_request: ERROR memory bounds check failed (data)!\n");
			return (NULL);
		}
	}

	if (name_len > 0) memcpy (&data[out_name_addr], rlib_cb_save_data_name, name_len);
	data[out_name_addr + name_len] = '\0';
	if (size > 0) memcpy (&data[out_data_addr], rlib_cb_save_data_buf, size);
	rlib_cb_save_data_req = 0;
	pthread_mutex_unlock (&rlib_cb_mutex);

	ret = size;
	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

// ---------------------------------------------------------------------------
// file text callback wrappers
// ---------------------------------------------------------------------------

U1 *raylib_set_load_file_text_callback (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: enable (1 = install, 0 = remove)
	// return: nothing
	S8 enable ALIGN;

	sp = stpopi ((U1 *) &enable, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	SetLoadFileTextCallback (enable ? rlib_load_file_text_cb : NULL);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_set_load_file_text_response (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: text_addr
	// return: 1 ok, 0 error
	//
	// Stages the NUL terminated text the load callback hands to raylib. The
	// staging stays active until it is replaced; call with an empty string
	// to clear it. LoadFileText is synchronous, so the response has to be
	// staged BEFORE the raylib call that triggers the load request.
	S8 text_addr ALIGN;
	S8 slen ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &text_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (text_addr < 0)
	{
		printf ("raylib_set_load_file_text_response: ERROR illegal address!\n");
		return (NULL);
	}

	slen = strlen_safe ((const char *) &data[text_addr], RLIB_CB_FILETEXT_MAX - 1);
	// slen counts the characters, the NUL byte sits at text_addr + slen
	if (memory_bounds (text_addr, slen) != 0)
	{
		printf ("raylib_set_load_file_text_response: ERROR memory bounds check failed!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_cb_mutex);
	memcpy (rlib_cb_load_text_resp, &data[text_addr], slen + 1);
	rlib_cb_load_text_resp_staged = 1;
	pthread_mutex_unlock (&rlib_cb_mutex);

	ret = 1;
	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_get_load_file_text_request (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: out_addr
	// return: 1 if a load request is pending (file name written to out_addr),
	//         0 otherwise; the request flag is cleared by the call
	S8 out_addr ALIGN;
	char name_buf[RLIB_CB_PATH_LEN];
	S8 len ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_cb_mutex);
	if (rlib_cb_load_text_req == 0)
	{
		pthread_mutex_unlock (&rlib_cb_mutex);
		ret = 0;
		sp = stpushi (ret, sp, sp_bottom);
		return (sp);
	}
	rlib_cb_copy_name ((U1 *) name_buf, (const char *) rlib_cb_load_text_name, RLIB_CB_PATH_LEN);
	rlib_cb_load_text_req = 0;
	len = strlen_safe (name_buf, RLIB_CB_PATH_LEN - 1);
	pthread_mutex_unlock (&rlib_cb_mutex);

	if (memory_bounds (out_addr, len) != 0)
	{
		printf ("raylib_get_load_file_text_request: ERROR memory bounds check failed!\n");
		return (NULL);
	}
	if (len > 0) memcpy (&data[out_addr], name_buf, len);
	data[out_addr + len] = '\0';

	ret = 1;
	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_set_save_file_text_callback (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: enable (1 = install, 0 = remove)
	// return: nothing
	S8 enable ALIGN;

	sp = stpopi ((U1 *) &enable, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	SetSaveFileTextCallback (enable ? rlib_save_file_text_cb : NULL);
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_get_save_file_text_request (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: out_name_addr, out_text_addr
	// return: 1 if a save request is pending (file name and text written to
	//         out_name_addr / out_text_addr), 0 otherwise; the request flag
	//         is cleared by the call
	S8 out_name_addr ALIGN;
	S8 out_text_addr ALIGN;
	S8 name_len ALIGN;
	S8 text_len ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &out_text_addr, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &out_name_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_cb_mutex);
	if (rlib_cb_save_text_req == 0)
	{
		pthread_mutex_unlock (&rlib_cb_mutex);
		ret = 0;
		sp = stpushi (ret, sp, sp_bottom);
		return (sp);
	}
	name_len = strlen_safe ((const char *) rlib_cb_save_text_name, RLIB_CB_PATH_LEN - 1);
	text_len = strlen_safe (rlib_cb_save_text_buf, RLIB_CB_FILETEXT_MAX - 1);

	if (memory_bounds (out_name_addr, name_len) != 0)
	{
		pthread_mutex_unlock (&rlib_cb_mutex);
		printf ("raylib_get_save_file_text_request: ERROR memory bounds check failed (name)!\n");
		return (NULL);
	}
	if (memory_bounds (out_text_addr, text_len) != 0)
	{
		pthread_mutex_unlock (&rlib_cb_mutex);
		printf ("raylib_get_save_file_text_request: ERROR memory bounds check failed (text)!\n");
		return (NULL);
	}

	if (name_len > 0) memcpy (&data[out_name_addr], rlib_cb_save_text_name, name_len);
	data[out_name_addr + name_len] = '\0';
	if (text_len > 0) memcpy (&data[out_text_addr], rlib_cb_save_text_buf, text_len);
	data[out_text_addr + text_len] = '\0';
	rlib_cb_save_text_req = 0;
	pthread_mutex_unlock (&rlib_cb_mutex);

	ret = 1;
	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

// ---------------------------------------------------------------------------
// audio callback wrappers
// ---------------------------------------------------------------------------

U1 *raylib_set_audio_stream_callback (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: stream_handle, enable (1 = fill from sample stage, 0 = remove)
	// return: nothing
	//
	// With enable 1 raylib requests audio data from the callback on the audio
	// thread; the frames are pulled from the sample stage (raylib_stage_audio_
	// data), silence is played when the stage runs dry. Disable the callback
	// before unloading the stream.
	S8 stream_handle ALIGN;
	S8 enable ALIGN;

	sp = stpopi ((U1 *) &enable, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &stream_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if ((stream_handle < 0) || (stream_handle >= RAYLIB_MAX_AUDIOSTREAMS) || (raylib_audiostreams_used[stream_handle] == 0))
	{
		printf ("raylib_set_audio_stream_callback: ERROR invalid audio stream handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	if (enable)
	{
		rlib_cb_stream_fmt[stream_handle] = (S8) raylib_audiostreams[stream_handle].sampleSize |
			((S8) raylib_audiostreams[stream_handle].channels << 8);
		SetAudioStreamCallback (raylib_audiostreams[stream_handle], rlib_stream_cbs[stream_handle]);
		rlib_cb_stream_cb_set[stream_handle] = 1;
	}
	else
	{
		SetAudioStreamCallback (raylib_audiostreams[stream_handle], NULL);
		rlib_cb_stream_cb_set[stream_handle] = 0;
	}
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_stage_audio_data (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: data_addr, sample_count
	// return: number of samples accepted into the stage
	//
	// Copies interleaved stereo float pairs (L,R,L,R...) from VM data memory
	// (F8 array) into the sample stage that feeds the audio stream callback.
	// A trailing unpaired sample is dropped. Samples are dropped when the
	// stage is full, poll raylib_get_audio_stage_count () to avoid that.
	S8 data_addr ALIGN;
	S8 sample_count ALIGN;
	S8 i ALIGN;
	S8 accepted ALIGN;
	F8 ld ALIGN, rd ALIGN;
	float l, r;

	sp = stpopi ((U1 *) &sample_count, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &data_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (sample_count < 0)
	{
		printf ("raylib_stage_audio_data: ERROR sample count out of range!\n");
		return (NULL);
	}
	if (sample_count > 0)
	{
		if (memory_bounds (data_addr, (sample_count - 1) * 8) != 0)
		{
			printf ("raylib_stage_audio_data: ERROR memory bounds check failed!\n");
			return (NULL);
		}
	}

	accepted = 0;
	pthread_mutex_lock (&rlib_cb_mutex);
	for (i = 0; i + 1 < sample_count; i += 2)
	{
		if (rlib_cb_audio_stage_count >= RLIB_CB_AUDIO_STAGE) break;

		memcpy (&ld, &data[data_addr + i * 8], sizeof (F8));
		memcpy (&rd, &data[data_addr + i * 8 + 8], sizeof (F8));
		l = (float) ld;
		r = (float) rd;

		rlib_cb_audio_stage[rlib_cb_audio_stage_head * 2] = l;
		rlib_cb_audio_stage[rlib_cb_audio_stage_head * 2 + 1] = r;
		rlib_cb_audio_stage_head = (rlib_cb_audio_stage_head + 1) % RLIB_CB_AUDIO_STAGE;
		if (rlib_cb_audio_stage_count < RLIB_CB_AUDIO_STAGE) rlib_cb_audio_stage_count++;
		accepted += 2;
	}
	pthread_mutex_unlock (&rlib_cb_mutex);

	sp = stpushi (accepted, sp, sp_bottom);
	return (sp);
}

U1 *raylib_get_audio_stage_count (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// return: number of samples (stereo pairs x 2) waiting in the stage
	S8 ret ALIGN;

	pthread_mutex_lock (&rlib_cb_mutex);
	ret = rlib_cb_audio_stage_count * 2;
	pthread_mutex_unlock (&rlib_cb_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_attach_audio_stream_processor (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: stream_handle, enable (1 = attach, 0 = detach)
	// return: nothing
	//
	// The attached processor captures the float stereo frames it receives
	// (frames x 2 samples) into the capture ring of
	// raylib_get_audio_processor_capture ().
	S8 stream_handle ALIGN;
	S8 enable ALIGN;

	sp = stpopi ((U1 *) &enable, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &stream_handle, sp, sp_top);
	if (sp == NULL) return (NULL);

	if ((stream_handle < 0) || (stream_handle >= RAYLIB_MAX_AUDIOSTREAMS) || (raylib_audiostreams_used[stream_handle] == 0))
	{
		printf ("raylib_attach_audio_stream_processor: ERROR invalid audio stream handle!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_mutex);
	if (enable)
	{
		if (rlib_cb_stream_proc_set[stream_handle] == 0)
		{
			AttachAudioStreamProcessor (raylib_audiostreams[stream_handle], rlib_audio_capture_cb);
			rlib_cb_stream_proc_set[stream_handle] = 1;
		}
	}
	else
	{
		if (rlib_cb_stream_proc_set[stream_handle] != 0)
		{
			DetachAudioStreamProcessor (raylib_audiostreams[stream_handle], rlib_audio_capture_cb);
			rlib_cb_stream_proc_set[stream_handle] = 0;
		}
	}
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_attach_audio_mixed_processor (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: enable (1 = attach, 0 = detach)
	// return: nothing
	//
	// The attached processor captures the float stereo frames of the whole
	// audio pipeline (frames x 2 samples) into the capture ring of
	// raylib_get_audio_processor_capture ().
	S8 enable ALIGN;

	sp = stpopi ((U1 *) &enable, sp, sp_top);
	if (sp == NULL) return (NULL);

	pthread_mutex_lock (&rlib_mutex);
	if (enable)
	{
		if (rlib_cb_mixed_proc_set == 0)
		{
			AttachAudioMixedProcessor (rlib_audio_capture_cb);
			rlib_cb_mixed_proc_set = 1;
		}
	}
	else
	{
		if (rlib_cb_mixed_proc_set != 0)
		{
			DetachAudioMixedProcessor (rlib_audio_capture_cb);
			rlib_cb_mixed_proc_set = 0;
		}
	}
	pthread_mutex_unlock (&rlib_mutex);

	return (sp);
}

U1 *raylib_get_audio_processor_capture (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: out_addr, max_samples
	// return: number of samples written to out_addr (oldest captured first,
	//         interleaved stereo float pairs as F8 values)
	S8 out_addr ALIGN;
	S8 max_samples ALIGN;
	S8 n ALIGN;
	S8 i, idx ALIGN;
	F8 d ALIGN;

	sp = stpopi ((U1 *) &max_samples, sp, sp_top);
	if (sp == NULL) return (NULL);
	sp = stpopi ((U1 *) &out_addr, sp, sp_top);
	if (sp == NULL) return (NULL);

	if (max_samples < 0)
	{
		printf ("raylib_get_audio_processor_capture: ERROR sample count out of range!\n");
		return (NULL);
	}

	pthread_mutex_lock (&rlib_cb_mutex);
	n = max_samples;
	if (n > rlib_cb_audio_cap_count) n = rlib_cb_audio_cap_count;

	if (n > 0)
	{
		if (memory_bounds (out_addr, (n - 1) * 8) != 0)
		{
			pthread_mutex_unlock (&rlib_cb_mutex);
			printf ("raylib_get_audio_processor_capture: ERROR memory bounds check failed!\n");
			return (NULL);
		}

		idx = (rlib_cb_audio_cap_head - rlib_cb_audio_cap_count + RLIB_CB_AUDIO_CAP) % RLIB_CB_AUDIO_CAP;
		for (i = 0; i < n; i++)
		{
			d = (F8) rlib_cb_audio_cap[(idx + i) % RLIB_CB_AUDIO_CAP];
			memcpy (&data[out_addr + i * 8], &d, sizeof (F8));
		}
	}
	pthread_mutex_unlock (&rlib_cb_mutex);

	sp = stpushi (n, sp, sp_bottom);
	return (sp);
}

U1 *raylib_get_audio_processor_invocations (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// return: number of times the attached audio processors were invoked
	//         since the last raylib_clear_audio_capture ()
	S8 ret ALIGN;

	pthread_mutex_lock (&rlib_cb_mutex);
	ret = rlib_cb_audio_proc_calls;
	pthread_mutex_unlock (&rlib_cb_mutex);

	sp = stpushi (ret, sp, sp_bottom);
	return (sp);
}

U1 *raylib_clear_audio_capture (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: none
	// return: nothing (drops all captured samples and the call counter)
	pthread_mutex_lock (&rlib_cb_mutex);
	rlib_cb_audio_cap_head = 0;
	rlib_cb_audio_cap_count = 0;
	rlib_cb_audio_proc_calls = 0;
	pthread_mutex_unlock (&rlib_cb_mutex);

	return (sp);
}

// raylib module end marker
