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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>
#include <poll.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <netpacket/packet.h>
#include <arpa/inet.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#include <sys/stat.h>
#include <fcntl.h>

#include "common.h"
#include "lldp_rtnl.h"
#include "messages.h"

#define NLMSG(c) ((struct nlmsghdr *) (c))

#define LOG(...) log_message(MSG_INFO_DEBUG_STRING, __VA_ARGS__)

#define NLMSG_SIZE 1024
/**
 * rtnl_recv - receive from a routing netlink socket
 * @s: routing netlink socket with data ready to be received
 *
 * Returns:	0 when NLMSG_DONE is received
 * 		<0 on error
 * 		>0 when more data is expected
 */
static int rtnl_recv(int s, rtnl_handler *fn, void *arg)
{
	char buf[8192];
	struct nlmsghdr *nh;
	int len;
	int rc = 0;
	bool more = false;

	LOG("%s", __func__);
more:
	len = recv(s, buf, sizeof(buf), 0);
	if (len < 0) {
		LOG("netlink recvmsg error");
		return len;
	}

	for (nh = NLMSG(buf); NLMSG_OK(nh, len); nh = NLMSG_NEXT(nh, len)) {
		if (nh->nlmsg_flags & NLM_F_MULTI)
			more = true;

		switch (nh->nlmsg_type) {
		case NLMSG_NOOP:
			LOG("NLMSG_NOOP");
			break;
		case NLMSG_ERROR:
			rc = ((struct nlmsgerr *)NLMSG_DATA(nh))->error;
			LOG("NLMSG_ERROR (%d) %s", rc, strerror(-rc));
			break;
		case NLMSG_DONE:
			more = false;
			LOG("NLMSG_DONE");
			break;
		default:
			if (!fn || fn(nh, arg) < 0)
				LOG("unexpected netlink message type %d",
					 nh->nlmsg_type);
			break;
		}
	}
	if (more)
		goto more;
	return rc;
}

#define NLMSG_TAIL(nmsg) \
	((struct rtattr *)(((void *)(nmsg)) + NLMSG_ALIGN((nmsg)->nlmsg_len)))

static void add_rtattr(struct nlmsghdr *n, int type, const void *data, int alen)
{
	struct rtattr *rta = NLMSG_TAIL(n);
	int len = RTA_LENGTH(alen);

	rta->rta_type = type;
	rta->rta_len = len;
	memcpy(RTA_DATA(rta), data, alen);
	n->nlmsg_len = NLMSG_ALIGN(n->nlmsg_len) + RTA_ALIGN(len);
}

static struct rtattr *add_rtattr_nest(struct nlmsghdr *n, int type)
{
	struct rtattr *nest = NLMSG_TAIL(n);

	add_rtattr(n, type, NULL, 0);
	return nest;
}

static void end_rtattr_nest(struct nlmsghdr *n, struct rtattr *nest)
{
	nest->rta_len = (void *)NLMSG_TAIL(n) - (void *)nest;
}

static ssize_t rtnl_send_linkmode(int s, int ifindex,
				  char *ifname, __u8 linkmode)
{
	struct {
		struct nlmsghdr nh;
		struct ifinfomsg ifm;
		char attrbuf[
			RTA_SPACE(IFNAMSIZ)	/* IFNAME */
			+ RTA_SPACE(1)];	/* LINKMODE */
	} req = {
		.nh = {
			.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg)),
			.nlmsg_type = RTM_SETLINK,
			.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK,
		},
		.ifm = {
			.ifi_index = ifindex,
		},
	};

	if (ifname)
		add_rtattr(&req.nh, IFLA_IFNAME, ifname, strlen(ifname));
	add_rtattr(&req.nh, IFLA_LINKMODE, &linkmode, 1);

	return send(s, &req, req.nh.nlmsg_len, 0);
}

static int rtnl_set_linkmode(int ifindex, char *ifname, __u8 linkmode)
{
	int s;
	int rc;

	s = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);
	if (s < 0)
		return s;
	rc = rtnl_send_linkmode(s, ifindex, ifname, linkmode);
	if (rc < 0)
		goto out;
	rc = rtnl_recv(s, NULL, NULL);
out:
	close(s);
	return rc;
}

static ssize_t rtnl_send_operstate(int s, int ifindex,
				   char *ifname, __u8 operstate)
{
	struct {
		struct nlmsghdr nh;
		struct ifinfomsg ifm;
		char attrbuf[
			RTA_SPACE(IFNAMSIZ)	/* IFNAME */
			+ RTA_SPACE(1)];	/* OPERSTATE */
	} req = {
		.nh = {
			.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg)),
			.nlmsg_type = RTM_SETLINK,
			.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK,
		},
		.ifm = {
			.ifi_index = ifindex,
		},
	};

	if (ifname)
		add_rtattr(&req.nh, IFLA_IFNAME, ifname, strlen(ifname));
	add_rtattr(&req.nh, IFLA_OPERSTATE, &operstate, 1);

	return send(s, &req, req.nh.nlmsg_len, 0);
}

static ssize_t rtnl_recv_operstate(int s, int ifindex,
				   char *ifname, __u8 *operstate)
{
	struct nlmsghdr *nlh;
	struct ifinfomsg *ifi;
	struct rtattr *rta;
	int attrlen;
	int rc = -1; 

	nlh = malloc(NLMSG_SIZE);
	if (!nlh)
		return rc;

	/* send ifname request */
	nlh->nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
	nlh->nlmsg_type = RTM_GETLINK;
	nlh->nlmsg_flags = NLM_F_REQUEST;

	ifi = NLMSG_DATA(nlh);
	ifi->ifi_family = AF_UNSPEC;
	ifi->ifi_index = ifindex;	

	if (ifname)
		add_rtattr(nlh, IFLA_IFNAME, ifname, strlen(ifname));

	rc = send(s, nlh, nlh->nlmsg_len, 0);
	if (rc < 0)
		goto out;

	/* recv ifname reply */
	memset(nlh, 0, NLMSG_SIZE);
	rc = recv(s, (void *) nlh, NLMSG_SIZE, MSG_DONTWAIT);
	if (rc < 0)
		goto out;
	ifi = NLMSG_DATA(nlh);
	rta = IFLA_RTA(ifi);
	attrlen = NLMSG_PAYLOAD(nlh, 0) - sizeof(struct ifinfomsg);
	while (RTA_OK(rta, attrlen)) {
		if (rta->rta_type == IFLA_OPERSTATE)
			memcpy(operstate, RTA_DATA(rta), sizeof(__u8));
		rta = RTA_NEXT(rta, attrlen);
	}

out:
	free(nlh);
	return rc;
}

int set_operstate(char *ifname, __u8 operstate)
{
	int s;
	int rc;
	int ifindex = 0;

	s = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);
	if (s < 0)
		return s;
	rc = rtnl_send_operstate(s, ifindex, ifname, operstate);
	if (rc < 0)
		goto out;
	rc = rtnl_recv(s, NULL, NULL);
out:
	close(s);
	return rc;
}

int get_operstate(char *ifname)
{
	int s, ifq;
	int ifindex = 0;
	struct ifreq ifr;
	__u8 operstate;

	/* fill in ifr_ifindex for kernel versions that require it */
	ifq = socket(PF_PACKET, SOCK_DGRAM, 0);
	if (ifq < 0)
		return ifq;
	os_strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name) - 1);
	if (ioctl(ifq, SIOCGIFINDEX, &ifr) == 0)
		ifindex = ifr.ifr_ifindex;
	close(ifq);

	s = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);
	if (s < 0)
		return s;

	rtnl_recv_operstate(s, ifindex, ifname, &operstate);
	close(s);
	return operstate;
}

int set_linkmode(char *ifname, __u8 linkmode)
{
	return rtnl_set_linkmode(0, ifname, linkmode);
}
