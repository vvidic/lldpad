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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <linux/rtnetlink.h>
#include <linux/if.h>
#include <linux/if_vlan.h>
#include <linux/sockios.h>
#include <syslog.h>
#include <unistd.h>
#include "lldpad.h"
#include "lldp_mod.h"
#include "common.h"
#include "eloop.h"
#include "drv_cfg.h"
#include "event_iface.h"
#include "lldp_util.h"
#include "dcb_driver_interface.h"
#include "config.h"
#include "lldp/l2_packet.h"
#include "config.h"
#include "lldp/states.h"

static void event_if_decode_rta(int type, struct rtattr *rta);

#define MAX_PAYLOAD 4096  /* maximum payload size*/

static char *device_name = NULL;
static int link_status = 0;

static void event_if_decode_rta(int type, struct rtattr *rta)
{

	TRACE1("    rta_type  =", rta->rta_len);
	
	switch (type) {
	case IFLA_ADDRESS:
		TRACE(" IFLA_ADDRESS\n");
		break;
	case IFLA_BROADCAST:
		TRACE(" IFLA_BROADCAST\n");
		break;
	case IFLA_OPERSTATE:
		TRACE1(" IFLA_OPERSTATE ", type);
		link_status = (*((int *)RTA_DATA(rta)));
		break;
	case IFLA_LINKMODE:
		TRACE1(" IFLA_LINKMODE  ", type)
		TRACE2("        LINKMODE = ", (*((int *)RTA_DATA(rta)))? 
			"IF_LINK_MODE_DORMANT": "IF_LINK_MODE_DEFAULT")
		break;
	case IFLA_IFNAME:
		device_name =  (char *)RTA_DATA(rta);
		TRACE(" IFLA_IFNAME\n")
		TRACE2(" device name is ", device_name)
		break;
	default:
		TRACE1(" unknown type : ", type)
		break;
	}
}

int oper_add_device(char *device_name)
{
	struct lldp_module *np;
	const struct lldp_mod_ops *ops;
	struct port *port;
	int err;

	port = porthead;
	while (port != NULL) {
		if (!strncmp(device_name, port->ifname, MAX_DEVICE_NAME_LEN))
			break;
		port = port->next;
	}

	if (!port) {
		if (is_bond(device_name))
			err = add_bond_port(device_name);
		else
			err = add_adapter(device_name);

		if (err) {
			printf("%s: Error adding device %s\n",
				__func__, device_name);
			return err;
		} else
			printf("%s: Adding device %s\n",
				__func__, device_name);
	} else if (!port->portEnabled)
		reinit_port(device_name);

	if (!port || !port->portEnabled) {
		LIST_FOREACH(np, &lldp_head, lldp) {
			ops = np->ops;
			if (ops->lldp_mod_ifup)
				ops->lldp_mod_ifup(device_name);
		}
	}
	set_lldp_port_enable_state(device_name, 1);
	return 0;
}

static void event_if_decode_nlmsg(int route_type, void *data, int len)
{
	struct lldp_module *np;
	const struct lldp_mod_ops *ops;
	struct rtattr *rta;
	int attrlen;
	int val, err;

	switch (route_type) {
	case RTM_NEWLINK:		
	case RTM_DELLINK:
	case RTM_SETLINK:
	case RTM_GETLINK:
		TRACE("  IFINFOMSG\n");
		TRACE1("  ifi_family = ",
			((struct ifinfomsg *)data)->ifi_family);
		TRACE1("  ifi_type   = ",
			((struct ifinfomsg *)data)->ifi_type);
		TRACE1("  ifi_index  = ",
			((struct ifinfomsg *)data)->ifi_index);
		TRACE1("  ifi_flags  = ",
			((struct ifinfomsg *)data)->ifi_flags);
		TRACE1("  ifi_change = ",
			((struct ifinfomsg *)data)->ifi_change);

		/* print attributes */
		rta = IFLA_RTA(data);

		attrlen = len - sizeof(struct ifinfomsg);
		while (RTA_OK(rta, attrlen)) {
			event_if_decode_rta(rta->rta_type, rta);
			rta = RTA_NEXT(rta, attrlen);
		}

		TRACE1("link status: ", link_status);
		TRACE2("device name: ", device_name);

		switch (link_status) {
		case IF_OPER_DOWN:
			printf("******* LINK DOWN: %s\n", device_name);

			err = is_valid_lldp_device(device_name);
			if (!err)
				break;

			LIST_FOREACH(np, &lldp_head, lldp) {
				ops = np->ops;
				if (ops->lldp_mod_ifdown)
					ops->lldp_mod_ifdown(device_name);
			}

			/* Disable Port */
			set_lldp_port_enable_state(device_name, 0);

			if (route_type == RTM_DELLINK) {
				printf("%s: %s: device removed!\n",
					__func__, device_name);
				remove_adapter(device_name);
			}
			break;
		case IF_OPER_DORMANT:
			printf("******* LINK DORMANT: %s\n", device_name);
			err = is_valid_lldp_device(device_name);
			if (!err)
				break;
			set_port_oper_delay(device_name);
			oper_add_device(device_name);
			break;
		case IF_OPER_UP:
			printf("******* LINK UP: %s\n",	device_name);
			err = is_valid_lldp_device(device_name);
			if (!err)
				break;
			oper_add_device(device_name);
			printf("%s: %s: Programming HW on LINK_UP\n",
				__func__, device_name);
			set_hw_all(device_name);
			break;
		default:
			break;
		}
		break;
	case RTM_NEWADDR:
	case RTM_DELADDR:
	case RTM_GETADDR:
		TRACE("Address change.\n");
		break;
	default:
		TRACE("No decode for this type\n");
	}
}


static void event_if_process_recvmsg(struct nlmsghdr *nlmsg)
{

	/* print out details */
	event_if_decode_nlmsg(nlmsg->nlmsg_type, NLMSG_DATA(nlmsg),
		NLMSG_PAYLOAD(nlmsg, 0));
}

static void event_iface_receive(int sock, void *eloop_ctx, void *sock_ctx)
{
	struct nlmsghdr *nlh;
	struct sockaddr_nl dest_addr;
	char buf[MAX_MSG_SIZE];
	socklen_t fromlen = sizeof(dest_addr);
	int result;
	
	result = recvfrom(sock, buf, sizeof(buf), MSG_DONTWAIT,
		       (struct sockaddr *) &dest_addr, &fromlen);

	if (result < 0) {
		perror("recvfrom(Event interface)");
		if ((errno == ENOBUFS) || (errno == EAGAIN))
			eloop_register_timeout(INI_TIMER, 0, scan_port,
					       NULL, NULL);
		return;
	}

	TRACE("PRINT BUF info.\n")

	device_name = NULL;
	link_status = IF_OPER_UNKNOWN;
	nlh = (struct nlmsghdr *)buf;
	event_if_process_recvmsg(nlh);
}

int event_iface_init()
{
	int fd;
	int rcv_size = MAX_MSG_SIZE;
	struct sockaddr_nl snl;

	fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE);

	if (fd < 0)
		return fd;

	if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcv_size, sizeof(int)) < 0) {
		close(fd);
		return -EIO;
	}

	memset((void *)&snl, 0, sizeof(struct sockaddr_nl));
	snl.nl_family = AF_NETLINK;
	snl.nl_groups = RTMGRP_LINK;

	if (bind(fd, (struct sockaddr *)&snl, sizeof(struct sockaddr_nl)) < 0) {
		close(fd);
		return -EIO;
	}

	eloop_register_read_sock(fd, event_iface_receive, NULL, NULL);
	return 0;
}

int event_iface_deinit()
{
	return 0;
}
