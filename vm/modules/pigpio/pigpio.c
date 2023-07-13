*
 * This file pigpio.c is part of L1vm.
 *
 * (c) Copyright Stefan Pietzonke (jay-t@gmx.net), 2017
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

// pigpio.c Raspberry Pi library
// just the important basic stuff here

#include "../../../include/global.h"
#include "../../../include/stack.h"

#include <pigpio.h>

S2 init_memory_bounds (struct data_info *data_info_orig, S8 data_info_ind_orig)
{
	return (0);
}

// GPIO setup/quit ----------------------------------------

U1 *gpio_setup (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    if (gpioInitialise () < 0)
    {
        // pigpio initialisation failed.

        sp = stpushi (1, sp, sp_bottom);
        if (sp == NULL)
        {
	        // error
	        printf ("gpio_setup: ERROR: stack corrupt!\n");
	        return (NULL);
        }
    }
    else
    {
        // pigpio initialised okay.

        sp = stpushi (0, sp, sp_bottom);
        if (sp == NULL)
        {
	        // error
	        printf ("gpio_setup: ERROR: stack corrupt!\n");
	        return (NULL);
        }
    }
    return (sp);
}

U1 *gpio_quit (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    gpioTerminate ();

    return (sp);
}


// GPIO basic ---------------------------------------------

U1 *gpio_set_mode (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 mode ALIGN;
    S8 pin ALIGN;

    sp = stpopi ((U1 *) &mode, sp, sp_top);
 	if (sp == NULL)
 	{
 		// error
 		printf ("gpio_set_mode: ERROR: stack corrupt!\n");
 		return (NULL);
 	}

    sp = stpopi ((U1 *) &pin, sp, sp_top);
 	if (sp == NULL)
 	{
 		// error
 		printf ("gpio_set_mode: ERROR: stack corrupt!\n");
 		return (NULL);
 	}

    gpioSetMode (pin, mode);

    return (sp);
}

U1 *gpio_get_mode (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 mode ALIGN;
    S8 pin ALIGN;

    sp = stpopi ((U1 *) &pin, sp, sp_top);
 	if (sp == NULL)
 	{
 		// error
 		printf ("gpio_get_mode: ERROR: stack corrupt!\n");
 		return (NULL);
 	}

    mode = gpioGetMode (pin, mode);

    sp = stpushi (mode, sp, sp_bottom);
    if (sp == NULL)
    {
 	   // error
 	   printf ("gpio_get_mode: ERROR: stack corrupt!\n");
 	   return (NULL);
    }

    return (sp);
}

U1 *gpio_pullupdn_control (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 pud ALIGN;
    S8 pin ALIGN;
    S8 ret;

    sp = stpopi ((U1 *) &pud, sp, sp_top);
 	if (sp == NULL)
 	{
 		// error
 		printf ("gpio_pullupdn_control: ERROR: stack corrupt!\n");
 		return (NULL);
 	}

    sp = stpopi ((U1 *) &pin, sp, sp_top);
 	if (sp == NULL)
 	{
 		// error
 		printf ("gpio_pullupdn_control: ERROR: stack corrupt!\n");
 		return (NULL);
 	}

    ret = gpioSetPullUpDown (pin, pud);

    sp = stpushi (ret, sp, sp_bottom);
    if (sp == NULL)
    {
 	   // error
 	   printf ("gpio_pullupdn_control: ERROR: stack corrupt!\n");
 	   return (NULL);
    }

    return (sp);
}

U1 *gpio_digital_write (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// arguments: pin, value

    S8 value ALIGN;
    S8 pin ALIGN;

    sp = stpopi ((U1 *) &value, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gpio_digital_write: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &pin, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gpio_digital_write: ERROR: stack corrupt!\n");
		return (NULL);
	}

    gpioWrite (pin, value);
    return (sp);
}

U1 *gpio_digital_read (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// arguments: pin
	// return: value

	S8 value ALIGN;
	S8 pin ALIGN;

	sp = stpopi ((U1 *) &pin, sp, sp_top);
   	if (sp == NULL)
    {
	    // error
	    printf ("gpio_digital_read: ERROR: stack corrupt!\n");
	    return (NULL);
    }

     value = gpioRead (pin);
   
    // printf ("gpio_digital_read: pin: %lli, value: %lli\n", pin, value);
   
    sp = stpushi (value, sp, sp_bottom);
    if (sp == NULL)
    {
	    // error
	    printf ("gpio_digital_read: ERROR: stack corrupt!\n");
	    return (NULL);
    }

   return (sp);
}


// PWM ----------------------------------------------------

U1 *gpio_pwm (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 pwm ALIGN;
    S8 pin ALIGN;

    sp = stpopi ((U1 *) &pwm, sp, sp_top);
 	if (sp == NULL)
 	{
 		// error
 		printf ("gpio_pwm: ERROR: stack corrupt!\n");
 		return (NULL);
 	}

    sp = stpopi ((U1 *) &pin, sp, sp_top);
 	if (sp == NULL)
 	{
 		// error
 		printf ("gpio_pwm: ERROR: stack corrupt!\n");
 		return (NULL);
 	}

    gpioPWM (pin, pwm);

    return (sp);
}

U1 *gpio_set_pwm_freq (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 freq ALIGN;
    S8 pin ALIGN;
    S8 ret ALIGN;

    sp = stpopi ((U1 *) &freq, sp, sp_top);
 	if (sp == NULL)
 	{
 		// error
 		printf ("gpio_set_pwm_freq: ERROR: stack corrupt!\n");
 		return (NULL);
 	}

    sp = stpopi ((U1 *) &pin, sp, sp_top);
 	if (sp == NULL)
 	{
 		// error
 		printf ("gpio_set_pwm_freq: ERROR: stack corrupt!\n");
 		return (NULL);
 	}

    ret = gpioSetPWMfrequency (pin, freq);

    sp = stpushi (ret, sp, sp_bottom);
    if (sp == NULL)
    {
	    // error
	    printf ("gpio_set_pwm_freq: ERROR: stack corrupt!\n");
	    return (NULL);
    }

    return (sp);
}

U1 *gpio_set_pwm_range (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 range ALIGN;
    S8 pin ALIGN;
    S8 ret ALIGN;

    sp = stpopi ((U1 *) &range, sp, sp_top);
 	if (sp == NULL)
 	{
 		// error
 		printf ("gpio_set_pwm_range: ERROR: stack corrupt!\n");
 		return (NULL);
 	}

    sp = stpopi ((U1 *) &pin, sp, sp_top);
 	if (sp == NULL)
 	{
 		// error
 		printf ("gpio_set_pwm_range: ERROR: stack corrupt!\n");
 		return (NULL);
 	}

    ret = gpioSetPWMrange (pin, range);

    sp = stpushi (ret, sp, sp_bottom);
    if (sp == NULL)
    {
	    // error
	    printf ("gpio_set_pwm_range: ERROR: stack corrupt!\n");
	    return (NULL);
    }

    return (sp);
}


// servo --------------------------------------------------

U1 *gpio_servo (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 pulse ALIGN;
    S8 pin ALIGN;
    S8 ret ALIGN;

    sp = stpopi ((U1 *) &pulse, sp, sp_top);
 	if (sp == NULL)
 	{
 		// error
 		printf ("gpio_servo: ERROR: stack corrupt!\n");
 		return (NULL);
 	}

    sp = stpopi ((U1 *) &pin, sp, sp_top);
 	if (sp == NULL)
 	{
 		// error
 		printf ("gpio_servo: ERROR: stack corrupt!\n");
 		return (NULL);
 	}

    ret = gpioServo (pin, pulse);

    sp = stpushi (ret, sp, sp_bottom);
    if (sp == NULL)
    {
	    // error
	    printf ("gpio_servo: ERROR: stack corrupt!\n");
	    return (NULL);
    }

    return (sp);
}

U1 *gpio_get_servo (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 pulse ALIGN;
    S8 pin ALIGN;

    sp = stpopi ((U1 *) &pin, sp, sp_top);
 	if (sp == NULL)
 	{
 		// error
 		printf ("gpio_get_servo: ERROR: stack corrupt!\n");
 		return (NULL);
 	}

    pulse = gpioGetServoPulsewidth (pin);

    sp = stpushi (pulse, sp, sp_bottom);
    if (sp == NULL)
    {
	    // error
	    printf ("gpio_get_servo: ERROR: stack corrupt!\n");
	    return (NULL);
    }

    return (sp);
}


// I2C ----------------------------------------------------

U1 *gpio_i2c_open (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 handle ALIGN;
    S8 bus ALIGN;
    S8 addr ALIGN;
    S8 flags ALIGN;

    sp = stpopi ((U1 *) &flags, sp, sp_top);
 	if (sp == NULL)
 	{
 		// error
 		printf ("gpio_i2c_open: ERROR: stack corrupt!\n");
 		return (NULL);
 	}

    sp = stpopi ((U1 *) &addr, sp, sp_top);
 	if (sp == NULL)
 	{
 		// error
 		printf ("gpio_i2c_open: ERROR: stack corrupt!\n");
 		return (NULL);
 	}

    sp = stpopi ((U1 *) &bus, sp, sp_top);
 	if (sp == NULL)
 	{
 		// error
 		printf ("gpio_i2c_open: ERROR: stack corrupt!\n");
 		return (NULL);
 	}

    handle = i2cOpen (bus, addr, flags);

    sp = stpushi (handle, sp, sp_bottom);
    if (sp == NULL)
    {
	    // error
	    printf ("gpio_i2c_open: ERROR: stack corrupt!\n");
	    return (NULL);
    }

    return (sp);
}

U1 *gpio_i2c_close (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 handle ALIGN;

    sp = stpopi ((U1 *) &handle, sp, sp_top);
 	if (sp == NULL)
 	{
 		// error
 		printf ("gpio_i2c_close: ERROR: stack corrupt!\n");
 		return (NULL);
 	}

    i2cClose (handle);

    return (sp);
}

U1 *gpio_i2c_read_byte (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 handle ALIGN;
    S8 reg ALIGN;
    S8 ret ALIGN;

    sp = stpopi ((U1 *) &reg, sp, sp_top);
 	if (sp == NULL)
 	{
 		// error
 		printf ("gpio_i2c_read_byte: ERROR: stack corrupt!\n");
 		return (NULL);
 	}

    sp = stpopi ((U1 *) &handle, sp, sp_top);
 	if (sp == NULL)
 	{
 		// error
 		printf ("gpio_i2c_read_byte: ERROR: stack corrupt!\n");
 		return (NULL);
 	}

    ret = i2cReadByteData (handle, reg);

    sp = stpushi (ret, sp, sp_bottom);
    if (sp == NULL)
    {
	    // error
	    printf ("gpio_i2c_read_byte: ERROR: stack corrupt!\n");
	    return (NULL);
    }

    return (sp);
}

U1 *gpio_i2c_read_word (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 handle ALIGN;
    S8 reg ALIGN;
    S8 ret ALIGN;

    sp = stpopi ((U1 *) &reg, sp, sp_top);
 	if (sp == NULL)
 	{
 		// error
 		printf ("gpio_i2c_read_word: ERROR: stack corrupt!\n");
 		return (NULL);
 	}

    sp = stpopi ((U1 *) &handle, sp, sp_top);
 	if (sp == NULL)
 	{
 		// error
 		printf ("gpio_i2c_read_word: ERROR: stack corrupt!\n");
 		return (NULL);
 	}

    ret = i2cReadWordData (handle, reg);

    sp = stpushi (ret, sp, sp_bottom);
    if (sp == NULL)
    {
	    // error
	    printf ("gpio_i2c_read_word: ERROR: stack corrupt!\n");
	    return (NULL);
    }

    return (sp);
}

U1 *gpio_i2c_write_byte (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 handle ALIGN;
    S8 reg ALIGN;
    S8 byte ALIGN;
    S8 ret ALIGN;

    sp = stpopi ((U1 *) &byte, sp, sp_top);
 	if (sp == NULL)
 	{
 		// error
 		printf ("gpio_i2c_write_byte: ERROR: stack corrupt!\n");
 		return (NULL);
 	}    

    sp = stpopi ((U1 *) &reg, sp, sp_top);
 	if (sp == NULL)
 	{
 		// error
 		printf ("gpio_i2c_write_byte: ERROR: stack corrupt!\n");
 		return (NULL);
 	}

    sp = stpopi ((U1 *) &handle, sp, sp_top);
 	if (sp == NULL)
 	{
 		// error
 		printf ("gpio_i2c_write_byte: ERROR: stack corrupt!\n");
 		return (NULL);
 	}

    ret =  i2cWriteByteData (handle, reg, byte);

    sp = stpushi (ret, sp, sp_bottom);
    if (sp == NULL)
    {
	    // error
	    printf ("gpio_i2c_write_byte: ERROR: stack corrupt!\n");
	    return (NULL);
    }

    return (sp);
}

U1 *gpio_i2c_write_word (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 handle ALIGN;
    S8 reg ALIGN;
    S8 word ALIGN;
    S8 ret ALIGN;

    sp = stpopi ((U1 *) &word, sp, sp_top);
 	if (sp == NULL)
 	{
 		// error
 		printf ("gpio_i2c_write_word: ERROR: stack corrupt!\n");
 		return (NULL);
 	}    

    sp = stpopi ((U1 *) &reg, sp, sp_top);
 	if (sp == NULL)
 	{
 		// error
 		printf ("gpio_i2c_write_word: ERROR: stack corrupt!\n");
 		return (NULL);
 	}

    sp = stpopi ((U1 *) &handle, sp, sp_top);
 	if (sp == NULL)
 	{
 		// error
 		printf ("gpio_i2c_write_word: ERROR: stack corrupt!\n");
 		return (NULL);
 	}

    ret =  i2cWriteWordData (handle, reg, word);

    sp = stpushi (ret, sp, sp_bottom);
    if (sp == NULL)
    {
	    // error
	    printf ("gpio_i2c_write_word: ERROR: stack corrupt!\n");
	    return (NULL);
    }

    return (sp);
}
