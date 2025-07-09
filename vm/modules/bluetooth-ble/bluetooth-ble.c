/*
 * This file bluetooth-ble.c is part of L1vm.
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

// code taken from the bluetoothble project example code
// and modified to fit

#include "../../../include/global.h"
#include "../../../include/stack.h"

#include <simpleble_c/simpleble.h>

#define SOCKETOPEN 1              // state flags
#define SOCKETCLOSED 0

// protos
extern S2 memory_bounds (S8 start, S8 offset_access);
size_t strlen_safe (const char * str, S8  maxlen);

struct data_info data_info[MAXDATAINFO];
S8 data_info_ind;

S2 init_memory_bounds (struct data_info *data_info_orig, S8 data_info_ind_orig)
{
	memcpy (&data_info, &data_info_orig, sizeof (data_info_orig));
	data_info_ind = data_info_ind_orig;

	return (0);
}

// Define maximum sizes for lists
#define PERIPHERAL_LIST_SIZE (size_t)64
#define CHARACTERISTIC_ENTRIES_SIZE (size_t)64  // Max characteristics to store

#define MAXPERSTR 256

// Structure to hold service and characteristic UUIDs for easy selection
typedef struct {
    simpleble_uuid_t service_uuid;
    simpleble_uuid_t characteristic_uuid;
} CharacteristicEntry;

struct simpleble_data_t
{
    uint8_t *data;
    size_t *data_len;
};

struct bluetooth_socket
{
    U1 state; // open: SOCKETOPEN, closed: SOCKETCLOSED
    S8 adapter ALIGN; // adapter number
};

struct peripherals
{
    char identifier[MAXPERSTR];
    char address[MAXPERSTR];
};

S8 max_bluetooth_conn ALIGN = 0;
struct bluetooth_socket *bluetooth_socket = NULL;
S8 bluetooth_adapter ALIGN = 0;
S8 max_bluetooth_adapter ALIGN = 0;

struct peripherals *peripherals;
S8 max_peripherals ALIGN = 0;

// Global variables
static void clean_on_exit(void);

static void adapter_on_scan_start(simpleble_adapter_t adapter, void* userdata);
static void adapter_on_scan_stop(simpleble_adapter_t adapter, void* userdata);
static void adapter_on_scan_found(simpleble_adapter_t adapter, simpleble_peripheral_t peripheral, void* userdata);

static simpleble_peripheral_t peripheral_list[PERIPHERAL_LIST_SIZE] = {0};
static size_t peripheral_list_len = 0;
static simpleble_adapter_t adapter = NULL;

static CharacteristicEntry characteristic_entries[CHARACTERISTIC_ENTRIES_SIZE];
static size_t characteristic_entries_len = 0;

static simpleble_err_t err_code = SIMPLEBLE_SUCCESS;
static simpleble_peripheral_t selected_peripheral;

// helper functions
S8 bluetooth_get_free_socket (void)
{
    S8 i ALIGN = 0;
    S8 ret ALIGN = -1;

    for (i = 0; i < max_bluetooth_conn; i++)
    {
        if (bluetooth_socket[i].state == SOCKETCLOSED)
        {
            return (i);
        }
    }

    return (ret); // no open socket handle found!
}

S2 bluetooth_set_socket (S8 ind)
{
    if (ind < 0 || ind >= max_bluetooth_conn)
    {
        return (1); // index error!
    }

    if (bluetooth_socket[ind].state == SOCKETCLOSED)
    {
        bluetooth_socket[ind].state = SOCKETOPEN;
        return (0); // all ok!
    }

    return (1);
}

S2 bluetooth_close_socket (S8 ind)
{
    if (ind < 0 || ind >= max_bluetooth_conn)
    {
        return (1); // index error!
    }

    if (bluetooth_socket[ind].state == SOCKETOPEN)
    {
        bluetooth_socket[ind].state = SOCKETCLOSED;
        return (0); // all ok!
    }

    return (1);
}

S2 bluetooth_check_socket (S8 ind)
{
    if (ind < 0 || ind >= max_bluetooth_conn)
    {
        return (1); // index error!
    }

    if (bluetooth_socket[ind].state == SOCKETOPEN)
    {
        return (0); // all ok!
    }

    return (1);
}

// bluetooth socket API
U1 *bluetooth_get_adapter (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    // get the number of bluetooth adapters
    // and allocate the max sockets memory struct

    S8 max_bluetooth_ind ALIGN = 0;
    S8 i ALIGN = 0;

    sp = stpopi ((U1 *) &max_bluetooth_ind, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("bluetooth_get_adapter: ERROR: stack corrupt!\n");
		return (NULL);
	}

    if (max_bluetooth_ind == 0)
    {
        printf ("bluetooth_get_adapter: max connections is zero!\n");
        return (NULL);
    }

    size_t adapter_count = simpleble_adapter_get_count ();
    if (adapter_count == 0)
    {
        printf("bluetooth_get_adapter: no adapter was found.\n");

        sp = stpushi (-1, sp, sp_bottom);  // neg one: error
        if (sp == NULL)
        {
            // error
             printf ("bluetooth_get_adapter: ERROR: stack corrupt!\n");
            return (NULL);
        }
        return (sp);
    }

    // adapter found: allocate memory for sockets
    if (bluetooth_socket != NULL)
    {
        printf ("bluetooth_get_adapter: ERROR: sockets already allocated!\n");

        sp = stpushi (-1, sp, sp_bottom); // neg one: error
        if (sp == NULL)
        {
            // error
            printf ("bluetooth_get_adapter: ERROR: stack corrupt!\n");
            return (NULL);
        }
        return (sp);
    }

    bluetooth_socket = (struct bluetooth_socket*) calloc (max_bluetooth_ind, sizeof (struct bluetooth_socket));
    if (bluetooth_socket == NULL)
    {
        printf ("bluetooth_get_adapter: ERROR can't allocate %lli memory indexes!\n", max_bluetooth_ind);

        sp = stpushi (-1, sp, sp_bottom); // neg one: error
        if (sp == NULL)
        {
            // error
            printf ("bluetooth_get_adapter: ERROR: stack corrupt!\n");
            return (NULL);
        }
        return (sp);
    }

    // set global vars
    max_bluetooth_conn = max_bluetooth_ind;
    max_bluetooth_adapter = adapter_count;

    // init struct
    for (i = 0; i < max_bluetooth_conn; i++)
    {
        bluetooth_socket[i].state = SOCKETCLOSED;
    }

    // return number of adaptors:
    sp = stpushi (adapter_count, sp, sp_bottom);
    if (sp == NULL)
    {
        // error
        printf ("bluetooth_get_adapter: ERROR: stack corrupt!\n");
        return (NULL);
    }
    return (sp);
}

U1 *bluetooth_set_adapter (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 set_adapter ALIGN;

    sp = stpopi ((U1 *) &set_adapter, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("bluetooth_set_adapter: ERROR: stack corrupt!\n");
		return (NULL);
	}

    if (set_adapter >= max_bluetooth_adapter || set_adapter < 0)
    {
        printf ("bluetooth_set_adapter: error adaptor index not in range!\n");
        printf ("must be in 0 - %lli range!\n", max_bluetooth_adapter - 1);

        sp = stpushi (1, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("bluetooth_get_adapter: ERROR: stack corrupt!\n");
            return (NULL);
        }
        return (sp);
    }

    // all checks ok;
    bluetooth_adapter = set_adapter;

    sp = stpushi (0, sp, sp_bottom);
    if (sp == NULL)
    {
        // error
        printf ("bluetooth_get_adapter: ERROR: stack corrupt!\n");
        return (NULL);
    }
    return (sp);
}

U1 *bluetooth_open_adapter (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    adapter = simpleble_adapter_get_handle (bluetooth_adapter);
    if (adapter == NULL) {
        printf("bluetooth_open_adapter: failed to get adapter handle.\n");

        sp = stpushi (1, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("bluetooth_open_adapter: ERROR: stack corrupt!\n");
            return (NULL);
        }
        return (sp);
    }

    // Set up callbacks for scanning events
    simpleble_adapter_set_callback_on_scan_start (adapter, adapter_on_scan_start, NULL);
    simpleble_adapter_set_callback_on_scan_stop (adapter, adapter_on_scan_stop, NULL);
    simpleble_adapter_set_callback_on_scan_found (adapter, adapter_on_scan_found, NULL);

    // Start scanning for 5 seconds
    simpleble_adapter_scan_for (adapter, 60000);

    printf ("peripheral list entries: %li\n", peripheral_list_len);

    if (peripheral_list_len == 0) {
        printf("bluetooth_open_adapter: no connectable devices found during scan.\n");
        // Ensure to release adapter handle even if no devices are found
        simpleble_adapter_release_handle (adapter);

        sp = stpushi (1, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("bluetooth_open_adapter: ERROR: stack corrupt!\n");
            return (NULL);
        }
        return (sp);
    }

    max_peripherals = peripheral_list_len;
    peripherals = (struct peripherals*) calloc (max_peripherals, sizeof (struct peripherals));
    if (peripherals == NULL)
    {
        printf ("bluetooth_open_adapter: ERROR can't allocate %lli peripherial indexes!\n", max_peripherals);

        sp = stpushi (1, sp, sp_bottom); // error
        if (sp == NULL)
        {
            // error
            printf ("bluetooth_open_adapter: ERROR: stack corrupt!\n");
            return (NULL);
        }
        return (sp);
    }

    for (size_t i = 0; i < peripheral_list_len; i++)
    {
        simpleble_peripheral_t peripheral = peripheral_list[i];
        char* peripheral_identifier = simpleble_peripheral_identifier (peripheral);
        char* peripheral_address = simpleble_peripheral_address (peripheral);
        //printf("[%zu] %s [%s]\n", i, peripheral_identifier, peripheral_address);

        if (strlen_safe (peripheral_identifier, MAXPERSTR) >= MAXPERSTR)
        {
            printf ("bluetooth_open_adapter: error can't safe peripheral identifier!\n");

            free (peripherals);

            sp = stpushi (-1, sp, sp_bottom); // error
            if (sp == NULL)
            {
                // error
                printf ("bluetooth_open_adapter: ERROR: stack corrupt!\n");
                return (NULL);
            }
            return (sp);
        }

        if (strlen_safe (peripheral_address, MAXPERSTR) >= MAXPERSTR)
        {
            printf ("bluetooth_open_adapter: error can't safe peripheral address!\n");

            free (peripherals);

            sp = stpushi (-1, sp, sp_bottom); // error
            if (sp == NULL)
            {
                // error
                printf ("bluetooth_open_adapter: ERROR: stack corrupt!\n");
                return (NULL);
            }
            return (sp);
        }

        strcpy (peripherals[i].identifier, peripheral_identifier);
        strcpy (peripherals[i].address, peripheral_address);

        simpleble_free (peripheral_identifier); // Free the allocated string
        simpleble_free (peripheral_address);    // Free the allocated string
    }

    // bluetooth_adapter = (S8) adapter;

    sp = stpushi (max_peripherals, sp, sp_bottom); // error
    if (sp == NULL)
    {
        // error
        printf ("bluetooth_open_adapter: ERROR: stack corrupt!\n");
        return (NULL);
    }
    return (sp);
}

U1 *bluetooth_get_peripheral (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 peripheral ALIGN = 0;
    S8 peripheral_identifier_addr ALIGN = 0;
    S8 peripheral_address_addr ALIGN = 0;
    S8 data_len ALIGN = 0;

    sp = stpopi ((U1 *) &peripheral_address_addr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("bluetooth_get_peripheral: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &peripheral_identifier_addr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("bluetooth_get_peripheral: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &peripheral, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("bluetooth_get_peripheral: ERROR: stack corrupt!\n");
		return (NULL);
	}

    if (peripheral >= max_peripherals)
    {
        // error return empty strings
		printf ("bluetooth_get_pheripheral: id number out of range!\n");

        #if BOUNDSCHECK
		if (memory_bounds (peripheral_address_addr, 0) != 0)
		{
			printf ("bluetooth_get_peripheral: ERROR: address string overflow!\n");
			return (NULL);
		}

        if (memory_bounds (peripheral_identifier_addr, 0) != 0)
		{
			printf ("bluetooth_get_peripheral: ERROR: identifier string overflow!\n");
			return (NULL);
		}
		#endif

        strcpy ((char *) &data[peripheral_address_addr], "");
        strcpy ((char *) &data[peripheral_identifier_addr], "");

        sp = stpushi (1, sp, sp_bottom); // error
        if (sp == NULL)
        {
            // error
            printf ("bluetooth_get_peripheral: ERROR: stack corrupt!\n");
            return (NULL);
        }
        return (sp);
    }

    // check size of return string variables
    data_len = strlen_safe (peripherals[peripheral].address, STRINGMOD_MAXSTRLEN);
    if (data_len > 0)
    {
    #if BOUNDSCHECK
    if (memory_bounds (peripheral_address_addr, data_len - 1) != 0)
		{
			printf ("bluetooth_get_peripheral: ERROR: address string overflow!\n");
			return (NULL);
		}
    #endif
    }

    data_len = strlen_safe (peripherals[peripheral].identifier, STRINGMOD_MAXSTRLEN);
    if (data_len > 0)
    {
    #if BOUNDSCHECK
    if (memory_bounds (peripheral_identifier_addr, data_len - 1) != 0)
		{
			printf ("bluetooth_get_peripheral: ERROR: identifier string overflow!\n");
			return (NULL);
		}
    #endif
    }

    // copy strings
    strcpy ((char *) &data[peripheral_address_addr], peripherals[peripheral].address);
    strcpy ((char *) &data[peripheral_identifier_addr], peripherals[peripheral].identifier);

    // all ok!
    sp = stpushi (0, sp, sp_bottom); // error
    if (sp == NULL)
    {
         // error
        printf ("bluetooth_get_peripheral: ERROR: stack corrupt!\n");
        return (NULL);
    }
    return (sp);
}

U1 *bluetooth_connect (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 device ALIGN;
    S8 socket ALIGN;

    sp = stpopi ((U1 *) &device, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("bluetooth_connect: ERROR: stack corrupt!\n");
		return (NULL);
	}

    if (device < 0 || device >= max_peripherals)
    {
        printf ("bluetooth_connect: error device out of range!\n");
        printf ("must be in 0 - %lli\n", max_peripherals - 1);

        sp = stpushi (-1, sp, sp_bottom); // error
        if (sp == NULL)
        {
            // error
            printf ("bluetooth_connect: ERROR: stack corrupt!\n");
            return (NULL);
        }
        return (sp);
    }

    socket = bluetooth_get_free_socket ();
    if (socket == -1)
    {
        printf ("bluetooth_connect: error: can't get free bluetooth socket!\n");

        sp = stpushi (-1, sp, sp_bottom); // error
        if (sp == NULL)
        {
            // error
            printf ("bluetooth_connect: ERROR: stack corrupt!\n");
            return (NULL);
        }
        return (sp);
    }

    if (bluetooth_set_socket (socket) != 0)
    {
        printf ("bluetooth_connect: error: can't use socket: %lli!\n", socket);

        sp = stpushi (-1, sp, sp_bottom); // error
        if (sp == NULL)
        {
            // error
            printf ("bluetooth_connect: ERROR: stack corrupt!\n");
            return (NULL);
        }
        return (sp);
    }

    selected_peripheral = peripheral_list[device];

    char* peripheral_identifier = simpleble_peripheral_identifier (selected_peripheral);
    char* peripheral_address = simpleble_peripheral_address (selected_peripheral);
    printf("Connecting to %s [%s]\n", peripheral_identifier, peripheral_address);
    simpleble_free(peripheral_identifier);
    simpleble_free(peripheral_address);

    // Connect to the selected peripheral
    err_code = simpleble_peripheral_connect (selected_peripheral);
    if (err_code != SIMPLEBLE_SUCCESS)
    {
        printf ("bluetooth_connect: error: can't connect to: %lli!\n", device);

        sp = stpushi (-1, sp, sp_bottom); // error
        if (sp == NULL)
        {
            // error
            printf ("bluetooth_connect: ERROR: stack corrupt!\n");
            return (NULL);
        }
        return (sp);
    }

    // Get and display services and characteristics
    size_t services_count = simpleble_peripheral_services_count (selected_peripheral);
    printf("Successfully connected, listing %zu services.\n", services_count);

    characteristic_entries_len = 0; // Reset for storing new characteristics

    for (size_t i = 0; i < services_count; i++) {
        simpleble_service_t service;
        err_code = simpleble_peripheral_services_get (selected_peripheral, i, &service);

        if (err_code != SIMPLEBLE_SUCCESS) {
            printf("bluetooth_connect: error: failed to get service at index %zu.\n", i);
            simpleble_peripheral_disconnect (selected_peripheral);

            sp = stpushi (-1, sp, sp_bottom); // error
            if (sp == NULL)
            {
                // error
                printf ("bluetooth_connect: ERROR: stack corrupt!\n");
                return (NULL);
            }
        return (sp);
        }

        printf("Service: %s - (%zu characteristics)\n", service.uuid.value, service.characteristic_count);
        for (size_t j = 0; j < service.characteristic_count; j++) {
            printf("  Characteristic: %s - (%zu descriptors)\n", service.characteristics[j].uuid.value,
                   service.characteristics[j].descriptor_count);
            for (size_t k = 0; k < service.characteristics[j].descriptor_count; k++) {
                printf("    Descriptor: %s\n", service.characteristics[j].descriptors[k].uuid.value);
            }

            // Store this characteristic entry if space is available
            if (characteristic_entries_len < CHARACTERISTIC_ENTRIES_SIZE) {
                characteristic_entries[characteristic_entries_len].service_uuid = service.uuid;
                characteristic_entries[characteristic_entries_len].characteristic_uuid = service.characteristics[j].uuid;
                printf("  [%zu] (for writing) Service: %s, Char: %s\n", characteristic_entries_len,
                       characteristic_entries[characteristic_entries_len].service_uuid.value,
                       characteristic_entries[characteristic_entries_len].characteristic_uuid.value);
                characteristic_entries_len++;
            } else {
                printf("  Warning: Max characteristic entries reached. Not storing %s.\n", service.characteristics[j].uuid.value);
            }
        }
    }

    if (characteristic_entries_len == 0)
    {
        printf ("bluetooth_connect: No characteristics found for writing data. Disconnecting.\n");
        simpleble_peripheral_disconnect(selected_peripheral);
    }

    sp = stpushi (characteristic_entries_len, sp, sp_bottom); // return number of characteristic entries for bluetooth_get_characteristic()
    if (sp == NULL)
    {
         // error
        printf ("bluetooth_connect: ERROR: stack corrupt!\n");
        return (NULL);
    }
    return (sp);
}

U1 *bluetooth_get_characteristic (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 char_index ALIGN;
    S8 char_addr ALIGN;
    S8 service_uuid_addr ALIGN;
    S8 data_len ALIGN;

    sp = stpopi ((U1 *) &service_uuid_addr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("bluetooth_get_characteristic: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &char_addr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("bluetooth_get_characteristic: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &char_index, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("bluetooth_get_characteristic: ERROR: stack corrupt!\n");
		return (NULL);
	}

    if (char_index < 0 || char_index >= (S8) characteristic_entries_len)
    {
        printf ("bluetooth_get_characteristic: error: index out of range!");
        printf ("Must be in range of: 0 - %li\n", characteristic_entries_len);
        return (NULL);
    }

    // check size of return string variables
    data_len = strlen_safe ((const char *) &characteristic_entries[char_index].characteristic_uuid.value, STRINGMOD_MAXSTRLEN);
    #if BOUNDSCHECK
    if (memory_bounds (char_addr, data_len - 1) != 0)
		{
			printf ("bluetooth_get_characteristic: ERROR: address string overflow!\n");

            sp = stpushi (1, sp, sp_bottom); // return number of characteristic entries for bluetooth_get_characteristic()
            if (sp == NULL)
            {
                // error
                printf ("bluetooth_get_characteristic: ERROR: stack corrupt!\n");
                return (NULL);
            }
            return (sp);
		}
    #endif

    data_len = strlen_safe ((const char *) &characteristic_entries[char_index].service_uuid.value, STRINGMOD_MAXSTRLEN);
    #if BOUNDSCHECK
    if (memory_bounds (service_uuid_addr, data_len - 1) != 0)
		{
			printf ("bluetooth_get_characteristic: ERROR: service string overflow!\n");

            sp = stpushi (1, sp, sp_bottom); // return number of characteristic entries for bluetooth_get_characteristic()
            if (sp == NULL)
            {
                // error
                printf ("bluetooth_get_characteristic: ERROR: stack corrupt!\n");
                return (NULL);
            }
            return (sp);
		}
    #endif

    // copy strings
    strcpy ((char *) &data[char_addr], (const char *) &characteristic_entries[char_index].characteristic_uuid.value);
    strcpy ((char *) &data[service_uuid_addr], (const char *) &characteristic_entries[char_index].service_uuid.value);

    sp = stpushi (0, sp, sp_bottom); // all ok!
    if (sp == NULL)
    {
        // error
         printf ("bluetooth_get_characteristic: ERROR: stack corrupt!\n");
        return (NULL);
    }
    return (sp);
}

// write
U1 *bluetooth_write (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 socket ALIGN;
    S8 char_index ALIGN;
    S8 string_addr ALIGN;
    S8 peripheral ALIGN;
    S8 data_len ALIGN;

    sp = stpopi ((U1 *) &data_len, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("bluetooth_write: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &string_addr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("bluetooth_write: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &char_index, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("bluetooth_write: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &peripheral, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("bluetooth_write: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &socket, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("bluetooth_write: ERROR: stack corrupt!\n");
		return (NULL);
	}

    if (bluetooth_check_socket (socket) != 0)
    {
        printf ("bluetooth_write: error: socket %lli is closed!\n", socket);
        return (NULL);
    }

    if (char_index < 0 || char_index >= (S8) characteristic_entries_len)
    {
        printf ("bluetooth_write: error: characteristic index out of range!");
        printf ("Must be in range of: 0 - %li\n", characteristic_entries_len - 1);
        return (NULL);
    }
    if (peripheral < 0 || peripheral >= (S8) peripheral_list_len)
    {
        printf ("bluetooth_write: error: peripheral index out of range!");
        printf ("Must be in range of: 0 - %li\n", peripheral_list_len - 1);
        return (NULL);
    }

    #if BOUNDSCHECK
    if (memory_bounds (string_addr, (S8) data_len - 1) != 0)
	{
        printf ("bluetooth_write: ERROR: address string overflow!\n");
        return (NULL);
	}
    #endif

    // Prepare data for writing
    struct simpleble_data_t data_to_send;
    data_to_send.data = (uint8_t*) &data[string_addr];
    data_to_send.data_len = (size_t*) &data_len;

    simpleble_peripheral_t *peripheral_conn = peripheral_list[peripheral];

    printf("Attempting to write '%s' to Service: %s, Characteristic: %s\n",
           &data[string_addr],
           characteristic_entries[char_index].service_uuid.value,
           characteristic_entries[char_index].characteristic_uuid.value);

    // Perform the write operation (using write_command for unacknowledged write)
    err_code = simpleble_peripheral_write_command (
        peripheral_conn,
        characteristic_entries[char_index].service_uuid,
        characteristic_entries[char_index].characteristic_uuid,
        data_to_send.data, *data_to_send.data_len
    );

    if (err_code == SIMPLEBLE_SUCCESS)
    {
        printf("Data successfully sent (write command)!\n");

        sp = stpushi (0, sp, sp_bottom); // return number of characteristic entries for bluetooth_get_characteristic()
        if (sp == NULL)
        {
            // error
            printf ("bluetooth_send: ERROR: stack corrupt!\n");
            return (NULL);
        }
        return (sp);
    }
    else
    {
        printf("Failed to send data (Error code: %d).\n", err_code);

        sp = stpushi (err_code, sp, sp_bottom); // return number of characteristic entries for bluetooth_get_characteristic()
        if (sp == NULL)
        {
            // error
            printf ("bluetooth_write: ERROR: stack corrupt!\n");
            return (NULL);
        }
        return (sp);
    }
}

// read
U1 *bluetooth_read (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 socket ALIGN;
    S8 char_index ALIGN;
    S8 string_addr ALIGN;
    S8 peripheral ALIGN;
    S8 i ALIGN;
    uint8_t read_byte;
    U1 read_data = 0;

    sp = stpopi ((U1 *) &string_addr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("bluetooth_read: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &char_index, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("bluetooth_read: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &peripheral, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("bluetooth_read: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &socket, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("bluetooth_read: ERROR: stack corrupt!\n");
		return (NULL);
	}

    if (bluetooth_check_socket (socket) != 0)
    {
        printf ("bluetooth_read: error: socket %lli is closed!\n", socket);
        return (NULL);
    }

    if (char_index < 0 || char_index >= (S8) characteristic_entries_len)
    {
        printf ("bluetooth_read: error: characteristic index out of range!");
        printf ("Must be in range of: 0 - %li\n", characteristic_entries_len - 1);
        return (NULL);
    }
    if (peripheral < 0 || peripheral >= (S8) peripheral_list_len)
    {
        printf ("bluetooth_read: error: peripheral index out of range!");
        printf ("Must be in range of: 0 - %li\n", peripheral_list_len - 1);
        return (NULL);
    }

    // Prepare data for reading
    //struct simpleble_data_t data_to_read;
    uint8_t* received_data = NULL; // Pointer to store the allocated data
    size_t received_data_len = 0;   // Variable to store the length

    simpleble_peripheral_t *peripheral_conn = peripheral_list[peripheral];

    printf("Attempting to recieve from Service: %s, Characteristic: %s\n",
           characteristic_entries[char_index].service_uuid.value,
           characteristic_entries[char_index].characteristic_uuid.value);

    // Perform the read operation
    err_code = simpleble_peripheral_read (
        peripheral_conn,
        characteristic_entries[char_index].service_uuid,
        characteristic_entries[char_index].characteristic_uuid,
        &received_data, &received_data_len
    );

    if (err_code != SIMPLEBLE_SUCCESS)
    {
        printf ("bluetooth_read: error can't read data! error code: %i\n", err_code);

        free (received_data);

        sp = stpushi (1, sp, sp_bottom); // return number of characteristic entries for bluetooth_get_characteristic()
        if (sp == NULL)
        {
            // error
            printf ("bluetooth_read: ERROR: stack corrupt!\n");
            return (NULL);
        }
        return (sp);
    }

    #if BOUNDSCHECK
    if (memory_bounds (string_addr, (S8) received_data_len - 1) != 0)
		{
			printf ("bluetooth_read: ERROR: address string overflow!\n");

            free (received_data);

            sp = stpushi (1, sp, sp_bottom); // return number of characteristic entries for bluetooth_get_characteristic()
            if (sp == NULL)
            {
                // error
                printf ("bluetooth_read: ERROR: stack corrupt!\n");
                return (NULL);
            }
            return (sp);
		}
    #endif

    // copy data
    if (received_data_len > 0)
    {
        for (i = 0; i < (S8) received_data_len; i++)
        {
            read_byte = received_data[i];
            read_data = read_byte;
            data[string_addr + i] = read_data;
        }
    }

    free (received_data);

    if (err_code == SIMPLEBLE_SUCCESS)
    {
        printf("Data successfully read (read command)!\n");

        sp = stpushi (0, sp, sp_bottom); // return number of characteristic entries for bluetooth_get_characteristic()
        if (sp == NULL)
        {
            // error
            printf ("bluetooth_read: ERROR: stack corrupt!\n");
            return (NULL);
        }
        return (sp);
    }
    else
    {
        printf("Failed to read data (Error code: %d).\n", err_code);

        sp = stpushi (err_code, sp, sp_bottom); // return number of characteristic entries for bluetooth_get_characteristic()
        if (sp == NULL)
        {
            // error
            printf ("bluetooth_read: ERROR: stack corrupt!\n");
            return (NULL);
        }
        return (sp);
    }
}

U1 *bluetooth_disconnect (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 socket ALIGN;
    S8 peripheral ALIGN;

    sp = stpopi ((U1 *) &peripheral, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("bluetooth_disconnect: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &socket, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("bluetooth_disconnect: ERROR: stack corrupt!\n");
	}

    if (bluetooth_check_socket (socket) != 0)
    {
        printf ("bluetooth_read: error: socket %lli is closed!\n", socket);

        sp = stpushi (1, sp, sp_bottom); // return number of characteristic entries for bluetooth_get_characteristic()
        if (sp == NULL)
        {
            // error
            printf ("bluetooth_disconnect: ERROR: stack corrupt!\n");
            return (NULL);
        }
        return (sp);
    }

    if (peripheral < 0 || peripheral >= (S8) peripheral_list_len)
    {
        printf ("bluetooth_read: error: peripheral index out of range!");
        printf ("Must be in range of: 0 - %li\n", peripheral_list_len - 1);

        sp = stpushi (1, sp, sp_bottom); // return number of characteristic entries for bluetooth_get_characteristic()
        if (sp == NULL)
        {
            // error
            printf ("bluetooth_disconnect: ERROR: stack corrupt!\n");
            return (NULL);
        }
        return (sp);
    }

    simpleble_peripheral_disconnect (peripheral_list[peripheral]);

    if (bluetooth_close_socket (socket) != 0)
    {
        printf ("bluetooth_disconnect: error: closing socket %lli is not open!\n", socket);

        sp = stpushi (1, sp, sp_bottom); // return number of characteristic entries for bluetooth_get_characteristic()
        if (sp == NULL)
        {
            // error
            printf ("bluetooth_disconnect: ERROR: stack corrupt!\n");
            return (NULL);
        }
        return (sp);
    }

    // all ok!
    sp = stpushi (0, sp, sp_bottom); // return number of characteristic entries for bluetooth_get_characteristic()
    if (sp == NULL)
    {
        // error
        printf ("bluetooth_disconnect: ERROR: stack corrupt!\n");
        return (NULL);
    }
    return (sp);
}

U1 *bluetooth_end (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    clean_on_exit ();
    if (bluetooth_socket != NULL)
    {
        free (bluetooth_socket);
        bluetooth_socket = NULL;
    }

    return (sp);
}

// Function to clean up allocated SimpleBLE resources on program exit
static void clean_on_exit(void) {
    // printf("Releasing allocated resources.\n");

    // Release all saved peripherals
    for (size_t i = 0; i < peripheral_list_len; i++) {
        simpleble_peripheral_release_handle(peripheral_list[i]);
    }

    // Release the adapter handle
    simpleble_adapter_release_handle(adapter);
}

// Callback function when scanning starts
static void adapter_on_scan_start(simpleble_adapter_t adapter, void* userdata) {
    char* identifier = simpleble_adapter_identifier(adapter);
    if (identifier == NULL) {
        return;
    }
    printf("Adapter %s started scanning.\n", identifier);
    simpleble_free(identifier); // Free the allocated string
}

// Callback function when scanning stops
static void adapter_on_scan_stop(simpleble_adapter_t adapter, void* userdata) {
    char* identifier = simpleble_adapter_identifier(adapter);
    if (identifier == NULL) {
        return;
    }
    printf("Adapter %s stopped scanning.\n", identifier);
    simpleble_free(identifier); // Free the allocated string
}

// Callback function when a peripheral is found during scanning
static void adapter_on_scan_found(simpleble_adapter_t adapter, simpleble_peripheral_t peripheral, void* userdata) {
    char* adapter_identifier = simpleble_adapter_identifier(adapter);
    char* peripheral_identifier = simpleble_peripheral_identifier(peripheral);
    char* peripheral_address = simpleble_peripheral_address(peripheral);

    if (adapter_identifier == NULL || peripheral_identifier == NULL || peripheral_address == NULL) {
        return;
    }

    printf("Adapter %s found device: %s [%s]\n", adapter_identifier, peripheral_identifier, peripheral_address);

    if (peripheral_list_len < PERIPHERAL_LIST_SIZE) {
        // Save the peripheral
        peripheral_list[peripheral_list_len++] = peripheral;
    } else {
        // As there was no space left for this peripheral, release the associated handle.
        simpleble_peripheral_release_handle(peripheral);
    }

    // Let's not forget to release all allocated memory.
    simpleble_free(peripheral_identifier);
    simpleble_free(peripheral_address);
}
