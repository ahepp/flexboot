/*
 * Copyright (C) 2007 Michael Brown <mbrown@fensystems.co.uk>.
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

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <byteswap.h>
#include <errno.h>
#include <ipxe/errortab.h>
#include <ipxe/malloc.h>
#include <ipxe/if_arp.h>
#include <ipxe/if_ether.h>
#include <ipxe/ethernet.h>
#include <ipxe/iobuf.h>
#include <ipxe/netdevice.h>
#include <ipxe/infiniband.h>
#include <ipxe/ib_pathrec.h>
#include <ipxe/ib_mcast.h>
#include <ipxe/retry.h>
#include <ipxe/ipoib.h>

/** @file
 *
 * IP over Infiniband
 */

/** Number of IPoIB send work queue entries */
#define IPOIB_NUM_SEND_WQES 8
/** Number of IPoIB receive work queue entries */
#define IPOIB_NUM_RECV_WQES 16
/** Number of IPoIB send completion entries */
#define IPOIB_NUM_SEND_CQES IPOIB_NUM_SEND_WQES
/** Number of IPoIB recieve completion entries */
#define IPOIB_NUM_RECV_CQES IPOIB_NUM_RECV_WQES

/** An IPoIB device */
struct ipoib_device {
	/** Network device */
	struct net_device *netdev;
	/** Underlying Infiniband device */
	struct ib_device *ibdev;
	/** RX Completion queue */
	struct ib_completion_queue *rx_cq;
	/** TX Completion queue */
	struct ib_completion_queue *tx_cq;
	/** Queue pair */
	struct ib_queue_pair *qp;
	/** Local MAC */
	struct ipoib_mac mac;
	/** Broadcast MAC */
	struct ipoib_mac broadcast;
	/** Joined to IPv4 broadcast multicast group
	 *
	 * This flag indicates whether or not we have initiated the
	 * join to the IPv4 broadcast multicast group.
	 */
	int broadcast_joined;
	/** IPv4 broadcast multicast group membership */
	struct ib_mc_membership broadcast_membership;
	/** REMAC cache */
	struct list_head peers;
	uint32_t total_peers;
};

/** Broadcast IPoIB address */
static struct ipoib_mac ipoib_broadcast = {
	.flags__qpn = htonl ( IB_QPN_BROADCAST ),
	.gid.bytes = { 0xff, 0x12, 0x40, 0x1b, 0x00, 0x00, 0x00, 0x00,
		       0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff },
};

/** Link status for "broadcast join in progress" */
#define EINPROGRESS_JOINING __einfo_error ( EINFO_EINPROGRESS_JOINING )
#define EINFO_EINPROGRESS_JOINING __einfo_uniqify \
	( EINFO_EINPROGRESS, 0x01, "Joining" )

/** Human-readable message for the link status */
struct errortab ipoib_errors[] __errortab = {
	__einfo_errortab ( EINFO_EINPROGRESS_JOINING ),
};

static int ipoib_open ( struct net_device *netdev );
static void ipoib_close ( struct net_device *netdev );
static int ipoib_transmit ( struct net_device *netdev,
			    struct io_buffer *iobuf );
static void ipoib_poll ( struct net_device *netdev );

/** IPoIB network device operations */
static struct net_device_operations ipoib_operations = {
	.open		= ipoib_open,
	.close		= ipoib_close,
	.transmit	= ipoib_transmit,
	.poll		= ipoib_poll,
};

/****************************************************************************
 *
 * IPoIB REMAC cache
 *
 ****************************************************************************
 */

/** An IPoIB REMAC cache entry */
struct ipoib_peer {
	/** List of REMAC cache entries */
	struct list_head list;
	/** Remote Ethermet MAC */
	struct ipoib_remac remac;
	/** MAC address */
	struct ipoib_mac mac;
};

static uint32_t ipoib_add_peer ( struct ipoib_device *ipoib,
								struct ipoib_peer * peer ) {
	list_add ( &peer->list, &ipoib->peers );
	ipoib->total_peers++;
	return ipoib->total_peers;
}

static uint32_t ipoib_remove_peer( struct ipoib_device *ipoib,
								struct ipoib_peer * peer ) {
	if ( ipoib->total_peers > 0 ) {
		list_del ( &peer->list );
		ipoib->total_peers--;
	}
	return ipoib->total_peers;
}

/**
 * Find IPoIB MAC from REMAC
 *
 * @v ipoib		IPoIB device
 * @v remac		Remote Ethernet MAC
 * @ret mac		IPoIB MAC (or NULL if not found)
 */
static struct ipoib_mac * ipoib_find_remac ( struct ipoib_device *ipoib,
					     const struct ipoib_remac *remac ) {
	struct ipoib_peer *peer;

	/* Check for broadcast REMAC */
	if ( is_broadcast_ether_addr ( remac ) )
		return &ipoib->broadcast;

	/* Try to find via REMAC cache */
	list_for_each_entry ( peer, &ipoib->peers, list ) {
		if ( memcmp ( remac, &peer->remac,
			      sizeof ( peer->remac ) ) == 0 ) {
			/* Move peer to start of list */
			list_del ( &peer->list );
			list_add ( &peer->list, &ipoib->peers );
			return &peer->mac;
		}
	}

	DBGC ( ipoib, "IPoIB %p unknown REMAC %s\n",
	       ipoib, eth_ntoa ( remac ) );
	return NULL;
}

/**
 * Add IPoIB MAC to REMAC cache
 *
 * @v ipoib		IPoIB device
 * @v remac		Remote Ethernet MAC
 * @v mac		IPoIB MAC
 * @ret rc		Return status code
 */
static int ipoib_map_remac ( struct ipoib_device *ipoib,
			     const struct ipoib_remac *remac,
			     const struct ipoib_mac *mac ) {
	struct ipoib_peer *peer;

	/* Check for existing entry in REMAC cache */
	list_for_each_entry ( peer, &ipoib->peers, list ) {
		if ( memcmp ( remac, &peer->remac,
			      sizeof ( peer->remac ) ) == 0 ) {
			/* Move peer to start of list */
			list_del ( &peer->list );
			list_add ( &peer->list, &ipoib->peers );
			/* Update MAC */
			memcpy ( &peer->mac, mac, sizeof ( peer->mac ) );
			return 0;
		}
	}

	/* Create new entry */
	peer = malloc ( sizeof ( *peer ) );
	if ( ! peer ) {
		DBGC ( ipoib, "IPoIB %p insufficient space for new peer\n",
				       ipoib );
		return -ENOMEM;
	}
	memcpy ( &peer->remac, remac, sizeof ( peer->remac ) );
	memcpy ( &peer->mac, mac, sizeof ( peer->mac ) );
	ipoib_add_peer ( ipoib, peer );

	return 0;
}

/**
 * Flush REMAC cache
 *
 * @v ipoib		IPoIB device
 */
static void ipoib_flush_remac ( struct ipoib_device *ipoib ) {
	struct ipoib_peer *peer;
	struct ipoib_peer *tmp;

	list_for_each_entry_safe ( peer, tmp, &ipoib->peers, list ) {
		ipoib_remove_peer( ipoib, peer );
		free ( peer );
	}
}
extern unsigned int neighbour_discard ( void *ll_addr, size_t ll_addr_len );
#define IPOIB_MIN_DROP_CACHE 3
/**
 * Discard some entries from the REMAC cache
 *
 * @ret discarded	Number of cached items discarded
 */
static unsigned int ipoib_discard_remac ( void ) {
	struct ib_device *ibdev;
	struct net_device *netdev;
	struct ipoib_device *ipoib;
	struct ipoib_peer *peer, *tmp;
	unsigned int discarded = 0;

	/* Try to discard one cache entry for each IPoIB device */
	for_each_ibdev ( ibdev ) {
		netdev = ib_get_ownerdata ( ibdev );
		/* Skip non-Infiniband ports */
		if ( netdev->op != &ipoib_operations )
			continue;

		ipoib = netdev->priv;
		if ( list_empty ( &ipoib->peers ) || ( ipoib->total_peers < IPOIB_MIN_DROP_CACHE ) )
			continue;

		list_for_each_entry_reverse_safe ( peer, tmp, &ipoib->peers, list ) {
			ipoib_remove_peer( ipoib, peer );
			/* WA: Remove from neghbors list
			 * malloc() may discard a remac but not the corresponfing
			 * ARP neighbour, as a result the download process may get stuck.
			 * The optimal fix wiil be that the IPoIB stack must know how to
			 * recover from a state where the ARP table (neighbours list)
			 * has an entry of a specific neighbour but the REMACs list doesn't
			 */
			neighbour_discard ( ( void * ) & ( peer->remac ),
					netdev->ll_protocol->ll_addr_len );
			free ( peer );
			discarded++;
			break;
		}
	}

	return discarded;
}

/** IPoIB cache discarder */
struct cache_discarder ipoib_discarder __cache_discarder ( CACHE_EXPENSIVE ) = {
	.discard = ipoib_discard_remac,
};

/****************************************************************************
 *
 * IPoIB link layer
 *
 ****************************************************************************
 */

static void ipoib_def_eth_addr ( const union ib_guid *guid,
			uint8_t *eth_addr ) {
	eth_addr[0] = guid->bytes[0];
	eth_addr[1] = guid->bytes[1];
	eth_addr[2] = guid->bytes[2];
	eth_addr[3] = guid->bytes[5];
	eth_addr[4] = guid->bytes[6];
	eth_addr[5] = guid->bytes[7];
}

/**
 * Generate Mellanox Ethernet-compatible compressed link-layer address
 *
 * @v ll_addr		Link-layer address
 * @v eth_addr		Ethernet-compatible address to fill in
 */
static void ipoib_mlx_eth_addr ( const union ib_guid *guid,
				uint8_t *eth_addr ) {
	eth_addr[0] = ( ( guid->bytes[3] == 2 ) ? 0x00 : 0x02 );
	eth_addr[1] = guid->bytes[1];
	eth_addr[2] = guid->bytes[2];
	eth_addr[3] = guid->bytes[5];
	eth_addr[4] = guid->bytes[6];
	eth_addr[5] = guid->bytes[7];
}

void ( * ipoib_hw_guid2mac ) ( const void *hw_addr, void *ll_addr ) = NULL;

/**
 * Initialise IPoIB link-layer address
 *
 * @v hw_addr		Hardware address
 * @v ll_addr		Link-layer address
 */
static void ipoib_init_addr ( const void *hw_addr, void *ll_addr ) {
	const uint8_t *guid = hw_addr;

	/* If specified, query the HW for the ETH MAC */
	if ( ipoib_hw_guid2mac )
		ipoib_hw_guid2mac ( hw_addr, ll_addr );
	else if ( ( guid[0] == 0x0 ) &&
		    ( guid[1] == 0x02 ) &&
		    ( guid[2] == 0xc9 ) )
		ipoib_mlx_eth_addr ( hw_addr, ll_addr );
	else
		ipoib_def_eth_addr ( hw_addr, ll_addr );
}

/** IPoIB protocol */
struct ll_protocol ipoib_protocol __ll_protocol = {
	.name		= "IPoIB",
	.ll_proto	= htons ( ARPHRD_ETHER ),
	.hw_addr_len	= sizeof ( union ib_guid ),
	.ll_addr_len	= ETH_ALEN,
	.ll_header_len	= ETH_HLEN,
	.client_id_len	= IPOIB_ALEN,
	.push		= eth_push,
	.pull		= eth_pull,
	.init_addr	= ipoib_init_addr,
	.ntoa		= eth_ntoa,
	.mc_hash	= eth_mc_hash,
	.eth_addr	= eth_eth_addr,
	.eui64		= eth_eui64,
	.flags		= LL_NAME_ONLY,
};

/**
 * Allocate IPoIB device
 *
 * @v priv_size		Size of driver private data
 * @ret netdev		Network device, or NULL
 */
struct net_device * alloc_ipoibdev ( size_t priv_size ) {
	struct net_device *netdev;

	netdev = alloc_netdev ( priv_size );
	if ( netdev ) {
		netdev->ll_protocol = &ipoib_protocol;
		netdev->ll_broadcast = eth_broadcast;
		netdev->max_pkt_len = IB_MAX_PAYLOAD_SIZE;
	}
	return netdev;
}

/****************************************************************************
 *
 * IPoIB translation layer
 *
 ****************************************************************************
 */

/**
 * Translate transmitted ARP packet
 *
 * @v netdev		Network device
 * @v iobuf		Packet to be transmitted (with no link-layer headers)
 * @ret rc		Return status code
 */
static int ipoib_translate_tx_arp ( struct net_device *netdev,
				    struct io_buffer *iobuf ) {
	struct ipoib_device *ipoib = netdev->priv;
	struct arphdr *arphdr = iobuf->data;
	struct ipoib_mac *target_ha = NULL;
	void *sender_pa;
	void *target_pa;

	/* Do nothing unless ARP contains eIPoIB link-layer addresses */
	if ( arphdr->ar_hln != ETH_ALEN )
		return 0;

	/* Fail unless we have room to expand packet */
	if ( iob_tailroom ( iobuf ) < ( 2 * ( sizeof ( ipoib->mac ) -
					      ETH_ALEN ) ) ) {
		DBGC ( ipoib, "IPoIB %p insufficient space in TX ARP\n",
		       ipoib );
		return -ENOBUFS;
	}

	/* Look up REMAC, if applicable */
	if ( arphdr->ar_op == ARPOP_REPLY ) {
		target_ha = ipoib_find_remac ( ipoib, arp_target_pa ( arphdr ));
		if ( ! target_ha )
			return -ENXIO;
	}

	/* Construct new packet */
	iob_put ( iobuf, ( 2 * ( sizeof ( ipoib->mac ) - ETH_ALEN ) ) );
	sender_pa = arp_sender_pa ( arphdr );
	target_pa = arp_target_pa ( arphdr );
	arphdr->ar_hrd = htons ( ARPHRD_INFINIBAND );
	arphdr->ar_hln = sizeof ( ipoib->mac );
	memcpy ( arp_target_pa ( arphdr ), target_pa, arphdr->ar_pln );
	memcpy ( arp_sender_pa ( arphdr ), sender_pa, arphdr->ar_pln );
	memcpy ( arp_sender_ha ( arphdr ), &ipoib->mac, sizeof ( ipoib->mac ) );
	memset ( arp_target_ha ( arphdr ), 0, sizeof ( ipoib->mac ) );
	if ( target_ha ) {
		memcpy ( arp_target_ha ( arphdr ), target_ha,
			 sizeof ( *target_ha ) );
	}

	return 0;
}

/**
 * Translate transmitted packet
 *
 * @v netdev		Network device
 * @v iobuf		Packet to be transmitted (with no link-layer headers)
 * @v net_proto		Network-layer protocol (in network byte order)
 * @ret rc		Return status code
 */
static int ipoib_translate_tx ( struct net_device *netdev,
				struct io_buffer *iobuf, uint16_t net_proto ) {

	switch ( net_proto ) {
	case htons ( ETH_P_ARP ) :
		return ipoib_translate_tx_arp ( netdev, iobuf );
	case htons ( ETH_P_IP ) :
		/* No translation needed */
		return 0;
	default:
		/* Cannot handle other traffic via eIPoIB */
		return -ENOTSUP;
	}
}

/**
 * Translate received ARP packet
 *
 * @v netdev		Network device
 * @v iobuf		Received packet (with no link-layer headers)
 * @v remac		Constructed Remote Ethernet MAC
 * @ret rc		Return status code
 */
static int ipoib_translate_rx_arp ( struct net_device *netdev,
				    struct io_buffer *iobuf,
				    struct ipoib_remac *remac ) {
	struct ipoib_device *ipoib = netdev->priv;
	struct arphdr *arphdr = iobuf->data;
	void *sender_pa;
	void *target_pa;
	int rc;

	/* Do nothing unless ARP contains IPoIB link-layer addresses */
	if ( arphdr->ar_hln != sizeof ( ipoib->mac ) )
		return 0;

	/* Create REMAC cache entry */
	if ( ( rc = ipoib_map_remac ( ipoib, remac,
				      arp_sender_ha ( arphdr ) ) ) != 0 ) {
		DBGC ( ipoib, "IPoIB %p could not map REMAC: %s\n",
		       ipoib, strerror ( rc ) );
		return rc;
	}

	/* Construct new packet */
	sender_pa = arp_sender_pa ( arphdr );
	target_pa = arp_target_pa ( arphdr );
	arphdr->ar_hrd = htons ( ARPHRD_ETHER );
	arphdr->ar_hln = ETH_ALEN;
	memcpy ( arp_sender_pa ( arphdr ), sender_pa, arphdr->ar_pln );
	memcpy ( arp_target_pa ( arphdr ), target_pa, arphdr->ar_pln );
	memcpy ( arp_sender_ha ( arphdr ), remac, ETH_ALEN );
	memset ( arp_target_ha ( arphdr ), 0, ETH_ALEN );
	if ( arphdr->ar_op == ARPOP_REPLY ) {
		/* Assume received replies were directed to us */
		memcpy ( arp_target_ha ( arphdr ), netdev->ll_addr, ETH_ALEN );
	}
	iob_unput ( iobuf, ( 2 * ( sizeof ( ipoib->mac ) - ETH_ALEN ) ) );

	return 0;
}

/**
 * Translate received packet
 *
 * @v netdev		Network device
 * @v iobuf		Received packet (with no link-layer headers)
 * @v remac		Constructed Remote Ethernet MAC
 * @v net_proto		Network-layer protocol (in network byte order)
 * @ret rc		Return status code
 */
static int ipoib_translate_rx ( struct net_device *netdev,
				struct io_buffer *iobuf,
				struct ipoib_remac *remac,
				uint16_t net_proto ) {

	switch ( net_proto ) {
	case htons ( ETH_P_ARP ) :
		return ipoib_translate_rx_arp ( netdev, iobuf, remac );
	case htons ( ETH_P_IP ) :
		/* No translation needed */
		return 0;
	default:
		/* Cannot handle other traffic via eIPoIB */
		return -ENOTSUP;
	}
}

/****************************************************************************
 *
 * IPoIB network device
 *
 ****************************************************************************
 */

/**
 * Transmit packet via IPoIB network device
 *
 * @v netdev		Network device
 * @v iobuf		I/O buffer
 * @ret rc		Return status code
 */
static int ipoib_transmit ( struct net_device *netdev,
			    struct io_buffer *iobuf ) {
	struct ipoib_device *ipoib = netdev->priv;
	struct ib_device *ibdev = ipoib->ibdev;
	struct ethhdr *ethhdr;
	struct ipoib_hdr *ipoib_hdr;
	struct ipoib_mac *mac;
	struct ib_work_queue *wq = &ipoib->qp->send;
	struct ib_address_vector dest;
	unsigned long end_timer;
	uint16_t net_proto;
	int rc;

	/* Sanity check */
	if ( iob_len ( iobuf ) < sizeof ( *ethhdr ) ) {
		DBGC ( ipoib, "IPoIB %p buffer too short\n", ipoib );
		return -EINVAL;
	}

	/* Attempting transmission while link is down will put the
	 * queue pair into an error state, so don't try it.
	 */
	if ( ! ib_link_ok ( ibdev ) )
		return -ENETUNREACH;

	/* Strip eIPoIB header */
	ethhdr = iobuf->data;
	net_proto = ethhdr->h_protocol;
	iob_pull ( iobuf, sizeof ( *ethhdr ) );

	/* Identify destination address */
	mac = ipoib_find_remac ( ipoib, ( ( void *) ethhdr->h_dest ) );
	if ( ! mac ) {
		DBGC ( ipoib, "IPoIB %p No REMAC found for this peer\n", ipoib );
		return -ENXIO;
	}

	/* Translate packet if applicable */
	if ( ( rc = ipoib_translate_tx ( netdev, iobuf, net_proto ) ) != 0 )
		return rc;

	/* Prepend real IPoIB header */
	ipoib_hdr = iob_push ( iobuf, sizeof ( *ipoib_hdr ) );
	ipoib_hdr->proto = net_proto;
	ipoib_hdr->reserved = 0;

	/* Construct address vector */
	memset ( &dest, 0, sizeof ( dest ) );
	dest.qpn = ( ntohl ( mac->flags__qpn ) & IB_QPN_MASK );
	dest.gid_present = 1;
	memcpy ( &dest.gid, &mac->gid, sizeof ( dest.gid ) );
	if ( ( rc = ib_resolve_path ( ibdev, &dest ) ) != 0 ) {
		/* Path not resolved yet */
		return rc;
	}

	/** Poll for free IO buffer */
	if ( wq->iobufs[wq->next_idx & ( wq->num_wqes - 1 )] ) {
		end_timer = currticks() +
			( ticks_per_sec() * POLL_SEND_IOBUFFER_TIMEOUT );
		while ( wq->iobufs[wq->next_idx & ( wq->num_wqes - 1 )] &&
				( currticks() < end_timer ) ) {
			ib_poll_cq ( ibdev, ipoib->qp->send.cq );
		}
	}

	return ib_post_send ( ibdev, ipoib->qp, &dest, iobuf );
}

/**
 * Handle IPoIB send completion
 *
 * @v ibdev		Infiniband device
 * @v qp		Queue pair
 * @v iobuf		I/O buffer
 * @v rc		Completion status code
 */
static void ipoib_complete_send ( struct ib_device *ibdev __unused,
				  struct ib_queue_pair *qp,
				  struct io_buffer *iobuf, int rc ) {
	struct ipoib_device *ipoib = ib_qp_get_ownerdata ( qp );

	netdev_tx_complete_err ( ipoib->netdev, iobuf, rc );
}

/**
 * Handle IPoIB receive completion
 *
 * @v ibdev		Infiniband device
 * @v qp		Queue pair
 * @v dest		Destination address vector, or NULL
 * @v source		Source address vector, or NULL
 * @v iobuf		I/O buffer
 * @v rc		Completion status code
 */
static void ipoib_complete_recv ( struct ib_device *ibdev __unused,
				  struct ib_queue_pair *qp,
				  struct ib_address_vector *dest,
				  struct ib_address_vector *source,
				  struct io_buffer *iobuf, int rc ) {
	struct ipoib_device *ipoib = ib_qp_get_ownerdata ( qp );
	struct net_device *netdev = ipoib->netdev;
	struct ipoib_hdr *ipoib_hdr;
	struct ethhdr *ethhdr;
	struct ipoib_remac remac;
	uint16_t net_proto;

	/* Record errors */
	if ( rc != 0 ) {
		DBGC ( ipoib, "IPoIB %p received packet with error\n", ipoib );
		netdev_rx_err ( netdev, iobuf, rc );
		return;
	}

	/* Sanity check */
	if ( iob_len ( iobuf ) < sizeof ( struct ipoib_hdr ) ) {
		DBGC ( ipoib, "IPoIB %p received packet too short to "
		       "contain IPoIB header\n", ipoib );
		DBGC_HD ( ipoib, iobuf->data, iob_len ( iobuf ) );
		netdev_rx_err ( netdev, iobuf, -EIO );
		return;
	}
	if ( ! source ) {
		DBGC ( ipoib, "IPoIB %p received packet without address "
		       "vector\n", ipoib );
		netdev_rx_err ( netdev, iobuf, -ENOTTY );
		return;
	}

	/* Strip real IPoIB header */
	ipoib_hdr = iobuf->data;
	net_proto = ipoib_hdr->proto;
	iob_pull ( iobuf, sizeof ( *ipoib_hdr ) );

	/* Construct source address from remote QPN and LID */
	remac.qpn = htonl ( source->qpn | EIPOIB_QPN_LA );
	remac.lid = htons ( source->lid );

	/* Translate packet if applicable */
	if ( ( rc = ipoib_translate_rx ( netdev, iobuf, &remac,
					 net_proto ) ) != 0 ) {
		netdev_rx_err ( netdev, iobuf, rc );
		return;
	}

	/* Prepend eIPoIB header */
	ethhdr = iob_push ( iobuf, sizeof ( *ethhdr ) );
	memcpy ( &ethhdr->h_source, &remac, sizeof ( ethhdr->h_source ) );
	ethhdr->h_protocol = net_proto;

	/* Construct destination address */
	if ( dest->gid_present && ( memcmp ( &dest->gid, &ipoib->broadcast.gid,
					     sizeof ( dest->gid ) ) == 0 ) ) {
		/* Broadcast GID; use the Ethernet broadcast address */
		memcpy ( &ethhdr->h_dest, eth_broadcast,
			 sizeof ( ethhdr->h_dest ) );
	} else {
		/* Assume destination address is local Ethernet MAC */
		memcpy ( &ethhdr->h_dest, netdev->ll_addr,
			 sizeof ( ethhdr->h_dest ) );
	}

	/* Hand off to network layer */
	netdev_rx ( netdev, iobuf );
}

/** IPoIB completion operations */
static struct ib_completion_queue_operations ipoib_cq_op = {
	.complete_send = ipoib_complete_send,
	.complete_recv = ipoib_complete_recv,
};

/**
 * Allocate IPoIB receive I/O buffer
 *
 * @v len		Length of buffer
 * @ret iobuf		I/O buffer, or NULL
 *
 * Some Infiniband hardware requires 2kB alignment of receive buffers
 * and provides no way to disable header separation.  The result is
 * that there are only four bytes of link-layer header (the real IPoIB
 * header) before the payload.  This is not sufficient space to insert
 * an eIPoIB link-layer pseudo-header.
 *
 * We therefore allocate I/O buffers offset to start slightly before
 * the natural alignment boundary, in order to allow sufficient space.
 */
static struct io_buffer * ipoib_alloc_iob ( size_t len ) {
	struct io_buffer *iobuf;
	size_t reserve_len;
	size_t alloc_len;

	/* Calculate additional length required at start of buffer */
	reserve_len = ( sizeof ( struct ethhdr ) -
			sizeof ( struct ipoib_hdr ) );

	/* Allocate buffer */
	alloc_len = ( len + reserve_len + sizeof ( struct ib_global_route_header ) );
	iobuf = alloc_iob_raw ( alloc_len, len, -reserve_len );
	if ( iobuf ) {
		iob_reserve ( iobuf, reserve_len );
	}
	return iobuf;
}

/** IPoIB queue pair operations */
static struct ib_queue_pair_operations ipoib_qp_op = {
	.alloc_iob = ipoib_alloc_iob,
};

/**
 * Poll IPoIB network device
 *
 * @v netdev		Network device
 */
static void ipoib_poll ( struct net_device *netdev ) {
	struct ipoib_device *ipoib = netdev->priv;
	struct ib_device *ibdev = ipoib->ibdev;

	/* Poll Infiniband device */
	ib_poll_eq ( ibdev );

	/* Poll the retry timers (required for IPoIB multicast join) */
	retry_poll();
}

/**
 * Handle IPv4 broadcast multicast group join completion
 *
 * @v ibdev		Infiniband device
 * @v qp		Queue pair
 * @v membership	Multicast group membership
 * @v rc		Status code
 * @v mad		Response MAD (or NULL on error)
 */
void ipoib_join_complete ( struct ib_device *ibdev __unused,
			   struct ib_queue_pair *qp __unused,
			   struct ib_mc_membership *membership, int rc,
			   union ib_mad *mad __unused ) {
	struct ipoib_device *ipoib = container_of ( membership,
				   struct ipoib_device, broadcast_membership );

	/* Record join status as link status */
	netdev_link_err ( ipoib->netdev, rc );
}

/**
 * Join IPv4 broadcast multicast group
 *
 * @v ipoib		IPoIB device
 * @ret rc		Return status code
 */
static int ipoib_join_broadcast_group ( struct ipoib_device *ipoib ) {
	int rc;

	if ( ( rc = ib_mcast_join ( ipoib->ibdev, ipoib->qp,
				    &ipoib->broadcast_membership,
				    &ipoib->broadcast.gid,
				    ipoib_join_complete ) ) != 0 ) {
		DBGC ( ipoib, "IPoIB %p could not join broadcast group: %s\n",
		       ipoib, strerror ( rc ) );
		return rc;
	}
	ipoib->broadcast_joined = 1;

	return 0;
}

/**
 * Leave IPv4 broadcast multicast group
 *
 * @v ipoib		IPoIB device
 */
static void ipoib_leave_broadcast_group ( struct ipoib_device *ipoib ) {

	if ( ipoib->broadcast_joined ) {
		ib_mcast_leave ( ipoib->ibdev, ipoib->qp,
				 &ipoib->broadcast_membership );
		ipoib->broadcast_joined = 0;
	}
}

/**
 * Handle link status change
 *
 * @v ibdev		Infiniband device
 */
static void ipoib_link_state_changed ( struct ib_device *ibdev ) {
	struct net_device *netdev = ib_get_ownerdata ( ibdev );
	struct ipoib_device *ipoib = netdev->priv;
	union ib_guid *guid;
	int rc;

	/* Leave existing broadcast group */
	ipoib_leave_broadcast_group ( ipoib );

	/* Update MAC address based on potentially-new GID prefix */
	memcpy ( &ipoib->mac.gid.s.prefix, &ibdev->gid.s.prefix,
		 sizeof ( ipoib->mac.gid.s.prefix ) );

	/* Calculate client identifier ( GUID ) */
#define CLIENT_ID_GUID_OFFSET 12
	guid = ( ( union ib_guid * ) ( netdev->client_id + CLIENT_ID_GUID_OFFSET ) );
	memcpy ( guid, &ibdev->gid.s.guid, sizeof ( union ib_guid ) );

	/* Update broadcast GID based on potentially-new partition key */
	ipoib->broadcast.gid.words[2] =
		htons ( ibdev->pkey | IB_PKEY_FULL );

	/* Set net device link state to reflect Infiniband link state */
	rc = ib_link_rc ( ibdev );
	netdev_link_err ( netdev, ( rc ? rc : -EINPROGRESS_JOINING ) );

	/* Join new broadcast group */
	if ( ib_is_open ( ibdev ) && ib_link_ok ( ibdev ) &&
	     ( ( rc = ipoib_join_broadcast_group ( ipoib ) ) != 0 ) ) {
		DBGC ( ipoib, "IPoIB %p could not rejoin broadcast group: "
		       "%s\n", ipoib, strerror ( rc ) );
		netdev_link_err ( netdev, rc );
		return;
	}
}

/**
 * Open IPoIB network device
 *
 * @v netdev		Network device
 * @ret rc		Return status code
 */
static int ipoib_open ( struct net_device *netdev ) {
	struct ipoib_device *ipoib = netdev->priv;
	struct ib_device *ibdev = ipoib->ibdev;
	int rc;

	/* Open IB device */
	if ( ( rc = ib_open ( ibdev ) ) != 0 ) {
		DBGC ( ipoib, "IPoIB %p could not open device: %s\n",
		       ipoib, strerror ( rc ) );
		goto err_ib_open;
	}

	/* Allocate send and receive completion queue */
	ipoib->tx_cq = ib_create_cq ( ibdev, IPOIB_NUM_SEND_CQES, &ipoib_cq_op );
	if ( ! ipoib->tx_cq ) {
		DBGC ( ipoib, "IPoIB %p could not allocate send completion queue\n", ipoib );
		rc = -ENOMEM;
		goto err_create_tx_cq;
	}
	ipoib->rx_cq = ib_create_cq ( ibdev, IPOIB_NUM_RECV_CQES, &ipoib_cq_op );
	if ( ! ipoib->rx_cq ) {
		DBGC ( ipoib, "IPoIB %p could not allocate recieve completion queue\n", ipoib );
		rc = -ENOMEM;
		goto err_create_rx_cq;
	}

	/* Allocate queue pair */
	ipoib->qp = ib_create_qp ( ibdev, IB_QPT_UD, IPOIB_NUM_SEND_WQES,
				   ipoib->tx_cq, IPOIB_NUM_RECV_WQES, ipoib->rx_cq,
				   &ipoib_qp_op );
	if ( ! ipoib->qp ) {
		DBGC ( ipoib, "IPoIB %p could not allocate queue pair\n",
		       ipoib );
		rc = -ENOMEM;
		goto err_create_qp;
	}
	ib_qp_set_ownerdata ( ipoib->qp, ipoib );

	/* Update MAC address with QPN */
	ipoib->mac.flags__qpn = htonl ( ipoib->qp->qpn );

	/* Fill receive rings */
	ib_refill_recv ( ibdev, ipoib->qp );

	/* Fake a link status change to join the broadcast group */
	ipoib_link_state_changed ( ibdev );

	return 0;

	ib_destroy_qp ( ibdev, ipoib->qp );
 err_create_qp:
	ib_destroy_cq ( ibdev, ipoib->rx_cq );
 err_create_rx_cq:
	ib_destroy_cq ( ibdev, ipoib->tx_cq );
 err_create_tx_cq:
	ib_close ( ibdev );
 err_ib_open:
	return rc;
}

/**
 * Close IPoIB network device
 *
 * @v netdev		Network device
 */
static void ipoib_close ( struct net_device *netdev ) {
	struct ipoib_device *ipoib = netdev->priv;
	struct ib_device *ibdev = ipoib->ibdev;

	/* Flush REMAC cache */
	ipoib_flush_remac ( ipoib );

	/* Leave broadcast group */
	ipoib_leave_broadcast_group ( ipoib );

	/* Remove QPN from MAC address */
	ipoib->mac.flags__qpn = 0;

	/* Tear down the queues */
	ib_destroy_qp ( ibdev, ipoib->qp );

	ib_destroy_cq ( ibdev, ipoib->rx_cq );
	ib_destroy_cq ( ibdev, ipoib->tx_cq );

	/* Close IB device */
	ib_close ( ibdev );
}

uint8_t mellanox_general_client_id[MAX_LL_ADDR_LEN] = {255,0,0,0,0,0,2,0,0,2,0xC9,0,0,0,0,0,0,0,0,0};
/**
 * Probe IPoIB device
 *
 * @v ibdev		Infiniband device
 * @ret rc		Return status code
 */
static int ipoib_probe ( struct ib_device *ibdev ) {
	struct net_device *netdev;
	struct ipoib_device *ipoib;
	int rc;

	/* Allocate network device */
	netdev = alloc_ipoibdev ( sizeof ( *ipoib ) );
	if ( ! netdev )
		return -ENOMEM;
	netdev_init ( netdev, &ipoib_operations );
	ipoib = netdev->priv;
	ib_set_ownerdata ( ibdev, netdev );
	netdev->dev = ibdev->dev;
	memset ( ipoib, 0, sizeof ( *ipoib ) );
	ipoib->netdev = netdev;
	ipoib->ibdev = ibdev;
	INIT_LIST_HEAD ( &ipoib->peers );

	/* Extract hardware address */
	memcpy ( netdev->hw_addr, &ibdev->gid.s.guid,
		 sizeof ( ibdev->gid.s.guid ) );

	/* Set local MAC address */
	memcpy ( &ipoib->mac.gid.s.guid, &ibdev->gid.s.guid,
		 sizeof ( ipoib->mac.gid.s.guid ) );

	/* Set default broadcast MAC address */
	memcpy ( &ipoib->broadcast, &ipoib_broadcast,
		 sizeof ( ipoib->broadcast ) );

	/* Register network device */
	if ( ( rc = register_netdev ( netdev ) ) != 0 )
		goto err_register_netdev;

	/* Update client identifier to IPoIB client identifier */
	memcpy ( netdev->client_id, &mellanox_general_client_id, MAX_LL_ADDR_LEN );

	return 0;

 err_register_netdev:
	netdev_nullify ( netdev );
	netdev_put ( netdev );
	return rc;
}

/**
 * Remove IPoIB device
 *
 * @v ibdev		Infiniband device
 */
static void ipoib_remove ( struct ib_device *ibdev ) {
	struct net_device *netdev = ib_get_ownerdata ( ibdev );

	unregister_netdev ( netdev );
	netdev_nullify ( netdev );
	netdev_put ( netdev );
}

/** IPoIB driver */
struct ib_driver ipoib_driver __ib_driver = {
	.name = "IPoIB",
	.probe = ipoib_probe,
	.notify = ipoib_link_state_changed,
	.remove = ipoib_remove,
};
