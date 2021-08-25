/* 
 * FP16 V1.0.4 - Copyright (C) 2011 Eng. Juan Camilo Gomez C. MSc.
 * All rights reserved
 * 
 * Fixed-Point math Q16.16 with rounding and saturated arithmetic 
 */
#ifndef FP16_H
#define FP16_H

#ifdef __cplusplus
extern "C" {
#endif

    #include <stdint.h>
    #include <stdlib.h>
    #include <ctype.h>
    
    typedef int32_t fp_t;
    
    #define FP_EPSILON          (  1 ) 
    #define FP_MIN              ( -2147483647 ) /* -32767.99998 */
    #define FP_MAX              (  2147483647 ) /* +32767.99998 */
    #define FP_OVERFLOW         (  0x80000000 ) /* overflow */
    
    #define FP_PI               (  205887  )    /* pi */
    #define FP_2PI              (  411775 )     /* 2*pi */ 
    #define FP_NPI              ( -205887 )     /* -pi */
    #define FP_1_DIV_PI         (  20861 )      /* 1/pi */
    #define FP_E                (  178145 )     /* e */
    #define FP_E4               (  3578144 )    /* e^4 */
    #define FP_P4_DIV_PI        (  83443 )      /* 4/pi */
    #define FP_N4_DIV_PI        ( -83443 )      /* -4/pi */
    #define FP_PI_DIV_2         (  102944 )     /* pi/2 */
    #define FP_PI_DIV_4         (  51471 )      /* pi/4 */
    #define FP_3PI_DIV_4        (  154415 )     /* 3*pi/4 */      
    #define FP_1_DIV_2          (  32768 )      /* 1/2 */
    #define FP_LN2              (  45426 )      /* log(2) */
    #define FP_LN10             (  150902 )     /* log(10) */
    #define FP_SQRT2            (  92682 )      /* sqrt(2) */
    #define FP_180_DIV_PI       (  3754936 )    /* 180/pi */
    #define FP_PI_DIV_180       (  1144 )       /* pi/180 */
    #define FP_180              (  11796480 )   /* 180 */
    #define FP_360              (  23592960 )   /* 360 */
    #define FP_EXP_MAX          (  681391 )     /* max value for fp_exp function*/
    #define FP_EXP_MIN          ( -681391 )     /* min value for fp_exp function*/
    /*used only for internal operations*/
    #define FP_1                (  65536 )      /* 1 */
    #define FP_2                (  131072 )     /* 2 */
    #define FP_3                (  196608 )     /* 3 */
    #define FP_N16              ( -1048576 )    /* -16 */
    #define FP_100              (  6553600 )    /* 100 */
    
    #define fp16_constant(x)    ( (fp_t)(((x) >= 0) ? ((x) * 65536.0 + 0.5) : ((x) * 65536.0 - 0.5)) )
    
    typedef struct{
        fp_t min, max;
        uint8_t rounding, saturate;
    }fp_instance_t;
    
    void fp16_init(fp_instance_t *instance);
    void fp16_select(fp_instance_t *instance);
    

    int fp16_fp2int(fp_t x);
    fp_t fp16_int2fp(int x);
    
    fp_t fp16_float2fp(float x);
    float fp16_fp2float(fp_t x);
    
    fp_t fp16_double2fp(double x);
    double fp16_fp2double(fp_t x);    
    
    
    fp_t fp16_abs(fp_t x);
    fp_t fp16_floor(fp_t x);
    fp_t fp16_ceil(fp_t x);
    fp_t fp16_round(fp_t x);
    
    fp_t fp16_add(fp_t X, fp_t Y);
    fp_t fp16_sub(fp_t X, fp_t Y);
    fp_t fp16_mul(fp_t x, fp_t y);
    fp_t fp16_div(fp_t x, fp_t y);
    fp_t fp16_mod(fp_t x, fp_t y);
    
    fp_t fp16_sqrt(fp_t x);
    fp_t fp16_exp(fp_t x);
    fp_t fp16_log(fp_t x);
    fp_t fp16_log2(fp_t x);
    
    fp_t fp16_rad2deg(fp_t x);
    fp_t fp16_deg2rad(fp_t x);
    
    fp_t fp16_wraptopi(fp_t x);
    fp_t fp16_wrapto180(fp_t x);
    fp_t fp16_sin(fp_t x);
    fp_t fp16_cos(fp_t x);
    fp_t fp16_tan(fp_t x);
    fp_t fp16_atan2(fp_t y , fp_t x);
    fp_t fp16_atan(fp_t x);
    fp_t fp16_asin(fp_t x);
    fp_t fp16_acos(fp_t x);
    fp_t fp16_cosh(fp_t x);
    fp_t fp16_sinh(fp_t x);
    fp_t fp16_tanh(fp_t x);
    
    fp_t fp16_polyval(fp_t *p, size_t n, fp_t x);
    
    fp_t fp16_ipow(fp_t x, fp_t y);
    fp_t fp16_pow(fp_t x, fp_t y);

    char* fp16_fptoA(fp_t num, char *str, int decimals);
    fp_t fp16_Atofp(char *s);
    
#ifdef __cplusplus
}
#endif

#endif /* FP16_H */

