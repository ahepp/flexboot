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

#include <ipxe/driver_settings.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <byteswap.h>
#include <ipxe/in.h>
#include <errno.h>

struct settings_page {
	struct generic_settings **new_settings;
	struct generic_settings **parent;
	char *name;
	const struct settings_scope *scope;
};

struct extended_options {
	struct extended_setting *ext;
	int size;
	char *array [8];
};

struct extended_description {
	struct extended_setting *ext;
	char description[258];
};


/*************************** NV_ROOT - root for all driver settings ***************/

struct generic_settings* nv_settings_root = NULL;
struct list_head driver_settings_list = LIST_HEAD_INIT ( driver_settings_list );

/* All menu's scopes, used for defining which settings appear in each page */
const struct settings_scope main_scope;
const struct settings_scope port_scope;
const struct settings_scope fw_scope;
const struct settings_scope nic_scope;
const struct settings_scope iscsi_scope;
const struct settings_scope iscsi_general_scope;
const struct settings_scope iscsi_init_scope;
const struct settings_scope iscsi_target_scope;

/********************* All settings for menu ***********************/

struct setting virt_mode_setting __setting( SETTING_HERMON, virt_mode ) = {
	.name = "virt_mode",
	.description = "Virtualization mode",
	.type = &setting_type_string,
	.scope = &main_scope,
};

struct extended_setting ext_virt_mode __table_entry (NV_CONFIG, 01) = {
	.setting = &virt_mode_setting,
	.type = OPTION,
};

struct setting virt_num_setting __setting( SETTING_HERMON, virt_num ) = {
	.name = "virt_num",
	.description = "Number of virtual functions",
	.type = &setting_type_int32,
	.scope = &main_scope,
};

struct extended_setting ext_virt_num __table_entry (NV_CONFIG, 01) = {
	.setting = &virt_num_setting,
	.type = INPUT,
};

struct setting virt_num_max_setting __setting( SETTING_HERMON, virt_num_max ) = {
	.name = "virt_num_max",
	.description = "Max number of VFs supported",
	.type = &setting_type_int32,
	.scope = &main_scope,
	.hidden = 1,
};

struct extended_setting ext_virt_num_max __table_entry (NV_CONFIG, 01) = {
	.setting = &virt_num_max_setting,
	.type = LABEL,
};

struct setting blink_leds_setting __setting ( SETTING_HERMON, blink_leds ) = {
	.name = "blink_leds",
	.description = "Blink leds",
	.type = &setting_type_int32,
	.scope = &port_scope,
};

struct extended_setting ext_blink_leds __table_entry (NV_CONFIG, 01) = {
	.setting = &blink_leds_setting,
	.type = INPUT,
};

struct setting device_name_setting __setting ( SETTING_HERMON, device_name ) = {
	.name = "device_name",
	.description = "Device name",
	.type = &setting_type_string,
	.scope = &main_scope,
};

struct extended_setting ext_device_name __table_entry (NV_CONFIG, 01) = {
	.setting = &device_name_setting,
	.type = LABEL,
};

struct setting chip_type_setting __setting ( SETTING_HERMON, chip_type ) = {
	.name = "chip_type",
	.description = "Chip type",
	.type = &setting_type_string,
	.scope = &main_scope,
};

struct extended_setting ext_chip_type __table_entry (NV_CONFIG, 01) = {
	.setting = &chip_type_setting,
	.type = LABEL,
};

struct setting pci_id_setting __setting ( SETTING_HERMON, pci_id ) = {
	.name = "pci_id",
	.description = "PCI device ID",
	.type = &setting_type_int32,
	.scope = &main_scope,
};

struct extended_setting ext_pci_id __table_entry (NV_CONFIG, 01) = {
	.setting = &pci_id_setting,
	.type = LABEL,
};

struct setting bus_dev_fun_setting __setting ( SETTING_HERMON, bus_dev_fun ) = {
	.name = "bus_dev_fun",
	.description = "Bus:Device:Function",
	.type = &setting_type_string,
	.scope = &main_scope,
};

struct extended_setting ext_bus_dev_fun __table_entry (NV_CONFIG, 01) = {
	.setting = &bus_dev_fun_setting,
	.type = LABEL,
};

struct setting mac_add_setting __setting ( SETTING_HERMON, mac_add ) = {
	.name = "mac_add",
	.description = "MAC Address",
	.type = &setting_type_string,
	.scope = &port_scope,
};

struct extended_setting ext_mac_add __table_entry (NV_CONFIG, 01) = {
	.setting = &mac_add_setting,
	.type = LABEL,
};

struct setting virt_mac_setting __setting ( SETTING_HERMON, virt_mac ) = {
	.name = "virt_mac",
	.description = "Virtual MAC Address",
	.type = &setting_type_string,
	.scope = &port_scope,
	.hidden = 1,
};

struct extended_setting ext_virt_mac __table_entry (NV_CONFIG, 01) = {
	.setting = &virt_mac_setting,
	.type = LABEL,
};

struct setting flex_version_setting __setting( SETTING_HERMON, flex_version ) = {
	.name = "flex_version",
	.description = "Flexboot version",
	.type = &setting_type_string,
	.scope = &fw_scope,
};

struct extended_setting ext_flex_version __table_entry (NV_CONFIG, 01) = {
	.setting = &flex_version_setting,
	.type = LABEL,
};

struct setting fw_version_setting __setting( SETTING_HERMON, fw_version ) = {
	.name = "fw_version",
	.description = "Family firmware version",
	.type = &setting_type_string,
	.scope = &fw_scope,
};

struct extended_setting ext_fw_version __table_entry (NV_CONFIG, 01) = {
	.setting = &fw_version_setting,
	.type = LABEL,
};

struct setting boot_protocol_setting __setting( SETTING_HERMON, boot_protocol ) = {
	.name = "boot_protocol",
	.description = "Legacy boot protocol",
	.type = &setting_type_string,
	.scope = &nic_scope,
};

struct extended_setting ext_boot_protocol __table_entry (NV_CONFIG, 01) = {
	.setting = &boot_protocol_setting,
	.type = OPTION,
};

struct setting virt_lan_setting __setting( SETTING_HERMON, virt_lan ) = {
	.name = "virt_lan",
	.description = "Virtual LAN mode",
	.type = &setting_type_string,
	.scope = &nic_scope,
};

struct extended_setting ext_virt_lan __table_entry (NV_CONFIG, 01) = {
	.setting = &virt_lan_setting,
	.type = OPTION,
};

struct setting virt_id_setting __setting( SETTING_HERMON, virt_id ) = {
	.name = "virt_id",
	.description = "Virtual LAN ID",
	.type = &setting_type_int32,
	.scope = &nic_scope,
};

struct extended_setting ext_virt_id __table_entry (NV_CONFIG, 01) = {
	.setting = &virt_id_setting,
	.type = INPUT,
};

struct setting opt_rom_setting __setting( SETTING_HERMON, opt_rom ) = {
	.name = "opt_rom",
	.description = "Option ROM",
	.type = &setting_type_string,
	.scope = &nic_scope,
};

struct extended_setting ext_opt_rom __table_entry (NV_CONFIG, 01) = {
	.setting = &opt_rom_setting,
	.type = OPTION,
};

struct setting boot_retries_setting __setting( SETTING_HERMON, boot_retries ) = {
	.name = "boot_retries",
	.description = "Boot retry count",
	.type = &setting_type_string,
	.scope = &nic_scope,
};

struct extended_setting ext_boot_retries __table_entry (NV_CONFIG, 01) = {
	.setting = &boot_retries_setting,
	.type = OPTION,
};

struct setting wol_setting __setting( SETTING_HERMON, wol ) = {
	.name = "wol",
	.description = "Wake on LAN",
	.type = &setting_type_string,
	.scope = &nic_scope,
};

struct extended_setting ext_wol __table_entry (NV_CONFIG, 01) = {
	.setting = &wol_setting,
	.type = OPTION,
};

struct setting ip_ver_setting __setting( SETTING_HERMON, ip_ver ) = {
	.name = "ip_ver",
	.description = "IP Version",
	.type = &setting_type_string,
	.scope = &iscsi_general_scope,
};

struct extended_setting ext_ip_ver __table_entry ( NV_CONFIG, 01 ) = {
	.setting = &ip_ver_setting,
	.type = LABEL,
};

struct setting dhcp_ip_setting __setting( SETTING_HERMON, dhcp_ip ) = {
	.name = "dhcp_ip",
	.description = "DHCP IP",
	.type = &setting_type_string,
	.scope = &iscsi_general_scope,
};

struct extended_setting ext_dhcp_ip __table_entry ( NV_CONFIG, 01 ) = {
	.setting = &dhcp_ip_setting,
	.type = OPTION,
};

struct setting dhcp_iscsi_setting __setting( SETTING_HERMON, dhcp_iscsi ) = {
	.name = "dhcp_iscsi",
	.description = "DHCP Parameters",
	.type = &setting_type_string,
	.scope = &iscsi_general_scope,
};

struct extended_setting ext_dhcp_iscsi __table_entry ( NV_CONFIG, 01 ) = {
	.setting = &dhcp_iscsi_setting,
	.type = OPTION,
};

struct setting iscsi_chap_setting __setting( SETTING_HERMON, iscsi_chap ) = {
	.name = "iscsi_chap_auth",
	.description = "CHAP Authentication",
	.type = &setting_type_string,
	.scope = &iscsi_general_scope,
};

struct extended_setting ext_iscsi_chap __table_entry ( NV_CONFIG, 01 ) = {
	.setting = &iscsi_chap_setting,
	.type = OPTION,
};

struct setting iscsi_mutual_chap_setting __setting( SETTING_HERMON, iscsi_mutual_chap ) = {
	.name = "iscsi_mutual_chap_auth",
	.description = "CHAP Mutual Authentication",
	.type = &setting_type_string,
	.scope = &iscsi_general_scope,
};

struct extended_setting ext_iscsi_mutual_chap __table_entry ( NV_CONFIG, 01 ) = {
	.setting = &iscsi_mutual_chap_setting,
	.type = OPTION,
};

struct setting ipv4_add_setting __setting( SETTING_HERMON, ipv4_add ) = {
	.name = "ipv4_add",
	.description = "IPv4 Address",
	.type = &setting_type_ipv4,
	.scope = &iscsi_init_scope,
};

struct extended_setting ext_ipv4_add __table_entry (NV_CONFIG, 01) = {
	.setting = &ipv4_add_setting,
	.type = INPUT,
};

struct setting subnet_mask_setting __setting( SETTING_HERMON, subnet_mask ) = {
	.name = "subnet_mask",
	.description = "Subnet mask",
	.type = &setting_type_ipv4,
	.scope = &iscsi_init_scope,
};

struct extended_setting ext_subnet_mask __table_entry (NV_CONFIG, 01) = {
	.setting = &subnet_mask_setting,
	.type = INPUT,
};

struct setting ipv4_gateway_setting __setting( SETTING_HERMON, ipv4_gateway ) = {
	.name = "ipv4_gateway",
	.description = "IPv4 Default Gateway",
	.type = &setting_type_ipv4,
	.scope = &iscsi_init_scope,
};

struct extended_setting ext_ipv4_gateway __table_entry (NV_CONFIG, 01) = {
	.setting = &ipv4_gateway_setting,
	.type = INPUT,
};

struct setting ipv4_dns_setting __setting( SETTING_HERMON, ipv4_dns ) = {
	.name = "ipv4_dns",
	.description = "IPv4 Primary DNS",
	.type = &setting_type_ipv4,
	.scope = &iscsi_init_scope,
};

struct extended_setting ext_ipv4_dns __table_entry (NV_CONFIG, 01) = {
	.setting = &ipv4_dns_setting,
	.type = INPUT,
};

struct setting iscsi_init_name_setting __setting( SETTING_HERMON, iscsi_init_name ) = {
	.name = "iscsi_init_name",
	.description = "iSCSI Name",
	.type = &setting_type_string,
	.scope = &iscsi_init_scope,
};

struct extended_setting ext_iscsi_init_name __table_entry (NV_CONFIG, 01) = {
	.setting = &iscsi_init_name_setting,
	.type = INPUT,
};

struct setting init_chapid_setting __setting( SETTING_HERMON, init_chapid ) = {
	.name = "init_chapid",
	.description = "CHAP ID",
	.type = &setting_type_string,
	.scope = &iscsi_init_scope,
};

struct extended_setting ext_init_chapid __table_entry (NV_CONFIG, 01) = {
	.setting = &init_chapid_setting,
	.type = INPUT,
};

struct setting init_chapsec_setting __setting( SETTING_HERMON, init_chapsec ) = {
	.name = "init_chapsec",
	.description = "CHAP Secret",
	.type = &setting_type_string,
	.scope = &iscsi_init_scope,
};

struct extended_setting ext_init_chapsec __table_entry (NV_CONFIG, 01) = {
	.setting = &init_chapsec_setting,
	.type = INPUT,
};

struct setting connect_setting __setting( SETTING_HERMON, connect ) = {
	.name = "connect",
	.description = "Connect",
	.type = &setting_type_string,
	.scope = &iscsi_target_scope,
};

struct extended_setting ext_connect __table_entry (NV_CONFIG, 01) = {
	.setting = &connect_setting,
	.type = OPTION,
};

struct setting target_ip_setting __setting( SETTING_HERMON, target_ip ) = {
	.name = "target_ip",
	.description = "IP Address",
	.type = &setting_type_ipv4,
	.scope = &iscsi_target_scope,
};

struct extended_setting ext_target_ip __table_entry (NV_CONFIG, 01) = {
	.setting = &target_ip_setting,
	.type = INPUT,
};

struct setting tcp_port_setting __setting( SETTING_HERMON, tcp_port ) = {
	.name = "tcp_port",
	.description = "TCP Port",
	.type = &setting_type_int32,
	.scope = &iscsi_target_scope,
};

struct extended_setting ext_tcp_port __table_entry (NV_CONFIG, 01) = {
	.setting = &tcp_port_setting,
	.type = INPUT,
};

struct setting boot_lun_setting __setting( SETTING_HERMON, boot_lun ) = {
	.name = "boot_lun",
	.description = "Boot LUN",
	.type = &setting_type_int32,
	.scope = &iscsi_target_scope,
};

struct extended_setting ext_boot_lun __table_entry (NV_CONFIG, 01) = {
	.setting = &boot_lun_setting,
	.type = INPUT,
};

struct setting iscsi_target_name_setting __setting( SETTING_HERMON, iscsi_target_name ) = {
	.name = "iscsi_target_name",
	.description = "iSCSI Name",
	.type = &setting_type_string,
	.scope = &iscsi_target_scope,
};

struct extended_setting ext_iscsi_target_name __table_entry (NV_CONFIG, 01) = {
	.setting = &iscsi_target_name_setting,
	.type = INPUT,
};

struct setting target_chapid_setting __setting( SETTING_HERMON, target_chapid ) = {
	.name = "target_chapid",
	.description = "CHAP ID",
	.type = &setting_type_string,
	.scope = &iscsi_target_scope,
};

struct extended_setting ext_target_chapid __table_entry (NV_CONFIG, 01) = {
	.setting = &target_chapid_setting,
	.type = INPUT,
};

struct setting target_chapsec_setting __setting( SETTING_HERMON, target_chapsec ) = {
	.name = "target_chapsec",
	.description = "CHAP Secret",
	.type = &setting_type_string,
	.scope = &iscsi_target_scope,
};

struct extended_setting ext_target_chapsec __table_entry (NV_CONFIG, 01) = {
	.setting = &target_chapsec_setting,
	.type = INPUT,
};

/*************************** Extended Settings functions ********************************/

struct extended_setting *find_extended ( const struct setting *setting ) {
	struct extended_setting *ext;

	for_each_table_entry (ext, NV_CONFIG) {
		if (strcmp (ext->setting->name, setting->name) == 0) {
			return ext;
		}
	}
	return NULL;
}

//**************************************************************************************//

int create_options_list ( struct extended_setting *ext, int size, char **array ) {
	struct options_list *options = malloc ( sizeof (*options) );
	int i;

	options->options_array = malloc ( size * sizeof (char*));
	for ( i = 0; i < size; i++ ) {
		( (char**)options->options_array )[i] = malloc ( strlen( array[i] ) + 1 );
		strcpy ( ((char**)options->options_array)[i], array[i] );
	}
	options->options_num = size;
	options->current_index = 0;
	ext->data = options;
	return 0;
}

#define STR_NO_RETRIES	"No retries"
#define STR_1_RETRY	"1 Retry"
#define STR_2_RETRIES	"2 Retries"
#define STR_3_RETRIES	"3 Retries"
#define STR_4_RETRIES	"4 Retries"
#define STR_5_RETRIES	"5 Retries"
#define STR_6_RETRIES	"6 Retries"
#define STR_INDEF_RET	"Indefinite Retries"

struct settings * driver_settings_from_netdev ( struct net_device *netdev ) {
	struct settings *settings_root;
	struct settings *settings;

	if ( ! nv_settings_root ) {
		printf ( "%s: nv_settings_root is not initialized\n", __FUNCTION__ );
		return NULL;
	}

	settings_root = &nv_settings_root->settings;

	list_for_each_entry ( settings, &settings_root->children, siblings ) {
		/* The driver settings starts with the name of the net device,
		 * for example, net0: ..., so its enough to compare the the 4th
		 * char which is the number of the net device.
		 */
		if ( settings->name[3] == netdev->name[3] )
			return settings;
	}

	return NULL;
}

int driver_settings_get_boot_prot_val ( struct settings *settings ) {
	char buf[20] = { 0 };
	if ( fetchf_setting ( settings, &boot_protocol_setting, NULL, NULL, buf, sizeof ( buf ) ) <= 0 )
		return BOOT_PROTOCOL_NONE;
	if ( buf[0] == 'P' )
		return BOOT_PROTOCOL_PXE;
	if ( buf[0] == 'i' )
		return BOOT_PROTOCOL_ISCSI;
	return BOOT_PROTOCOL_NONE;
}

int driver_settings_get_boot_ret_val ( struct settings *settings ) {
       char buf[20] = { 0 };

       if ( fetchf_setting ( settings, &boot_retries_setting, NULL, NULL, buf, sizeof ( buf ) ) <= 0 )
               return 0;
       if ( buf[0] == 'I' )
               return 7;
       if ( buf[0] == 'N' )
               return 0;
       return ( buf[0] - '0' );
}

const char * driver_settings_get_boot_ret_str ( unsigned int value ) {
	switch ( value ) {
		case 1: return STR_1_RETRY;
		case 2:	return STR_2_RETRIES;
		case 3: return STR_3_RETRIES;
		case 4: return STR_4_RETRIES;
		case 5: return STR_5_RETRIES;
		case 6: return STR_6_RETRIES;
		case 7: return STR_INDEF_RET;
		case 0:
			;/* Fall through */
		default:
			;/* Fall through */
	}

	return STR_NO_RETRIES;
}

struct extended_options extended_options_list[] = {
	{ &ext_virt_mode,			2, { STR_NONE, STR_SRIOV } },
	{ &ext_boot_protocol,		3, { STR_NONE, STR_PXE, STR_ISCSI } },
	{ &ext_virt_lan,			2, { STR_DISABLE, STR_ENABLE } },
	{ &ext_opt_rom,				2, { STR_DISABLE, STR_ENABLE } },
	{ &ext_boot_retries,		8, { STR_NO_RETRIES, STR_1_RETRY, STR_2_RETRIES,
									 STR_3_RETRIES, STR_4_RETRIES, STR_5_RETRIES,
									 STR_6_RETRIES, STR_INDEF_RET } },
	{ &ext_wol,					2, { STR_DISABLE, STR_ENABLE } },
	{ &ext_connect,				2, { STR_DISABLE, STR_ENABLE } },
	{ &ext_dhcp_ip,				2, { STR_DISABLE, STR_ENABLE } },
	{ &ext_dhcp_iscsi,			2, { STR_DISABLE, STR_ENABLE } },
	{ &ext_iscsi_chap,			2, { STR_DISABLE, STR_ENABLE } },
	{ &ext_iscsi_mutual_chap,	2, { STR_DISABLE, STR_ENABLE } },
};

struct extended_description extended_description_list[] = {
	{ &ext_virt_num, "Enter a non-negative number smaller than maximum supported" },
	{ &ext_blink_leds, "Enter positive number in range 0-15" },
	{ &ext_virt_id, "Enter positive number in range 1-4094" },
	{ &ext_ipv4_add, "Specify the IPv4 address of the iSCSI initiator" },
	{ &ext_subnet_mask, "Specify the IPv4 subnet mask of the iSCSI initiator" },
	{ &ext_ipv4_gateway, "Specify the IPv4 default gateway of the iSCSI initiator" },
	{ &ext_ipv4_dns, "Specify the IPv4 primary DNS IP of the iSCSI initiator" },
	{ &ext_iscsi_init_name, "Specify the initiator iSCSI Qualified Name. Maximum length is 223" },
	{ &ext_init_chapid, "iSCSI initiator reverse CHAP ID. Maximum length is 128" },
	{ &ext_init_chapsec, "iSCSI initiator reverse CHAP secret. Length should be 0 or 12 to 16" },
	{ &ext_target_ip,"Specify the IP address of the first iSCSI target" },
	{ &ext_tcp_port, "TCP port number of the first iSCSI target. Enter number in range 1-65535" },
	{ &ext_boot_lun, "Boot LUN on the first iSCSI storage target. Enter number in range 0-255" },
	{ &ext_iscsi_target_name, "iSCSI Qualified Name of the first iSCSI storage target. Maximum length is 223" },
	{ &ext_target_chapid, "First iSCSI storage target CHAP ID. Maximum length is 128" },
	{ &ext_target_chapsec, "First iSCSI storage target CHAP secret. Length should be 0 or 12 to 16" },
	{ &ext_boot_protocol, "Select boot protocol priority. If chosen, protocol will be tried first" },
};

void fill_extended_settings (void){

	unsigned int i;
	char *instructions;

	for ( i = 0 ; i < ( sizeof ( extended_options_list ) / sizeof ( extended_options_list[0] ) ) ; i++ ) {
		create_options_list ( extended_options_list[i].ext, extended_options_list[i].size,
					extended_options_list[i].array);
	}

	for ( i = 0 ; i < ( sizeof ( extended_description_list ) / sizeof ( extended_description_list[0] ) ) ; i++ ) {
		instructions = malloc ( strlen ( extended_description_list[i].description ) + 1 );
		strcpy (instructions, extended_description_list[i].description);
		extended_description_list[i].ext->instructions = instructions;
	}

}

int init_driver_settings ( struct driver_settings *driver_settings,
		struct settings_operations *operations, char *name ) {
	struct generic_settings *root_settings;
	struct generic_settings *main_settings = &driver_settings->generic_settings;
	struct generic_settings *nic_settings = NULL;
	struct generic_settings *iscsi_settings = NULL;
	struct generic_settings *iscsi_general_settings = NULL;
	struct generic_settings *iscsi_initiator_settings = NULL;
	struct generic_settings *iscsi_target_settings = NULL;
	unsigned int i;

	if ( ! nv_settings_root ) {
		printf ( "%s: nv_settings_root is not initialized\n", __FUNCTION__ );
		return -EINVAL;
	}

	root_settings = nv_settings_root;

	struct settings_page settings_pages[] = {
		{ &main_settings, &root_settings, name, &port_scope },
		{ &nic_settings, &main_settings,"NIC Configuration", &nic_scope },
		{ &iscsi_settings, &main_settings, "iSCSI Configuration", &iscsi_scope },
		{ &iscsi_general_settings, &iscsi_settings, "iSCSI General Parameters", &iscsi_general_scope },
		{ &iscsi_initiator_settings, &iscsi_settings, "iSCSI Initiator Parameters", &iscsi_init_scope },
		{ &iscsi_target_settings, &iscsi_settings, "iSCSI First target Parameters", &iscsi_target_scope },
	};

	for ( i = 0 ; i < ( sizeof ( settings_pages ) / sizeof ( settings_pages[0] ) ) ; i++ ) {
		if ( i != 0 ) {  /* The first settings were already allocated in driver_settings */
			*settings_pages[i].new_settings = malloc (sizeof (struct generic_settings));
		}
		generic_settings_init ( *settings_pages[i].new_settings, NULL);
		register_settings (&(*settings_pages[i].new_settings)->settings, &(*settings_pages[i].parent)->settings, settings_pages[i].name );
		(*settings_pages[i].new_settings)->settings.op = operations;
		(*settings_pages[i].new_settings)->settings.default_scope = settings_pages[i].scope;
	}

	/* Add to global list of driver settings */
	list_add ( &driver_settings->list, &driver_settings_list );
	/* When finished with settings blocks, start adding info for extended settings */
	fill_extended_settings ();

	return 0;
}


int init_firmware_settings ( struct settings_operations *operations ) {
	struct generic_settings *root_settings;
	struct generic_settings *firmware_settings;

	if ( ! nv_settings_root ) {
		printf ( "%s: nv_settings_root is not initialized\n", __FUNCTION__ );
		return -EINVAL;
	}

	root_settings = nv_settings_root;
	firmware_settings = zalloc ( sizeof ( struct generic_settings ) );
	if ( firmware_settings == NULL ) {
		printf ( "%s: Failed to allocate memory for firmware settings\n", __FUNCTION__ );
		return -ENOMEM;
	}

	generic_settings_init ( firmware_settings, NULL);
	register_settings ( &firmware_settings->settings, &root_settings->settings, "Firmware Image Properties" );
	firmware_settings->settings.op = operations;
	firmware_settings->settings.default_scope = &fw_scope;

	return 0;
}

static void destroy_settings_children ( struct settings *settings ) {
	struct settings *iterator;
	struct settings *temp_settings;
	struct generic_settings *generic;

	if ( list_empty ( &settings->children ) )
		return;

	list_for_each_entry_safe ( iterator, temp_settings, &settings->children, siblings ) {
		destroy_settings_children ( iterator );
		list_del ( &iterator->siblings );
		generic = container_of ( iterator, struct generic_settings, settings);
		free ( generic );
	}

	return;
}

static void destroy_modified_list ( struct driver_settings *driver_settings ) {
	struct extended_setting *iterator;
	struct extended_setting *temp_ext;

	if ( list_empty ( &driver_settings->modified_list ) )
		return;

	list_for_each_entry_safe ( iterator, temp_ext, &driver_settings->modified_list, modified ) {
		list_del ( &iterator->modified );
		free ( iterator );
	}

	return;
}

void destroy_driver_settings () {
	struct settings *settings_root;
	struct settings *settings;
	struct settings *temp_settings;
	struct generic_settings *generic;
	struct extended_setting *ext;
	struct options_list *options;
	struct driver_settings *driver_settings;
	unsigned int i;

	if ( ! nv_settings_root ) {
		return;
	}

	settings_root = &nv_settings_root->settings;

	/* Free all memory allocated for modified lists */
	list_for_each_entry ( driver_settings, &driver_settings_list, list )
		destroy_modified_list ( driver_settings );

	/* Free all memory allocated for settings */
	list_for_each_entry_safe ( settings, temp_settings, &settings_root->children, siblings ) {
		if ( settings ) {
			destroy_settings_children ( settings );
			list_del ( &settings->siblings );
			if ( settings->default_scope != &port_scope ) {
				generic = container_of ( settings, struct generic_settings, settings);
				free ( generic );
			} else {
				unregister_settings ( settings );
			}
		}
	}
	unregister_settings ( settings_root );

	/* Free all memory allocated for extended setting */
	for_each_table_entry ( ext, NV_CONFIG ) {
		if ( ext->instructions )
			free ( ext->instructions );
		if ( ext->type == OPTION ) {
			options = (struct options_list*)ext->data;
			if ( options ) {
				for ( i = 0; i < options->options_num; i ++ ) {
					if ( options->options_array[i] )
						free ( options->options_array[i] );
				}
				free (options);
			}
		}
	}

	/* Set root to NULL so function is executed once */
	nv_settings_root = NULL;

	return;
}

int is_protocol_iscsi ( struct settings *settings ) {
	struct settings *main_settings;
	struct settings *nic_settings;
	char protocol[8] = { 0 };

	main_settings = settings;
        while ( main_settings->default_scope != &port_scope )
                main_settings = main_settings->parent;

	list_for_each_entry ( nic_settings, &main_settings->children, siblings ) {
		if ( nic_settings->default_scope == &nic_scope )
			break;
	}

	generic_settings_fetch ( nic_settings, &boot_protocol_setting, protocol, sizeof ( protocol ) );
        if ( protocol[0] == 'i' )
                return 1;

	return 0;
}

int root_path_store ( struct settings *netdev_settings, struct settings *driver_settings ) {
	struct settings *tmp_settings;
	char ip_address[256] = { 0 };
	char port[256] = { 0 };
	char boot_lun[256] = { 0 };
	char iscsi_name[256] = { 0 };
	char root_path[MAX_ROOT_PATH_LEN] = { 0 };
	char connect[10] = { 0 };
	char dhcp_ip[10] = { 0 };
	char dhcp_iscsi[10] = { 0 };
	int rc;

	tmp_settings = driver_settings;
	while ( tmp_settings->default_scope != &port_scope )
		tmp_settings = tmp_settings->parent;

	/* First check if connect is enabled and iscsi is the boot protocol,
	 * if not then don't save in system settings */
	fetchf_setting ( tmp_settings, &connect_setting, NULL, NULL, connect, sizeof ( connect ) );
	fetchf_setting ( tmp_settings, &dhcp_iscsi_setting, NULL, NULL, dhcp_iscsi, sizeof ( dhcp_iscsi ) );
	fetchf_setting ( tmp_settings, &dhcp_ip_setting, NULL, NULL, dhcp_ip, sizeof ( dhcp_ip ) );
	if ( ( ( dhcp_ip[0] == 'E' ) && ( dhcp_iscsi[0] == 'E' ) ) || ( connect[0] != 'E' ) ) {
		storef_setting ( netdev_settings, &root_path_setting, NULL );
		/* Clear the root_path setting from DHCP/pxebs/proxyDHCP if it exists */
		list_for_each_entry ( tmp_settings, &netdev_settings->children, siblings ) {
			storef_setting ( tmp_settings, &root_path_setting, NULL );
		}
		return SUCCESS;
	}

	fetchf_setting ( driver_settings, &target_ip_setting, NULL, NULL, ip_address, sizeof ( ip_address ) );
	fetchf_setting ( driver_settings, &tcp_port_setting, NULL, NULL, port, sizeof ( port ) );
	fetchf_setting ( driver_settings, &boot_lun_setting, NULL, NULL, boot_lun, sizeof ( boot_lun ) );
	fetchf_setting ( driver_settings, &iscsi_target_name_setting, NULL, NULL, iscsi_name, sizeof ( iscsi_name ) );

	snprintf ( root_path, MAX_ROOT_PATH_LEN, "iscsi:%s:%s:%s:%s:%s",
						ip_address[0] ? ip_address : "\0",
						"\0", /*protocol */
						port[0]	? port	: "\0",
						boot_lun[0] ? boot_lun : "\0",
						iscsi_name[0] ? iscsi_name : "\0");

	rc = store_setting ( netdev_settings, &root_path_setting, root_path, sizeof ( root_path ) );
	if ( rc != 0 )
		return INVALID_INPUT;

	return SUCCESS;
}

