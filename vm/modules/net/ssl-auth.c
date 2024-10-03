/*
 * This file ssl-auth.c is part of L1vm.
 *
 * (c) Copyright Stefan Pietzonke (jay-t@gmx.net), 2020
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

// authenticate via username + password by curl
// send a string data and get an answer string back
// return value is 0 on success or 1 on error
// on error an empty string is copied back!

#include "../../../include/global.h"

#if __linux__ || __HAIKU__
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

#include <curl/curl.h>

// protos
U1 *stpushi (S8 data, U1 *sp, U1 *sp_bottom);
U1 *stpopii (S8 data, U1 *sp, U1 *sp_bottom);

size_t strlen_safe (const char *str, S8 maxlen);

// protos
S2 memory_bounds (S8 start, S8 offset_access);

static size_t write_memory_callback(void *ptr, size_t size, size_t nmemb, void *data)
{
    char **buffer = (char **)data;
    size_t buffer_size = *buffer ? strlen(*buffer) : 0;
    size_t new_size = buffer_size + size * nmemb + 1; // +1 for null terminator

    *buffer = realloc(*buffer, new_size);
    if (!*buffer) {
        return 0; // Memory allocation failed
    }

    memcpy(*buffer + buffer_size, ptr, size * nmemb);
    (*buffer)[new_size - 1] = '\0'; // Null terminate the string

    return size * nmemb;
}

U1 *ssl_auth_send (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 url_addr ALIGN;
    S8 name_addr ALIGN;
    S8 password_addr ALIGN;
    S8 return_buffer_addr ALIGN;
    S8 buffer_size ALIGN;
    S8 ret_code ALIGN = 0;
    S8 send_data_addr ALIGN;

    CURL *curl;
    CURLcode res;
    char *buffer = NULL;

    if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: ssl_auth_send: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

    sp = stpopi ((U1 *) &return_buffer_addr, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("ssl_auth_send: ERROR: stack corrupt!\n");
        return (NULL);
    }

    sp = stpopi ((U1 *) &send_data_addr, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("ssl_auth_send: ERROR: stack corrupt!\n");
        return (NULL);
    }

    sp = stpopi ((U1 *) &password_addr, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("ssl_auth_send: ERROR: stack corrupt!\n");
        return (NULL);
    }

    sp = stpopi ((U1 *) &name_addr, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("ssl_auth_send: ERROR: stack corrupt!\n");
        return (NULL);
    }

    sp = stpopi ((U1 *) &url_addr, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("ssl_auth_send: ERROR: stack corrupt!\n");
        return (NULL);
    }

    curl_global_init (CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init ();
    if(curl)
    {
        curl_easy_setopt (curl, CURLOPT_URL, &data[url_addr]);
        curl_easy_setopt (curl, CURLOPT_USERNAME, &data[name_addr]);
        curl_easy_setopt (curl, CURLOPT_PASSWORD, &data[password_addr]);
        curl_easy_setopt (curl, CURLOPT_TIMEOUT, 5L); // 5 seconds
        curl_easy_setopt (curl, CURLOPT_WRITEFUNCTION, write_memory_callback);
        curl_easy_setopt (curl, CURLOPT_CAINFO, "../localhost.pem");
        curl_easy_setopt (curl, CURLOPT_WRITEDATA, &buffer);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, &data[send_data_addr]);

        curl_easy_setopt (curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
        curl_easy_setopt (curl, CURLOPT_SSL_CIPHER_LIST, "ECDHE+AESGCM:ECDHE+CHACHA20:DH+AESGCM:DH+CHACHA20");

        res = curl_easy_perform (curl);
        if (res != CURLE_OK)
        {
            fprintf(stderr, "cURL error: %s\n", curl_easy_strerror(res));

            // return 1 as error code!
            ret_code = 1;
        }
        else
        {
            long http_code;
            curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &http_code);
            //printf("Status: %ld\n", http_code);
            //printf("Body: %s\n", buffer);

            if (http_code == 200)
            {
                // got HTTP OK
                buffer_size = strlen_safe (buffer, STRINGMOD_MAXSTRLEN);

                if (memory_bounds (return_buffer_addr, buffer_size) != 0)
                {
                    printf ("ssl_auth_send: ERROR: dest string overflow!\nNeed %lli bytes as buffer!\n", buffer_size);
                    ret_code = 1;

                    // return empty string;
                    strcpy ((char *) &data[return_buffer_addr], "");
                }
                else
                {
                    // return string variable is big enough - copy string
                    strcpy ((char *) &data[return_buffer_addr], buffer);
                }
            }
            else
            {
                // HTTP not OK!
                ret_code = 1;

                // return empty string;
                strcpy ((char *) &data[return_buffer_addr], "");
            }

            free (buffer);
            curl_easy_cleanup (curl);
        }
    }

    curl_global_cleanup();

    sp = stpushi (ret_code, sp, sp_bottom);
	if (sp == NULL)
	{
		// ERROR:
		printf ("ssl_auth_send: ERROR: stack corrupt!\n");
		return (NULL);
	}
    return (sp);
}
