/*
 * This mqtt.c is part of L1vm.
 *
 * (c) Copyright Stefan Pietzonke (info@midnight-coding.de), 2026
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

#include "MQTTClient.h"

#define MAXCLIENTS 32

// max bytes
#define MAXSTR 256
#define MAXPAYLOAD 4096

#define STATUS_UNUSED 0
#define STATUS_OPEN 1
#define STATUS_CLOSED 2

#define MSG_EMPTY 0
#define MSG_REC 1

struct mqtt_client
{
    U1 status;
    MQTTClient client;
    MQTTClient_connectOptions conn_opts;

    U1 address[MAXSTR];
    U1 client_id[MAXSTR];
    U1 topic[MAXSTR];
    U1 payload[MAXPAYLOAD];
    S8 qos;
    S8 timeout;

    // pub msg
    MQTTClient_message pubmsg;
    MQTTClient_deliveryToken token;

    volatile MQTTClient_deliveryToken deliveredtoken;

    U1 message_arrived;
    U1 *data;
    S8 data_address;  // data array return message address, string must be at least PAYLOAD bytes long!
};

struct mqtt_client mqtt_client[MAXCLIENTS];


// protos
extern S2 memory_bounds (S8 start, S8 offset_access);
size_t strlen_safe (const char * str, S8  maxlen);

struct data_info data_info[MAXDATAINFO];
S8 data_info_ind;


S2 init_memory_bounds (struct data_info *data_info_orig, S8 data_info_ind_orig)
{
    S8 i ALIGN;
    memcpy (&data_info, &data_info_orig, sizeof (data_info_orig));
    data_info_ind = data_info_ind_orig;

    for (i = 0; i < MAXCLIENTS; i++)
    {
        mqtt_client[i].status = STATUS_UNUSED;
    }

    return (0);
}

S8 get_free_client (void)
{
    S8 i ALIGN;

    for (i = 0; i < MAXCLIENTS; i++)
    {
        if (mqtt_client[i].status == STATUS_UNUSED || mqtt_client[i].status == STATUS_CLOSED)
        {
            return (i);
        }
    }

    // no free handle
    return (-1);
}

void delivered (void *context, MQTTClient_deliveryToken dt)
{
    S8 handle ALIGN = (S8) context;

    printf("Message with token value %d delivery confirmed\n", dt);
    printf ("delivered: handle: %lli\n", handle);
    mqtt_client[handle].deliveredtoken = dt;
}

int msgarrvd (void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
    S8 handle ALIGN = (S8) context;
    S8 i ALIGN;
    S8 address ALIGN;

    char* payloadptr;
    U1 *data;

    if (message->payloadlen >= MAXPAYLOAD)
    {
        printf ("mqtt_msgarrvd: error payload: %i too big! Must be below %i\n", message->payloadlen, MAXPAYLOAD);
        return 0;
    }

    payloadptr = message->payload;
    for(i = 0; i <message->payloadlen; i++)
    {
        mqtt_client[handle].payload[i] = *payloadptr++;
    }

    mqtt_client[handle].payload[i] = '\0';
    address = mqtt_client[handle].data_address;

    data = mqtt_client[handle].data;
    strcpy ((char *) &data[address], (const char *) mqtt_client[handle].payload);

    mqtt_client[handle].message_arrived = MSG_REC;

    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

void connlost (void *context, char *cause)
{
    printf("\nmqtt: onnection lost\n");
    printf("     cause: %s\n", cause);
}

U1 *mqtt_open_client (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 handle ALIGN;
    S8 address ALIGN;
    S8 client_id ALIGN;
    S8 retstr_address ALIGN;
    S8 topic ALIGN;
    S8 qos ALIGN;
    S8 rc ALIGN;

    S8 err ALIGN;

    sp = stpopi ((U1 *) &retstr_address, sp, sp_top);
    if (sp == NULL)
    {
        // error
		printf ("mqtt_open_client: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &qos, sp, sp_top);
    if (sp == NULL)
    {
        // error
		printf ("mqtt_open_client: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &topic, sp, sp_top);
    if (sp == NULL)
    {
        // error
		printf ("mqtt_open_client: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &client_id, sp, sp_top);
    if (sp == NULL)
    {
        // error
		printf ("mqtt_open_client: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &address, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mqtt_open_client: ERROR: stack corrupt!\n");
		return (NULL);
	}

    // check string lengths
    if (strlen_safe ((const char *) &data[address], MAXSTR) >= MAXSTR)
    {
        sp = stpushi (-1, sp, sp_bottom); // error
        if (sp == NULL)
        {
            // error
            printf ("mqtt_open_client: ERROR: stack corrupt!\n");
            return (NULL);
        }
        return (sp);
    }

    if (strlen_safe ((const char *) &data[client_id], MAXSTR) >= MAXSTR)
    {
        sp = stpushi (-1, sp, sp_bottom); // error
        if (sp == NULL)
        {
            // error
            printf ("mqtt_open_client: ERROR: stack corrupt!\n");
            return (NULL);
        }
        return (sp);
    }

    if (strlen_safe ((const char *) &data[topic], MAXSTR) >= MAXSTR)
    {
        sp = stpushi (-1, sp, sp_bottom); // error
        if (sp == NULL)
        {
            // error
            printf ("mqtt_open_client: ERROR: stack corrupt!\n");
            return (NULL);
        }
        return (sp);
    }

    if (memory_bounds (retstr_address, MAXPAYLOAD - 1) != 0)
	{
       printf ("mqtt_open_client: error: return string length must be at least %i bytes or higher!\n", MAXPAYLOAD);

        sp = stpushi (-1, sp, sp_bottom); // error
        if (sp == NULL)
        {
            // error
            printf ("mqtt_open_client: ERROR: stack corrupt!\n");
            return (NULL);
        }
        return (sp);
    }

    handle = get_free_client ();
    if (handle == -1)
    {
        sp = stpushi (-1, sp, sp_bottom); // error
        if (sp == NULL)
        {
            // error
            printf ("mqtt_open_client: ERROR: stack corrupt!\n");
            return (NULL);
       }
       return (sp);
    }

    // save address & client_id
    strcpy ((char *) mqtt_client[handle].address, (char *) &data[address]);
    strcpy ((char *) mqtt_client[handle].client_id, (char *) &data[client_id]);
    strcpy ((char *) mqtt_client[handle].topic, (char *) &data[topic]);
    mqtt_client[handle].qos = qos;

    err = MQTTClient_create (&mqtt_client[handle].client, (const char *) mqtt_client[handle].address, (const char *) mqtt_client[handle].client_id, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    if (err !=  MQTTCLIENT_SUCCESS)
    {
        printf ("mqtt_open_client: client create error: %lli !\n", err);
        sp = stpushi (-1, sp, sp_bottom); // error
        if (sp == NULL)
        {
            // error
            printf ("mqtt_open_client: ERROR: stack corrupt!\n");
            return (NULL);
       }
       return (sp);
    }

    mqtt_client[handle].conn_opts.keepAliveInterval = 20;
    mqtt_client[handle].conn_opts.cleansession = 1;
    mqtt_client[handle].data_address = retstr_address;
    mqtt_client[handle].data = data;

    MQTTClient_setCallbacks ((void *) mqtt_client[handle].client, (void *) handle, connlost, msgarrvd, delivered);
    if ((rc = MQTTClient_connect (mqtt_client[handle].client, &mqtt_client[handle].conn_opts)) != MQTTCLIENT_SUCCESS)
    {
        printf("mqtt_open_client: failed to connect, return code %lli\n", rc);

        sp = stpushi (-1, sp, sp_bottom); // error
        if (sp == NULL)
        {
            // error
            printf ("mqtt_open_client: ERROR: stack corrupt!\n");
            return (NULL);
        }
        return (sp);
    }

    // subscribe
    MQTTClient_subscribe (mqtt_client[handle].client, (const char *) mqtt_client[handle].topic, mqtt_client[handle].qos);

    mqtt_client[handle].message_arrived = MSG_EMPTY;

    sp = stpushi (handle, sp, sp_bottom); // ok
    if (sp == NULL)
    {
        // error
        printf ("mqtt_open_client: ERROR: stack corrupt!\n");
        return (NULL);
    }

    return (sp);
}

U1 *mqtt_get_msg (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 handle ALIGN;

    sp = stpopi ((U1 *) &handle, sp, sp_top);
    if (sp == NULL)
    {
        // error
		printf ("mqtt_get_msg: ERROR: stack corrupt!\n");
		return (NULL);
	}

    if (handle < 0 || handle >= MAXCLIENTS)
    {
        printf ("mqtt_get_msg: error: handle out of range!");

        sp = stpushi (1, sp, sp_bottom); // error
        if (sp == NULL)
        {
            // error
            printf ("mqtt_get_msg: ERROR: stack corrupt!\n");
            return (NULL);
        }
        return (sp);
    }

    if (mqtt_client[handle].status != STATUS_OPEN)
    {
        printf ("mqtt_get_msg: error: handle not open!");

        sp = stpushi (1, sp, sp_bottom); // error
        if (sp == NULL)
        {
            // error
            printf ("mqtt_get_msg: ERROR: stack corrupt!\n");
            return (NULL);
        }
        return (sp);
    }

    // return message status
    sp = stpushi (mqtt_client[handle].message_arrived, sp, sp_bottom);
    if (sp == NULL)
    {
        // error
        printf ("mqtt_get_msg: ERROR: stack corrupt!\n");
        return (NULL);
    }

    if (mqtt_client[handle].message_arrived == MSG_REC)
    {
        // mark messages as read: empty
        mqtt_client[handle].message_arrived = MSG_EMPTY;
    }

    return (sp);
}

U1 *mqtt_close_client (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 handle ALIGN;

    sp = stpopi ((U1 *) &handle, sp, sp_top);
    if (sp == NULL)
    {
        // error
		printf ("mqtt_close_client: ERROR: stack corrupt!\n");
		return (NULL);
	}

    if (handle < 0 || handle >= MAXCLIENTS)
    {
        printf ("mqtt_close_client: error: handle out of range!");

        sp = stpushi (1, sp, sp_bottom); // error
        if (sp == NULL)
        {
            // error
            printf ("mqtt_close_client: ERROR: stack corrupt!\n");
            return (NULL);
        }
        return (sp);
    }

    if (mqtt_client[handle].status != STATUS_OPEN)
    {
        printf ("mqtt_close_client: error: handle not open!");

        sp = stpushi (1, sp, sp_bottom); // error
        if (sp == NULL)
        {
            // error
            printf ("mqtt_close_client: ERROR: stack corrupt!\n");
            return (NULL);
        }
        return (sp);
    }

    mqtt_client[handle].status = STATUS_CLOSED;

    sp = stpushi (0, sp, sp_bottom); // ok
    if (sp == NULL)
    {
        // error
        printf ("mqtt_close_client: ERROR: stack corrupt!\n");
        return (NULL);
    }

    MQTTClient_disconnect (mqtt_client[handle].client, 10000);
    MQTTClient_destroy (&mqtt_client[handle].client);

    return (sp);
}

U1 *mqtt_open_client_pub_msg (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 handle ALIGN;
    S8 address ALIGN;
    S8 client_id ALIGN;
    S8 rc ALIGN;
    S8 err ALIGN;

    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;

    sp = stpopi ((U1 *) &client_id, sp, sp_top);
    if (sp == NULL)
    {
        // error
		printf ("mqtt_open_client_pub_msg: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &address, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mqtt_open_client_pub_msg: ERROR: stack corrupt!\n");
		return (NULL);
	}

     // check string lengths
    if (strlen_safe ((const char *) &data[address], MAXSTR) >= MAXSTR)
    {
        sp = stpushi (-1, sp, sp_bottom); // error
        if (sp == NULL)
        {
            // error
            printf ("mqtt_open_client_pub_msg: ERROR: stack corrupt!\n");
            return (NULL);
        }
        return (sp);
    }

    if (strlen_safe ((const char *) &data[client_id], MAXSTR) >= MAXSTR)
    {
        sp = stpushi (-1, sp, sp_bottom); // error
        if (sp == NULL)
        {
            // error
            printf ("mqtt_open_pub_msg: ERROR: stack corrupt!\n");
            return (NULL);
        }
        return (sp);
    }

    handle = get_free_client ();
    if (handle == -1)
    {
        sp = stpushi (-1, sp, sp_bottom); // error
        if (sp == NULL)
        {
            // error
            printf ("mqtt_open_client_pub_msg: ERROR: stack corrupt!\n");
            return (NULL);
       }
       return (sp);
    }

    strcpy ((char *) mqtt_client[handle].address, (char *) &data[address]);
    strcpy ((char *) mqtt_client[handle].client_id, (char *) &data[client_id]);

    err = MQTTClient_create (&mqtt_client[handle].client, (const char *) mqtt_client[handle].address, (const char*) mqtt_client[handle].client_id, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    if (err !=  MQTTCLIENT_SUCCESS)
    {
        printf ("mqtt_open_client_pub_msg: client create error: %lli !\n", err);
        sp = stpushi (-1, sp, sp_bottom); // error
        if (sp == NULL)
        {
            // error
            printf ("mqtt_open_client_pub_msg: ERROR: stack corrupt!\n");
            return (NULL);
       }
       return (sp);
    }

    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;

    MQTTClient_setCallbacks ((void *) mqtt_client[handle].client, (void *) handle, connlost, msgarrvd, delivered);

    if ((rc = MQTTClient_connect (mqtt_client[handle].client, &conn_opts)) != MQTTCLIENT_SUCCESS)
    {
        printf("mqtt_open_client_pub_msg: failed to connect, return code %lli\n", rc);

        sp = stpushi (-1, sp, sp_bottom); // error
        if (sp == NULL)
        {
            // error
            printf ("mqtt_open_client_pub_msg: ERROR: stack corrupt!\n");
            return (NULL);
        }
        return (sp);
    }

    mqtt_client[handle].status = STATUS_OPEN;

    sp = stpushi (handle, sp, sp_bottom); // ok
    if (sp == NULL)
    {
        // error
        printf ("mqtt_open_client_pub_msg: ERROR: stack corrupt!\n");
        return (NULL);
    }
    return (sp);
}

U1 *mqtt_send_msg (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 handle ALIGN;
    S8 topic ALIGN;
    S8 msg ALIGN;
    S8 qos ALIGN;
    S8 payloadlen ALIGN;
    S8 deliveredtoken ALIGN;
    S8 ret ALIGN;

    MQTTClient_message pubmsg = MQTTClient_message_initializer;

    sp = stpopi ((U1 *) &qos, sp, sp_top);
    if (sp == NULL)
    {
        // error
		printf ("mqtt_send_msg: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &msg, sp, sp_top);
    if (sp == NULL)
    {
        // error
		printf ("mqtt_send_msg: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &topic, sp, sp_top);
    if (sp == NULL)
    {
        // error
		printf ("mqtt_send_msg ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &handle, sp, sp_top);
    if (sp == NULL)
    {
        // error
		printf ("mqtt_send_msg: ERROR: stack corrupt!\n");
		return (NULL);
	}

   if (handle < 0 || handle >= MAXCLIENTS)
    {
        printf ("mqtt_send_msg: error: handle out of range!");

        sp = stpushi (1, sp, sp_bottom); // error
        if (sp == NULL)
        {
            // error
            printf ("mqtt_send_msg: ERROR: stack corrupt!\n");
            return (NULL);
        }
        return (sp);
    }

    if (mqtt_client[handle].status != STATUS_OPEN)
    {
        printf ("mqtt_send_msg: error: handle not open!\n");

        sp = stpushi (1, sp, sp_bottom); // error
        if (sp == NULL)
        {
            // error
            printf ("mqtt_send_msg: ERROR: stack corrupt!\n");
            return (NULL);
        }
        return (sp);
    }

    payloadlen = strlen_safe ((const char *) &data[msg], MAXPAYLOAD);
    if (payloadlen >= MAXPAYLOAD)
    {
        printf ("mqtt_send_msg: error: message is longer as %i bytes!\n", MAXPAYLOAD);

        sp = stpushi (1, sp, sp_bottom); // error
        if (sp == NULL)
        {
            // error
            printf ("mqtt_send_msg: ERROR: stack corrupt!\n");
            return (NULL);
        }
        return (sp);
    }

    pubmsg.payload = &data[msg];
    pubmsg.payloadlen = payloadlen;
    pubmsg.qos = qos;
    pubmsg.retained = 0;

    mqtt_client[handle].deliveredtoken = 0;

    ret = MQTTClient_publishMessage (mqtt_client[handle].client, (const char *) &data[topic], &pubmsg, &mqtt_client[handle].token);
    if (ret != MQTTCLIENT_SUCCESS)
    {
        sp = stpushi (1, sp, sp_bottom); // error
        if (sp == NULL)
        {
            // error
            printf ("mqtt_send_msg: ERROR: stack corrupt!\n");
            return (NULL);
        }
        return (sp);
    }

    while (mqtt_client[handle].deliveredtoken != mqtt_client[handle].token);

    MQTTClient_disconnect (mqtt_client[handle].client, 10000);
    MQTTClient_destroy (&mqtt_client[handle].client);

    mqtt_client[handle].status = STATUS_CLOSED;

    sp = stpushi (0, sp, sp_bottom); // ok
    if (sp == NULL)
    {
        // error
        printf ("mqtt_send_msg: ERROR: stack corrupt!\n");
        return (NULL);
    }
    return (sp);
}
