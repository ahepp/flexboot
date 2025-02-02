/*
 * Copyright (C) 2008 Michael Brown <mbrown@fensystems.co.uk>.
 * Copyright (C) 2014 Mellanox Technologies Ltd.
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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <byteswap.h>
#include <ipxe/infiniband.h>
#include <ipxe/ib_smc.h>

/**
 * @file
 *
 * Infiniband Subnet Management Client
 *
 */

/**
 * Issue local MAD
 *
 * @v ibdev		Infiniband device
 * @v attr_id		Attribute ID, in network byte order
 * @v attr_mod		Attribute modifier, in network byte order
 * @v local_mad		Method for issuing local MADs
 * @v mad		Management datagram to fill in
 * @ret rc		Return status code
 */
static int ib_smc_mad ( struct ib_device *ibdev, uint16_t attr_id,
			uint32_t attr_mod, ib_local_mad_t local_mad,
			union ib_mad *mad , uint8_t method)
{
	int rc;

	/* Construct MAD */
	if ( method != IB_MGMT_METHOD_SET)
		memset ( mad, 0, sizeof ( *mad ) );

	mad->hdr.base_version	= IB_MGMT_BASE_VERSION;
	mad->hdr.mgmt_class	= IB_MGMT_CLASS_SUBN_LID_ROUTED;
	mad->hdr.class_version	= 1;
	mad->hdr.method		= method;
	mad->hdr.attr_id	= attr_id;
	mad->hdr.attr_mod	= attr_mod;

	/* Issue MAD */
	if ( ( rc = local_mad ( ibdev, mad ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Get node information
 *
 * @v ibdev		Infiniband device
 * @v local_mad		Method for issuing local MADs
 * @v mad		Management datagram to fill in
 * @ret rc		Return status code
 */
static int ib_smc_get_node_info ( struct ib_device *ibdev,
				  ib_local_mad_t local_mad,
				  union ib_mad *mad ) {
	int rc;

	/* Issue MAD */
	if ( ( rc = ib_smc_mad ( ibdev, htons ( IB_SMP_ATTR_NODE_INFO ), 0,
				 local_mad, mad,  IB_MGMT_METHOD_GET) ) != 0 ) {
		DBGC ( ibdev, "IBDEV %p could not get node info: %s\n",
		       ibdev, strerror ( rc ) );
		return rc;
	}
	return 0;
}

/**
 * Get port information
 *
 * @v ibdev		Infiniband device
 * @v local_mad		Method for issuing local MADs
 * @v mad		Management datagram to fill in
 * @ret rc		Return status code
 */
static int ib_smc_get_port_info ( struct ib_device *ibdev,
				  ib_local_mad_t local_mad,
				  union ib_mad *mad ) {
	int rc;

	/* Issue MAD */
	if ( ( rc = ib_smc_mad ( ibdev, htons ( IB_SMP_ATTR_PORT_INFO ),
				 htonl ( ibdev->port ), local_mad, mad,
				 IB_MGMT_METHOD_GET )) != 0 ) {
		DBGC ( ibdev, "IBDEV %p could not get port info: %s\n",
		       ibdev, strerror ( rc ) );
		return rc;
	}
	return 0;
}

/**
 * Get GUID information
 *
 * @v ibdev		Infiniband device
 * @v local_mad		Method for issuing local MADs
 * @v mad		Management datagram to fill in
 * @ret rc		Return status code
 */
static int ib_smc_get_guid_info ( struct ib_device *ibdev,
				  ib_local_mad_t local_mad,
				  union ib_mad *mad ) {
	int rc;

	/* Issue MAD */
	if ( ( rc = ib_smc_mad ( ibdev, htons ( IB_SMP_ATTR_GUID_INFO ), 0,
				 local_mad, mad, IB_MGMT_METHOD_GET) ) != 0 ) {
		DBGC ( ibdev, "IBDEV %p could not get GUID info: %s\n",
		       ibdev, strerror ( rc ) );
		return rc;
	}
	return 0;
}

/**
 * Get partition key table
 *
 * @v ibdev		Infiniband device
 * @v local_mad		Method for issuing local MADs
 * @v mad		Management datagram to fill in
 * @ret rc		Return status code
 */
static int ib_smc_get_pkey_table ( struct ib_device *ibdev,
				   ib_local_mad_t local_mad,
				   union ib_mad *mad ) {
	int rc;

	/* Issue MAD */
	if ( ( rc = ib_smc_mad ( ibdev, htons ( IB_SMP_ATTR_PKEY_TABLE ), 0,
				 local_mad, mad, IB_MGMT_METHOD_GET) ) != 0 ) {
		DBGC ( ibdev, "IBDEV %p could not get pkey table: %s\n",
		       ibdev, strerror ( rc ) );
		return rc;
	}
	return 0;
}

/**
 * Set Infiniband port speed using SMC
 *
 * @v ibdev		Infiniband device
 * @v local_mad		Method for issuing local MADs
 * @ret rc		Return status code
 */
int ib_smc_set_port_info ( struct ib_device *ibdev,
			  ib_local_mad_t local_mad)
{
	union ib_mad mad;
	struct ib_port_info *port_info	= &mad.smp.smp_data.port_info;
	int rc;

	/* Get current port info */
	if ( ( rc = ib_smc_get_port_info ( ibdev, local_mad, &mad ) ) != 0 ) {
		return rc;
	}

	port_info->link_speed_active__link_speed_enabled &= ~0xf; //zero out enabled speed
	port_info->link_speed_active__link_speed_enabled |= 0x1; // set link speed to 2.5Gbps
	port_info->port_phys_state__link_down_def_state	&= 0xf;
	port_info->port_phys_state__link_down_def_state	|= 0x2 << 4;
	port_info->link_speed_ext_ena			= 0x1e;
#define IB_MAD_PORT_INFO_DISABLE_FDR 0x1e
	port_info->ext_val &= ~(cpu_to_be32(0xf));
	port_info->ext_val |= cpu_to_be32(IB_MAD_PORT_INFO_DISABLE_FDR);

	if ( ( rc = ib_smc_mad ( ibdev, htons ( IB_SMP_ATTR_PORT_INFO ),
				 htonl ( ibdev->port ), local_mad, &mad,
				 IB_MGMT_METHOD_SET )) != 0 )
	{
		printf("IBDEV %p could not set port info: %s\n",
		       ibdev, strerror ( rc ) );
		return rc;
	}

	DBGC ( ibdev, "IBDEV %p set port info link speed to 2.5Gbps\n", ibdev);
	return 0;
}

/**
 * Get Infiniband parameters using SMC
 *
 * @v ibdev		Infiniband device
 * @v local_mad		Method for issuing local MADs
 * @ret rc		Return status code
 */
static int ib_smc_get ( struct ib_device *ibdev, ib_local_mad_t local_mad ) {
	union ib_mad mad;
	struct ib_node_info *node_info = &mad.smp.smp_data.node_info;
	struct ib_port_info *port_info = &mad.smp.smp_data.port_info;
	struct ib_guid_info *guid_info = &mad.smp.smp_data.guid_info;
	struct ib_pkey_table *pkey_table = &mad.smp.smp_data.pkey_table;
	int rc;

	/* Node info gives us the node GUID */
	if ( ( rc = ib_smc_get_node_info ( ibdev, local_mad, &mad ) ) != 0 )
		return rc;
	memcpy ( &ibdev->node_guid, &node_info->node_guid,
		 sizeof ( ibdev->node_guid ) );

	/* Port info gives us the link state, the first half of the
	 * port GID and the SM LID.
	 */
	if ( ( rc = ib_smc_get_port_info ( ibdev, local_mad, &mad ) ) != 0 )
		return rc;
	memcpy ( &ibdev->gid.s.prefix, port_info->gid_prefix,
		 sizeof ( ibdev->gid.s.prefix ) );
	ibdev->lid = ntohs ( port_info->lid );
	ibdev->sm_lid = ntohs ( port_info->mastersm_lid );
	ibdev->link_width_enabled = port_info->link_width_enabled;
	ibdev->link_width_supported = port_info->link_width_supported;
	ibdev->link_width_active = port_info->link_width_active;
	ibdev->link_speed_supported =
		( port_info->link_speed_supported__port_state >> 4 );
	ibdev->port_state =
		( port_info->link_speed_supported__port_state & 0xf );
	ibdev->link_speed_active =
		( port_info->link_speed_active__link_speed_enabled >> 4 );
	ibdev->link_speed_enabled =
		( port_info->link_speed_active__link_speed_enabled & 0xf );
	ibdev->sm_sl = ( port_info->neighbour_mtu__mastersm_sl & 0xf );

	/* GUID info gives us the second half of the port GID */
	if ( ( rc = ib_smc_get_guid_info ( ibdev, local_mad, &mad ) ) != 0 )
		return rc;
	memcpy ( &ibdev->gid.s.guid, guid_info->guid[0],
		 sizeof ( ibdev->gid.s.guid ) );

	/* Get partition key */
	if ( ( rc = ib_smc_get_pkey_table ( ibdev, local_mad, &mad ) ) != 0 )
		return rc;
	ibdev->pkey = ntohs ( pkey_table->pkey[IB_PKEY_DEFAULT_IDX] );

	DBGC ( ibdev, "IBDEV %p port GID is " IB_GID_FMT "\n",
	       ibdev, IB_GID_ARGS ( &ibdev->gid ) );

	return 0;
}

/**
 * Initialise Infiniband parameters using SMC
 *
 * @v ibdev		Infiniband device
 * @v local_mad		Method for issuing local MADs
 * @ret rc		Return status code
 */
int ib_smc_init ( struct ib_device *ibdev, ib_local_mad_t local_mad ) {
	int rc;

	/* Get MAD parameters */
	if ( ( rc = ib_smc_get ( ibdev, local_mad ) ) != 0 )
		return rc;

	return 0;
}

/**
 * Update Infiniband parameters using SMC
 *
 * @v ibdev		Infiniband device
 * @v local_mad		Method for issuing local MADs
 * @ret rc		Return status code
 */
int ib_smc_update ( struct ib_device *ibdev, ib_local_mad_t local_mad ) {
	int rc;

	/* Get MAD parameters */
	if ( ( rc = ib_smc_get ( ibdev, local_mad ) ) != 0 )
		return rc;

	/* Notify Infiniband core of potential link state change */
	ib_link_state_changed ( ibdev );

	return 0;
}
