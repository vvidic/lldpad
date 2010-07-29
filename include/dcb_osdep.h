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

#ifndef _DCB_OSDEP_H_
#define _DCB_OSDEP_H_

#include "includes.h"
#include <asm/types.h>
#include <stdint.h>
#include <linux/dcbnl.h>
#include <linux/if_ether.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#define IN

#define max(x,y)  ((x>y)?(x):(y))
#define Sleep(x)  sleep(x)

typedef unsigned long  ULONG;

#define OutputDebugString(string) printf("%s.\n", string);

#ifdef DCB_APP_IDTYPE_ETHTYPE
#define DCB_APP_DRV_IF_SUPPORTED
#endif

#define APP_FCOE_ETHTYPE ETH_P_FCOE

#endif
