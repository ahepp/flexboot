/*
 * Copyright (C) 2014-2015 Mellanox Technologies Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <ipxe/settings.h>
#include <ipxe/tables.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <ipxe/timer.h>
#include <ipxe/netdevice.h>
#include <ipxe/driver_settings.h>
#include <ipxe/in.h>
#include <ipxe/infiniband.h>
#include <ipxe/vlan.h>
#include <ipxe/iscsi.h>
#include "hermon.h"
#include "hermon_settings.h"

/* Hermon setting operation */
struct hermon_setting_operation {
	/** Setting */
	const struct setting *setting;
	int ( *applies ) ( struct settings *settings );
	/** Store setting (or NULL if not supported)
	 *
	 * @v netdev		Network device
	 * @v data		Setting data, or NULL to clear setting
	 * @v len		Length of setting data
	 * @ret rc		Return status code
	 */
	int ( * store ) ( struct settings *settings, const void *data,
			  size_t len, const struct setting *setting );
	/* Store setting to flash ( or NULL if not supported */
	int ( * nv_store ) ( struct settings *settings, void *priv );
};

struct hermon_device_name {
	/** Device ID */
	uint16_t device_id;
	/** Device name */
	char* name;
};

struct net_device* get_netdev_from_settings ( struct settings *settings ) {
	struct settings *main_settings = settings;
        struct generic_settings *generic;
        struct driver_settings *driver_set;
        struct hermon_port *port;
	struct net_device *trunk;
	struct net_device *netdev;

	while ( main_settings->default_scope != &port_scope )
		main_settings = main_settings->parent;

	generic = container_of ( main_settings, struct generic_settings, settings);
	driver_set = container_of ( generic, struct driver_settings, generic_settings);
	port = container_of ( driver_set, struct hermon_port, driver_settings);
	trunk = port->netdev;

	netdev =  vlan_present( trunk );
	if ( netdev )
		return netdev;

	return trunk;

}

int netdevice_settings_store ( struct settings *settings, const struct setting *setting,
				const void *data, size_t len ) {
	struct settings *port_settings = settings;
        struct net_device *netdev = get_netdev_from_settings ( settings );
	char dhcp_ip_en[10] = { 0 };
        int rc;

	/* Check if boot protocol is iSCSI, if not then don't store system settings */
	if ( ! is_protocol_iscsi ( settings ) )
		return SUCCESS;

	while ( port_settings->default_scope != &port_scope )
		port_settings = port_settings->parent;

	fetchf_setting ( port_settings, &dhcp_ip_setting, NULL, NULL, dhcp_ip_en, sizeof ( dhcp_ip_en ) );
	if ( dhcp_ip_en[0] == 'E' )
		return SUCCESS;

	if ( ( rc = store_setting ( netdev_settings ( netdev ), setting, data, len ) ) == 0 )
		return SUCCESS;

	return INVALID_INPUT;
}

int ipv4_address_store ( struct settings *settings , const void *data,
		    size_t len, const struct setting *setting __unused)  {
	return netdevice_settings_store ( settings, &ip_setting, data, len );
}

int subnet_mask_store ( struct settings *settings , const void *data,
                    size_t len, const struct setting *setting __unused)  {
        return netdevice_settings_store( settings, &netmask_setting, data, len );
}

int ipv4_gateway_store ( struct settings *settings , const void *data,
                    size_t len, const struct setting *setting __unused)  {
        return netdevice_settings_store( settings, &gateway_setting, data, len );
}

int ipv4_dns_store ( struct settings *settings , const void *data,
                    size_t len, const struct setting *setting __unused)  {
        return netdevice_settings_store( settings, &dns_setting, data, len );
}

int iscsi_settings_check_and_store ( struct settings *settings, const struct setting *setting,
		const void *data, size_t len, unsigned int max_len, unsigned int min_len ) {
	struct net_device *netdev = get_netdev_from_settings ( settings );

	if ( len != 0 && ( len > max_len || len < min_len ) )
		return INVALID_INPUT;

	if ( store_setting ( netdev_settings ( netdev ), setting, data, len ) )
		return INVALID_INPUT;

	return SUCCESS;
}

int iscsi_initiator_name_store ( struct settings *settings , const void *data,
                    size_t len , const struct setting *setting __unused ) {
	return iscsi_settings_check_and_store ( settings, &initiator_iqn_setting,
			data, len, MAX_ISCSI_NAME, MIN_ISCSI_NAME );
}

int iscsi_initiator_chapid_store ( struct settings *settings , const void *data,
                    size_t len , const struct setting *setting __unused ) {
	struct settings *port_settings = settings;
	char chap_en[10] = { 0 };
	char mut_chap_en[10] = { 0 };
	char connect[10] = { 0 };

	while ( port_settings->default_scope != &port_scope )
		port_settings = port_settings->parent;

	fetchf_setting ( port_settings, &connect_setting, NULL, NULL, connect, sizeof ( connect ) );
	fetchf_setting ( port_settings, &iscsi_chap_setting, NULL, NULL, chap_en, sizeof ( chap_en ) );
	fetchf_setting ( port_settings, &iscsi_mutual_chap_setting, NULL, NULL, mut_chap_en, sizeof ( mut_chap_en ) );
	if ( ( chap_en[0] == 'D' ) || ( mut_chap_en[0] == 'D' ) || ( connect[0] != 'E' ) ) {
		return SUCCESS;
	}

	return iscsi_settings_check_and_store ( settings, &reverse_username_setting,
			data, len, MAX_CHAP_ID , MIN_CHAP_ID );
}

int iscsi_initiator_chapsec_store ( struct settings *settings , const void *data,
                    size_t len , const struct setting *setting __unused ) {
	struct settings *port_settings = settings;
	char chap_en[10] = { 0 };
	char mut_chap_en[10] = { 0 };
	char connect[10] = { 0 };

	while ( port_settings->default_scope != &port_scope )
		port_settings = port_settings->parent;

	fetchf_setting ( port_settings, &connect_setting, NULL, NULL, connect, sizeof ( connect ) );
	fetchf_setting ( port_settings, &iscsi_chap_setting, NULL, NULL, chap_en, sizeof ( chap_en ) );
	fetchf_setting ( port_settings, &iscsi_mutual_chap_setting, NULL, NULL, mut_chap_en, sizeof ( mut_chap_en ) );
	if ( ( chap_en[0] == 'D' ) || ( mut_chap_en[0] == 'D' ) || ( connect[0] != 'E' ) ) {
		return SUCCESS;
	}

	return iscsi_settings_check_and_store ( settings, &reverse_password_setting,
			data, len, MAX_CHAP_SECRET , MIN_CHAP_SECRET );
}

int iscsi_target_chapid_store ( struct settings *settings , const void *data,
                    size_t len , const struct setting *setting __unused ) {
	struct settings *port_settings = settings;
	char chap_en[10] = { 0 };
	char connect[10] = { 0 };

	while ( port_settings->default_scope != &port_scope )
		port_settings = port_settings->parent;

	fetchf_setting ( port_settings, &connect_setting, NULL, NULL, connect, sizeof ( connect ) );
	fetchf_setting ( port_settings, &iscsi_chap_setting, NULL, NULL, chap_en, sizeof ( chap_en ) );
	if ( ( chap_en[0] == 'D' ) || ( connect[0] != 'E' ) ) {
		return SUCCESS;
	}

	return iscsi_settings_check_and_store ( settings, &username_setting,
			data, len, MAX_CHAP_ID , MIN_CHAP_ID );
}

int iscsi_target_chapsec_store ( struct settings *settings , const void *data,
                    size_t len , const struct setting *setting __unused ) {
	struct settings *port_settings = settings;
	char chap_en[10] = { 0 };
	char connect[10] = { 0 };

	while ( port_settings->default_scope != &port_scope )
		port_settings = port_settings->parent;

	fetchf_setting ( port_settings, &connect_setting, NULL, NULL, connect, sizeof ( connect ) );
	fetchf_setting ( port_settings, &iscsi_chap_setting, NULL, NULL, chap_en, sizeof ( chap_en ) );
	if ( ( chap_en[0] == 'D' ) || ( connect[0] != 'E' ) ) {
		return SUCCESS;
	}

	return iscsi_settings_check_and_store ( settings, &password_setting,
		data, len, MAX_CHAP_SECRET , MIN_CHAP_SECRET );
}

int iscsi_tgt_store_root_path ( struct settings *settings, const struct setting *setting,
			const void *data, size_t len ) {
	struct net_device *netdev;
	char old[256];
	int fetch_rc, rc;

	if ( ! settings )
		return INVALID_INPUT;

	/* Save previous value in case of error */
	fetch_rc = fetchf_setting ( settings, setting, NULL, NULL, old, sizeof ( old ) );
	if ( ( rc = generic_settings_store ( settings, setting, data, len ) ) )
		return rc;

	netdev = get_netdev_from_settings ( settings );
	if ( ( rc = root_path_store ( netdev_settings ( netdev ), settings ) ) ) {
		if ( fetch_rc > 0 )
			generic_settings_store ( settings, setting, old, sizeof ( old ) );
		return rc;
	}

	return 0;
}

int target_ip_store ( struct settings *settings, const void *data,
                    size_t len, const struct setting *setting ) {
	return iscsi_tgt_store_root_path ( settings, setting, data, len );
}

int target_iscsi_name_store ( struct settings *settings , const void *data,
                    size_t len , const struct setting *setting ) {
	if ( data && len > MAX_ISCSI_NAME ) {
		return INVALID_INPUT;
	}

	return iscsi_tgt_store_root_path ( settings, setting, data, len );
}

int target_tcp_port_store ( struct settings *settings , const void *data,
                    size_t len, const struct setting *setting ) {
	int32_t input = 0;

	if ( data ) {
		input = ntohl ( *( ( int32_t * ) data ) );

		if ( input > MAX_TCP_PORT || input < MIN_TCP_PORT )
			return INVALID_INPUT;
	}

	return iscsi_tgt_store_root_path ( settings, setting, data, len );
}

int target_boot_lun_store ( struct settings *settings , const void *data,
                    size_t len, const struct setting *setting ) {
	int32_t input = 0;

	if ( data ) {
		input = ntohl ( *( ( int32_t * ) data ) );

		if ( input > MAX_BOOT_LUN || input < MIN_BOOT_LUN )
			return INVALID_INPUT;
	}

	return iscsi_tgt_store_root_path ( settings, setting, data, len );
}


struct driver_settings* get_driver_settings_from_settings ( struct settings *settings ) {
	struct settings *main_settings = settings;
	struct generic_settings *generic;
	struct driver_settings *driver_settings;

	/* If these are not hermon_settings, needs to get to main settings */
	if ( ( settings->default_scope != &main_scope ) && ( settings->default_scope != &fw_scope ) ) {
		while ( main_settings->default_scope != &port_scope )
			main_settings = main_settings->parent;
	} else {
		if ( settings->default_scope == &fw_scope )
			main_settings = main_settings->parent;
	}

	generic = container_of ( main_settings, struct generic_settings, settings);
	driver_settings = container_of ( generic, struct driver_settings, generic_settings);

	return driver_settings;
}

struct hermon_port* get_hermon_port_from_settings ( struct settings *settings ) {
	struct settings *port_settings = settings;

	if ( ! settings )
		return NULL;

	while ( port_settings->default_scope != &port_scope )
		port_settings = port_settings->parent;

	struct driver_settings *driver_settings = get_driver_settings_from_settings ( port_settings );
	struct hermon_port* port = container_of ( driver_settings, struct hermon_port, driver_settings );
	return port;
}

int hermon_wol_applies ( struct settings *settings ) {
	struct hermon_port *port = get_hermon_port_from_settings ( settings );
	if ( ! port )
		return DOES_NOT_APPLY;
	struct hermon *hermon = ib_get_drvdata ( port->ibdev );

	if ( ( port->ibdev->port == 1 ) && hermon->cap.nv_config_flags.nv_config_wol_port1 )
		return APPLIES;

	if ( ( port->ibdev->port == 2 ) && hermon->cap.nv_config_flags.nv_config_wol_port2 )
		return APPLIES;

	return DOES_NOT_APPLY;
}

int hermon_blink_leds_applies ( struct settings *settings ) {
	struct hermon_port *port = get_hermon_port_from_settings ( settings );
	if ( ! port )
		return DOES_NOT_APPLY;
        struct hermon *hermon = ib_get_drvdata ( port->ibdev );

	return hermon->cap.port_beacon;
}

int hermon_vm_applies (struct settings *settings ) {
	struct driver_settings* driver_settings = get_driver_settings_from_settings ( settings );
	struct hermon *hermon = container_of ( driver_settings, struct hermon, hermon_settings );

	return hermon->cap.nv_config_flags.nv_config_sriov_en;
}

static int hermon_nv_write_blink_leds ( struct hermon *hermon,
			struct hermon_port *port, int duration ) {
	struct hermon_nvmem_port_conf *conf = & ( port->port_nv_conf );
	union hermonprm_set_port set_port;
	int rc = 0;

	conf->blink_leds = duration;
	/* Set blink LEDs if supported */
	if ( hermon->cap.port_beacon ) {
		memset ( &set_port, 0, sizeof ( set_port ) );
		MLX_FILL_1 ( &set_port.beacon, 0, duration, conf->blink_leds );
		MLX_FILL_1 ( &set_port.beacon, 0, version, 0 );
		if ( ( rc = hermon_cmd_set_port ( hermon, port->ibdev->port,
			&set_port, 4 ) ) != 0 ) {
			DBGC ( hermon, "ConnectX3 %p port %d could not set port: %s\n",
				hermon, port->ibdev->port, strerror ( rc ) );
		}
	}
	return rc;
}

int blink_leds_store ( struct settings *settings, const void *data,
			  size_t len,const  struct setting *setting ) {
	struct hermon_port *port;
	struct hermon *hermon;
	int32_t input;
	int rc;

	if ( !data  || ntohl ( * ( int32_t * ) data ) == 0 )
		return SUCCESS;

	input = ntohl ( * ( int32_t * ) data );

	if ( input > MAX_BLINK_LEDS || input < 0 )
		return INVALID_INPUT;

	port = get_hermon_port_from_settings ( settings );
	if ( !port )
		return INVALID_INPUT;
	hermon = ib_get_drvdata ( port->ibdev );
	generic_settings_store ( settings, setting, data, len );

	if ( ( rc = hermon_open ( hermon ) ) != 0 ) {
		printf ( " Couldn't open hermon (rc = %d) \n", rc );
		return INVALID_INPUT; /* TODO - change to einval */
	}

	if ( (rc =  hermon_nv_write_blink_leds ( hermon, port, input ) ) != 0 ) {
		DBGC ( hermon, "Failed to store blink leds (rc = %d)\n",        rc );
	}

	/* Close hardware */
	hermon_close ( hermon );

	return STORED;
}

#define MAX_SETTING_STR 128
int virt_mode_restore ( struct settings *settings, const void *data,
						size_t len __unused, const struct setting *setting ) {
	struct driver_settings* driver_settings = get_driver_settings_from_settings ( settings );
        struct hermon *hermon = container_of ( driver_settings, struct hermon, hermon_settings );

	/* If setting is deleted, set default */
	if ( !data ) {
		if ( hermon->defaults.sriov_en )
			generic_settings_store ( settings, setting, STR_SRIOV, strlen ( STR_SRIOV ) + 1 );
		else
			generic_settings_store ( settings, setting, STR_NONE, strlen ( STR_NONE ) + 1 );
		return STORED;
	}

	return SUCCESS;
}

#define RESTORE_ENABLED_DISABLED( defaultdata )		do {								\
	struct hermon_port *port;											\
		if ( !data ) {												\
			port = get_hermon_port_from_settings ( settings );						\
			if ( ! port )																							\
				return SUCCESS;																			\
			if ( port->defaults.defaultdata )								\
				generic_settings_store ( settings, setting, STR_ENABLE, strlen ( STR_ENABLE ) + 1 );	\
			else												\
				generic_settings_store ( settings, setting, STR_DISABLE, strlen ( STR_DISABLE ) + 1 );	\
			return STORED;											\
		}													\
		return SUCCESS;												\
	} while ( 0 )


int virt_num_check_or_restore ( struct settings *settings, const void *data,
				size_t len __unused, const struct setting *setting ) {
	struct driver_settings* driver_settings = get_driver_settings_from_settings ( settings );
	struct hermon *hermon = container_of ( driver_settings, struct hermon, hermon_settings );
	int32_t maximum, number;
	int rc;

	/* If setting is deleted, set default */
	if ( !data ) {
		number = ntohl ( hermon->defaults.total_vfs  );
		generic_settings_store ( settings, setting, &number, sizeof ( number ) );
		return STORED;
	}

	number = ntohl ( * ( int32_t * ) data );
	if ( number < 0 )
		return INVALID_INPUT;

	if ( ( rc = generic_settings_fetch ( settings, &virt_num_max_setting,
						&maximum, sizeof ( maximum ) ) ) <= 0 )
		return rc;

	maximum = htonl ( maximum );
	if ( number >= maximum )
		return INVALID_INPUT;

	return SUCCESS;
}

int boot_protocol_restore ( struct settings *settings, const void *data,
			size_t len __unused, const struct setting *setting ) {
	struct hermon_port *port;

        /* If setting is deleted, set default */
        if ( !data ) {
		port = get_hermon_port_from_settings ( settings );
		if ( ! port )
			return SUCCESS;

		/** Assuming boot protocol is 0-none, 1-pxe, 2-iscsi */
		switch ( port->defaults.boot_protocol ) {
			case 2  : generic_settings_store ( settings, setting, STR_ISCSI, strlen ( STR_ISCSI ) + 1 );
				   break;
			case 1 : generic_settings_store ( settings, setting, STR_PXE, strlen ( STR_PXE ) + 1 );
				   break;
			default : generic_settings_store ( settings, setting, STR_NONE, strlen ( STR_NONE ) + 1 );
				   break;
		}
		return STORED;
	}

	return SUCCESS;
}

int virt_lan_restore ( struct settings *settings, const void *data,
			size_t len __unused, const struct setting *setting ) {
	RESTORE_ENABLED_DISABLED ( boot_vlan_en );
}

int virt_id_check_or_restore ( struct settings *settings, const void *data,
			size_t len __unused ,const  struct setting *setting ) {
	struct hermon_port *port;
	int32_t number;

        /* If setting is deleted, set default */
	if ( !data ) {
		port = get_hermon_port_from_settings ( settings );
		if ( ! port )
			return SUCCESS;
		number = ntohl ( port->defaults.boot_vlan  );
		generic_settings_store ( settings, setting, &number, sizeof ( number ) );
		return STORED;
	}

	number = ntohl ( * ( int32_t * ) data );

	if ( number > MAX_VIRTUAL_ID || number < MIN_VIRTUAL_ID )
		return INVALID_INPUT;

	return SUCCESS;
}

int opt_rom_restore ( struct settings *settings, const void *data,
                    size_t len __unused ,const  struct setting *setting ) {
	RESTORE_ENABLED_DISABLED ( boot_option_rom_en );
}

int boot_retries_restore ( struct settings *settings, const void *data,
                    size_t len __unused ,const  struct setting *setting ) {
	struct hermon_port *port;

	const char *retries;
	/* If setting is deleted, set default */
	if ( !data ) {
		port = get_hermon_port_from_settings ( settings );
		if ( ! port )
			return SUCCESS;
		retries = driver_settings_get_boot_ret_str ( port->defaults.boot_retry_count );
		generic_settings_store ( settings, setting, retries, 10 );
		return STORED;
	}

	return SUCCESS;
}

int wol_restore ( struct settings *settings, const void *data,
                    size_t len __unused ,const  struct setting *setting ) {
        RESTORE_ENABLED_DISABLED ( en_wol_magic );
}

int dhcp_ip_restore ( struct settings *settings, const void *data,
		size_t len __unused,const struct setting *setting ) {
	RESTORE_ENABLED_DISABLED( iscsi_ipv4_dhcp_en );
}

int dhcp_iscsi_restore ( struct settings *settings, const void *data,
		size_t len __unused,const struct setting *setting ) {
	RESTORE_ENABLED_DISABLED( iscsi_dhcp_params_en );
}

int iscsi_chap_restore ( struct settings *settings, const void *data,
		size_t len __unused,const struct setting *setting ) {
	RESTORE_ENABLED_DISABLED( iscsi_chap_auth_en );
}

int iscsi_mutual_chap_restore ( struct settings *settings, const void *data,
		size_t len __unused,const struct setting *setting ) {
	RESTORE_ENABLED_DISABLED( iscsi_chap_mutual_auth_en );
}

int connect_restore ( struct settings *settings, const void *data,
		size_t len __unused,const struct setting *setting ) {
	struct hermon_port *port;
	if ( !data ) {
		port = get_hermon_port_from_settings ( settings );
		if ( ! port )
			return SUCCESS;
		if ( ( port->port_nv_conf.nic.boot_conf.legacy_boot_prot == BOOT_PROTOCOL_ISCSI ) &&
		     ( port->defaults.iscsi_dhcp_params_en == 0 ) )
			generic_settings_store ( settings, setting, STR_ENABLE, strlen ( STR_ENABLE ) + 1 );
		else
			generic_settings_store ( settings, setting, STR_DISABLE, strlen ( STR_DISABLE ) + 1 );
		return STORED;
	}
	return SUCCESS;
}

#define HERMON_FETCH_SETTING( settings, setting )                                       \
        do {                                                                            \
		memset ( buf, 0, sizeof ( buf ) );                                      \
                if ( ( rc = fetchf_setting ( settings, setting , NULL, NULL, buf,       \
                                        sizeof ( buf ) ) ) <= 0 ) {                     \
                        DBGC ( hermon, "Failed to fetch %s setting (rc = %d)\n",        \
                                ( setting )->name, rc );                                \
                }                                                                       \
        } while ( 0 )

int hermon_flash_write_tlv ( struct hermon *hermon, void *src, uint32_t port_num,
                            uint32_t tlv_type, uint32_t len ) {
	struct hermon_tlv_header tlv;
	int rc;

	memset ( &tlv, 0, sizeof ( tlv ) );
	tlv.type        = tlv_type;
	tlv.length      = len;
	tlv.type_mod    = port_num;
	tlv.version     = hermon_get_tlv_version ( tlv_type );
	tlv.data        = src;

	if ( ( rc = hermon_write_conf_tlv ( hermon, &tlv ) ) )
		DBGC ( hermon, "Failed to save tlv type %d in the flash (rc = %d)\n",
			tlv_type, rc );
	return rc;
}

static int virt_nv_store ( struct settings *settings, void *priv ) {
	struct hermon *hermon = ( struct hermon * ) priv;
	struct hermon_nvmem_conf *conf = & ( hermon->hermon_nv_conf );
	char buf[255] = {0};
	uint32_t dword;
	int rc;

	HERMON_FETCH_SETTING ( settings, &virt_mode_setting );
	conf->virt_conf.virt_mode = ( buf[0] == 'S' );
	HERMON_FETCH_SETTING ( settings, &virt_num_setting );
	conf->virt_conf.num_of_vfs = strtoul ( buf, NULL, 10 );

	dword = cpu_to_be32 ( conf->virt_conf.dword );
	if ( ( rc = hermon_flash_write_tlv ( hermon, &dword, 0, VIRTUALIZATION_TYPE, sizeof ( dword ) ) ) )
		return -EACCES;
	return 0;
}

static struct hermon_port * hermon_get_port_from_settings ( struct hermon *hermon,
						struct settings *settings ) {
	unsigned int i;
	for ( i = 0; i < hermon->num_ports; i++ ) {
		if ( settings == & ( hermon->port[i].driver_settings.generic_settings.settings ) )
			return & ( hermon->port[i] );
	}
	return NULL;
}

#define PORT_CHECK( port )							\
	do {									\
		if ( ! port ) {							\
			DBGC ( hermon, "Failed to get port from settings\n" );	\
			return -ENODEV;						\
		}								\
	} while ( 0 )

static int nic_boot_nv_store ( struct settings *settings, void *priv ) {
	struct hermon *hermon = ( struct hermon * ) priv;
	struct hermon_port *port = hermon_get_port_from_settings ( hermon, settings );
	struct hermon_nic_conf *nic_conf;
	union hermon_nic_boot_conf *boot_conf;;
	union hermon_nic_boot_conf reversed_boot_conf;
	char buf[255] = {0};
	int rc;

	PORT_CHECK( port );

	nic_conf = & ( port->port_nv_conf.nic );
	boot_conf = & ( nic_conf->boot_conf );
	HERMON_FETCH_SETTING ( settings, &boot_protocol_setting );
	switch ( buf[0] ) {
		case 'N' : boot_conf->legacy_boot_prot = BOOT_PROTOCOL_NONE;
			break;
		case 'P' : boot_conf->legacy_boot_prot = BOOT_PROTOCOL_PXE;
			break;
		case 'i' : boot_conf->legacy_boot_prot = BOOT_PROTOCOL_ISCSI;
			break;
	}
	HERMON_FETCH_SETTING ( settings, &virt_lan_setting );
	boot_conf->en_vlan = ( buf[0] == 'E' );

	HERMON_FETCH_SETTING ( settings, &virt_id_setting );
	boot_conf->vlan_id = strtoul ( buf, NULL, 10 );

	HERMON_FETCH_SETTING ( settings, &opt_rom_setting );
	boot_conf->en_option_rom = ( buf[0] == 'E' );

	boot_conf->boot_retry_count = driver_settings_get_boot_ret_val ( settings );
	reversed_boot_conf.dword = cpu_to_be32 ( boot_conf->dword );

	if ( ( rc = hermon_flash_write_tlv ( hermon, &reversed_boot_conf, port->ibdev->port,
				BOOT_SETTINGS_TYPE,  sizeof ( reversed_boot_conf ) ) ) )
		return -EACCES;
	return 0;
}

static int wol_nv_store ( struct settings *settings, void *priv ) {
	struct hermon *hermon = ( struct hermon * ) priv;
	struct hermon_port *port = hermon_get_port_from_settings ( hermon, settings );
        union hermon_wol_conf *wol_conf;
	char buf[255] = {0};
	uint32_t swapped[2];
	int rc;

	PORT_CHECK( port );

	wol_conf = & ( port->port_nv_conf.nic.wol_conf );
	HERMON_FETCH_SETTING ( settings, &wol_setting );
	wol_conf->en_wol_magic = ( buf[0] == 'E' );
	swapped[0] = cpu_to_be32(wol_conf->dword[0]);
	swapped[1] = cpu_to_be32(wol_conf->dword[1]);

	if ( ( rc = hermon_flash_write_tlv ( hermon, swapped, port->ibdev->port, WAKE_ON_LAN_TYPE,
				sizeof ( *wol_conf ) ) ) )
		return -EACCES;
	return 0;
}

static int dhcp_flags_nv_store ( struct settings *settings, void *priv ) {
	struct hermon *hermon = ( struct hermon * ) priv;
	struct hermon_port *port = hermon_get_port_from_settings ( hermon, settings );
	union hermon_iscsi_init_dhcp_conf  *conf;
	union hermon_iscsi_init_dhcp_conf swapped;
	char buf[255] = {0};
	int rc;

	PORT_CHECK( port );

	conf = & ( port->port_nv_conf.iscsi.init_dhcp_conf );
	HERMON_FETCH_SETTING ( settings, &dhcp_ip_setting );
	conf->ipv4_dhcp_en = ( buf[0] == 'E' );
	HERMON_FETCH_SETTING ( settings, &dhcp_iscsi_setting );
	conf->dhcp_iscsi_en = ( buf[0] == 'E' );

	swapped.dword = cpu_to_be32 ( conf->dword );
	if ( ( rc = hermon_flash_write_tlv ( hermon, &swapped, port->ibdev->port,
			ISCSI_INITIATOR_DHCP_CONF_TYPE,	sizeof ( swapped ) ) ) ) {
		return -EACCES;
	}
	return 0;
}

static int iscsi_gen_flags_nv_store ( struct settings *settings, void *priv ) {
	struct hermon *hermon = ( struct hermon * ) priv;
	struct hermon_port *port = hermon_get_port_from_settings ( hermon, settings );
	union hermon_iscsi_general *gen_conf;
	union hermon_iscsi_general swapped;
	char buf[255] = {0};
	int rc;

	PORT_CHECK( port );

	gen_conf = & ( port->port_nv_conf.iscsi.gen_conf );
	HERMON_FETCH_SETTING ( settings, &iscsi_chap_setting );
	gen_conf->chap_auth_en = ( buf[0] == 'E' );
	HERMON_FETCH_SETTING ( settings, &iscsi_mutual_chap_setting );
	gen_conf->chap_mutual_auth_en = ( buf[0] == 'E' );

	swapped.dword[0] = cpu_to_be32 ( gen_conf->dword[0] );
	swapped.dword[1] = cpu_to_be32 ( gen_conf->dword[1] );
	swapped.dword[2] = cpu_to_be32 ( gen_conf->dword[2] );

	if ( ( rc = hermon_flash_write_tlv ( hermon, &swapped, port->ibdev->port,
			ISCSI_GENERAL_SETTINGS_TYPE, sizeof ( swapped ) ) ) ) {
		return -EACCES;
	}
	return 0;
}

int hermon_flash_invalidate_tlv ( struct hermon *hermon, uint32_t port_num,
				uint32_t tlv_type ) {
	struct hermon_tlv_header tlv;
	int rc;

	memset ( &tlv, 0, sizeof ( tlv ) );
	tlv.type        = tlv_type;
	tlv.type_mod    = port_num;
	tlv.version     = hermon_get_tlv_version ( tlv_type );

	if ( ( rc = hermon_invalidate_conf_tlv ( hermon, &tlv ) ) ) {
		DBGC ( hermon, "Failed to invalidate tlv type %d (rc = %d)\n",
			tlv_type, rc );
	}

	return rc;
}

static int iscsi_parameters_nv_store ( struct hermon *hermon, struct hermon_port *port,
				struct settings *settings,  struct setting *setting,
				void *source, uint32_t size, uint32_t tlv_type ) {
	int port_num = port->ibdev->port;
	char buf[255] = {0};
	int rc;

	HERMON_FETCH_SETTING ( settings, setting );
	if ( rc > 0 ) {
		memcpy ( source, buf, size );
		rc = hermon_flash_write_tlv ( hermon, source, port_num, tlv_type, size );
	} else {
		rc = 0;
		/* Connect will always be fetched since it has a default value, this check will always return true */
		if ( strncmp (setting->name, "Connect", strlen ( "Connect" ) ) )
			hermon_flash_invalidate_tlv ( hermon, port_num, tlv_type );
	}
	if ( rc )
		return -EACCES;
	return 0;
}


static int ipv4_address_nv_store ( struct settings *settings, void *priv ) {
	struct hermon *hermon = ( struct hermon * ) priv;
	struct hermon_port *port = hermon_get_port_from_settings ( hermon, settings );

	PORT_CHECK( port );
	return iscsi_parameters_nv_store ( hermon, port, settings, &ipv4_add_setting,
			&port->port_nv_conf.iscsi.initiator_params.ipv4_addr,
			sizeof ( port->port_nv_conf.iscsi.initiator_params.ipv4_addr ),
			ISCSI_INITIATOR_IPV4_ADDR );
}

static int subnet_mask_nv_store ( struct settings *settings, void *priv) {
	struct hermon *hermon = ( struct hermon * ) priv;
        struct hermon_port *port = hermon_get_port_from_settings ( hermon, settings );

	PORT_CHECK( port );
	return iscsi_parameters_nv_store ( hermon, port, settings, &subnet_mask_setting,
			&port->port_nv_conf.iscsi.initiator_params.subnet,
			sizeof ( port->port_nv_conf.iscsi.initiator_params.subnet ),
			ISCSI_INITIATOR_SUBNET );
}

static int ipv4_gateway_nv_store ( struct settings *settings, void *priv ) {
	struct hermon *hermon = ( struct hermon * ) priv;
        struct hermon_port *port = hermon_get_port_from_settings ( hermon, settings );

	PORT_CHECK( port );
	return iscsi_parameters_nv_store ( hermon, port, settings, &ipv4_gateway_setting,
			&port->port_nv_conf.iscsi.initiator_params.gateway,
			sizeof ( port->port_nv_conf.iscsi.initiator_params.gateway ),
			ISCSI_INITIATOR_IPV4_GATEWAY );
}

static int ipv4_dns_nv_store ( struct settings *settings, void *priv ) {
	struct hermon *hermon = ( struct hermon * ) priv;
        struct hermon_port *port = hermon_get_port_from_settings ( hermon, settings );

	PORT_CHECK( port );
	return iscsi_parameters_nv_store ( hermon, port, settings, &ipv4_dns_setting,
			&port->port_nv_conf.iscsi.initiator_params.primary_dns,
			sizeof ( port->port_nv_conf.iscsi.initiator_params.primary_dns ),
			ISCSI_INITIATOR_IPV4_PRIM_DNS );
}

static int iscsi_initiator_name_nv_store ( struct settings *settings, void *priv ) {
	struct hermon *hermon = ( struct hermon * ) priv;
        struct hermon_port *port = hermon_get_port_from_settings ( hermon, settings );

	PORT_CHECK( port );
	return iscsi_parameters_nv_store ( hermon, port, settings,  &iscsi_init_name_setting,
		&port->port_nv_conf.iscsi.initiator_params.name,
		sizeof ( port->port_nv_conf.iscsi.initiator_params.name ),
		ISCSI_INITIATOR_NAME );
}

static int iscsi_initiator_chapid_nv_store ( struct settings *settings, void *priv ) {
	struct hermon *hermon = ( struct hermon * ) priv;
	struct hermon_port *port = hermon_get_port_from_settings ( hermon, settings );

	PORT_CHECK( port );
	return iscsi_parameters_nv_store ( hermon, port, settings, &init_chapid_setting,
		&port->port_nv_conf.iscsi.initiator_params.chap_id,
		sizeof ( port->port_nv_conf.iscsi.initiator_params.chap_id ),
		ISCSI_INITIATOR_CHAP_ID );
}

static int iscsi_initiator_chapsec_nv_store ( struct settings *settings, void *priv ) {
	struct hermon *hermon = ( struct hermon * ) priv;
	struct hermon_port *port = hermon_get_port_from_settings ( hermon, settings );

	PORT_CHECK( port );
	return iscsi_parameters_nv_store ( hermon, port, settings, &init_chapsec_setting,
		&port->port_nv_conf.iscsi.initiator_params.chap_pass,
		sizeof ( port->port_nv_conf.iscsi.initiator_params.chap_pass ),
		ISCSI_INITIATOR_CHAP_PWD );
}


static int connect_nv_store ( struct settings *settings, void *priv ) {
	struct hermon *hermon = ( struct hermon * ) priv;
	struct hermon_port *port = hermon_get_port_from_settings ( hermon, settings );

        PORT_CHECK( port );
	return iscsi_parameters_nv_store ( hermon, port, settings, &connect_setting,
		&port->port_nv_conf.iscsi.first_tgt_params.connect,
		sizeof ( port->port_nv_conf.iscsi.first_tgt_params.connect ),
		CONNECT_FIRST_TGT );
}

static int target_ip_nv_store ( struct settings *settings, void *priv ) {
	struct hermon *hermon = ( struct hermon * ) priv;
	struct hermon_port *port = hermon_get_port_from_settings ( hermon, settings );

        PORT_CHECK( port );
	return iscsi_parameters_nv_store ( hermon, port, settings, &target_ip_setting,
		&port->port_nv_conf.iscsi.first_tgt_params.ip_addr,
		sizeof ( port->port_nv_conf.iscsi.first_tgt_params.ip_addr ),
		FIRST_TGT_IP_ADDRESS );
}

static int target_tcp_port_nv_store ( struct settings *settings, void *priv ) {
	struct hermon *hermon = ( struct hermon * ) priv;
	struct hermon_port *port = hermon_get_port_from_settings ( hermon, settings );

        PORT_CHECK( port );
	return iscsi_parameters_nv_store ( hermon, port, settings, &tcp_port_setting,
		&port->port_nv_conf.iscsi.first_tgt_params.tcp_port,
		sizeof ( port->port_nv_conf.iscsi.first_tgt_params.tcp_port ),
		FIRST_TGT_TCP_PORT );
}

static int target_boot_lun_nv_store ( struct settings *settings, void *priv ) {
	struct hermon *hermon = ( struct hermon * ) priv;
	struct hermon_port *port = hermon_get_port_from_settings ( hermon, settings );

        PORT_CHECK( port );
	return iscsi_parameters_nv_store ( hermon, port, settings, &boot_lun_setting,
		&port->port_nv_conf.iscsi.first_tgt_params.lun,
		sizeof ( port->port_nv_conf.iscsi.first_tgt_params.lun ),
		FIRST_TGT_BOOT_LUN );
}

static int target_iscsi_name_nv_store ( struct settings *settings, void *priv ) {
	struct hermon *hermon = ( struct hermon * ) priv;
	struct hermon_port *port = hermon_get_port_from_settings ( hermon, settings );

        PORT_CHECK( port );
	return iscsi_parameters_nv_store ( hermon, port, settings, &iscsi_target_name_setting,
		&port->port_nv_conf.iscsi.first_tgt_params.name,
		sizeof ( port->port_nv_conf.iscsi.first_tgt_params.name ),
		FIRST_TGT_ISCSI_NAME );
}

static int iscsi_target_chapid_nv_store ( struct settings *settings, void *priv ) {
	struct hermon *hermon = ( struct hermon * ) priv;
	struct hermon_port *port = hermon_get_port_from_settings ( hermon, settings );

        PORT_CHECK( port );
	return iscsi_parameters_nv_store ( hermon, port, settings, &target_chapid_setting,
		&port->port_nv_conf.iscsi.first_tgt_params.chap_id,
		sizeof ( port->port_nv_conf.iscsi.first_tgt_params.chap_id ),
		FIRST_TGT_CHAP_ID );
}

static int iscsi_target_chapsec_nv_store ( struct settings *settings, void *priv ) {
	struct hermon *hermon = ( struct hermon * ) priv;
	struct hermon_port *port = hermon_get_port_from_settings ( hermon, settings );

        PORT_CHECK( port );
	return iscsi_parameters_nv_store ( hermon, port, settings, &target_chapsec_setting,
		&port->port_nv_conf.iscsi.first_tgt_params.chap_pass,
		sizeof ( port->port_nv_conf.iscsi.first_tgt_params.chap_pass ),
		FIRST_TGT_CHAP_PWD );
}

static struct hermon_setting_operation hermon_setting_operations[] = {
	{ &virt_mode_setting,           &hermon_vm_applies, &virt_mode_restore, &virt_nv_store },
	{ &virt_num_setting,		NULL, &virt_num_check_or_restore, &virt_nv_store },
	{ &virt_num_max_setting,	NULL, NULL, NULL },
	{ &blink_leds_setting,		&hermon_blink_leds_applies, &blink_leds_store, NULL },
	{ &device_name_setting, 	NULL, NULL, NULL },
	{ &chip_type_setting,		NULL, NULL, NULL },
	{ &pci_id_setting,		NULL, NULL, NULL },
	{ &bus_dev_fun_setting,		NULL, NULL, NULL },
	{ &mac_add_setting,		NULL, NULL, NULL },
	{ &virt_mac_setting,		NULL, NULL, NULL },
	{ &flex_version_setting,	NULL, NULL, NULL },
	{ &fw_version_setting,		NULL, NULL, NULL },
	{ &boot_protocol_setting, 	NULL, &boot_protocol_restore, &nic_boot_nv_store },
	{ &virt_lan_setting,		NULL, &virt_lan_restore, &nic_boot_nv_store },
	{ &virt_id_setting,		NULL, &virt_id_check_or_restore, &nic_boot_nv_store },
	{ &opt_rom_setting,		NULL, &opt_rom_restore, &nic_boot_nv_store },
	{ &boot_retries_setting,	NULL, &boot_retries_restore, &nic_boot_nv_store },
	{ &wol_setting,			&hermon_wol_applies, &wol_restore, &wol_nv_store },
	{ &dhcp_ip_setting,		NULL, &dhcp_ip_restore, &dhcp_flags_nv_store },
	{ &dhcp_iscsi_setting,		NULL, &dhcp_iscsi_restore, &dhcp_flags_nv_store },
	{ &iscsi_chap_setting,		NULL, &iscsi_chap_restore, &iscsi_gen_flags_nv_store },
	{ &iscsi_mutual_chap_setting,	NULL, &iscsi_mutual_chap_restore, &iscsi_gen_flags_nv_store },
	{ &ip_ver_setting, 		NULL, NULL, NULL },
	{ &ipv4_add_setting,		NULL, &ipv4_address_store, &ipv4_address_nv_store },
	{ &subnet_mask_setting, 	NULL, &subnet_mask_store, &subnet_mask_nv_store },
	{ &ipv4_gateway_setting,	NULL, &ipv4_gateway_store, &ipv4_gateway_nv_store },
	{ &ipv4_dns_setting,		NULL, &ipv4_dns_store, &ipv4_dns_nv_store },
	{ &iscsi_init_name_setting,	NULL, &iscsi_initiator_name_store, &iscsi_initiator_name_nv_store },
	{ &init_chapid_setting,		NULL, &iscsi_initiator_chapid_store, &iscsi_initiator_chapid_nv_store },
	{ &init_chapsec_setting,	NULL, &iscsi_initiator_chapsec_store, &iscsi_initiator_chapsec_nv_store },
	{ &connect_setting,		NULL, &connect_restore, &connect_nv_store },
	{ &target_ip_setting,		NULL, &target_ip_store, &target_ip_nv_store },
	{ &tcp_port_setting,		NULL, &target_tcp_port_store, &target_tcp_port_nv_store },
	{ &boot_lun_setting,		NULL, &target_boot_lun_store, &target_boot_lun_nv_store },
	{ &iscsi_target_name_setting,	NULL, &target_iscsi_name_store, &target_iscsi_name_nv_store },
	{ &target_chapid_setting,	NULL, &iscsi_target_chapid_store, &iscsi_target_chapid_nv_store, },
	{ &target_chapsec_setting,	NULL, &iscsi_target_chapsec_store, &iscsi_target_chapsec_nv_store },
};

static int is_modified ( struct list_head *list, const char *name ) {
	struct extended_setting *ext;

	list_for_each_entry ( ext, list, modified ) {
		if ( strcmp (ext->setting->name, name ) == 0 )
			return 1;
	}
	return 0;
}

int hermon_store ( struct settings *settings,
		   const struct setting *setting,
		   const void *data, size_t len ) {

	struct hermon_setting_operation *op;
	struct extended_setting *ext;
	struct extended_setting *new_ext;
	struct driver_settings *driver_settings;
	unsigned int i;
	int rc = 0;

	/* Handle hermon specific settings */
	for ( i = 0 ; i < ( sizeof ( hermon_setting_operations ) /
			    sizeof ( hermon_setting_operations[0] ) ) ; i++ ) {
		op = &hermon_setting_operations[i];
		if ( setting_cmp ( setting, op->setting ) == 0 ) {
			if ( op->store ) {
				if ( ( rc = op->store ( settings, data, len, setting ) ) == INVALID_INPUT )
					return rc;
			}
			break;
		}
	}

	if ( rc != STORED )
		rc = generic_settings_store ( settings, setting, data, len );
	else
		rc = SUCCESS;

	/* Save setting in modified list - only for driver settings */
	driver_settings = get_driver_settings_from_settings ( settings );
	if ( rc == SUCCESS && driver_settings->store_menu ) {
		if ( ! is_modified ( &driver_settings->modified_list, setting->name ) ) {
			new_ext = malloc ( sizeof ( struct extended_setting ) );
			ext = find_extended ( setting );
			memcpy ( new_ext, ext, sizeof ( struct extended_setting ) );
			list_add ( &new_ext->modified, &driver_settings->modified_list );
		}
	}
	return rc;
}

int hermon_applies ( struct settings *settings, const struct setting *setting ) {
	struct hermon_setting_operation *op;
	unsigned int i, rc;

	for ( i = 0 ; i < ( sizeof ( hermon_setting_operations ) /
			    sizeof ( hermon_setting_operations[0] ) ) ; i++ ) {
		op = &hermon_setting_operations[i];
		if ( setting_cmp ( setting, op->setting ) == 0 ) {
			if ( setting->scope == settings->default_scope) {
				if ( op->applies ) {
					if ( ( rc = op->applies ( settings ) ) )
						return APPLIES;
					else
						return DOES_NOT_APPLY;
				} else {
					return APPLIES;
				}
				break;
			}
		}
	}
	return DOES_NOT_APPLY;
}

struct settings_operations hermon_settings_operations = {
	.store = &hermon_store,
	.fetch = &generic_settings_fetch,
	.applies = &hermon_applies,
};

int modified_has_duplicates ( struct list_head *modified_list,
                                struct extended_setting *ext ) {
	struct extended_setting *cur_extended = ext;
	list_for_each_entry_continue ( cur_extended, modified_list, modified ) {
		if ( ext->nv_store == cur_extended->nv_store )
			return 1;
	}
	return 0;
}

int hermon_nv_store ( struct driver_settings *driver __unused, void *priv ) {
	struct hermon *hermon = ( struct hermon * ) priv;
	struct driver_settings *driver_settings;
	struct extended_setting *ext;
	struct extended_setting *temp;
	int rc, flag = 0;

	if ( ! nv_settings_root ) {
		printf ( "%s: nv_settings_root is not initialized\n", __FUNCTION__ );
		return -22;
	}

	/* Make sure flash access is supported */
        if ( ! hermon->cap.nv_mem_access_supported )
                return 0;

	/* Check that something was modified */
	list_for_each_entry ( driver_settings, &driver_settings_list, list ) {
		if ( ! list_empty ( & ( driver_settings->modified_list ) ) ) {
			flag = 1;
			break;
		}
	}
	if ( ! flag )
		return 0;

	/* Open hardware */
	if ( ( rc = hermon_open ( hermon ) ) != 0 )
		return rc;

	list_for_each_entry ( driver_settings, &driver_settings_list, list ) {
		if ( ! list_empty ( &driver_settings->modified_list ) ) {
			list_for_each_entry_safe ( ext, temp, &driver_settings->modified_list, modified ) {
				if ( ext->nv_store && ! modified_has_duplicates ( &driver_settings->modified_list, ext ) ) {
					if ( ( rc = ext->nv_store ( &driver_settings->generic_settings.settings, hermon ) ) ) {
						printf ("Setting %s couldn't be saved - %s \n",
							ext->setting->name, strerror( rc ) );
					}
				}
			}
		}
	}

	/* Close hardware */
	hermon_close ( hermon );

	return 0;
}

void hermon_restore_port_default ( struct driver_settings *driver_settings ) {
	struct hermon_setting_operation *op;
	struct settings *settings;
	struct extended_setting *ext;
	unsigned int i;
	int rc;

	for ( i = 0 ; i < ( sizeof ( hermon_setting_operations ) /
                            sizeof ( hermon_setting_operations[0] ) ) ; i++ ) {
		op = &hermon_setting_operations[i];
		if ( fetch_setting_origin ( &driver_settings->generic_settings.settings, op->setting, &settings ) )
			continue;
		ext = find_extended ( op->setting );
		if ( settings && ext->type != LABEL ) {
			if ( ( rc = hermon_store ( settings, op->setting, NULL, 0 ) ) )
				DBG ( "Failed to store the default value of %s (rc = %d)\n",
						op->setting->name, rc );
		}
	}
}

void hermon_restore_default ( struct driver_settings *driver_settings __unused ) {
	struct hermon_setting_operation *op;
        struct settings *settings;
        struct extended_setting *ext;
        unsigned int i;
        int rc;

	if ( ! nv_settings_root ) {
		printf ( "%s: nv_settings_root is not initialized\n", __FUNCTION__ );
		return;
	}

	settings = &nv_settings_root->settings;

	for ( i = 0 ; i < ( sizeof ( hermon_setting_operations ) /
				sizeof ( hermon_setting_operations[0] ) ) ; i++ ) {
		op = &hermon_setting_operations[i];
		if ( !setting_applies ( settings, op->setting ) )
			continue;
		ext = find_extended ( op->setting );
		if ( ext->type != LABEL ) {
			if ( ( rc = hermon_store ( settings, op->setting, NULL, 0 ) ) )
				DBG ( "Failed to store the default value of %s (rc = %d)\n",
						op->setting->name, rc );
		}
	}
}

int get_virt_id_type ( struct settings *settings ) {
	char buf[MAX_SETTING_STR] = { 0 };
	int rc;

	rc = fetchf_setting ( settings, &virt_lan_setting, NULL, NULL, buf, MAX_SETTING_STR );
	if ( ( rc > 0 ) && ( buf[0] == 'D' ) )
		return LABEL;

	return INPUT;
}

int get_virt_num_type ( struct settings *settings ) {
	char buffer[MAX_SETTING_STR] = { 0 };
	int rc;

	if ( setting_applies ( settings, &virt_mode_setting ) ) {
		rc = fetchf_setting ( settings, &virt_mode_setting, NULL, NULL, buffer, MAX_SETTING_STR );
		if ( ( rc > 0 ) && ( buffer[0] == 'S' ) )
			return INPUT;
	}

	return LABEL;
}

#define HERMON_PORT_LABEL_LEN 100
int hermon_init_port_settings ( struct hermon *hermon, struct hermon_port *port ) {
	struct driver_settings *driver_settings = &port->driver_settings;
	struct net_device *netdev = port->netdev;
	char *menu_name;
	char tmp[HERMON_PORT_LABEL_LEN] = { 0 };

	if ( hermon->cap.nv_mem_access_supported == 0 )
		return 0;

	snprintf ( tmp, HERMON_PORT_LABEL_LEN, "%s : Port %d - %s ", netdev->name, port->ibdev->port, netdev_addr( netdev ) );
	menu_name = zalloc ( ( strlen ( tmp ) + 1 ) );
	if ( menu_name == NULL ) {
		printf ( "%s: Failed to allocate memory for port name\n", __FUNCTION__ );
		return -ENOMEM;
	}
	strcpy ( menu_name, tmp );

	driver_settings->priv = hermon;
	driver_settings->nv_store = NULL;
	driver_settings->set_default = &hermon_restore_port_default;

	INIT_LIST_HEAD ( &driver_settings->modified_list );
	driver_settings->store_menu = 0;
	init_driver_settings ( driver_settings, &hermon_settings_operations, menu_name );

	/* Set the function pointers of the extended settings */
	ext_virt_id.get_extended_type = &get_virt_id_type;
	ext_virt_num.get_extended_type = &get_virt_num_type;

	return 0;
}

static void init_extended_nv_store () {
	struct hermon_setting_operation *op;
	struct extended_setting *ext;
	unsigned int i;

	for ( i = 0 ; i < ( sizeof ( hermon_setting_operations ) /
		sizeof ( hermon_setting_operations[0] ) ) ; i++ ) {
		op = &hermon_setting_operations[i];
		ext = find_extended ( op->setting );
		ext->nv_store = op->nv_store;
	}
}

int hermon_init_settings ( struct hermon *hermon ) {
	struct driver_settings *driver_settings = &hermon->hermon_settings;
	int rc;

	if ( hermon->cap.nv_mem_access_supported == 0 )
		return 0;

	generic_settings_init ( &hermon->hermon_settings.generic_settings, NULL);
        if ( ( rc = register_settings (&hermon->hermon_settings.generic_settings.settings,
		NULL , "System setup" ) ) != 0 )
		return rc;
        hermon->hermon_settings.generic_settings.settings.op = &hermon_settings_operations;
        hermon->hermon_settings.generic_settings.settings.default_scope = &main_scope;
	nv_settings_root = &hermon->hermon_settings.generic_settings;

	driver_settings->nv_store = &hermon_nv_store;
	driver_settings->set_default = &hermon_restore_default;
	driver_settings->priv = hermon;

	INIT_LIST_HEAD ( &driver_settings->modified_list );
	driver_settings->store_menu = 0;

	/* Add to driver settings list  */
	list_add ( &driver_settings->list, &driver_settings_list );

	init_firmware_settings ( &hermon_settings_operations );
	init_extended_nv_store ();

	return 0;
}

struct hermon_device_name hermon_device_names [] = {
	{ 0x1003, "ConnectX3 HCA" },
	{ 0x1007, "ConnectX3-Pro HCA" },
};

char * get_device_name ( struct hermon *hermon ) {
	unsigned int i;
	for ( i = 0 ; i < ( sizeof ( hermon_device_names ) /
				sizeof ( hermon_device_names[0] ) ) ; i++ ) {
		if ( hermon->pci->id->device == hermon_device_names[i].device_id )
			return hermon_device_names[i].name;
	}
	return NULL;
}


