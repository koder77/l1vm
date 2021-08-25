/* 
 * FP16 V1.0.4 - Copyright (C) 2011 Eng. Juan Camilo Gomez C. MSc.
 * All rights reserved
 * 
 * Fixed-Point math Q16.16 with rounding and saturated arithmetic 
 */
#include "fp16.h"

static fp_instance_t fp_default = {FP_MIN, FP_MAX, 1u, 0u};
static fp_instance_t *fp = &fp_default;
static const fp_t fp_unity = FP_1;
static const float fp_1divunity_float = 1.0f/65536.0f;
static const double fp_1divunity_double = 1.0/65536.0;

static uint32_t fp16_overflow_check(uint32_t res, uint32_t x, uint32_t y);
static fp_t fp16_rs(fp_t x);
static fp_t fp16_log2i(fp_t x);

static char *fp16_itoa(char *buf, uint32_t scale, uint32_t value, uint8_t skip);
/*============================================================================*/
/* void fp16_init(fp_instance_t *instance){

Initialize the fixed-point instance.

Parameters:

    - instance : The fixed-point instance.
*/
void fp16_init(fp_instance_t *instance){
    instance->max = FP_MAX;
    instance->min = FP_MIN;
}
/*============================================================================*/
/* void fp16_select(fp_instance_t *instance){

Select the current instance to perform fixed-point operations.

Parameters:

    - instance : The fixed-point instance. Pass NULL to use the 
                default configuration

*/
void fp16_select(fp_instance_t *instance){
    if( NULL == instance){
        fp = &fp_default;
    }
    else{
        fp = instance;
    }
}
/*============================================================================*/
/* int fp16_fp2int(fp_t x)

Returns the fixed-point value x converted to int.

Parameters:

    - x : This is the fixed-point(q16.16) value.

Return value:
 
    This function returns x converted to int.
*/
int fp16_fp2int(fp_t x){
    int RetValue;
    if(fp->rounding){
	if (x >= 0){
            RetValue =  (x + (fp_unity >> 1)) / fp_unity;
        }
	else{
            RetValue =  (x - (fp_unity >> 1)) / fp_unity;
        }	        
    }
    else{
        RetValue = x >> 16;
    }
    return RetValue;
}
/*============================================================================*/
/* fp_t fp16_int2fp(int x)

Returns the int value x converted to fixed-point q16.16.

Parameters:

    - x : This is the int value.

Return value:
 
    This function returns x converted to fixed-point q16.16.
*/
fp_t fp16_int2fp(int x){ 
    return x << 16; 
}
/*============================================================================*/
/* fp_t fp16_float2fp(double x)

Returns the float value x converted to fixed-point q16.16.

Parameters:

    - x : This is the float value.

Return value:
 
    This function returns x converted to fixed-point q16.16.
*/
fp_t fp16_float2fp(float x){
    float RetValue;
    RetValue = x * (float)fp_unity;
    if(fp->rounding){
        RetValue += (RetValue >= 0.0f) ? 0.5f : -0.5f;
    }    
    return (fp_t)RetValue;
}
/*============================================================================*/
/* double fp16_fp2float(fp_t x)

Returns the fixed-point value x converted to float.

Parameters:

    - x : This is the fixed-point(q16.16) value.

Return value:
 
    This function returns x converted to float.
*/
float fp16_fp2float(fp_t x){
    return (float)x * fp_1divunity_float; 
}
/*============================================================================*/
/* fp_t fp16_double2fp(double x)

Returns the double value x converted to fixed-point q16.16.

Parameters:

    - x : This is the double value.

Return value:
 
    This function returns x converted to fixed-point q16.16.
*/
fp_t fp16_double2fp(double x){
    double RetValue;
    RetValue = x * (double)fp_unity;
    if(fp->rounding){
        RetValue += (RetValue >= 0.0 )? 0.5 : -0.5;
    }    
    return (fp_t)RetValue;
}
/*============================================================================*/
/* double fp16_fp2double(fp_t x)

Returns the fixed-point value x converted to double.

Parameters:

    - x : This is the fixed-point(q16.16) value.

Return value:
 
    This function returns x converted to double.
*/
double fp16_fp2double(fp_t x){
    return (double)x * fp_1divunity_double; 
}
/*============================================================================*/
/* fp_t fp16_abs(fp_t x)

Returns the absolute value of x.

Parameters:

    - x : This is the fixed-point(q16.16) value.

Return value:
 
    This function returns the absolute value of x.
*/
fp_t fp16_abs(fp_t x){
    return ((x < 0) ? -x : x);
}
/*============================================================================*/
/* fp_t fp16_floor(fp_t x)

Returns the largest integer value less than or equal to x.

Parameters:

    - x : This is the fixed-point(q16.16) value.

Return value:
 
    This function returns the largest integral value not greater than x.
*/
fp_t fp16_floor(fp_t x){ 
    return (x & 0xFFFF0000ul); 
}
/*============================================================================*/
/* fp_t fp16_floor(fp_t x)

Returns the smallest integer value greater than or equal to x.

Parameters:

    - x : This is the fixed-point(q16.16) value.

Return value:
 
    This function returns the smallest integral value not less than x.
*/
fp_t fp16_ceil(fp_t x){ 
    return (x & 0xFFFF0000UL) + (x & 0x0000FFFFUL ? fp_unity : 0); 
}
/*============================================================================*/
/* fp_t fp16_round(fp_t x)

Returns the nearest integer value of the fixed-point argument x

Parameters:

    - x : This is the fixed-point(q16.16) value.

Return value:
 
    This function returns the nearest integral value of x.
*/
fp_t fp16_round(fp_t x){
    return fp16_floor( x + FP_1_DIV_2 );
}
/*============================================================================*/
static uint32_t fp16_overflow_check(uint32_t res, uint32_t x, uint32_t y){
    if (!((x ^ y) & 0x80000000) && ((x ^ res) & 0x80000000)){
        res =  FP_OVERFLOW;
    }    
    return res;
}
/*============================================================================*/
/* fp_t fp16_add(fp_t x, fp_t y)

Returns the fixed-point addition x + y.

Parameters:

    - x : This is the fixed-point(q16.16) value.
    - y : This is the fixed-point(q16.16) value.

Return value:
 
    This function returns the addition operation x+y. FP_OVERFLOW when an operation
    overflow is detected.
*/
fp_t fp16_add(fp_t X, fp_t Y){
    uint32_t x = (uint32_t)X, y = (uint32_t)Y;
    uint32_t RetValue;
    RetValue =  x + y;
    RetValue = fp16_overflow_check(RetValue, x, y);
    return (fp_t)RetValue;    
}
/*============================================================================*/
/* fp_t fp16_sub(fp_t x, fp_t y)

Returns the fixed-point subtraction x - y.

Parameters:

    - x : This is the fixed-point(q16.16) value.
    - y : This is the fixed-point(q16.16) value.

Return value:
 
    This function returns the subtraction operation x-y. FP_OVERFLOW when an operation
    overflow is detected.
*/
fp_t fp16_sub(fp_t X, fp_t Y){
    uint32_t x = (uint32_t)X, y = (uint32_t)Y; 
    uint32_t RetValue;
    RetValue =  x - y;
    RetValue = fp16_overflow_check(RetValue, x, y);
    return (fp_t)RetValue;
}
/*============================================================================*/
/* fp_t fp16_mul(fp_t x, fp_t y)

Returns the fixed-point product operation x*y.

Parameters:

    - x : This is the fixed-point(q16.16) value.
    - y : This is the fixed-point(q16.16) value.

Return value:
 
    This function returns the product operation x*y. FP_OVERFLOW when an operation
    overflow is detected.
*/
fp_t fp16_mul(fp_t x, fp_t y){
    fp_t RetValue = FP_OVERFLOW;
    int32_t a, c, ac, adcb, mulH;
    uint32_t b, d, bd, tmp, tmp2, mulL;
    
    a = (x >> 16);
    c = (y >> 16);
    b = (x & 0xFFFF);
    d = (y & 0xFFFF);
    ac = a*c;
    adcb = a*d + c*b;
    bd = b*d;    
    mulH = ac + (adcb >> 16);
    tmp = adcb << 16;
    mulL = bd + tmp;
    if( mulL < bd ){
        mulH++;
    }
    if (mulH >> 31 == mulH >> 15){
        if( fp->rounding ){
            tmp2 = mulL;
            mulL -= FP_1_DIV_2;
            mulL -= (uint32_t)mulH >> 31;
            if( mulL > tmp2){
                mulH--;
            }
            RetValue = (mulH << 16) | (mulL >> 16);
            RetValue += 1;  
        }
        else{
            RetValue = (mulH << 16) | (mulL >> 16);
        }		
    }
    return RetValue;
}
/*============================================================================*/
/* fp_t fp16_div(fp_t x, fp_t y)

Returns the fixed-point division operation x/y.

Parameters:

    - x : This is the fixed-point(q16.16) value.
    - y : This is the fixed-point(q16.16) value.

Return value:
 
    This function returns the division operation x/y. FP_OVERFLOW when an operation
    overflow is detected.
*/
fp_t fp16_div(fp_t x, fp_t y){
    uint32_t xrem, xdiv;
    uint32_t quotient = 0ul, bit = 0x10000ul;
    fp_t RetValue;
    RetValue = fp->min;
    if (y != 0){
        xrem = (x >= 0) ? x : (-x);
        xdiv = (y >= 0) ? y : (-y);

        while (xdiv < xrem){
            xdiv <<= 1;
            bit <<= 1;
        }
        RetValue = FP_OVERFLOW;
        if (bit){
            if (xdiv & 0x80000000){
                if ( xrem >= xdiv ){
                    quotient |= bit;
                    xrem -= xdiv;
                }
                xdiv >>= 1;
                bit >>= 1;
            }

            while (bit && xrem){
                if (xrem >= xdiv){
                    quotient |= bit;
                    xrem -= xdiv;
                }
                xrem <<= 1;
                bit >>= 1;
            }	 
            if( fp->rounding ){
                if (xrem >= xdiv){
                    quotient++;
                }
            }

            RetValue = quotient;

            if ((x ^ y) & 0x80000000){
                if (quotient == fp->min){
                    RetValue = FP_OVERFLOW;
                }
                else{
                    RetValue = -RetValue;
                }
            }
        }
        
    if( fp->saturate ){
        if( FP_OVERFLOW == RetValue ){
            RetValue = ((x >= 0) == (y >= 0))? fp->max : fp->min;
        }
    }
        
    }
    return RetValue;
}
/*============================================================================*/
/* fp_t fp16_mod(fp_t x, fp_t y)

Returns the modulo operation x % y.

Parameters:

    - x : This is the fixed-point(q16.16) value.
    - y : This is the fixed-point(q16.16) value.

Return value:
 
    This function returns the modulo operation of x with y.
*/
fp_t fp16_mod(fp_t x, fp_t y){
    fp_t RetValue;
    RetValue = x % y;
    return RetValue;
}
/*============================================================================*/
/* fp_t fp16_sqrt(fp_t x)

Returns the fixed-point square root of x.

Parameters:

    - X : This is the fixed-point(q16.16) value.

Return value:
 
    This function returns the square root of x. For negative numbers, returns
    FP_OVERFLOW.
*/
fp_t fp16_sqrt(fp_t x){
    uint32_t bit;
    uint8_t  n;
    fp_t RetValue = FP_OVERFLOW;
   
    if( x > 0){
        RetValue = 0;
        bit = (x & 0xFFF00000)? (uint32_t)1 << 30 : (uint32_t)1 << 18;
        while (bit > x){
            bit >>= 2;
        }
            
       	for (n = 0u; n < 2u; n++){
            while (bit){
                if (x >= RetValue + bit){
                    x -= RetValue + bit;
                    RetValue = (RetValue >> 1) + bit;
		}
		else{
                    RetValue = (RetValue >> 1);
		}
		bit >>= 2;
            }
		
            if (n == 0u){
		if (x > 65535){
                    x -= RetValue;
                    x = (x << 16) - FP_1_DIV_2;
                    RetValue = (RetValue << 16) + FP_1_DIV_2;
		}
		else{
                    x <<= 16;
                    RetValue <<= 16;
		}
		bit = 1 << 14;
            }
	}  
    }
    if( fp->rounding ){
	if (x > RetValue){
            RetValue++;
	}        
    }
    return (fp_t)RetValue;
}
/*============================================================================*/
/* fp_t fp16_exp(fp_t x)

Returns the fixed-point value of e raised to the xth power.

Parameters:

    - X : This is the fixed-point(q16.16) value.

Return value:
 
    This function returns the exponential value of x.
*/
fp_t fp16_exp(fp_t x) {
    fp_t RetValue, term;
    uint8_t isNegative;
    int i;
    
    if(x == 0 ){
        RetValue = fp_unity;
    }
    else if(x == fp_unity ){
        RetValue = FP_E;
    }
    else if(x >= FP_EXP_MAX   ){
        RetValue =  fp->max;
    }  
    else if(x <= FP_EXP_MIN  ){
        RetValue = 0;
    }
    else{
        isNegative = (x < 0);
        if (isNegative) {
            x = -x;
        }
        
        RetValue = x + fp_unity;
        term = x;
       
        for (i = 2; i < 30; i++){
            term = fp16_mul(term, fp16_div(x, fp16_int2fp(i)));
            RetValue += term;

            if ((term < 500) && ((i > 15) || (term < 20))){
                break;
            }	
        }

        if ( isNegative ){
            RetValue = fp16_div(fp_unity, RetValue);  
        }
    }
    return RetValue;        
}
/*============================================================================*/
/* fp_t fp16_log(fp_t x)

Returns the fixed-point natural logarithm (base-e logarithm) of x.

Parameters:

    - X : This is the fixed-point(q16.16) value.

Return value:
 
    This function returns natural logarithm of x. For negative values 
    returns FP_OVERFLOW
*/
fp_t fp16_log(fp_t x){
    fp_t guess = FP_2;
    fp_t delta, e;
    fp_t RetValue = FP_OVERFLOW;
    int scaling = 0;
    int count = 0;
    
    if (x > 0){
        while (x > FP_100){
            x = fp16_div(x, FP_E4);
            scaling += 4;
        }

        while (x < fp_unity){
            x = fp16_mul(x, FP_E4);
            scaling -= 4;
        }

        do{
            e = fp16_exp(guess);
            delta = fp16_div(x - e, e);

            if (delta > FP_3){
                delta = FP_3;
            }
            guess += delta;
        } while ((count++ < 10) && ((delta > 1) || (delta < -1)));

        RetValue = guess + fp16_int2fp(scaling);
    } 
    return RetValue;
}
/*============================================================================*/
static fp_t fp16_rs(fp_t x){
    fp_t RetValue;
    if( fp->rounding ){
        RetValue = (x >> 1) + (x & 1);
    }
    else{
        RetValue = x >> 1;
    }
    
    return RetValue;
}
/*============================================================================*/
static fp_t fp16_log2i(fp_t x){
    fp_t RetValue = 0;
    int i;

    while(x >= FP_2){
	RetValue++;
	x = fp16_rs(x);
    }

    if(x == 0){
        RetValue = RetValue << 16;
    }
    else{
        for(i = 16; i > 0; i--){
            x = fp16_mul(x, x);
            RetValue <<= 1;
            if(x >= FP_2){
                RetValue |= 1;
                x = fp16_rs(x);
            }
        }
        if(fp->rounding){
            x = fp16_mul(x, x);
            if(x >= FP_2){
                RetValue++;
            }
        }        
    }

    return RetValue;    
}
/*============================================================================*/
/* fp_t fp16_log(fp_t x)

Returns the fixed-point log base 2 of x.

Parameters:

    - X : This is the fixed-point(q16.16) value.

Return value:
 
    This function returns log base 2 of x. For negative values 
    returns FP_OVERFLOW
*/
fp_t fp16_log2(fp_t x){
    fp_t RetValue = FP_OVERFLOW;
    fp_t inv;
    if (x > 0){
        if (x < fp_unity){
            if (x == 1){
                RetValue = FP_N16;
            }
            else{
                inv = fp16_div( fp_unity, x );
                RetValue = -fp16_log2i( inv );   
            }
        }        
        else{
            RetValue = fp16_log2i(x);
        }
    }
    if( fp->saturate ){
        if( FP_OVERFLOW == RetValue ){
            RetValue = fp->min;
        }
    }
    
    return RetValue;
}
/*============================================================================*/
/* fp_t fp16_rad2deg(fp_t x)

Converts angle units from radians to degrees.

Parameters:

    - X : This is the fixed-point(q16.16) value representing an angle expressed
          in radians. 

Return value:
 
    This function returns the angle converted in degrees.
*/
fp_t fp16_rad2deg(fp_t x){
    return fp16_mul( fp16_wraptopi(x), FP_180_DIV_PI);
}
/*============================================================================*/
/* fp_t fp16_deg2rad(fp_t x)

Converts angle units from degrees to radians.

Parameters:

    - X : This is the fixed-point(q16.16) value representing an angle expressed
          in degrees. 

Return value:
 
    This function returns the angle converted in radianscollapse .
*/
fp_t fp16_deg2rad(fp_t x){
    return fp16_mul( fp16_wrapto180(x), FP_PI_DIV_180);
}
/*============================================================================*/
/* fp_t fp16_wraptopi(fp_t x)

Wrap the fixed-point angle in radians to [−pi pi]

Parameters:

    - X : This is the fixed-point(q16.16) value representing an angle expressed
          in radians. 

Return value:
 
    This function returns the wrapped angle in the range [−pi, pi] of x.
*/
fp_t fp16_wraptopi(fp_t x){
    if( ( x < -FP_PI ) || ( x > FP_PI ) ){
        while ( x > FP_PI ) {
            x -= FP_2PI;
        }
        while ( x <= -FP_PI ) {
            x += FP_2PI;
        }
    }
    return x;
}
/* fp_t fp16_wraptopi(fp_t x)

Wrap the fixed-point angle in degrees to [−180 180]

Parameters:

    - X : This is the fixed-point(q16.16) value representing an angle expressed
          in radians. 

Return value:
 
    This function returns the wrapped angle in the range [−180, 180] of x.
*/
fp_t fp16_wrapto180(fp_t x){
    if( ( x < -FP_180 ) || ( x > FP_180 ) ){
        while ( x > FP_180 ) {
            x -= FP_360;
        }
        while ( x <= -FP_PI ) {
            x += FP_360;
        }
    }
    return x;
}
/*============================================================================*/
/* fp_t fp16_sin(fp_t x)

Computes the fixed-point sine of the radian angle x. 

Parameters:

    - X : This is the fixed-point(q16.16) value representing an angle expressed
          in radians. 

Return value:
 
    This function returns sine of x.
*/
fp_t fp16_sin(fp_t x){ /*5-step aproximation using taylor series*/
    fp_t RetValue;
    fp_t x2;
    
    x = fp16_wraptopi(x);
    x2 = fp16_mul(x ,x);

    RetValue = x;
    x = fp16_mul(x, x2);
    RetValue -= (x / 6);
    x = fp16_mul(x, x2);
    RetValue += (x / 120);
    x = fp16_mul(x, x2);
    RetValue -= (x / 5040);
    x = fp16_mul(x, x2);
    RetValue += (x / 362880);
    x = fp16_mul(x, x2);
    RetValue -= (x / 39916800);

    return RetValue;
}
/*============================================================================*/
/* fp_t fp16_cos(fp_t x)

Computes the fixed-point cosine of the radian angle x. 

Parameters:

    - X : This is the fixed-point(q16.16) value representing an angle expressed
          in radians. 

Return value:
 
    This function returns cosine of x.
*/
fp_t fp16_cos(fp_t x){
    return fp16_sin( x + FP_PI_DIV_2 );
}
/*============================================================================*/
/* fp_t fp16_tan(fp_t x)

Computes the fixed-point tangent of the radian angle x. 

Parameters:

    - X : This is the fixed-point(q16.16) value representing an angle expressed
          in radians. 

Return value:
 
    This function returns tangent of x.
*/
fp_t fp16_tan(fp_t x){
    return fp16_div( fp16_sin(x), fp16_cos(x) );
}
/*============================================================================*/
/* fp_t fp16_atan2(fp_t y , fp_t yx)

Computes the fixed-point arc tangent in radians of y/x based on the signs of
both values to determine the correct quadrant.

Parameters:

    - X : This is the fixed-point(q16.16) value representing an x-coordinate.
    - Y : This is the fixed-point(q16.16) value representing an y-coordinate.

Return value:
 
    This function returns the principal arc tangent of y/x, in the interval
    [-pi,+pi] radians.
*/
fp_t fp16_atan2(fp_t y , fp_t x){ /*taken from libfixmath*/
    fp_t absy, mask, angle, r, r_3;

    mask = (y >> (sizeof(fp_t)*7));
    absy = (y + mask) ^ mask;
    if (x >= 0){
	r = fp16_div( (x - absy), (x + absy));
	angle = FP_PI_DIV_4;
    } 
    else {
	r = fp16_div( (x + absy), (absy - x));
	angle = FP_3PI_DIV_4;
    }
    r_3 = fp16_mul( fp16_mul(r, r), r );
    angle += fp16_mul(0x00003240 , r_3) - fp16_mul(0x0000FB50,r);
    if (y < 0){
	angle = -angle;
    }
    return angle;
}
/*============================================================================*/
/* fp_t fp16_atan(fp_t x)

Returns the fixed-point arc tangent of x in radians. 

Parameters:

    - X : This is the fixed-point(q16.16) value representing an angle expressed
          in radians. 

Return value:
 
    This function returns arc tangent of x.
*/
fp_t fp16_atan(fp_t x){
    return fp16_atan2(x, fp_unity);
}
/*============================================================================*/
/* fp_t fp16_asin(fp_t x)

Returns the fixed-point arc sine of x in radians. 

Parameters:

    - X : This is the fixed-point(q16.16) value representing an angle expressed
          in radians. 

Return value:
 
    This function returns arc sine of x.
*/
fp_t fp16_asin(fp_t x){
    fp_t RetValue = 0;
    if((x <= fp_unity) && (x >= -fp_unity)){
	RetValue = fp_unity - fp16_mul(x, x);
	RetValue = fp16_div( x, fp16_sqrt(RetValue) );
	RetValue = fp16_atan(RetValue); 
    }
    return RetValue;
}
/*============================================================================*/
/* fp_t fp16_acos(fp_t x)

Returns the fixed-point arc cosine of x in radians. 

Parameters:

    - X : This is the fixed-point(q16.16) value representing an angle expressed
          in radians. 

Return value:
 
    This function returns arc cosine of x.
*/
fp_t fp16_acos(fp_t x){
    return (FP_PI_DIV_2 - fp16_asin(x));
}
/*============================================================================*/
/* fp_t fp16_cosh(fp_t x)

Returns the fixed-point hyperbolic cosine of x. 

Parameters:

    - X : This is the fixed-point(q16.16) value 

Return value:
 
    This function returns hyperbolic cosine of x. 
    If overflow detected returns FP_OVERFLOW. 
    If the function saturates, returns FP_EXP_MAX or FP_EXP_MIN.
*/
fp_t fp16_cosh(fp_t x){
    fp_t epx, enx;
    fp_t RetValue = FP_OVERFLOW;
    
    if( 0 == x){
        RetValue = fp_unity;
    }
    else if( ( x >= FP_EXP_MAX ) || ( x <= FP_EXP_MIN ) ){
        RetValue = fp->max;
    }  
    else{
        epx = fp16_exp( x );
        enx = fp16_exp( -x );
        if( ( FP_OVERFLOW != epx ) && ( FP_OVERFLOW != enx ) ){
            RetValue = epx + enx;
            RetValue = ( RetValue >> 1 );
        }        
    }

    return RetValue;
    
}
/*============================================================================*/
/* fp_t fp16_sinh(fp_t x)

Returns the fixed-point hyperbolic sine of x. 

Parameters:

    - X : This is the fixed-point(q16.16) value 

Return value:
 
    This function returns hyperbolic sine of x. 
    If overflow detected returns FP_OVERFLOW. 
    If the function saturates, returns FP_EXP_MAX or FP_EXP_MIN.
*/
fp_t fp16_sinh(fp_t x){
    fp_t RetValue = FP_OVERFLOW;
    fp_t epx, enx;
    
    if( 0 == x){
        RetValue = fp_unity;
    }
    else if( x >= FP_EXP_MAX ){
        RetValue = fp->max;
    }  
    else if( x <= FP_EXP_MIN ) {
        RetValue = -fp->max;
    }
    else{
        epx = fp16_exp( x );
        enx = fp16_exp( -x );
        if( ( FP_OVERFLOW != epx ) && ( FP_OVERFLOW != enx ) ){
            RetValue = epx - enx;
            RetValue = ( RetValue >> 1 );
        }        
    }
    return RetValue;
}
/*============================================================================*/
/* fp_t fp16_tanh(fp_t x)

Returns the fixed-point hyperbolic tangent of x. 

Parameters:

    - X : This is the fixed-point(q16.16) value 

Return value:
 
    This function returns hyperbolic tangent of x. 
    If overflow detected returns FP_OVERFLOW. 
*/
fp_t fp16_tanh(fp_t x){
    fp_t RetValue = FP_OVERFLOW;
    fp_t epx, enx;
    
    if( 0 == x ){
        RetValue = 0;
    }
    else if ( x > 425984 ){ /* tanh for any x>6.5 ~= 1*/
        RetValue = fp_unity;
    }
    else if (x < -425984 ){ /* tanh for any x<6.5 ~= -1*/
        RetValue = -fp_unity;
    }
    else{
        RetValue = fp16_abs(x);
        epx = fp16_exp( RetValue ); 
        enx = fp16_exp( -RetValue );    
        RetValue = fp16_div( epx - enx, epx + enx );
        RetValue = ( x > 0)? RetValue : -RetValue;
    }
    
    return RetValue;
}
/*============================================================================*/
/* fp_t fp16_polyval(fp_t *p, size_t n, fp_t x)

Evaluates the fixed-point polynomial p at the point  x. The argument p is a
vector of length n+1 whose elements are the coefficients (in descending powers) 
of an nth-degree polynomial. 

Parameters:

    - p : Polynomial coefficients, specified as a fixed-point(q16.16) array.
    - n : the number of elements of the fixed-point array p. 
    - x : This is the fixed-point(q16.16) value to evaluate the polynomial

Return value:
 
    This function returns the polynomial evaluation p(x).
    If overflow detected returns FP_OVERFLOW.  
*/
fp_t fp16_polyval(fp_t *p, size_t n, fp_t x){ 
    fp_t fx;
    fp_t tmp;
    int32_t i;
    /*polynomial evaluation using Horner's method*/
    fx = p[0];
    for( i = 1; i < n; i++){
        tmp = fp16_mul(fx, x); 
        if( FP_OVERFLOW == tmp){
            fx = FP_OVERFLOW;
            break;
        }
        fx =  fp16_add( tmp , p[i] );
    }   
    return fx;     
}
/*============================================================================*/
/*fp_t fp16_ipow(fp_t x, fp_t y)
 
Returns x raised to the power of the integer part of y. (x^y) . 

Parameters:

    - x : This is the fixed-point(q16.16) base value
    - y : This is the fixed-point(q16.16) power value. (Only the integer part
         is taken)

Return value:
 
    This function returns the result of raising x to the power y. 
    FP_OVERFLOW when an operation overflow is detected.
*/
fp_t fp16_ipow(fp_t x, fp_t y){
    fp_t RetValue;
    fp_t n;
    int32_t i;
    
    RetValue = fp_unity;
    n = y >> 16;
    if( 0 == n ){
        RetValue = fp_unity;
    }
    else if ( fp_unity == n ){
        RetValue = x;
    }
    else{
        for(i=0; i < n; i++ ){
            RetValue = fp16_mul( x, RetValue );
            if( FP_OVERFLOW == RetValue ){
                break;
            }
        }
    }
    return RetValue;
}
/*============================================================================*/
/* fp_t fp16_pow(fp_t x, fp_t y)

Returns x raised to the power of y. (x^y) 

Parameters:

    - x : This is the fixed-point(q16.16) base value
    - y : This is the fixed-point(q16.16) power value

Return value:
 
    This function returns the result of raising x to the power y. 
    FP_OVERFLOW when an operation overflow is detected.
*/
fp_t fp16_pow(fp_t x, fp_t y){
    fp_t RetValue = FP_OVERFLOW;
    fp_t tmp;
    
    if( (0 == ( y & 0x0000FFFF) ) && (y > 0)  ){ /*handle integer exponent explicitly*/
        RetValue = fp16_ipow( x, y );
    }
    else{
        tmp = fp16_mul( y, fp16_log( fp16_abs(x) ) );
        if( FP_OVERFLOW != tmp ){
            RetValue = fp16_exp( tmp );
            if( x < 0 ){
                RetValue = -RetValue;
            }
        }        
    }        

    return RetValue;
}
/*============================================================================*/
static char *fp16_itoa(char *buf, uint32_t scale, uint32_t value, uint8_t skip){
    uint32_t digit;  
    while (scale){
        digit = (value / scale);
        if (!skip || digit || ( 1 == scale ) ){
            skip = 0u;
            *buf++ = '0' + digit;
            value %= scale;
        }
        scale /= 10u;
    }
    return buf;
}
/*============================================================================*/
/*char* fp16_fptoA(fp_t value, char *str, int decimals)

Converts the fixed-point value to a formatted string.

Parameters:
 
    - num : Value to be converted to a string.
    - str : Array in memory where to store the resulting null-terminated string.
    - decimals: The number of decimals to show in the string representation.
                Note: Max decimal allowed = 5
 
Return value:
 
    A pointer to the resulting null-terminated string, same as parameter str
*/
char* fp16_fptoA(fp_t num, char *str, int decimals){
    uint32_t uvalue, fpart, scale;
    int32_t ipart;
    const uint32_t itoa_scales[6] = { 1E0, 1E1, 1E2, 1E3, 1E4, 1E5 };
    char *RetValue = str;
    
    if( FP_OVERFLOW == num ){
        str[0] = 'o';
        str[1] = 'v';
        str[2] = 'e';
        str[3] = 'r';
        str[4] = 'f';
        str[5] = 'l';
        str[6] = 'o';
        str[7] = 'w';
        str[8] = '\0';
    }
    else{
        uvalue = (num >= 0) ? num : -num;
        if (num < 0){
            *str++ = '-';
        }

        ipart = uvalue >> 16;
        fpart = uvalue & 0xFFFF;
        if( decimals > 5){
            decimals = 5;
        }
        if( decimals < 0){
            decimals = 0;
        }
        scale = itoa_scales[decimals];
        fpart = fp16_mul(fpart, scale);

        if ( fpart >= scale ){
            ipart++;
            fpart -= scale;    
        }
        str = fp16_itoa(str, 10000, ipart, 1u);

        if (scale != 1){
            *str++ = '.';
            str = fp16_itoa(str, scale / 10, fpart, 0u);
        }
        *str = '\0';        
    }
    return RetValue;
}
/*============================================================================*/
/* fp_t fp16_Atofp(char *s)

Parses the C string s, interpreting its content as a fixed-point(q16.16) number
and returns its value as a fp_t.

The function first discards as many whitespace characters (as in isspace) as 
necessary until the first non-whitespace character is found. Then, starting from
this character, takes as many characters as possible that are valid following a
syntax resembling that of floating point literals, and interprets them as a 
fixed-point numerical value. The rest of the string after the last valid character
is ignored and has no effect on the behavior of this function.

Parameters:
 
    - s : The string beginning with the representation of a floating-point number.
 
Return value:
 
    On success, the function returns the converted floating point number as 
    a fixed-point value.
    If no valid conversion could be performed, the function returns zero (0.0) or
    FP_OVERFLOW.
    If the converted value would be out of the range of representable values by
    a fixed-point Q16.16, the functions returns FP_OVERFLOW.
*/
fp_t fp16_Atofp( char *s ){
    uint8_t negative;
    uint32_t ipart = 0ul, fpart = 0ul, scale = 1ul;
    int32_t count = 0;
    fp_t RetValue = FP_OVERFLOW;
    int point_seen, overflow = 0;
    char c;
    uint32_t digit;
    
    while( isspace( (int)*s) ){
        s++; /*discard whitespaces*/
    }

    negative = ( '-' == *s );
    if( '+' == *s || '-' == *s){
        s++; /*move to the next sign*/
    }

    for( point_seen = 0; '\0' != (c=*s); s++ ){
        if( '.' == c ){
            point_seen = 1;
        }
        else if( isdigit( (int)c ) ){
            digit = (uint32_t)c - (uint32_t)'0';
            if( point_seen ){ /* Decode the fractional part */
                scale *= 10;
                fpart *= 10;
                fpart += digit;
            }
            else{ /* Decode the decimal part */
                ipart *= 10;
                ipart += digit;
                count++;        
                overflow = ( 0 == count || count > 5 || ipart > 32768 || (!negative && ipart > 32767));
                if(overflow){
                    break;
                }
            }
        }
        else{
            break;
        }
    }
    if( 0 == overflow ){
        RetValue = ipart << 16;
        RetValue += fp16_div(fpart, scale);
        RetValue = ( negative )? -RetValue : RetValue;
    }   
    return RetValue;
}