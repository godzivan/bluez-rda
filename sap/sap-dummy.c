/*
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2010 ST-Ericsson SA
 *  Copyright (C) 2011 Tieto Poland
 *
 *  Author: Waldemar Rymarkiewicz <waldemar.rymarkiewicz@tieto.com>
 *          for ST-Ericsson
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <gdbus.h>

#include "log.h"
#include "sap.h"

#define SAP_DUMMY_IFACE "org.bluez.SimAccessTest"
#define SAP_DUMMY_PATH "/org/bluez/test"

enum {
	SIM_DISCONNECTED = 0x00,
	SIM_CONNECTED	 = 0x01,
	SIM_POWERED_OFF	 = 0x02,
	SIM_MISSING	 = 0x03
};

static DBusConnection *connection = NULL;

static int sim_card_conn_status = SIM_DISCONNECTED;
static void *sap_data = NULL; /* SAP server private data. */
static gboolean ongoing_call_status = FALSE;
static int max_msg_size_supported = 512;

void sap_connect_req(void *sap_device, uint16_t maxmsgsize)
{
	DBG("status: %d", sim_card_conn_status);

	if (sim_card_conn_status != SIM_DISCONNECTED) {
		sap_connect_rsp(sap_device, SAP_STATUS_CONNECTION_FAILED,
								maxmsgsize);
		return;
	}

	if (max_msg_size_supported > maxmsgsize) {
		sap_connect_rsp(sap_device, SAP_STATUS_MAX_MSG_SIZE_TOO_SMALL,
						max_msg_size_supported);
		return;
	}

	if (max_msg_size_supported < maxmsgsize) {
		sap_connect_rsp(sap_device,
				SAP_STATUS_MAX_MSG_SIZE_NOT_SUPPORTED,
				max_msg_size_supported);
		return;
	}

	if (ongoing_call_status) {
		sap_connect_rsp(sap_device, SAP_STATUS_OK_ONGOING_CALL,
						max_msg_size_supported);
		return;
	}

	sim_card_conn_status = SIM_CONNECTED;
	sap_data = sap_device;

	sap_connect_rsp(sap_device, SAP_STATUS_OK, maxmsgsize);
	sap_status_ind(sap_device, SAP_STATUS_CHANGE_CARD_RESET);
}

void sap_disconnect_req(void *sap_device, uint8_t linkloss)
{
	sim_card_conn_status = SIM_DISCONNECTED;
	sap_data = NULL;
	ongoing_call_status = FALSE;

	DBG("status: %d", sim_card_conn_status);

	if (linkloss)
		return;

	sap_disconnect_rsp(sap_device);
}

void sap_transfer_apdu_req(void *sap_device, struct sap_parameter *param)
{
	char apdu[] = "APDU response!";

	DBG("status: %d", sim_card_conn_status);

	if (sim_card_conn_status == SIM_MISSING)
		sap_transfer_apdu_rsp(sap_device,
				SAP_RESULT_ERROR_CARD_REMOVED, NULL, 0);
	else if (sim_card_conn_status == SIM_POWERED_OFF)
		sap_transfer_apdu_rsp(sap_device, SAP_RESULT_ERROR_POWERED_OFF,
								NULL, 0);
	else if (sim_card_conn_status != SIM_CONNECTED)
		sap_transfer_apdu_rsp(sap_device,
			SAP_RESULT_ERROR_NOT_ACCESSIBLE, NULL, 0);
	else
		sap_transfer_apdu_rsp(sap_device, SAP_RESULT_OK,
						(uint8_t *)&apdu, sizeof(apdu));
}

void sap_transfer_atr_req(void *sap_device)
{
	char atr[] = "ATR response!";

	DBG("status: %d", sim_card_conn_status);

	if (sim_card_conn_status == SIM_MISSING)
		sap_transfer_atr_rsp(sap_device, SAP_RESULT_ERROR_CARD_REMOVED,
								NULL, 0);
	else if (sim_card_conn_status == SIM_POWERED_OFF)
		sap_transfer_atr_rsp(sap_device, SAP_RESULT_ERROR_POWERED_OFF,
								NULL, 0);
	else if (sim_card_conn_status != SIM_CONNECTED)
		sap_transfer_atr_rsp(sap_device, SAP_RESULT_ERROR_NO_REASON,
								NULL, 0);
	else
		sap_transfer_atr_rsp(sap_device, SAP_RESULT_OK,
						(uint8_t *)&atr, sizeof(atr));
}

void sap_power_sim_off_req(void *sap_device)
{
	DBG("status: %d", sim_card_conn_status);

	if (sim_card_conn_status == SIM_MISSING) {
		sap_power_sim_off_rsp(sap_device,
					SAP_RESULT_ERROR_CARD_REMOVED);
	} else if (sim_card_conn_status == SIM_POWERED_OFF) {
		sap_power_sim_off_rsp(sap_device,
					SAP_RESULT_ERROR_POWERED_OFF);
	} else if (sim_card_conn_status != SIM_CONNECTED) {
		sap_power_sim_off_rsp(sap_device,
					SAP_RESULT_ERROR_NO_REASON);
	} else {
		sap_power_sim_off_rsp(sap_device, SAP_RESULT_OK);
		sim_card_conn_status = SIM_POWERED_OFF;
	}
}

void sap_power_sim_on_req(void *sap_device)
{
	DBG("status: %d", sim_card_conn_status);

	if (sim_card_conn_status == SIM_MISSING) {
		sap_power_sim_on_rsp(sap_device,
					SAP_RESULT_ERROR_CARD_REMOVED);
	} else if (sim_card_conn_status == SIM_POWERED_OFF) {
		sap_power_sim_on_rsp(sap_device, SAP_RESULT_OK);
		sim_card_conn_status = SIM_CONNECTED;
		return;
	} else if (sim_card_conn_status != SIM_CONNECTED) {
		sap_power_sim_on_rsp(sap_device,
					SAP_RESULT_ERROR_NOT_ACCESSIBLE);
	} else {
		sap_power_sim_on_rsp(sap_device,
					SAP_RESULT_ERROR_NO_REASON);
	}
}

void sap_reset_sim_req(void *sap_device)
{
	DBG("status: %d", sim_card_conn_status);

	if (sim_card_conn_status == SIM_MISSING) {
		sap_reset_sim_rsp(sap_device, SAP_RESULT_ERROR_CARD_REMOVED);
	} else if (sim_card_conn_status == SIM_POWERED_OFF) {
		sap_reset_sim_rsp(sap_device, SAP_RESULT_ERROR_POWERED_OFF);
	} else if (sim_card_conn_status != SIM_CONNECTED) {
		sap_reset_sim_rsp(sap_device, SAP_RESULT_ERROR_NO_REASON);
	} else {
		sap_reset_sim_rsp(sap_device, SAP_RESULT_OK);
	}
}

void sap_transfer_card_reader_status_req(void *sap_device)
{
	DBG("status: %d", sim_card_conn_status);

	if (sim_card_conn_status != SIM_CONNECTED) {
		sap_transfer_card_reader_status_rsp(sap_device,
					SAP_RESULT_ERROR_NO_REASON, 0xF1);
		return;
	}

	sap_transfer_card_reader_status_rsp(sap_device, SAP_RESULT_OK, 0xF1);
}

void sap_set_transport_protocol_req(void *sap_device,
					struct sap_parameter *param)
{
	sap_transport_protocol_rsp(sap_device, SAP_RESULT_NOT_SUPPORTED);
}

static inline DBusMessage *invalid_args(DBusMessage *msg)
{
	return g_dbus_create_error(msg, "org.bluez.Error.InvalidArguments",
					"Invalid arguments in method call");
}

static DBusMessage *ongoing_call(DBusConnection *conn, DBusMessage *msg,
						void *data)
{
	dbus_bool_t ongoing;

	if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_BOOLEAN, &ongoing,
						DBUS_TYPE_INVALID))
		return invalid_args(msg);

	if (ongoing_call_status && !ongoing) {
		/* An ongoing call has finished. Continue connection.*/
		sap_status_ind(sap_data, SAP_STATUS_CHANGE_CARD_RESET);
		ongoing_call_status = FALSE;
	} else if (!ongoing_call_status && ongoing) {
		/* An ongoing call has started.*/
		ongoing_call_status = TRUE;
	}

	DBG("OngoingCall status set to %d", ongoing_call_status);

	return dbus_message_new_method_return(msg);
}

static DBusMessage *max_msg_size(DBusConnection *conn, DBusMessage *msg,
						void *data)
{
	dbus_uint32_t size;

	if (sim_card_conn_status == SIM_CONNECTED)
		return g_dbus_create_error(msg, "org.bluez.Error.Failed",
				"Can't change msg size when connected.");

	if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_UINT32, &size,
							DBUS_TYPE_INVALID))
		return invalid_args(msg);

	max_msg_size_supported = size;

	DBG("MaxMessageSize set to %d", max_msg_size_supported);

	return dbus_message_new_method_return(msg);
}

static DBusMessage *disconnect_immediate(DBusConnection *conn, DBusMessage *msg,
						void *data)
{
	if (sim_card_conn_status == SIM_DISCONNECTED)
		return g_dbus_create_error(msg, "org.bluez.Error.Failed",
				"Already disconnected.");

	sim_card_conn_status = SIM_DISCONNECTED;
	sap_disconnect_ind(sap_data, SAP_DISCONNECTION_TYPE_IMMEDIATE);

	return dbus_message_new_method_return(msg);
}

static DBusMessage *card_status(DBusConnection *conn, DBusMessage *msg,
								void *data)
{
	dbus_uint32_t status;

	DBG("status %d", sim_card_conn_status);

	if (sim_card_conn_status != SIM_CONNECTED)
		return g_dbus_create_error(msg, "org.bluez.Error.Failed",
				"Can't change msg size when not connected.");

	if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_UINT32, &status,
							DBUS_TYPE_INVALID))
		return invalid_args(msg);

	switch (status) {
	case 0: /* card removed */
		sim_card_conn_status = SIM_MISSING;
		sap_status_ind(sap_data, SAP_STATUS_CHANGE_CARD_REMOVED);
		break;

	case 1: /* card inserted */
		if (sim_card_conn_status == SIM_MISSING) {
			sim_card_conn_status = SIM_CONNECTED;
			sap_status_ind(sap_data,
					SAP_STATUS_CHANGE_CARD_INSERTED);
		}
		break;

	case 2: /* card not longer available*/
		sim_card_conn_status = SIM_POWERED_OFF;
		sap_status_ind(sap_data, SAP_STATUS_CHANGE_CARD_NOT_ACCESSIBLE);
		break;

	default:
		return g_dbus_create_error(msg, "org.bluez.Error.Failed",
				"Unknown card status. Use 0, 1 or 2.");
	}

	DBG("Card status changed to %d", status);

	return dbus_message_new_method_return(msg);
}

static const GDBusMethodTable dummy_methods[] = {
	{ GDBUS_METHOD("OngoingCall",
				GDBUS_ARGS({ "ongoing", "b" }), NULL,
				ongoing_call) },
	{ GDBUS_METHOD("MaxMessageSize",
				GDBUS_ARGS({ "size", "u" }), NULL,
				max_msg_size) },
	{ GDBUS_METHOD("DisconnectImmediate", NULL, NULL,
				disconnect_immediate) },
	{ GDBUS_METHOD("CardStatus",
				GDBUS_ARGS({ "status", "" }), NULL,
				card_status) },
	{ }
};

int sap_init(void)
{
	connection = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);

	if (g_dbus_register_interface(connection, SAP_DUMMY_PATH,
				SAP_DUMMY_IFACE, dummy_methods, NULL, NULL,
				NULL, NULL) == FALSE) {
		error("sap-dummy interface %s init failed on path %s",
					SAP_DUMMY_IFACE, SAP_DUMMY_PATH);
		dbus_connection_unref(connection);
		connection = NULL;
		return -1;
	}

	return 0;
}

void sap_exit(void)
{
	g_dbus_unregister_interface(connection, SAP_DUMMY_PATH,
							SAP_DUMMY_IFACE);

	dbus_connection_unref(connection);
	connection = NULL;
}
