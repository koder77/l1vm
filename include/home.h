/*
 * This file home.h is part of L1vm.
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

size_t strlen_safe (const char * str, int maxlen);

// get home directory path
char *get_home (void)
{
	char *home_name;

	if (strlen_safe (SANDBOX_HOME, MAXLINELEN) > 0)
	{
		// user defined own sandbox home directory
		home_name = SANDBOX_HOME;
	}
	else
	{
		home_name = getenv ("HOME");
	}
	return (home_name);
}
