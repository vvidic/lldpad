/*******************************************************************************

  LLDP Agent Daemon (LLDPAD) Software
  Copyright(c) 2007-2010 Intel Corporation.

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

  Contact Information:
  e1000-eedc Mailing List <e1000-eedc@lists.sourceforge.net>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

*******************************************************************************/

#include <syslog.h>
#include <sys/ioctl.h>
#include <linux/if_packet.h>
#include <linux/pkt_sched.h>
#include <net/if.h>

#include "common.h"
#include "eloop.h"
#include "ports.h"
#include "messages.h"
#include "l2_packet.h"
#include "lldp_util.h"
#include "dcb_types.h"
#include "drv_cfg.h"
#include "lldp/states.h"

static struct port *bond_porthead = NULL;

struct l2_packet_data {
	int fd;
	char ifname[IFNAMSIZ + 1];
	int ifindex;
	u8 perm_mac_addr[ETH_ALEN];
	u8 curr_mac_addr[ETH_ALEN];
	u8 san_mac_addr[ETH_ALEN];
	void (*rx_callback)(void *ctx, unsigned int ifindex,
			    const u8 *buf, size_t len);
	void *rx_callback_ctx;
	int l2_hdr; /* whether to include layer 2 (Ethernet) header data
		     * buffers */
};

int l2_packet_get_own_src_addr(struct l2_packet_data *l2, u8 *addr)
{
	if (is_san_mac(l2->san_mac_addr))
		os_memcpy(addr, l2->san_mac_addr, ETH_ALEN);
	else {
		/* get an appropriate src MAC to use if the port is
	 	* part of a bond.
		*/
		struct port *bond_port = bond_porthead;
		while (bond_port != NULL) {
			if (get_src_mac_from_bond(bond_port, l2->ifname, addr))
				return 0;

			bond_port = bond_port->next;
		}
		os_memcpy(addr, l2->curr_mac_addr, ETH_ALEN);
	}

	return 0;
}

int l2_packet_get_own_addr(struct l2_packet_data *l2, u8 *addr)
{
	os_memcpy(addr, l2->perm_mac_addr, ETH_ALEN);
	return 0;
}


int l2_packet_send(struct l2_packet_data *l2, const u8 *dst_addr, u16 proto,
		   const u8 *buf, size_t len)
{
	int ret;
	if (l2 == NULL)
		return -1;

	if (l2->l2_hdr) {
		ret = send(l2->fd, buf, len, 0);
		if (ret < 0)
			perror("l2_packet_send - send");
	} else {
		struct sockaddr_ll ll;
		os_memset(&ll, 0, sizeof(ll));
		ll.sll_family = AF_PACKET;
		ll.sll_ifindex = l2->ifindex;
		ll.sll_protocol = htons(proto);
		ll.sll_halen = ETH_ALEN;
		os_memcpy(ll.sll_addr, dst_addr, ETH_ALEN);
		ret = sendto(l2->fd, buf, len, 0, (struct sockaddr *) &ll,
			     sizeof(ll));
		if (ret < 0)
			perror("l2_packet_send - sendto");
	}
	return ret;
}


static void l2_packet_receive(int sock, void *eloop_ctx, void *sock_ctx)
{
	struct l2_packet_data *l2 = eloop_ctx;
	u8 buf[2300];
	int res;
	struct sockaddr_ll ll;
	socklen_t fromlen;

	os_memset(&ll, 0, sizeof(ll));
	fromlen = sizeof(ll);
	res = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr *) &ll,
		       &fromlen);

	if (res < 0) {
		printf("receive ERROR = %d\n", res);
		perror("l2_packet_receive - recvfrom");
		return;
	}

	l2->rx_callback(l2->rx_callback_ctx, ll.sll_ifindex, buf, res);
}


struct l2_packet_data * l2_packet_init(
	const char *ifname, const u8 *own_addr, unsigned short protocol,
	void (*rx_callback)(void *ctx, unsigned int ifindex,
			    const u8 *buf, size_t len),
	void *rx_callback_ctx, int l2_hdr)
{
	struct l2_packet_data *l2;
	struct ifreq ifr;
	struct sockaddr_ll ll;

	l2 = os_zalloc(sizeof(struct l2_packet_data));
	if (l2 == NULL)
		return NULL;
	os_strncpy(l2->ifname, ifname, sizeof(l2->ifname));
	l2->rx_callback = rx_callback;
	l2->rx_callback_ctx = rx_callback_ctx;
	l2->l2_hdr = l2_hdr;

	l2->fd = socket(PF_PACKET, l2_hdr ? SOCK_RAW : SOCK_DGRAM,
			htons(protocol));

	if (l2->fd < 0) {
		perror("socket(PF_PACKET)");
		os_free(l2);
		return NULL;
	}

	os_strncpy(ifr.ifr_name, l2->ifname, sizeof(ifr.ifr_name));
	if (ioctl(l2->fd, SIOCGIFINDEX, &ifr) < 0) {
		perror("ioctl[SIOCGIFINDEX]");
		close(l2->fd);
		os_free(l2);
		return NULL;
	}
	l2->ifindex = ifr.ifr_ifindex;

	os_memset(&ll, 0, sizeof(ll));
	ll.sll_family = PF_PACKET;
	ll.sll_ifindex = ifr.ifr_ifindex;
	ll.sll_protocol = htons(protocol);
	if (bind(l2->fd, (struct sockaddr *) &ll, sizeof(ll)) < 0) {
		perror("bind[PF_PACKET]");
		close(l2->fd);
		os_free(l2);
		return NULL;
	}

	/* current hw address */
	if (ioctl(l2->fd, SIOCGIFHWADDR, &ifr) < 0) {
		perror("ioctl[SIOCGIFHWADDR]");
		close(l2->fd);
		os_free(l2);
		return NULL;
	}
	os_memcpy(l2->curr_mac_addr, ifr.ifr_hwaddr.sa_data, ETH_ALEN);

	if (get_perm_hwaddr(ifname, l2->perm_mac_addr, l2->san_mac_addr) != 0) {
		os_memcpy(l2->perm_mac_addr, ifr.ifr_hwaddr.sa_data, ETH_ALEN);
		memset(l2->san_mac_addr, 0xff, ETH_ALEN);
	}

	struct packet_mreq mr;
	memset(&mr, 0, sizeof(mr));
	mr.mr_ifindex = l2->ifindex;
	mr.mr_alen = ETH_ALEN;
	memcpy(mr.mr_address, multi_cast_source, ETH_ALEN);
	mr.mr_type = PACKET_MR_MULTICAST;
	if (setsockopt(l2->fd, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mr,
		sizeof(mr)) < 0) {
		perror("setsockopt");
		close(l2->fd);
		os_free(l2);
		return NULL;
	}
	int option = 1;
	int option_size = sizeof(option);
	if (setsockopt(l2->fd, SOL_PACKET, PACKET_ORIGDEV,
		&option, option_size) < 0) {
		perror("setsockopt SOL_PACKET");
		close(l2->fd);
		os_free(l2);
		return NULL;
	}

	option = TC_PRIO_CONTROL;
	if ( setsockopt(l2->fd, SOL_SOCKET, SO_PRIORITY, &option,
		sizeof(option_size)) < 0) {
		perror("setsockopt SOL_PRIORITY");
		close(l2->fd);
		os_free(l2);
		return NULL;
	}

	LLDPAD_INFO("%s MAC address is " MACSTR "\n",
		ifname, MAC2STR(l2->perm_mac_addr));

	eloop_register_read_sock(l2->fd, l2_packet_receive, l2, NULL);

	return l2;
}


void l2_packet_deinit(struct l2_packet_data *l2)
{
	if (l2 == NULL)
		return;

	if (l2->fd >= 0) {
		eloop_unregister_read_sock(l2->fd);
		close(l2->fd);
	}
		
	os_free(l2);
}

void l2_packet_get_port_state(struct l2_packet_data *l2, u8  *portEnabled)
{

	int s;
	struct ifreq ifr;

	s = socket(PF_INET, SOCK_DGRAM, 0);
	if (s < 0) {
		perror("socket");
		return;
	}
	os_memset(&ifr, 0, sizeof(ifr));
	os_strncpy(ifr.ifr_name, l2->ifname, sizeof(ifr.ifr_name));

	if (ioctl(s, SIOCGIFFLAGS, &ifr) < 0) {
		perror("ioctl[SIOCGIFFLAGS]");
		close(s);
		*portEnabled = 0;
		return;
	}
		
	*portEnabled = ifr.ifr_flags & IFF_UP;
	close(s);
	return;
}


int add_bond_port(const char *ifname)
{
	struct port *bond_newport;

	bond_newport = bond_porthead;
	while (bond_newport != NULL) {
		if(!strncmp(ifname, bond_newport->ifname,
			MAX_DEVICE_NAME_LEN))
			return 0;
		bond_newport = bond_newport->next;
	}

	bond_newport  = (struct port *)malloc(sizeof(struct port));
	if (bond_newport == NULL) {
		syslog(LOG_ERR, "failed to malloc bond port %s", ifname);
		return -1;
	}
	memset(bond_newport,0,sizeof(struct port));	
	bond_newport->next = NULL;
	bond_newport->ifname = strdup(ifname);
	if (bond_newport->ifname == NULL) {
		syslog(LOG_ERR, "failed to strdup bond name %s", ifname);
		goto fail1;
	}
	
	bond_newport->l2 = l2_packet_init(bond_newport->ifname, NULL,
		ETH_P_LLDP, recv_on_bond, bond_newport, 1);

	if (bond_newport->l2 == NULL) {
		syslog(LOG_ERR, "failed to open register layer 2 access to "
			   "ETH_P_LLDP");
		goto fail2;
	}

	if (bond_porthead)
		bond_newport->next = bond_porthead;
	bond_porthead = bond_newport;

	return 0;

fail2:
	free(bond_newport->ifname);
	bond_newport->ifname = NULL;
fail1:
	free(bond_newport);
	bond_newport = NULL;
	return -1;
}


void recv_on_bond(void *ctx, unsigned int ifindex, const u8 *buf, size_t len)
{
	struct port *port;
	

	/* Find the originating slave port object */
	for (port = porthead; port != NULL && port->l2->ifindex != ifindex;
		port = port->next)
		;

	if (port)
		rxReceiveFrame(port, ifindex, buf, len);
}


int remove_bond_port(const char *ifname)
{
	struct port *bond_port = bond_porthead;
	struct port *bond_parent = NULL;

	while (bond_port != NULL) {
		if (!strncmp(ifname, bond_port->ifname, MAX_DEVICE_NAME_LEN)) {
			printf("In remove_bond_port: Found bond port  %s\n",
				bond_port->ifname);
			break;
		}
		bond_parent = bond_port;
		bond_port = bond_port->next;
	}

	if (bond_port == NULL)
		return -1;

	l2_packet_deinit(bond_port->l2);

	if (bond_parent == NULL)
		bond_porthead = bond_port->next;
	else if (bond_parent->next == bond_port)     /* sanity check */
		bond_parent->next = bond_port->next;	
	else
		return -1;

	if (bond_port->ifname)
		free(bond_port->ifname);
	
	free(bond_port);
	return 0;
}

void remove_all_bond_ports(void)
{
	struct port *bond_port = bond_porthead;
	struct port *next_bond_port = bond_porthead;

	while (bond_port != NULL) {
		next_bond_port = bond_port->next;

		l2_packet_deinit(bond_port->l2);
		if (bond_port->ifname)
			free(bond_port->ifname);
		free(bond_port);

		bond_port = next_bond_port;
	}
}