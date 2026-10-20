/*
 * This file vecdb.c is part of L1vm.
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

/*
 * vecdb.c - L1VM wrapper for the embedded "vector-db" database.
 *
 * The database library lives in "vm/modules/vector-db" and is compiled
 * directly into this module (no libvecdb.so runtime dependency).
 *
 * A database handle is exposed to L1VM as a small non-negative index into
 * a static table of "vecdb *" handles.  Vectors travel between L1VM and the
 * library as double arrays (L1VM has no float type): the wrapper converts
 * them to/from float.  Payloads are opaque byte arrays and text is a
 * NUL terminated string.
 *
 * All exported wrappers are prefixed with "l1_vecdb_" so that they never
 * collide with the real library API symbols ("vecdb_*") which are linked
 * into the same shared object.
 */

#include "../../../include/global.h"
#include "../../../include/stack.h"

#include "../vector-db/include/vecdb.h"

// protos
S2 memory_bounds (S8 start, S8 offset_access);
size_t strlen_safe (const char * str, S8 maxlen);

// Windows MSYS2 fix for false positive bounds error:
#if _WIN32
#undef BOUNDSCHECK
#define BOUNDSCHECK 0
#endif

// limits
#define VECDB_MOD_MAX_DB       64
#define VECDB_MOD_MAX_MATCHES  1024
#define VECDB_MOD_MAX_DIM      (1 << 20)

// database handles, indexed by the L1VM visible handle
static vecdb *vecdb_db[VECDB_MOD_MAX_DB];

// cache of the last search results, accessed by the match_* functions
static vecdb_match vecdb_matches[VECDB_MOD_MAX_MATCHES];
static S8 vecdb_match_count = 0;

// module memory bounds info (the runtime uses its own exported copy)
struct data_info data_info[MAXDATAINFO];
S8 data_info_ind;

S2 init_memory_bounds (struct data_info *data_info_orig, S8 data_info_ind_orig)
{
	memcpy (&data_info, &data_info_orig, sizeof (data_info_orig));
	data_info_ind = data_info_ind_orig;

	return (0);
}

size_t strlen_safe (const char *str, S8 maxlen)
{
	 S8 ALIGN i = 0;

	 while (1)
	 {
		if (str[i] != '\0')
		{
			i++;
		}
		else
		{
			return (i);
		}

		// if maxlen < 0, then don't check for length overflow!
		if (maxlen > 0)
		{
			if (i >= maxlen)
			{
				return (0);
			}
		}
	}
}

// stack helpers --------------------------------------------------------
#define POP_I(v)  do { sp = stpopi ((U1 *) &(v), sp, sp_top); if (sp == NULL) { printf ("%s: ERROR: stack corrupt!\n", __func__); return (NULL); } } while (0)
#define POP_D(v)  do { sp = stpopd ((U1 *) &(v), sp, sp_top); if (sp == NULL) { printf ("%s: ERROR: stack corrupt!\n", __func__); return (NULL); } } while (0)
#define PUSH_I(v) do { sp = stpushi ((S8) (v), sp, sp_bottom); if (sp == NULL) { printf ("%s: ERROR: stack corrupt!\n", __func__); return (NULL); } } while (0)
#define PUSH_D(v) do { sp = stpushd ((F8) (v), sp, sp_bottom); if (sp == NULL) { printf ("%s: ERROR: stack corrupt!\n", __func__); return (NULL); } } while (0)

// helpers --------------------------------------------------------------
static S8 mod_find_slot (void)
{
	S8 i ALIGN;

	for (i = 0; i < VECDB_MOD_MAX_DB; i++)
	{
		if (vecdb_db[i] == NULL)
		{
			return (i);
		}
	}
	return (-1);
}

static vecdb *mod_get_db (S8 handle, const char *fn)
{
	if (handle < 0 || handle >= VECDB_MOD_MAX_DB || vecdb_db[handle] == NULL)
	{
		printf ("%s: ERROR: invalid database handle: %lli!\n", fn, handle);
		return (NULL);
	}
	return (vecdb_db[handle]);
}

// get the length of an L1VM string, with bounds check
static S8 mod_string_len (U1 *data, S8 addr, const char *fn)
{
	S8 len ALIGN;

	if (addr <= 0)
	{
		return (0);
	}

	len = (S8) strlen_safe ((const char *) &data[addr], STRINGMOD_MAXSTRLEN);

	#if BOUNDSCHECK
	if (memory_bounds (addr, len) != 0)
	{
		printf ("%s: ERROR: string address overflow: %lli!\n", fn, addr);
		return (-1);
	}
	#endif

	return (len);
}

// copy L1VM doubles into a float array. returns the number of floats, or -1 on error
static S8 data_get_floats (U1 *data, S8 addr, S8 dim, float *out, const char *fn)
{
	S8 i ALIGN;

	if (addr <= 0 || dim <= 0)
	{
		return (0);
	}

	if (dim > VECDB_MOD_MAX_DIM)
	{
		printf ("%s: ERROR: vector too long: %lli!\n", fn, dim);
		return (-1);
	}

	#if BOUNDSCHECK
	// l1vm memory_bounds() takes the offset of the last accessed element
	if (memory_bounds (addr, (dim - 1) * (S8) sizeof (F8)) != 0)
	{
		printf ("%s: ERROR: vector address overflow: %lli!\n", fn, addr);
		return (-1);
	}
	#endif

	for (i = 0; i < dim; i++)
	{
		F8 d ALIGN;

		memcpy (&d, &data[addr + i * (S8) sizeof (F8)], sizeof (F8));
		out[i] = (float) d;
	}
	return (dim);
}

// copy floats into L1VM doubles. returns the number of floats written, or -1 on error
static S8 data_put_floats (U1 *data, S8 addr, S8 maxdim, const float *in, S8 dim, const char *fn)
{
	S8 i ALIGN;
	S8 n ALIGN;

	if (addr <= 0 || maxdim <= 0 || in == NULL)
	{
		return (0);
	}

	n = (dim < maxdim) ? dim : maxdim;
	if (n <= 0)
	{
		return (0);
	}

	#if BOUNDSCHECK
	// l1vm memory_bounds() takes the offset of the last accessed element
	if (memory_bounds (addr, (n - 1) * (S8) sizeof (F8)) != 0)
	{
		printf ("%s: ERROR: dest vector overflow: %lli!\n", fn, addr);
		return (-1);
	}
	#endif

	for (i = 0; i < n; i++)
	{
		F8 d ALIGN = (F8) in[i];

		memcpy (&data[addr + i * (S8) sizeof (F8)], &d, sizeof (F8));
	}
	return (n);
}

// copy a C string into L1VM memory. returns 0, or -1 on error
static S8 data_put_string (U1 *data, S8 addr, S8 maxlen, const char *s, const char *fn)
{
	S8 len ALIGN;

	if (addr <= 0 || maxlen <= 0)
	{
		return (-1);
	}

	if (s == NULL)
	{
		s = "";
	}

	len = (S8) strlen (s);
	if (len >= maxlen)
	{
		len = maxlen - 1;
	}

	#if BOUNDSCHECK
	// l1vm memory_bounds() takes the offset of the last accessed byte (the NUL)
	if (memory_bounds (addr, len) != 0)
	{
		printf ("%s: ERROR: dest string overflow: %lli!\n", fn, addr);
		return (-1);
	}
	#endif

	memcpy (&data[addr], s, len);
	data[addr + len] = '\0';

	return (0);
}

// copy bytes into L1VM memory. returns the number of bytes copied, or -1 on error
static S8 data_put_bytes (U1 *data, S8 addr, S8 maxlen, const void *src, S8 len, const char *fn)
{
	S8 n ALIGN;

	if (addr <= 0 || maxlen <= 0)
	{
		return (-1);
	}

	n = (len < maxlen) ? len : maxlen;

	if (n > 0)
	{
		#if BOUNDSCHECK
		// l1vm memory_bounds() takes the offset of the last accessed byte
		if (memory_bounds (addr, n - 1) != 0)
		{
			printf ("%s: ERROR: dest payload overflow: %lli!\n", fn, addr);
			return (-1);
		}
		#endif

		memcpy (&data[addr], src, n);
	}
	return (n);
}

// lifecycle ------------------------------------------------------------

// (pathaddr dim metric index :vecdb_open !)
// pushes: handle, status (status on top)
U1 *l1_vecdb_open (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 pathaddr ALIGN;
	S8 dim ALIGN;
	S8 metric ALIGN;
	S8 index ALIGN;
	S8 handle ALIGN;
	S8 status ALIGN;

	vecdb_options opt;
	vecdb *db;

	POP_I (index);
	POP_I (metric);
	POP_I (dim);
	POP_I (pathaddr);

	if (pathaddr <= 0 || mod_string_len (data, pathaddr, __func__) < 0)
	{
		printf ("%s: ERROR: no database path given!\n", __func__);
		PUSH_I (-1);
		PUSH_I (VECDB_ERR_INVALID);
		return (sp);
	}

	vecdb_options_default (&opt);
	if (dim > 0)
	{
		opt.dim = (size_t) dim;
	}
	if (metric >= 0 && metric <= 2)
	{
		opt.metric = (vecdb_metric) metric;
	}
	if (index >= 0 && index <= 1)
	{
		opt.index = (vecdb_index) index;
	}

	db = vecdb_open ((const char *) &data[pathaddr], &opt);
	if (db == NULL)
	{
		handle = -1;
		status = VECDB_ERR_IO;
	}
	else
	{
		handle = mod_find_slot ();
		if (handle < 0)
		{
			printf ("%s: ERROR: no free database slot!\n", __func__);
			vecdb_close (db);
			handle = -1;
			status = VECDB_ERR_NOMEM;
		}
		else
		{
			vecdb_db[handle] = db;
			status = VECDB_OK;
		}
	}

	PUSH_I (handle);
	PUSH_I (status);
	return (sp);
}

// (pathaddr dim metric index hnsw_m ef_construction ef_search seed :vecdb_open_ex !)
// pushes: handle, status (status on top)
U1 *l1_vecdb_open_ex (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 pathaddr ALIGN;
	S8 dim ALIGN;
	S8 metric ALIGN;
	S8 index ALIGN;
	S8 hnsw_m ALIGN;
	S8 ef_construction ALIGN;
	S8 ef_search ALIGN;
	S8 seed ALIGN;
	S8 handle ALIGN;
	S8 status ALIGN;

	vecdb_options opt;
	vecdb *db;

	POP_I (seed);
	POP_I (ef_search);
	POP_I (ef_construction);
	POP_I (hnsw_m);
	POP_I (index);
	POP_I (metric);
	POP_I (dim);
	POP_I (pathaddr);

	if (pathaddr <= 0 || mod_string_len (data, pathaddr, __func__) < 0)
	{
		printf ("%s: ERROR: no database path given!\n", __func__);
		PUSH_I (-1);
		PUSH_I (VECDB_ERR_INVALID);
		return (sp);
	}

	vecdb_options_default (&opt);
	if (dim > 0)
	{
		opt.dim = (size_t) dim;
	}
	if (metric >= 0 && metric <= 2)
	{
		opt.metric = (vecdb_metric) metric;
	}
	if (index >= 0 && index <= 1)
	{
		opt.index = (vecdb_index) index;
	}
	if (hnsw_m > 0)
	{
		opt.hnsw_m = (size_t) hnsw_m;
	}
	if (ef_construction > 0)
	{
		opt.hnsw_ef_construction = (size_t) ef_construction;
	}
	if (ef_search > 0)
	{
		opt.hnsw_ef_search = (size_t) ef_search;
	}
	opt.seed = (uint64_t) seed;

	db = vecdb_open ((const char *) &data[pathaddr], &opt);
	if (db == NULL)
	{
		handle = -1;
		status = VECDB_ERR_IO;
	}
	else
	{
		handle = mod_find_slot ();
		if (handle < 0)
		{
			printf ("%s: ERROR: no free database slot!\n", __func__);
			vecdb_close (db);
			handle = -1;
			status = VECDB_ERR_NOMEM;
		}
		else
		{
			vecdb_db[handle] = db;
			status = VECDB_OK;
		}
	}

	PUSH_I (handle);
	PUSH_I (status);
	return (sp);
}

// (handle :vecdb_close !) -> status
U1 *l1_vecdb_close (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 handle ALIGN;
	S8 status ALIGN;

	POP_I (handle);

	if (handle < 0 || handle >= VECDB_MOD_MAX_DB)
	{
		PUSH_I (VECDB_ERR_INVALID);
		return (sp);
	}

	if (vecdb_db[handle] == NULL)
	{
		PUSH_I (VECDB_OK);
		return (sp);
	}

	status = vecdb_close (vecdb_db[handle]);
	vecdb_db[handle] = NULL;

	PUSH_I (status);
	return (sp);
}

// (handle :vecdb_flush !) -> status
U1 *l1_vecdb_flush (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 handle ALIGN;
	vecdb *db;

	POP_I (handle);

	db = mod_get_db (handle, __func__);
	if (db == NULL)
	{
		PUSH_I (VECDB_ERR_INVALID);
		return (sp);
	}

	PUSH_I (vecdb_flush (db));
	return (sp);
}

// (handle :vecdb_compact !) -> status
U1 *l1_vecdb_compact (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 handle ALIGN;
	vecdb *db;

	POP_I (handle);

	db = mod_get_db (handle, __func__);
	if (db == NULL)
	{
		PUSH_I (VECDB_ERR_INVALID);
		return (sp);
	}

	PUSH_I (vecdb_compact (db));
	return (sp);
}

// introspection --------------------------------------------------------

// (handle :vecdb_count !) -> count
U1 *l1_vecdb_count (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 handle ALIGN;
	vecdb *db;

	POP_I (handle);

	db = mod_get_db (handle, __func__);
	if (db == NULL)
	{
		PUSH_I (0);
		return (sp);
	}

	PUSH_I ((S8) vecdb_count (db));
	return (sp);
}

// (handle :vecdb_capacity !) -> capacity
U1 *l1_vecdb_capacity (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 handle ALIGN;
	vecdb *db;

	POP_I (handle);

	db = mod_get_db (handle, __func__);
	if (db == NULL)
	{
		PUSH_I (0);
		return (sp);
	}

	PUSH_I ((S8) vecdb_capacity (db));
	return (sp);
}

// (handle :vecdb_dim !) -> dim
U1 *l1_vecdb_dim (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 handle ALIGN;
	vecdb *db;

	POP_I (handle);

	db = mod_get_db (handle, __func__);
	if (db == NULL)
	{
		PUSH_I (0);
		return (sp);
	}

	PUSH_I ((S8) vecdb_dim (db));
	return (sp);
}

// (handle :vecdb_metric !) -> metric
U1 *l1_vecdb_metric (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 handle ALIGN;
	vecdb *db;

	POP_I (handle);

	db = mod_get_db (handle, __func__);
	if (db == NULL)
	{
		PUSH_I (VECDB_METRIC_L2);
		return (sp);
	}

	PUSH_I ((S8) vecdb_metric_of (db));
	return (sp);
}

// (handle :vecdb_index !) -> index
U1 *l1_vecdb_index (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 handle ALIGN;
	vecdb *db;

	POP_I (handle);

	db = mod_get_db (handle, __func__);
	if (db == NULL)
	{
		PUSH_I (VECDB_INDEX_FLAT);
		return (sp);
	}

	PUSH_I ((S8) vecdb_index_of (db));
	return (sp);
}

// (handle destaddr destlen :vecdb_path !) -> status
U1 *l1_vecdb_path (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 handle ALIGN;
	S8 destaddr ALIGN;
	S8 destlen ALIGN;
	vecdb *db;
	const char *path;

	POP_I (destlen);
	POP_I (destaddr);
	POP_I (handle);

	db = mod_get_db (handle, __func__);
	if (db == NULL)
	{
		PUSH_I (VECDB_ERR_INVALID);
		return (sp);
	}

	path = vecdb_path (db);
	if (data_put_string (data, destaddr, destlen, path, __func__) != 0)
	{
		PUSH_I (VECDB_ERR_INVALID);
		return (sp);
	}

	PUSH_I (VECDB_OK);
	return (sp);
}

// (handle ef :vecdb_set_ef_search !) -> status
U1 *l1_vecdb_set_ef_search (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 handle ALIGN;
	S8 ef ALIGN;
	vecdb *db;

	POP_I (ef);
	POP_I (handle);

	db = mod_get_db (handle, __func__);
	if (db == NULL)
	{
		PUSH_I (VECDB_ERR_INVALID);
		return (sp);
	}

	PUSH_I (vecdb_set_ef_search (db, (size_t) ef));
	return (sp);
}

// (status destaddr destlen :vecdb_strerror !) -> status
U1 *l1_vecdb_strerror (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 status_code ALIGN;
	S8 destaddr ALIGN;
	S8 destlen ALIGN;

	POP_I (destlen);
	POP_I (destaddr);
	POP_I (status_code);

	if (data_put_string (data, destaddr, destlen, vecdb_strerror ((int) status_code), __func__) != 0)
	{
		PUSH_I (VECDB_ERR_INVALID);
		return (sp);
	}

	PUSH_I (VECDB_OK);
	return (sp);
}

// (handle destaddr destlen :vecdb_last_error !) -> status
U1 *l1_vecdb_last_error (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 handle ALIGN;
	S8 destaddr ALIGN;
	S8 destlen ALIGN;
	vecdb *db;

	POP_I (destlen);
	POP_I (destaddr);
	POP_I (handle);

	db = mod_get_db (handle, __func__);
	if (db == NULL)
	{
		PUSH_I (VECDB_ERR_INVALID);
		return (sp);
	}

	if (data_put_string (data, destaddr, destlen, vecdb_last_error (db), __func__) != 0)
	{
		PUSH_I (VECDB_ERR_INVALID);
		return (sp);
	}

	PUSH_I (VECDB_OK);
	return (sp);
}

// (handle outaddr cap :vecdb_list_ids !) -> count
U1 *l1_vecdb_list_ids (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 handle ALIGN;
	S8 outaddr ALIGN;
	S8 cap ALIGN;
	vecdb *db;
	uint64_t *out;
	S8 i ALIGN;

	POP_I (cap);
	POP_I (outaddr);
	POP_I (handle);

	db = mod_get_db (handle, __func__);
	if (db == NULL)
	{
		PUSH_I (0);
		return (sp);
	}

	// count query only
	if (outaddr <= 0 || cap <= 0)
	{
		PUSH_I ((S8) vecdb_list_ids (db, NULL, 0));
		return (sp);
	}

	out = (uint64_t *) malloc ((size_t) cap * sizeof (uint64_t));
	if (out == NULL)
	{
		printf ("%s: ERROR: out of memory!\n", __func__);
		PUSH_I (0);
		return (sp);
	}

	S8 n ALIGN = (S8) vecdb_list_ids (db, out, (size_t) cap);

	if (n <= 0)
	{
		free (out);
		PUSH_I (0);
		return (sp);
	}

	#if BOUNDSCHECK
	// l1vm memory_bounds() takes the offset of the last accessed element
	if (memory_bounds (outaddr, (n - 1) * (S8) sizeof (S8)) != 0)
	{
		printf ("%s: ERROR: dest id array overflow: %lli!\n", __func__, outaddr);
		free (out);
		PUSH_I (0);
		return (sp);
	}
	#endif

	for (i = 0; i < n; i++)
	{
		memcpy (&data[outaddr + i * (S8) sizeof (S8)], &out[i], sizeof (S8));
	}

	free (out);
	PUSH_I (n);
	return (sp);
}

// writing --------------------------------------------------------------

// (handle id vecaddr dim payloadaddr payloadlen textaddr :vecdb_put !) -> status
U1 *l1_vecdb_put (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 handle ALIGN;
	S8 id ALIGN;
	S8 vecaddr ALIGN;
	S8 dim ALIGN;
	S8 payloadaddr ALIGN;
	S8 payloadlen ALIGN;
	S8 textaddr ALIGN;
	S8 status ALIGN;

	vecdb *db;
	float *vec = NULL;
	const void *payload = NULL;
	const char *text = NULL;

	POP_I (textaddr);
	POP_I (payloadlen);
	POP_I (payloadaddr);
	POP_I (dim);
	POP_I (vecaddr);
	POP_I (id);
	POP_I (handle);

	db = mod_get_db (handle, __func__);
	if (db == NULL)
	{
		PUSH_I (VECDB_ERR_INVALID);
		return (sp);
	}

	if (vecaddr > 0 && dim > 0)
	{
		if (dim > VECDB_MOD_MAX_DIM)
		{
			printf ("%s: ERROR: vector too long: %lli!\n", __func__, dim);
			PUSH_I (VECDB_ERR_INVALID);
			return (sp);
		}

		vec = (float *) malloc ((size_t) dim * sizeof (float));
		if (vec == NULL)
		{
			printf ("%s: ERROR: out of memory!\n", __func__);
			PUSH_I (VECDB_ERR_NOMEM);
			return (sp);
		}

		if (data_get_floats (data, vecaddr, dim, vec, __func__) < 0)
		{
			free (vec);
			PUSH_I (VECDB_ERR_INVALID);
			return (sp);
		}
	}
	else
	{
		dim = 0;
	}

	if (payloadaddr > 0 && payloadlen > 0)
	{
		#if BOUNDSCHECK
		// l1vm memory_bounds() takes the offset of the last accessed byte
		if (memory_bounds (payloadaddr, payloadlen - 1) != 0)
		{
			printf ("%s: ERROR: payload address overflow: %lli!\n", __func__, payloadaddr);
			if (vec != NULL) free (vec);
			PUSH_I (VECDB_ERR_INVALID);
			return (sp);
		}
		#endif

		payload = (const void *) &data[payloadaddr];
	}

	if (textaddr > 0)
	{
		if (mod_string_len (data, textaddr, __func__) < 0)
		{
			if (vec != NULL) free (vec);
			PUSH_I (VECDB_ERR_INVALID);
			return (sp);
		}
		text = (const char *) &data[textaddr];
	}

	status = vecdb_put (db, (uint64_t) id, vec, (size_t) dim, payload, (size_t) payloadlen, text);

	if (vec != NULL)
	{
		free (vec);
	}

	PUSH_I (status);
	return (sp);
}

// (handle id vecaddr dim payloadaddr payloadlen textaddr flags :vecdb_put_ex !) -> status
U1 *l1_vecdb_put_ex (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 handle ALIGN;
	S8 id ALIGN;
	S8 vecaddr ALIGN;
	S8 dim ALIGN;
	S8 payloadaddr ALIGN;
	S8 payloadlen ALIGN;
	S8 textaddr ALIGN;
	S8 flags ALIGN;
	S8 status ALIGN;

	vecdb *db;
	float *vec = NULL;
	const void *payload = NULL;
	const char *text = NULL;

	POP_I (flags);
	POP_I (textaddr);
	POP_I (payloadlen);
	POP_I (payloadaddr);
	POP_I (dim);
	POP_I (vecaddr);
	POP_I (id);
	POP_I (handle);

	db = mod_get_db (handle, __func__);
	if (db == NULL)
	{
		PUSH_I (VECDB_ERR_INVALID);
		return (sp);
	}

	if (vecaddr > 0 && dim > 0)
	{
		if (dim > VECDB_MOD_MAX_DIM)
		{
			printf ("%s: ERROR: vector too long: %lli!\n", __func__, dim);
			PUSH_I (VECDB_ERR_INVALID);
			return (sp);
		}

		vec = (float *) malloc ((size_t) dim * sizeof (float));
		if (vec == NULL)
		{
			printf ("%s: ERROR: out of memory!\n", __func__);
			PUSH_I (VECDB_ERR_NOMEM);
			return (sp);
		}

		if (data_get_floats (data, vecaddr, dim, vec, __func__) < 0)
		{
			free (vec);
			PUSH_I (VECDB_ERR_INVALID);
			return (sp);
		}
	}
	else
	{
		dim = 0;
	}

	if (payloadaddr > 0 && payloadlen > 0)
	{
		#if BOUNDSCHECK
		// l1vm memory_bounds() takes the offset of the last accessed byte
		if (memory_bounds (payloadaddr, payloadlen - 1) != 0)
		{
			printf ("%s: ERROR: payload address overflow: %lli!\n", __func__, payloadaddr);
			if (vec != NULL) free (vec);
			PUSH_I (VECDB_ERR_INVALID);
			return (sp);
		}
		#endif

		payload = (const void *) &data[payloadaddr];
	}

	if (textaddr > 0)
	{
		if (mod_string_len (data, textaddr, __func__) < 0)
		{
			if (vec != NULL) free (vec);
			PUSH_I (VECDB_ERR_INVALID);
			return (sp);
		}
		text = (const char *) &data[textaddr];
	}

	status = vecdb_put_ex (db, (uint64_t) id, vec, (size_t) dim, payload, (size_t) payloadlen, text, (unsigned) flags);

	if (vec != NULL)
	{
		free (vec);
	}

	PUSH_I (status);
	return (sp);
}

// (handle id vecaddr vecmax payloadaddr payloadmax textaddr textmax :vecdb_get !)
// pushes: payload_len, status (status on top)
U1 *l1_vecdb_get (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 handle ALIGN;
	S8 id ALIGN;
	S8 vecaddr ALIGN;
	S8 vecmax ALIGN;
	S8 payloadaddr ALIGN;
	S8 payloadmax ALIGN;
	S8 textaddr ALIGN;
	S8 textmax ALIGN;
	S8 payload_len ALIGN;

	vecdb *db;
	vecdb_record rec;
	int status;

	POP_I (textmax);
	POP_I (textaddr);
	POP_I (payloadmax);
	POP_I (payloadaddr);
	POP_I (vecmax);
	POP_I (vecaddr);
	POP_I (id);
	POP_I (handle);

	db = mod_get_db (handle, __func__);
	if (db == NULL)
	{
		PUSH_I (0);
		PUSH_I (VECDB_ERR_INVALID);
		return (sp);
	}

	memset (&rec, 0, sizeof (rec));
	status = vecdb_get (db, (uint64_t) id, &rec);

	if (status != VECDB_OK)
	{
		PUSH_I (0);
		PUSH_I (status);
		return (sp);
	}

	payload_len = (S8) rec.payload_len;

	if (rec.vec != NULL && vecaddr > 0 && vecmax > 0)
	{
		if (data_put_floats (data, vecaddr, vecmax, rec.vec, (S8) vecdb_dim (db), __func__) < 0)
		{
			PUSH_I (payload_len);
			PUSH_I (VECDB_ERR_INVALID);
			return (sp);
		}
	}

	if (payloadaddr > 0 && payloadmax > 0)
	{
		if (rec.payload != NULL)
		{
			if (data_put_bytes (data, payloadaddr, payloadmax, rec.payload, (S8) rec.payload_len, __func__) < 0)
			{
				PUSH_I (payload_len);
				PUSH_I (VECDB_ERR_INVALID);
				return (sp);
			}
		}
		else
		{
			data[payloadaddr] = 0;
		}
	}

	if (textaddr > 0 && textmax > 0)
	{
		if (data_put_string (data, textaddr, textmax, rec.text, __func__) != 0)
		{
			PUSH_I (payload_len);
			PUSH_I (VECDB_ERR_INVALID);
			return (sp);
		}
	}

	PUSH_I (payload_len);
	PUSH_I (VECDB_OK);
	return (sp);
}

// (handle id :vecdb_delete !) -> status
U1 *l1_vecdb_delete (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 handle ALIGN;
	S8 id ALIGN;
	vecdb *db;

	POP_I (id);
	POP_I (handle);

	db = mod_get_db (handle, __func__);
	if (db == NULL)
	{
		PUSH_I (VECDB_ERR_INVALID);
		return (sp);
	}

	PUSH_I (vecdb_delete (db, (uint64_t) id));
	return (sp);
}

// searching ------------------------------------------------------------

// (handle queryaddr dim k :vecdb_search_vector !) -> hit count
U1 *l1_vecdb_search_vector (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 handle ALIGN;
	S8 queryaddr ALIGN;
	S8 dim ALIGN;
	S8 k ALIGN;

	vecdb *db;
	float *query;
	S8 n ALIGN;

	POP_I (k);
	POP_I (dim);
	POP_I (queryaddr);
	POP_I (handle);

	db = mod_get_db (handle, __func__);
	if (db == NULL)
	{
		vecdb_match_count = 0;
		PUSH_I (0);
		return (sp);
	}

	if (queryaddr <= 0 || dim <= 0 || dim > VECDB_MOD_MAX_DIM)
	{
		vecdb_match_count = 0;
		PUSH_I (0);
		return (sp);
	}

	query = (float *) malloc ((size_t) dim * sizeof (float));
	if (query == NULL)
	{
		printf ("%s: ERROR: out of memory!\n", __func__);
		vecdb_match_count = 0;
		PUSH_I (0);
		return (sp);
	}

	if (data_get_floats (data, queryaddr, dim, query, __func__) < 0)
	{
		free (query);
		vecdb_match_count = 0;
		PUSH_I (0);
		return (sp);
	}

	if (k < 0)
	{
		k = 0;
	}
	if (k > VECDB_MOD_MAX_MATCHES)
	{
		k = VECDB_MOD_MAX_MATCHES;
	}

	n = (S8) vecdb_search_vector (db, query, (size_t) dim, (size_t) k, vecdb_matches, VECDB_MOD_MAX_MATCHES);

	free (query);

	vecdb_match_count = n;
	PUSH_I (n);
	return (sp);
}

// (handle queryaddr k :vecdb_search_text !) -> hit count
U1 *l1_vecdb_search_text (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 handle ALIGN;
	S8 queryaddr ALIGN;
	S8 k ALIGN;

	vecdb *db;
	S8 n ALIGN;

	POP_I (k);
	POP_I (queryaddr);
	POP_I (handle);

	db = mod_get_db (handle, __func__);
	if (db == NULL)
	{
		vecdb_match_count = 0;
		PUSH_I (0);
		return (sp);
	}

	if (queryaddr <= 0)
	{
		vecdb_match_count = 0;
		PUSH_I (0);
		return (sp);
	}

	if (mod_string_len (data, queryaddr, __func__) < 0)
	{
		vecdb_match_count = 0;
		PUSH_I (0);
		return (sp);
	}

	if (k < 0)
	{
		k = 0;
	}
	if (k > VECDB_MOD_MAX_MATCHES)
	{
		k = VECDB_MOD_MAX_MATCHES;
	}

	n = (S8) vecdb_search_text (db, (const char *) &data[queryaddr], (size_t) k, vecdb_matches, VECDB_MOD_MAX_MATCHES);

	vecdb_match_count = n;
	PUSH_I (n);
	return (sp);
}

// (handle vecaddr dim textaddr k alpha :vecdb_search_hybrid !) -> hit count
U1 *l1_vecdb_search_hybrid (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 handle ALIGN;
	S8 vecaddr ALIGN;
	S8 dim ALIGN;
	S8 textaddr ALIGN;
	S8 k ALIGN;
	F8 alpha ALIGN;

	vecdb *db;
	float *query = NULL;
	S8 n ALIGN;

	POP_D (alpha);
	POP_I (k);
	POP_I (textaddr);
	POP_I (dim);
	POP_I (vecaddr);
	POP_I (handle);

	db = mod_get_db (handle, __func__);
	if (db == NULL)
	{
		vecdb_match_count = 0;
		PUSH_I (0);
		return (sp);
	}

	if (vecaddr > 0 && dim > 0)
	{
		if (dim > VECDB_MOD_MAX_DIM)
		{
			vecdb_match_count = 0;
			PUSH_I (0);
			return (sp);
		}

		query = (float *) malloc ((size_t) dim * sizeof (float));
		if (query == NULL)
		{
			printf ("%s: ERROR: out of memory!\n", __func__);
			vecdb_match_count = 0;
			PUSH_I (0);
			return (sp);
		}

		if (data_get_floats (data, vecaddr, dim, query, __func__) < 0)
		{
			free (query);
			vecdb_match_count = 0;
			PUSH_I (0);
			return (sp);
		}
	}
	else
	{
		dim = 0;
	}

	if (textaddr > 0 && mod_string_len (data, textaddr, __func__) < 0)
	{
		if (query != NULL) free (query);
		vecdb_match_count = 0;
		PUSH_I (0);
		return (sp);
	}

	if (k < 0)
	{
		k = 0;
	}
	if (k > VECDB_MOD_MAX_MATCHES)
	{
		k = VECDB_MOD_MAX_MATCHES;
	}

	n = (S8) vecdb_search_hybrid (db, query, (size_t) dim,
	                              (textaddr > 0) ? (const char *) &data[textaddr] : NULL,
	                              (size_t) k, (float) alpha, vecdb_matches, VECDB_MOD_MAX_MATCHES);

	if (query != NULL)
	{
		free (query);
	}

	vecdb_match_count = n;
	PUSH_I (n);
	return (sp);
}

// cached search results ------------------------------------------------

// ( :vecdb_match_count !) -> hit count of the last search
U1 *l1_vecdb_match_count (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	PUSH_I (vecdb_match_count);
	return (sp);
}

// (index :vecdb_match_id !) -> id
U1 *l1_vecdb_match_id (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 index ALIGN;

	POP_I (index);

	if (index < 0 || index >= vecdb_match_count)
	{
		PUSH_I (-1);
		return (sp);
	}

	PUSH_I ((S8) vecdb_matches[index].id);
	return (sp);
}

// (index :vecdb_match_score !) -> score
U1 *l1_vecdb_match_score (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 index ALIGN;

	POP_I (index);

	if (index < 0 || index >= vecdb_match_count)
	{
		PUSH_D (0.0);
		return (sp);
	}

	PUSH_D ((F8) vecdb_matches[index].score);
	return (sp);
}

// (index destaddr destlen :vecdb_match_text !) -> status
U1 *l1_vecdb_match_text (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 index ALIGN;
	S8 destaddr ALIGN;
	S8 destlen ALIGN;

	POP_I (destlen);
	POP_I (destaddr);
	POP_I (index);

	if (index < 0 || index >= vecdb_match_count)
	{
		PUSH_I (VECDB_ERR_INVALID);
		return (sp);
	}

	if (data_put_string (data, destaddr, destlen, vecdb_matches[index].text, __func__) != 0)
	{
		PUSH_I (VECDB_ERR_INVALID);
		return (sp);
	}

	PUSH_I (VECDB_OK);
	return (sp);
}

// (index destaddr destlen :vecdb_match_payload !) -> payload_len
U1 *l1_vecdb_match_payload (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 index ALIGN;
	S8 destaddr ALIGN;
	S8 destlen ALIGN;
	S8 copied ALIGN;

	POP_I (destlen);
	POP_I (destaddr);
	POP_I (index);

	if (index < 0 || index >= vecdb_match_count)
	{
		PUSH_I (-1);
		return (sp);
	}

	if (vecdb_matches[index].payload == NULL || vecdb_matches[index].payload_len == 0)
	{
		PUSH_I (0);
		return (sp);
	}

	copied = data_put_bytes (data, destaddr, destlen, vecdb_matches[index].payload,
	                         (S8) vecdb_matches[index].payload_len, __func__);
	if (copied < 0)
	{
		PUSH_I (-1);
		return (sp);
	}

	PUSH_I ((S8) vecdb_matches[index].payload_len);
	return (sp);
}
