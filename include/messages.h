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

#ifndef _MESSAGES_H_
#define _MESSAGES_H_
#include <syslog.h>

#define MSG_INFO_DEBUG_STRING 1
#define MSG_INFO_PG_ENABLED 2
#define MSG_INFO_PG_DISABLED 3
#define MSG_INFO_PFC_ENABLED 4
#define MSG_INFO_PFC_DISABLED 5
#define MSG_INFO_APP_DISABLED 6
#define MSG_INFO_APP_ENABLED 7
#define MSG_INFO_PG_OPER 8
#define MSG_INFO_PFC_OPER 9
#define MSG_INFO_APP_OPER 10

#define MSG_ERR_PG_NONOPER 11
#define MSG_ERR_PFC_NONOPER 12
#define MSG_ERR_APP_NONOPER 13

#define MSG_ERR_SERVICE_START_FAILURE 14
#define MSG_ERR_RESOURCE_MEMORY 15
#define MSG_ERR_ADD_CARD_FAILURE 16

#define MSG_ERR_DCB_INVALID_TX_TOTAL_BWG 17
#define MSG_ERR_DCB_INVALID_RX_TOTAL_BWG 18
#define MSG_ERR_DCB_INVALID_TX_BWG_IDX 19
#define MSG_ERR_DCB_INVALID_RX_BWG_IDX 20
#define MSG_ERR_DCB_INVALID_TX_LSP_NZERO_BW_TC 21
#define MSG_ERR_DCB_INVALID_RX_LSP_NZERO_BW_TC 22
#define MSG_ERR_DCB_TOO_MANY_LSP_PGIDS 23
#define MSG_ERR_DCB_INVALID_TX_ZERO_BW_TC 24
#define MSG_ERR_DCB_INVALID_RX_ZERO_BW_TC 25
#define MSG_ERR_DCB_INVALID_TX_LSP_NZERO_BWG 26
#define MSG_ERR_DCB_INVALID_RX_LSP_NZERO_BWG 27
#define MSG_ERR_DCB_INVALID_TX_BWG 28
#define MSG_ERR_DCB_INVALID_RX_BWG 29
#define MSG_ERR_TX_SM_INVALID 30
#define MSG_ERR_RX_SM_INVALID 31
#define MSG_ERR_DCB_INVALID_CONFIG_FILE 32

#define MSG_INFO_BCN_DISABLED 33
#define MSG_INFO_BCN_ENABLED 34
#define MSG_INFO_BCN_OPER 35
#define MSG_ERR_BCN_NONOPER 36

#define MSG_INFO_LLINK_DISABLED 37
#define MSG_INFO_LLINK_ENABLED 38
#define MSG_INFO_LLINK_OPER 39
#define MSG_ERR_LLINK_NONOPER 40

/* the following msgid match up to syslog LOG_xxx */
#define MSG_SYSLOG_START	(MSG_ERR_LLINK_NONOPER + LOG_EMERG + 1)
#define MSG_SYSLOG_END		(MSG_SYSLOG_START + LOG_DEBUG)
#define MSG_SYSLOG(t) (LOG_##t + MSG_SYSLOG_START)
#define MSG2SYSLOG(i) ((i) -  MSG_SYSLOG_START)
#define MSG_IS_SYSLOG(i) \
	(((i) >= MSG_SYSLOG_START) && ((i) <= MSG_SYSLOG_END))

void log_message(u32 dwMsgId, const char *pFormat, ...);

#define LLDPAD_ERR(...) log_message(MSG_SYSLOG(ERR),  __VA_ARGS__)
#define LLDPAD_WARN(...) log_message(MSG_SYSLOG(WARNING), __VA_ARGS__)
#define LLDPAD_INFO(...) log_message(MSG_SYSLOG(INFO), __VA_ARGS__)
#define LLDPAD_DBG(...) log_message(MSG_SYSLOG(DEBUG), __VA_ARGS__)

#endif
