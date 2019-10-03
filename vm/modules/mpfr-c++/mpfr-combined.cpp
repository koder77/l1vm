#include "mpfr-head.cpp"

extern "C" U1 *mp_sqr_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_sqr_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_sqr_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_sqr_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_sqr_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = sqr (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_sqrt_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_sqrt_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_sqrt_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_sqrt_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_sqrt_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = sqrt (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_cbrt_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_cbrt_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_cbrt_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_cbrt_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_cbrt_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = cbrt (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_pow_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_pow_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_pow_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 b ALIGN;

sp = stpopi ((U1 *) &b, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_pow_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (b >= MAX_FLOAT_NUM || b < 0)
{
printf ("gmp_pow_float: ERROR float index b out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 a ALIGN;

sp = stpopi ((U1 *) &a, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_pow_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (a >= MAX_FLOAT_NUM || a < 0)
{
printf ("gmp_pow_float: ERROR float index a out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = pow (mpf_float[a],mpf_float[b]);
return (sp);
}

extern "C" U1 *mp_fabs_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_fabs_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_fabs_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_fabs_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_fabs_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = fabs (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_abs_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_abs_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_abs_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_abs_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_abs_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = abs (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_dim_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_dim_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_dim_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 b ALIGN;

sp = stpopi ((U1 *) &b, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_dim_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (b >= MAX_FLOAT_NUM || b < 0)
{
printf ("gmp_dim_float: ERROR float index b out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 a ALIGN;

sp = stpopi ((U1 *) &a, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_dim_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (a >= MAX_FLOAT_NUM || a < 0)
{
printf ("gmp_dim_float: ERROR float index a out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = dim (mpf_float[a],mpf_float[b]);
return (sp);
}

extern "C" U1 *mp_log_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_log_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_log_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_log_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_log_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = log (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_log2_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_log2_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_log2_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_log2_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_log2_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = log2 (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_logb_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_logb_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_logb_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_logb_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_logb_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = logb (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_log10_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_log10_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_log10_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_log10_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_log10_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = log10 (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_exp2_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_exp2_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_exp2_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_exp2_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_exp2_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = exp2 (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_exp10_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_exp10_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_exp10_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_exp10_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_exp10_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = exp10 (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_log1p_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_log1p_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_log1p_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_log1p_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_log1p_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = log1p (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_expm1_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_expm1_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_expm1_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_expm1_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_expm1_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = expm1 (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_cos_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_cos_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_cos_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_cos_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_cos_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = cos (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_sin_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_sin_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_sin_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_sin_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_sin_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = sin (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_tan_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_tan_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_tan_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_tan_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_tan_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = tan (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_sec_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_sec_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_sec_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_sec_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_sec_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = sec (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_csc_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_csc_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_csc_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_csc_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_csc_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = csc (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_cot_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_cot_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_cot_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_cot_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_cot_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = cot (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_acos_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_acos_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_acos_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_acos_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_acos_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = acos (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_asin_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_asin_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_asin_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_asin_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_asin_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = asin (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_atan_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_atan_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_atan_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_atan_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_atan_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = atan (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_atan2_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_atan2_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_atan2_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 x ALIGN;

sp = stpopi ((U1 *) &x, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_atan2_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (x >= MAX_FLOAT_NUM || x < 0)
{
printf ("gmp_atan2_float: ERROR float index x out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 y ALIGN;

sp = stpopi ((U1 *) &y, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_atan2_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (y >= MAX_FLOAT_NUM || y < 0)
{
printf ("gmp_atan2_float: ERROR float index y out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = atan2 (mpf_float[y],mpf_float[x]);
return (sp);
}

extern "C" U1 *mp_acot_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_acot_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_acot_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_acot_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_acot_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = acot (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_asec_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_asec_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_asec_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_asec_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_asec_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = asec (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_acsc_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_acsc_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_acsc_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_acsc_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_acsc_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = acsc (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_cosh_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_cosh_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_cosh_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_cosh_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_cosh_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = cosh (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_sinh_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_sinh_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_sinh_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_sinh_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_sinh_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = sinh (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_tanh_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_tanh_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_tanh_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_tanh_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_tanh_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = tanh (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_sech_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_sech_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_sech_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_sech_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_sech_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = sech (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_csch_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_csch_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_csch_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_csch_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_csch_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = csch (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_coth_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_coth_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_coth_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_coth_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_coth_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = coth (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_acosh_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_acosh_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_acosh_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_acosh_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_acosh_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = acosh (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_asinh_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_asinh_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_asinh_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_asinh_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_asinh_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = asinh (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_atanh_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_atanh_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_atanh_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_atanh_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_atanh_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = atanh (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_acoth_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_acoth_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_acoth_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_acoth_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_acoth_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = acoth (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_asech_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_asech_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_asech_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_asech_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_asech_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = asech (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_acsch_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_acsch_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_acsch_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_acsch_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_acsch_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = acsch (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_hypot_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_hypot_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_hypot_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 y ALIGN;

sp = stpopi ((U1 *) &y, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_hypot_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (y >= MAX_FLOAT_NUM || y < 0)
{
printf ("gmp_hypot_float: ERROR float index y out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 x ALIGN;

sp = stpopi ((U1 *) &x, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_hypot_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (x >= MAX_FLOAT_NUM || x < 0)
{
printf ("gmp_hypot_float: ERROR float index x out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = hypot (mpf_float[x],mpf_float[y]);
return (sp);
}

extern "C" U1 *mp_eint_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_eint_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_eint_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_eint_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_eint_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = eint (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_gamma_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_gamma_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_gamma_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_gamma_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_gamma_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = gamma (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_tgamma_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_tgamma_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_tgamma_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_tgamma_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_tgamma_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = tgamma (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_lngamma_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_lngamma_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_lngamma_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_lngamma_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_lngamma_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = lngamma (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_zeta_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_zeta_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_zeta_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_zeta_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_zeta_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = zeta (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_erf_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_erf_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_erf_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_erf_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_erf_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = erf (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_erfc_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_erfc_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_erfc_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_erfc_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_erfc_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = erfc (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_bessely0_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_bessely0_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_bessely0_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_bessely0_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_bessely0_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = bessely0 (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_bessely1_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_bessely1_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_bessely1_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_bessely1_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_bessely1_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = bessely1 (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_fma_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_fma_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_fma_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v3 ALIGN;

sp = stpopi ((U1 *) &v3, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_fma_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v3 >= MAX_FLOAT_NUM || v3 < 0)
{
printf ("gmp_fma_float: ERROR float index v3 out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v2 ALIGN;

sp = stpopi ((U1 *) &v2, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_fma_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v2 >= MAX_FLOAT_NUM || v2 < 0)
{
printf ("gmp_fma_float: ERROR float index v2 out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v1 ALIGN;

sp = stpopi ((U1 *) &v1, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_fma_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v1 >= MAX_FLOAT_NUM || v1 < 0)
{
printf ("gmp_fma_float: ERROR float index v1 out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = fma (mpf_float[v1],mpf_float[v2],mpf_float[v3]);
return (sp);
}

extern "C" U1 *mp_fms_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_fms_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_fms_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v3 ALIGN;

sp = stpopi ((U1 *) &v3, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_fms_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v3 >= MAX_FLOAT_NUM || v3 < 0)
{
printf ("gmp_fms_float: ERROR float index v3 out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v2 ALIGN;

sp = stpopi ((U1 *) &v2, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_fms_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v2 >= MAX_FLOAT_NUM || v2 < 0)
{
printf ("gmp_fms_float: ERROR float index v2 out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v1 ALIGN;

sp = stpopi ((U1 *) &v1, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_fms_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v1 >= MAX_FLOAT_NUM || v1 < 0)
{
printf ("gmp_fms_float: ERROR float index v1 out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = fms (mpf_float[v1],mpf_float[v2],mpf_float[v3]);
return (sp);
}

extern "C" U1 *mp_agm_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_agm_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_agm_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v2 ALIGN;

sp = stpopi ((U1 *) &v2, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_agm_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v2 >= MAX_FLOAT_NUM || v2 < 0)
{
printf ("gmp_agm_float: ERROR float index v2 out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v1 ALIGN;

sp = stpopi ((U1 *) &v1, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_agm_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v1 >= MAX_FLOAT_NUM || v1 < 0)
{
printf ("gmp_agm_float: ERROR float index v1 out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = agm (mpf_float[v1],mpf_float[v2]);
return (sp);
}

extern "C" U1 *mp_li2_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_li2_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_li2_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_li2_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_li2_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = li2 (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_fmod_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_fmod_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_fmod_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 y ALIGN;

sp = stpopi ((U1 *) &y, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_fmod_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (y >= MAX_FLOAT_NUM || y < 0)
{
printf ("gmp_fmod_float: ERROR float index y out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 x ALIGN;

sp = stpopi ((U1 *) &x, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_fmod_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (x >= MAX_FLOAT_NUM || x < 0)
{
printf ("gmp_fmod_float: ERROR float index x out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = fmod (mpf_float[x],mpf_float[y]);
return (sp);
}

extern "C" U1 *mp_rec_sqrt_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_rec_sqrt_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_rec_sqrt_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_rec_sqrt_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_rec_sqrt_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = rec_sqrt (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_digamma_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_digamma_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_digamma_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_digamma_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_digamma_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = digamma (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_ai_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_ai_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_ai_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_ai_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_ai_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = ai (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_rint_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_rint_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_rint_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_rint_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_rint_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = rint (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_ceil_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_ceil_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_ceil_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_ceil_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_ceil_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = ceil (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_floor_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_floor_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_floor_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_floor_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_floor_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = floor (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_round_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_round_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_round_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_round_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_round_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = round (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_trunc_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_trunc_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_trunc_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_trunc_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_trunc_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = trunc (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_rint_ceil_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_rint_ceil_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_rint_ceil_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_rint_ceil_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_rint_ceil_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = rint_ceil (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_rint_floor_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_rint_floor_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_rint_floor_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_rint_floor_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_rint_floor_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = rint_floor (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_rint_round_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_rint_round_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_rint_round_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_rint_round_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_rint_round_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = rint_round (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_rint_trunc_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_rint_trunc_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_rint_trunc_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_rint_trunc_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_rint_trunc_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = rint_trunc (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_frac_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_frac_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_frac_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_frac_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM || v < 0)
{
printf ("gmp_frac_float: ERROR float index v out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = frac (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_remainder_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_remainder_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_remainder_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 y ALIGN;

sp = stpopi ((U1 *) &y, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_remainder_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (y >= MAX_FLOAT_NUM || y < 0)
{
printf ("gmp_remainder_float: ERROR float index y out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 x ALIGN;

sp = stpopi ((U1 *) &x, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_remainder_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (x >= MAX_FLOAT_NUM || x < 0)
{
printf ("gmp_remainder_float: ERROR float index x out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = remainder (mpf_float[x],mpf_float[y]);
return (sp);
}

extern "C" U1 *mp_nexttoward_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_nexttoward_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_nexttoward_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 y ALIGN;

sp = stpopi ((U1 *) &y, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_nexttoward_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (y >= MAX_FLOAT_NUM || y < 0)
{
printf ("gmp_nexttoward_float: ERROR float index y out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 x ALIGN;

sp = stpopi ((U1 *) &x, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_nexttoward_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (x >= MAX_FLOAT_NUM || x < 0)
{
printf ("gmp_nexttoward_float: ERROR float index x out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = nexttoward (mpf_float[x],mpf_float[y]);
return (sp);
}

extern "C" U1 *mp_nextabove_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_nextabove_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_nextabove_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 x ALIGN;

sp = stpopi ((U1 *) &x, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_nextabove_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (x >= MAX_FLOAT_NUM || x < 0)
{
printf ("gmp_nextabove_float: ERROR float index x out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = nextabove (mpf_float[x]);
return (sp);
}

extern "C" U1 *mp_nextbelow_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_nextbelow_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_nextbelow_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 x ALIGN;

sp = stpopi ((U1 *) &x, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_nextbelow_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (x >= MAX_FLOAT_NUM || x < 0)
{
printf ("gmp_nextbelow_float: ERROR float index x out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = nextbelow (mpf_float[x]);
return (sp);
}

extern "C" U1 *mp_fmax_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_fmax_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_fmax_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 y ALIGN;

sp = stpopi ((U1 *) &y, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_fmax_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (y >= MAX_FLOAT_NUM || y < 0)
{
printf ("gmp_fmax_float: ERROR float index y out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 x ALIGN;

sp = stpopi ((U1 *) &x, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_fmax_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (x >= MAX_FLOAT_NUM || x < 0)
{
printf ("gmp_fmax_float: ERROR float index x out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = fmax (mpf_float[x],mpf_float[y]);
return (sp);
}

extern "C" U1 *mp_fmin_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_fmin_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
{
printf ("gmp_fmin_float: ERROR float index result out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 y ALIGN;

sp = stpopi ((U1 *) &y, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_fmin_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (y >= MAX_FLOAT_NUM || y < 0)
{
printf ("gmp_fmin_float: ERROR float index y out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 x ALIGN;

sp = stpopi ((U1 *) &x, sp, sp_top);
if (sp == NULL)
{
printf ("gmp_fmin_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (x >= MAX_FLOAT_NUM || x < 0)
{
printf ("gmp_fmin_float: ERROR float index x out of range! Must be 0 < %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = fmin (mpf_float[x],mpf_float[y]);
return (sp);
}
