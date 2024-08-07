#ifndef DRIVER_SETTINGS_H_
#define DRIVER_SETTINGS_H_

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
#include <ipxe/netdevice.h>

#define NV_CONFIG __table( struct extended_setting, "nv_config")

#define STR_ENABLE		"Enabled"
#define STR_DISABLE		"Disabled"
#define STR_ISCSI		"iSCSI"
#define STR_PXE			"PXE"
#define STR_NONE		"None"
#define STR_SRIOV		"SR-IOV"
#define STR_IPV4		"IPv4"

#define APPLIES 1
#define DOES_NOT_APPLY 0

#define SUCCESS		0
#define INVALID_INPUT	-1
#define STORED		1

#define MAX_ISCSI_NAME 223
#define MIN_ISCSI_NAME 0
#define MAX_CHAP_ID 128
#define MIN_CHAP_ID 0
#define MAX_CHAP_SECRET 16
#define MIN_CHAP_SECRET 12
#define MAX_TCP_PORT 65535
#define MIN_TCP_PORT 1
#define MIN_BOOT_LUN 0
#define MAX_BOOT_LUN 255
#define MAX_BLINK_LEDS 15
#define MAX_VIRTUAL_ID 4094
#define MIN_VIRTUAL_ID 1
#define MAX_ROOT_PATH_LEN 0xfd
#define MAX_STR_SETTING_BUF_SIZE 256

enum {
	BOOT_PROTOCOL_NONE	= 0,
	BOOT_PROTOCOL_PXE	= 1,
	BOOT_PROTOCOL_ISCSI	= 2,
	BOOT_PROTOCOL_FCOE	= 3,
};

struct options_list {
	char **options_array;
	unsigned int options_num;
	unsigned int current_index;
};

enum menu_setting_type { LABEL, INPUT, OPTION };

struct extended_setting {
	struct setting *setting;
	enum menu_setting_type type;
	void *data;
	char *instructions;
	struct list_head modified;

	int ( * get_extended_type ) ( struct settings *settings );
	int ( *nv_store ) ( struct settings *settings, void *priv );
};

struct driver_settings {
	struct generic_settings generic_settings;
	int ( * nv_store ) ( struct driver_settings *driver_settings, void *priv );
	void ( * set_default ) ( struct driver_settings *driver_settings );
	void *priv;
	struct list_head modified_list;
	uint8_t store_menu;
	struct list_head list;
};

extern struct generic_settings *nv_settings_root;
extern struct list_head driver_settings_list;

extern const struct settings_scope main_scope;
extern const struct settings_scope port_scope;
extern const struct settings_scope fw_scope;
extern const struct settings_scope nic_scope;
extern const struct settings_scope iscsi_scope;
extern const struct settings_scope iscsi_general_scope;
extern const struct settings_scope iscsi_init_scope;
extern const struct settings_scope iscsi_target_scope;

extern struct setting virt_mode_setting __setting( SETTING_HERMON, virt_mode );
extern struct setting virt_num_setting __setting( SETTING_HERMON, virt_num );
extern struct setting virt_num_max_setting __setting( SETTING_HERMON, virt_num_max );
extern struct setting blink_leds_setting __setting ( SETTING_HERMON, blink_leds );
extern struct setting device_name_setting __setting ( SETTING_HERMON, device_name );
extern struct setting chip_type_setting __setting ( SETTING_HERMON, chip_type );
extern struct setting pci_id_setting __setting ( SETTING_HERMON, pci_id );
extern struct setting bus_dev_fun_setting __setting ( SETTING_HERMON, bus_dev_fun );
extern struct setting mac_add_setting __setting ( SETTING_HERMON, mac_add );
extern struct setting virt_mac_setting __setting ( SETTING_HERMON, virt_mac );
extern struct setting flex_version_setting __setting( SETTING_HERMON, flex_version );
extern struct setting fw_version_setting __setting( SETTING_HERMON, fw_version );
extern struct setting boot_protocol_setting __setting( SETTING_HERMON, boot_protocol );
extern struct setting virt_lan_setting __setting( SETTING_HERMON, virt_lan );
extern struct setting virt_id_setting __setting( SETTING_HERMON, virt_id );
extern struct setting link_speed_setting __setting( SETTING_HERMON, link_speed );
extern struct setting opt_rom_setting __setting( SETTING_HERMON, opt_rom );
extern struct setting boot_retries_setting __setting( SETTING_HERMON, boot_retries );
extern struct setting boot_strap_setting __setting( SETTING_HERMON, boot_strap );
extern struct setting wol_setting __setting( SETTING_HERMON, wol );
extern struct setting dhcp_ip_setting __setting( SETTING_HERMON, dhcp_ip );
extern struct setting dhcp_iscsi_setting __setting( SETTING_HERMON, dhcp_iscsi );
extern struct setting iscsi_chap_setting __setting( SETTING_HERMON, iscsi_chap );
extern struct setting iscsi_mutual_chap_setting __setting( SETTING_HERMON, iscsi_mutual_chap );
extern struct setting ip_ver_setting __setting( SETTING_HERMON, ip_ver );
extern struct setting ipv4_add_setting __setting( SETTING_HERMON, ipv4_add );
extern struct setting subnet_mask_setting __setting( SETTING_HERMON, subnet_mask );
extern struct setting ipv4_gateway_setting __setting( SETTING_HERMON, ipv4_gateway );
extern struct setting ipv4_dns_setting __setting( SETTING_HERMON, ipv4_dns );
extern struct setting iscsi_init_name_setting __setting( SETTING_HERMON, iscsi_name );
extern struct setting init_chapid_setting __setting( SETTING_HERMON, init_chapid );
extern struct setting init_chapsec_setting __setting( SETTING_HERMON, init_chapsec );
extern struct setting connect_setting __setting( SETTING_HERMON, connect );
extern struct setting target_ip_setting __setting( SETTING_HERMON, target_ip );
extern struct setting tcp_port_setting __setting( SETTING_HERMON, tcp_port );
extern struct setting boot_lun_setting __setting( SETTING_HERMON, boot_lun );
extern struct setting iscsi_target_name_setting __setting( SETTING_HERMON, iscsi_target_name );
extern struct setting target_chapid_setting __setting( SETTING_HERMON, target_chapid );
extern struct setting target_chapsec_setting __setting( SETTING_HERMON, target_chapsec );

extern struct extended_setting ext_virt_mode __table_entry (NV_CONFIG, 01);
extern struct extended_setting ext_virt_num __table_entry (NV_CONFIG, 01);
extern struct extended_setting ext_virt_num_max __table_entry (NV_CONFIG, 01);
extern struct extended_setting ext_blink_leds __table_entry (NV_CONFIG, 01);
extern struct extended_setting ext_device_name __table_entry (NV_CONFIG, 01);
extern struct extended_setting ext_chip_type __table_entry (NV_CONFIG, 01);
extern struct extended_setting ext_pci_id __table_entry (NV_CONFIG, 01);
extern struct extended_setting ext_bus_dev_fun __table_entry (NV_CONFIG, 01);
extern struct extended_setting ext_mac __table_entry (NV_CONFIG, 01);
extern struct extended_setting ext_virt_mac __table_entry (NV_CONFIG, 01) ;
extern struct extended_setting ext_flex_version __table_entry (NV_CONFIG, 01);
extern struct extended_setting ext_fw_version __table_entry (NV_CONFIG, 01);
extern struct extended_setting ext_boot_protocol __table_entry (NV_CONFIG, 01);
extern struct extended_setting ext_virt_lan __table_entry (NV_CONFIG, 01);
extern struct extended_setting ext_virt_id __table_entry (NV_CONFIG, 01);
extern struct extended_setting ext_virt_id __table_entry (NV_CONFIG, 01);
extern struct extended_setting ext_opt_rom __table_entry (NV_CONFIG, 01);
extern struct extended_setting ext_boot_retries __table_entry (NV_CONFIG, 01);
extern struct extended_setting ext_boot_strap __table_entry (NV_CONFIG, 01);
extern struct extended_setting ext_wol __table_entry (NV_CONFIG, 01);
extern struct extended_setting ext_dhcp_ip __table_entry (NV_CONFIG, 01);
extern struct extended_setting ext_dhcp_iscsi __table_entry (NV_CONFIG, 01);
extern struct extended_setting ext_iscsi_chap __table_entry ( NV_CONFIG, 01 );
extern struct extended_setting ext_iscsi_mutual_chap __table_entry ( NV_CONFIG, 01 );
extern struct extended_setting ext_ip_ver __table_entry (NV_CONFIG, 01);
extern struct extended_setting ext_ipv4_add __table_entry (NV_CONFIG, 01);
extern struct extended_setting ext_subnet_mask __table_entry (NV_CONFIG, 01);
extern struct extended_setting ext_ipv4_gateway __table_entry (NV_CONFIG, 01);
extern struct extended_setting ext_ipv4_dns __table_entry (NV_CONFIG, 01);
extern struct extended_setting ext_iscsi_init_name __table_entry (NV_CONFIG, 01);
extern struct extended_setting ext_init_chapid __table_entry (NV_CONFIG, 01);
extern struct extended_setting ext_init_chapsec __table_entry (NV_CONFIG, 01);
extern struct extended_setting ext_connect __table_entry (NV_CONFIG, 01);
extern struct extended_setting ext_target_ip __table_entry (NV_CONFIG, 01);
extern struct extended_setting ext_tcp_port __table_entry (NV_CONFIG, 01);
extern struct extended_setting ext_boot_lun __table_entry (NV_CONFIG, 01);
extern struct extended_setting ext_iscsi_target_name __table_entry (NV_CONFIG, 01);
extern struct extended_setting ext_target_chapid __table_entry (NV_CONFIG, 01);
extern struct extended_setting ext_target_chapsec __table_entry (NV_CONFIG, 01);

struct settings * driver_settings_from_netdev ( struct net_device *netdev );
int driver_settings_get_boot_prot_val ( struct settings *settings );
int driver_settings_get_boot_ret_val ( struct settings *settings );
const char * driver_settings_get_boot_ret_str ( unsigned int value );
struct extended_setting* find_extended (const struct setting *setting);
int init_driver_settings (struct driver_settings *driver_settings, struct settings_operations *operations, char *name);
int is_protocol_iscsi ( struct settings *settings );
int root_path_store ( struct settings *netdev_settings, struct settings *driver_settings );
void destroy_driver_settings ();
int init_firmware_settings ( struct settings_operations *operations );
#endif /*DRIVER_SETTINGS_H_*/
