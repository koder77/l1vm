/*
 * This file gpio.c is part of L1vm.
 *
 * (c) Copyright Stefan Pietzonke (info@midnight-coding.de), 2017
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

// gpio.c Raspberry Pi library

#include "../../../include/global.h"
#include "../../../include/stack.h"

#include <wiringPi.h>
#include <wiringPiI2C.h>

S2 init_memory_bounds (struct data_info *data_info_orig, S8 data_info_ind_orig)
{
	return (0);
}


// GPIO ------------------------------------------------------

U1 *gpio_setup (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	wiringPiSetupSys ();
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

    digitalWrite (pin, value);
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

   value = digitalRead (pin);
   
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

U1 *gpio_pwm_write (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// arguments: pin, value

	S8 value ALIGN;
    S8 pin ALIGN;

    sp = stpopi ((U1 *) &value, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gpio_pwm_write: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &pin, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gpio_pwm_write: ERROR: stack corrupt!\n");
		return (NULL);
	}

	pwmWrite (pin, value);

	return (sp);
}

 U1 *gpio_pwm_Set_mode (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
 {
	 // arguments: mode

	S8 mode ALIGN;

	sp = stpopi ((U1 *) &mode, sp, sp_top);
 	if (sp == NULL)
 	{
 		// error
 		printf ("gpio_pwm_set_mode: ERROR: stack corrupt!\n");
 		return (NULL);
 	}

	pwmSetMode (mode);

	return (sp);
 }

U1 *gpio_pwm_set_range (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// arguments: range

	S8 range ALIGN;

	sp = stpopi ((U1 *) &range, sp, sp_top);
 	if (sp == NULL)
 	{
 		// error
 		printf ("gpio_pwm_set_range: ERROR: stack corrupt!\n");
 		return (NULL);
 	}

	pwmSetMode (range);

	return (sp);
}

U1 *gpio_pwm_set_clock (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// arguments: divisor

	S8 divisor ALIGN;

	sp = stpopi ((U1 *) &divisor, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gpio_pwm_set_clock: ERROR: stack corrupt!\n");
		return (NULL);
	}

	pwmSetMode (divisor);

	return (sp);
}

U1 *gpio_pullupdn_control (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// arguments: pin, pud

	S8 pin ALIGN;
	S8 pud ALIGN;

	sp = stpopi ((U1 *) &pud, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gpio_ppullupdn_control: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &pin, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gpio_ppullupdn_control: ERROR: stack corrupt!\n");
		return (NULL);
	}

	pullUpDnControl (pin, pud);

	return (sp);
}

U1 *gpio_pinmode (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// arguments: pin, mode

	S8 pin ALIGN;
	S8 mode ALIGN;

	sp = stpopi ((U1 *) &mode, sp, sp_top);
 	if (sp == NULL)
 	{
 		// error
 		printf ("gpio_pinmode: ERROR: stack corrupt!\n");
 		return (NULL);
 	}

	sp = stpopi ((U1 *) &pin, sp, sp_top);
 	if (sp == NULL)
 	{
 		// error
 		printf ("gpio_pinmode: ERROR: stack corrupt!\n");
 		return (NULL);
 	}

	pinMode (pin, mode);

	return (sp);
}

// I2C -------------------------------------------------------

U1 *i2c_setup (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// arguments: id
	// return: fd

	S8 id ALIGN;
	S8 fd ALIGN;

	sp = stpopi ((U1 *) &id, sp, sp_top);
 	if (sp == NULL)
 	{
 		// error
 		printf ("i2c_setup: ERROR: stack corrupt!\n");
 		return (NULL);
 	}

	if ((fd = wiringPiI2CSetup (id)) < 0)
	{
		printf ("i2c_setup: ERROR: opening I2C bus!\n");
	}

	sp = stpushi (fd, sp, sp_bottom);
    if (sp == NULL)
    {
 	   // error
 	   printf ("i2c_setup: ERROR: stack corrupt!\n");
 	   return (NULL);
    }

	return (sp);
}

U1 *i2c_read (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// arguments: fd
	// return: ret

	S8 fd ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &fd, sp, sp_top);
 	if (sp == NULL)
 	{
 		// error
 		printf ("i2c_setup: ERROR: stack corrupt!\n");
 		return (NULL);
 	}

	ret = wiringPiI2CRead (fd);
	if (ret < 0)
	{
		printf ("i2c_read: ERROR!\n");
	}

	sp = stpushi (ret, sp, sp_bottom);
    if (sp == NULL)
    {
 	   // error
 	   printf ("i2c_setup: ERROR: stack corrupt!\n");
 	   return (NULL);
    }

	return (sp);
}

U1 *i2c_write (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// arguments: fd, data
	// return: ret

	S8 fd ALIGN;
	S8 datav ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &datav, sp, sp_top);
 	if (sp == NULL)
 	{
 		// error
 		printf ("i2c_write: ERROR: stack corrupt!\n");
 		return (NULL);
 	}

	sp = stpopi ((U1 *) &fd, sp, sp_top);
 	if (sp == NULL)
 	{
 		// error
 		printf ("i2c_write: ERROR: stack corrupt!\n");
 		return (NULL);
 	}

	ret = wiringPiI2CWrite (fd, datav);
	if (ret < 0)
	{
		printf ("i2c_write: ERROR!\n");
		return (NULL);
	}

	return (sp);
}

U1 *i2c_writereg8 (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// arguments: fd, reg, data

	S8 fd ALIGN;
	S8 datav ALIGN;
	S8 reg ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &datav, sp, sp_top);
 	if (sp == NULL)
 	{
 		// error
 		printf ("i2c_writereg8: ERROR: stack corrupt!\n");
 		return (NULL);
 	}

	sp = stpopi ((U1 *) &reg, sp, sp_top);
 	if (sp == NULL)
 	{
 		// error
 		printf ("i2c_writereg8: ERROR: stack corrupt!\n");
 		return (NULL);
 	}

	sp = stpopi ((U1 *) &fd, sp, sp_top);
 	if (sp == NULL)
 	{
 		// error
 		printf ("i2c_writereg8: ERROR: stack corrupt!\n");
 		return (NULL);
 	}

	ret = wiringPiI2CWriteReg8 (fd, reg, datav);
	if (ret < 0)
	{
		printf ("i2c_writereg8: ERROR!\n");
		return (NULL);
	}

	return (sp);
}

U1 *i2c_writereg16 (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// arguments: fd, reg, data

	S8 fd ALIGN;
	S8 datav ALIGN;
	S8 reg ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &datav, sp, sp_top);
 	if (sp == NULL)
 	{
 		// error
 		printf ("i2c_writereg16: ERROR: stack corrupt!\n");
 		return (NULL);
 	}

	sp = stpopi ((U1 *) &reg, sp, sp_top);
 	if (sp == NULL)
 	{
 		// error
 		printf ("i2c_writereg16: ERROR: stack corrupt!\n");
 		return (NULL);
 	}

	sp = stpopi ((U1 *) &fd, sp, sp_top);
 	if (sp == NULL)
 	{
 		// error
 		printf ("i2c_writereg16: ERROR: stack corrupt!\n");
 		return (NULL);
 	}

	ret = wiringPiI2CWriteReg16 (fd, reg, datav);
	if (ret < 0)
	{
		printf ("i2c_writereg16: ERROR!\n");
		return (NULL);
	}

	return (sp);
}

U1 *i2c_readreg8 (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// arguments: fd, reg
	// return: ret

	S8 fd ALIGN;
	S8 reg ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &reg, sp, sp_top);
 	if (sp == NULL)
 	{
 		// error
 		printf ("i2c_readreg8: ERROR: stack corrupt!\n");
 		return (NULL);
 	}

	sp = stpopi ((U1 *) &fd, sp, sp_top);
 	if (sp == NULL)
 	{
 		// error
 		printf ("i2c_readreg8: ERROR: stack corrupt!\n");
 		return (NULL);
 	}

	ret = wiringPiI2CReadReg8 (fd, reg);
	if (ret < 0)
	{
		printf ("i2c_readreg8: ERROR!\n");
		return (NULL);
	}

	sp = stpushi (ret, sp, sp_bottom);
	if (sp == NULL)
	{
	   // error
	   printf ("i2c_readreg8: ERROR: stack corrupt!\n");
	   return (NULL);
	}

	return (sp);
}

U1 *i2c_readreg16 (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// arguments: fd, reg
	// return: ret

	S8 fd ALIGN;
	S8 reg ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &reg, sp, sp_top);
 	if (sp == NULL)
 	{
 		// error
 		printf ("i2c_readreg16: ERROR: stack corrupt!\n");
 		return (NULL);
 	}

	sp = stpopi ((U1 *) &fd, sp, sp_top);
 	if (sp == NULL)
 	{
 		// error
 		printf ("i2c_readreg16: ERROR: stack corrupt!\n");
 		return (NULL);
 	}

	ret = wiringPiI2CReadReg16 (fd, reg);
	if (ret < 0)
	{
		printf ("i2c_readreg16: ERROR!\n");
		return (NULL);
	}

	sp = stpushi (ret, sp, sp_bottom);
	if (sp == NULL)
	{
	   // error
	   printf ("i2c_readreg16: ERROR: stack corrupt!\n");
	   return (NULL);
	}

	return (sp);
}
