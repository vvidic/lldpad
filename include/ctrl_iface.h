/*******************************************************************************

  LLDP Agent Daemon (LLDPAD) Software 
  Copyright(c) 2007-2010 Intel Corporation.

  Substantially modified from:
  hostapd-0.5.7
  Copyright (c) 2002-2007, Jouni Malinen <jkmaline@cc.hut.fi> and
  contributors

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

#ifndef CTRL_IFACE_H
#define CTRL_IFACE_H

#include "dcb_protocol.h"
#include "sys/un.h"

struct ctrl_dst;

struct clif_data {
	char iface[IFNAMSIZ+1];
	char *ctrl_interface;
	gid_t ctrl_interface_gid;
	int ctrl_interface_gid_set;
	int ctrl_sock;
	struct ctrl_dst *ctrl_dst;
};

int ctrl_iface_init(struct clif_data *clifd);
int ctrl_iface_register(struct clif_data *clifd);
void ctrl_iface_deinit(struct clif_data *clifd);
void ctrl_iface_send(struct clif_data *clifd, int level, u32 moduleid,
				char *buf, size_t len);

int clif_iface_attach(struct clif_data *clifd,
				struct sockaddr_un *from,
				socklen_t fromlen,
				char *ibuf, int ilen,
				char *rbuf);
int clif_iface_detach(struct clif_data *clifd,
				struct sockaddr_un *from,
				socklen_t fromlen,
				char *ibuf, int ilen,
				char *rbuf);
int clif_iface_level(struct clif_data *clifd,
				struct sockaddr_un *from,
				socklen_t fromlen,
				char *ibuf, int ilen,
				char *rbuf);
int clif_iface_ping(struct clif_data *clifd,
				struct sockaddr_un *from,
				socklen_t fromlen,
				char *ibuf, int ilen,
				char *rbuf);
int clif_iface_cmd_unknown(struct clif_data *clifd,
				struct sockaddr_un *from,
				socklen_t fromlen,
				char *ibuf, int ilen,
				char *rbuf);
int clif_iface_module(struct clif_data *clifd,
				struct sockaddr_un *from,
				socklen_t fromlen,
				char *ibuf, int ilen,
				char *rbuf);
#endif /* CTRL_IFACE_H */
