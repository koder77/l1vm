/*
 * This file gps.c is part of L1vm.
 *
 * (c) Copyright Stefan Pietzonke (info@midnight-coding.de), 2025
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

// GPSD GPS receiver module

#include "../../../include/global.h"
#include "../../../include/stack.h"

#include <gps.h>

#define SERVER_NAME "localhost"
#define SERVER_PORT "2947"
#define MS_TO_KMH 3.6

struct gps_data_t g_gpsdata;
int ret;
int gps_init = 0;

// protos
S2 memory_bounds (S8 start, S8 offset_access);

struct data_info data_info[MAXDATAINFO];
S8 data_info_ind;

S2 init_memory_bounds (struct data_info *data_info_orig, S8 data_info_ind_orig)
{
    memcpy (&data_info, &data_info_orig, sizeof (data_info_orig));
    data_info_ind = data_info_ind_orig;

    return (0);
}

U1 *gps_setup (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    ret = gps_open (SERVER_NAME, SERVER_PORT, &g_gpsdata);
    if (ret != 0)
    {
        //printf("[GPS] Can't open...bye!\r\n");
        sp = stpushi (1, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("gps_setup: ERROR: stack corrupt!\n");
            return (NULL);
        }
    }
    else
    {
        // GPS open
        gps_init = 1;
        gps_stream (&g_gpsdata, WATCH_ENABLE | WATCH_JSON | WATCH_TIMING, NULL);

        sp = stpushi (0, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("gps_setup: ERROR: stack corrupt!\n");
            return (NULL);
        }
    }
    return (sp);
}

U1 *gps_quit (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    if (gps_init == 1)
    {
        gps_stream(&g_gpsdata, WATCH_DISABLE, NULL);
        gps_close(&g_gpsdata);

        gps_init = 0; // closed
    }

    return (sp);
}

U1 *gps_read_data (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    F8 latitude ALIGN = 0.0;
    F8 longitude ALIGN = 0.0;
    F8 altitude ALIGN = 0.0;
    F8 speed_kmh ALIGN = 0.0;
    F8 track ALIGN = 0.0;
    S8 satellites_used ALIGN = 0;
    S8 timestrptr ALIGN;
    S2 result = 0;

    sp = stpopi ((U1 *) &timestrptr, sp, sp_top);
    if (sp == NULL)
    {
        printf ("gps_read_data: ERROR: stack corrupt on input!\n");
        return (NULL);
    }

    if (memory_bounds (timestrptr, 30) != 0)
    {
        printf ("gps_read_data: ERROR: String pointer out of bounds!\n");
        return (NULL); // FATAL error time string overflow
    }

    if (gps_waiting (&g_gpsdata, 50000))
    {
        if (-1 == gps_read (&g_gpsdata, NULL, 0))
        {
            // return empty data
            sp = stpushi (satellites_used, sp, sp_bottom);
            if (sp == NULL)
            {
                // error
                printf ("gps_read_data: ERROR: stack corrupt!\n");
                return (NULL);
            }

            sp = stpushd (speed_kmh, sp, sp_bottom);
            if (sp == NULL)
            {
                // error
                printf ("gps_read_data: ERROR: stack corrupt!\n");
                return (NULL);
            }

            sp = stpushd (track, sp, sp_bottom);
            if (sp == NULL)
            {
                // error
                printf ("gps_read_data: ERROR: stack corrupt!\n");
                return (NULL);
            }

            sp = stpushd (altitude, sp, sp_bottom);
            if (sp == NULL)
            {
                // error
                printf ("gps_read_data: ERROR: stack corrupt!\n");
                return (NULL);
            }

            sp = stpushd (longitude, sp, sp_bottom);
            if (sp == NULL)
            {
                // error
                printf ("gps_read_data: ERROR: stack corrupt!\n");
                return (NULL);
            }

            sp = stpushd (latitude, sp, sp_bottom);
            if (sp == NULL)
            {
                // error
                printf ("gps_read_data: ERROR: stack corrupt!\n");
                return (NULL);
            }

            // error code no GPS read possible!
            sp = stpushi (5, sp, sp_bottom);
            if (sp == NULL)
            {
                // error
                printf ("gps_read_data: ERROR: stack corrupt!\n");
                return (NULL);
            }

            data[timestrptr] = '\0';   // empty time string

            return (sp);    // exit
        }
        else
        {
            if (g_gpsdata.fix.mode >= MODE_2D)
            {
                if (isfinite(g_gpsdata.fix.latitude) && isfinite(g_gpsdata.fix.longitude) && isfinite(g_gpsdata.fix.altitude) && isfinite(g_gpsdata.fix.speed) && isfinite(g_gpsdata.fix.track))
                {
                    latitude = g_gpsdata.fix.latitude;
                    longitude = g_gpsdata.fix.longitude;
                    altitude = g_gpsdata.fix.altitude;

                    speed_kmh = g_gpsdata.fix.speed * MS_TO_KMH;
                    track = g_gpsdata.fix.track;
                    satellites_used = g_gpsdata.satellites_used;

                    time_t rawtime = g_gpsdata.fix.time.tv_sec;
                    char time_buffer[30];

                    strncpy(time_buffer, ctime(&rawtime), sizeof(time_buffer) - 1);
                    if (time_buffer[strlen(time_buffer) - 1] == '\n')
                    {
                        time_buffer[strlen(time_buffer) - 1] = '\0';
                    }

                    memcpy((U1*) data + timestrptr, time_buffer, strlen(time_buffer) + 1);

                    result = 0; // success
                }
                else
                {
                    result = 3; // Fix, but no valid data (NaN)
                }
            }
            else
            {
                result = 1; // No 2D/3D Fix
            }
        }
    }
    else
    {
        usleep(10000);  // short break
        result = 4; // No new data
    }

    // return data
    sp = stpushi (satellites_used, sp, sp_bottom);
    if (sp == NULL)
    {
        // error
        printf ("gps_read_data: ERROR: stack corrupt!\n");
        return (NULL);
    }

    sp = stpushd (speed_kmh, sp, sp_bottom);
    if (sp == NULL)
    {
        // error
        printf ("gps_read_data: ERROR: stack corrupt!\n");
        return (NULL);
    }

    sp = stpushd (track, sp, sp_bottom);
    if (sp == NULL)
    {
        // error
        printf ("gps_read_data: ERROR: stack corrupt!\n");
        return (NULL);
    }

    sp = stpushd (altitude, sp, sp_bottom);
    if (sp == NULL)
    {
         // error
        printf ("gps_read_data: ERROR: stack corrupt!\n");
        return (NULL);
    }

    sp = stpushd (longitude, sp, sp_bottom);
    if (sp == NULL)
    {
        // error
        printf ("gps_read_data: ERROR: stack corrupt!\n");
        return (NULL);
    }

    sp = stpushd (latitude, sp, sp_bottom);
    if (sp == NULL)
    {
        // error
        printf ("gps_read_data: ERROR: stack corrupt!\n");
        return (NULL);
    }

    // error code
    sp = stpushi (result, sp, sp_bottom);
    if (sp == NULL)
    {
        // error
        printf ("gps_read_data: ERROR: stack corrupt!\n");
        return (NULL);
    }

    return (sp);
}

// functions to handle algorithms

// distance between two points on earth
#define R 6371.0 // Radius of the Earth in kilometers

// Function to convert degrees to radians
double toRadians (double degree)
{
    return (degree * (M_PI / 180.0));
}

double toDegrees (double radians)
{
    return (radians * 180.0 / M_PI);
}

// Function to calculate distance using Haversine formula
double haversine (double lat1, double lon1, double lat2, double lon2)
{
    double dLat ALIGN = toRadians(lat2 - lat1);
    double dLon ALIGN = toRadians(lon2 - lon1);

    lat1 = toRadians(lat1);
    lat2 = toRadians(lat2);

    double a ALIGN = sin(dLat/2) * sin(dLat/2) +
               cos(lat1) * cos(lat2) *
               sin(dLon/2) * sin(dLon/2);
    double c ALIGN = 2 * atan2(sqrt(a), sqrt(1-a));

    return R * c; // Distance in kilometers
}

U1 *gps_haversine_distance (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    F8 lat1 ALIGN;
    F8 lon1 ALIGN;
    F8 lat2 ALIGN;
    F8 lon2 ALIGN;
    F8 distance_km ALIGN;

    sp = stpopd ((U1 *) &lon2, sp, sp_top);
    if (sp == NULL)
    {
        printf ("gps_haversine_distance: ERROR: stack corrupt on input!\n");
        return (NULL);
    }

    sp = stpopd ((U1 *) &lat2, sp, sp_top);
    if (sp == NULL)
    {
        printf ("gps_haversine_distance: ERROR: stack corrupt on input!\n");
        return (NULL);
    }

    sp = stpopd ((U1 *) &lon1, sp, sp_top);
    if (sp == NULL)
    {
        printf ("gps_haversine_distance: ERROR: stack corrupt on input!\n");
        return (NULL);
    }

    sp = stpopd ((U1 *) &lat1, sp, sp_top);
    if (sp == NULL)
    {
        printf ("gps_haversine_distance: ERROR: stack corrupt on input!\n");
        return (NULL);
    }

    distance_km = haversine (lat1, lon1, lat2, lon2);

    sp = stpushd (distance_km, sp, sp_bottom);
    if (sp == NULL)
    {
        // error
        printf ("gps_haversine_distance: ERROR: stack corrupt!\n");
        return (NULL);
    }

    return (sp);
}

// bearing to a destination point
double bearing (double lat, double lon, double lat2, double lon2)
{
    double teta1 ALIGN = toRadians (lat);
    double teta2 ALIGN = toRadians (lat2);
    // double delta1 = toRadians (lat2-lat);
    double delta2 ALIGN = toRadians (lon2-lon);

    //==================Heading Formula Calculation================//

    double y ALIGN = sin(delta2) * cos(teta2);
    double x ALIGN = cos(teta1)*sin(teta2) - sin(teta1)*cos(teta2)*cos(delta2);
    double brng ALIGN = atan2(y,x);
    brng = toDegrees(brng);// radians to degrees
    brng = (((int)brng + 360) % 360 );

    return brng;
}

U1 *gps_bearing (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    F8 lat1 ALIGN = 0.0;
    F8 lon1 ALIGN = 0.0;
    F8 lat2 ALIGN = 0.0;
    F8 lon2 ALIGN = 0.0;
    F8 bearing_deg ALIGN = 0.0;

    sp = stpopd ((U1 *) &lon2, sp, sp_top);
    if (sp == NULL)
    {
        printf ("gps_bearing: ERROR: stack corrupt on input!\n");
        return (NULL);
    }

    sp = stpopd ((U1 *) &lat2, sp, sp_top);
    if (sp == NULL)
    {
        printf ("gps_bearing: ERROR: stack corrupt on input!\n");
        return (NULL);
    }

    sp = stpopd ((U1 *) &lon1, sp, sp_top);
    if (sp == NULL)
    {
        printf ("gps_bearing: ERROR: stack corrupt on input!\n");
        return (NULL);
    }

    sp = stpopd ((U1 *) &lat1, sp, sp_top);
    if (sp == NULL)
    {
        printf ("gps_bearing: ERROR: stack corrupt on input!\n");
        return (NULL);
    }

    bearing_deg = bearing (lat1, lon1, lat2, lon2);

    sp = stpushd (bearing_deg, sp, sp_bottom);
    if (sp == NULL)
    {
        // error
        printf ("gps_bearing: ERROR: stack corrupt!\n");
        return (NULL);
    }

    return (sp);
}
