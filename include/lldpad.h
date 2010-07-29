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

#ifndef LLDPAD_H
#define LLDPAD_H

#include <sys/queue.h>
#include "common.h"

#ifndef IFNAMSIZ
#define IFNAMSIZ 16
#endif

#define MAX_NAME_LEN  16  

#define PID_FILE "/var/run/lldpad.pid"

#define DEFAULT_CFG_FILE "/var/lib/lldpad/lldpad.conf"

#define DEFAULT_DCBX_VERSION  2

extern char *cfg_file_name;

#undef DCBTRACE
/*#define DCBTRACE*/
#ifdef DCBTRACE
#define TRACE(s) printf("%s\n", s);
#define TRACE1(s,d) printf("%s  %d\n", s, d); 
#define TRACE2(s,c) printf("%s  %s \n", s, c );
#define TRACE3(s,e) printf("%s %c", s, e);
#else
#define TRACE(s) 
#define TRACE1(s, d)
#define TRACE2(s, c)
#define TRACE3(s, e)
#endif 

void send_event(int level, u32 type, char *ebuf);

#endif /* LLDPAD_H */
