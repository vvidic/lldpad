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

#ifndef COMMON_H
#define COMMON_H

#include "dcb_osdep.h"
#include "os.h"
#include <sys/stat.h>

#ifdef __linux__
#include <endian.h>
#include <byteswap.h>
#endif /* __linux__ */

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
#include <sys/types.h>
#include <sys/endian.h>
#define __BYTE_ORDER	_BYTE_ORDER
#define	__LITTLE_ENDIAN	_LITTLE_ENDIAN
#define	__BIG_ENDIAN	_BIG_ENDIAN
#define bswap_16 bswap16
#define bswap_32 bswap32
#define bswap_64 bswap64
#endif /* defined(__FreeBSD__) || defined(__NetBSD__) ||
	* defined(__DragonFly__) */

#ifdef CONFIG_TI_COMPILER
#define __BIG_ENDIAN 4321
#define __LITTLE_ENDIAN 1234
#ifdef __big_endian__
#define __BYTE_ORDER __BIG_ENDIAN
#else
#define __BYTE_ORDER __LITTLE_ENDIAN
#endif
#endif /* CONFIG_TI_COMPILER */


#if defined(__CYGWIN__) || defined(CONFIG_NATIVE_WINDOWS)


static inline unsigned short wpa_swap_16(unsigned short v)
{
	return ((v & 0xff) << 8) | (v >> 8);
}

static inline unsigned int wpa_swap_32(unsigned int v)
{
	return ((v & 0xff) << 24) | ((v & 0xff00) << 8) |
		((v & 0xff0000) >> 8) | (v >> 24);
}

#define le_to_host16(n) (n)
#define host_to_le16(n) (n)
#define be_to_host16(n) wpa_swap_16(n)
#define host_to_be16(n) wpa_swap_16(n)
#define le_to_host32(n) (n)
#define be_to_host32(n) wpa_swap_32(n)
#define host_to_be32(n) wpa_swap_32(n)

#else /* __CYGWIN__ */

#ifndef __BYTE_ORDER
#ifndef __LITTLE_ENDIAN
#ifndef __BIG_ENDIAN
#define __LITTLE_ENDIAN 1234
#define __BIG_ENDIAN 4321
#if defined(sparc)
#define __BYTE_ORDER __BIG_ENDIAN
#endif
#endif /* __BIG_ENDIAN */
#endif /* __LITTLE_ENDIAN */
#endif /* __BYTE_ORDER */

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define le_to_host16(n) (n)
#define host_to_le16(n) (n)
#define be_to_host16(n) bswap_16(n)
#define host_to_be16(n) bswap_16(n)
#define le_to_host32(n) (n)
#define be_to_host32(n) bswap_32(n)
#define host_to_be32(n) bswap_32(n)
#define le_to_host64(n) (n)
#define host_to_le64(n) (n)
#define be_to_host64(n) bswap_64(n)
#define host_to_be64(n) bswap_64(n)
#elif __BYTE_ORDER == __BIG_ENDIAN
#define le_to_host16(n) bswap_16(n)
#define host_to_le16(n) bswap_16(n)
#define be_to_host16(n) (n)
#define host_to_be16(n) (n)
#define le_to_host32(n) bswap_32(n)
#define be_to_host32(n) (n)
#define host_to_be32(n) (n)
#define le_to_host64(n) bswap_64(n)
#define host_to_le64(n) bswap_64(n)
#define be_to_host64(n) (n)
#define host_to_be64(n) (n)
#ifndef WORDS_BIGENDIAN
#define WORDS_BIGENDIAN
#endif
#else
#error Could not determine CPU byte order
#endif

#endif /* __CYGWIN__ */

/* Macros for handling unaligned 16-bit variables */
#define WPA_GET_BE16(a) ((u16) (((a)[0] << 8) | (a)[1]))
#define WPA_PUT_BE16(a, val)			\
	do {					\
		(a)[0] = ((u16) (val)) >> 8;	\
		(a)[1] = ((u16) (val)) & 0xff;	\
	} while (0)

#define WPA_GET_LE16(a) ((u16) (((a)[1] << 8) | (a)[0]))
#define WPA_PUT_LE16(a, val)			\
	do {					\
		(a)[1] = ((u16) (val)) >> 8;	\
		(a)[0] = ((u16) (val)) & 0xff;	\
	} while (0)

#define WPA_GET_BE24(a) ((((u32) (a)[0]) << 16) | (((u32) (a)[1]) << 8) | \
			 ((u32) (a)[2]))
#define WPA_PUT_BE24(a, val)				\
	do {						\
		(a)[0] = (u8) (((u32) (val)) >> 16);	\
		(a)[1] = (u8) (((u32) (val)) >> 8);	\
		(a)[2] = (u8) (((u32) (val)) & 0xff);	\
	} while (0)

#define WPA_GET_BE32(a) ((((u32) (a)[0]) << 24) | (((u32) (a)[1]) << 16) | \
			 (((u32) (a)[2]) << 8) | ((u32) (a)[3]))
#define WPA_PUT_BE32(a, val)				\
	do {						\
		(a)[0] = (u8) (((u32) (val)) >> 24);	\
		(a)[1] = (u8) (((u32) (val)) >> 16);	\
		(a)[2] = (u8) (((u32) (val)) >> 8);	\
		(a)[3] = (u8) (((u32) (val)) & 0xff);	\
	} while (0)

#define WPA_PUT_BE64(a, val)				\
	do {						\
		(a)[0] = (u8) (((u64) (val)) >> 56);	\
		(a)[1] = (u8) (((u64) (val)) >> 48);	\
		(a)[2] = (u8) (((u64) (val)) >> 40);	\
		(a)[3] = (u8) (((u64) (val)) >> 32);	\
		(a)[4] = (u8) (((u64) (val)) >> 24);	\
		(a)[5] = (u8) (((u64) (val)) >> 16);	\
		(a)[6] = (u8) (((u64) (val)) >> 8);	\
		(a)[7] = (u8) (((u64) (val)) & 0xff);	\
	} while (0)


#ifndef ETH_ALEN
#define ETH_ALEN 6
#endif



int hwaddr_aton(const char *txt, u8 *addr);
int hexstr2bin(const char *hex, u8 *buf, size_t len);
int bin2hexstr(const u8 *hex, size_t hexlen, char *buf, size_t buflen);
void inc_byte_array(u8 *counter, size_t len);
void wpa_get_ntp_timestamp(u8 *buf);


#ifdef __GNUC__
#define PRINTF_FORMAT(a,b) __attribute__ ((format (printf, (a), (b))))
#define STRUCT_PACKED __attribute__ ((packed))
#else
#define PRINTF_FORMAT(a,b)
#define STRUCT_PACKED
#endif


/* Debugging function - conditional printf and hex dump. Driver wrappers can
 * use these for debugging purposes. */

enum { MSG_MSGDUMP, MSG_DEBUG, MSG_INFO, MSG_WARNING, MSG_ERROR, MSG_EVENT };

#define MSG_DCB MSG_EVENT

#ifdef CONFIG_NO_STDOUT_DEBUG

#define wpa_debug_print_timestamp() do { } while (0)
#define wpa_printf(args...) do { } while (0)
#define wpa_hexdump(l,t,b,le) do { } while (0)
#define wpa_hexdump_key(l,t,b,le) do { } while (0)
#define wpa_hexdump_ascii(l,t,b,le) do { } while (0)
#define wpa_hexdump_ascii_key(l,t,b,le) do { } while (0)
#define wpa_debug_open_file(void) do { } while (0)
#define wpa_debug_close_file(void) do { } while (0)

#else /* CONFIG_NO_STDOUT_DEBUG */

int wpa_debug_open_file(void);
void wpa_debug_close_file(void);

/**
 * wpa_debug_printf_timestamp - Print timestamp for debug output
 *
 * This function prints a timestamp in <seconds from 1970>.<microsoconds>
 * format if debug output has been configured to include timestamps in debug
 * messages.
 */
void wpa_debug_print_timestamp(void);

/**
 * wpa_printf - conditional printf
 * @level: priority level (MSG_*) of the message
 * @fmt: printf format string, followed by optional arguments
 *
 * This function is used to print conditional debugging and error messages. The
 * output may be directed to stdout, stderr, and/or syslog based on
 * configuration.
 *
 * Note: New line '\n' is added to the end of the text when printing to stdout.
 */
void wpa_printf(int level, char *fmt, ...)
PRINTF_FORMAT(2, 3);

/**
 * wpa_hexdump - conditional hex dump
 * @level: priority level (MSG_*) of the message
 * @title: title of for the message
 * @buf: data buffer to be dumped
 * @len: length of the buf
 *
 * This function is used to print conditional debugging and error messages. The
 * output may be directed to stdout, stderr, and/or syslog based on
 * configuration. The contents of buf is printed out has hex dump.
 */
void wpa_hexdump(int level, const char *title, const u8 *buf, size_t len);

/**
 * wpa_hexdump_key - conditional hex dump, hide keys
 * @level: priority level (MSG_*) of the message
 * @title: title of for the message
 * @buf: data buffer to be dumped
 * @len: length of the buf
 *
 * This function is used to print conditional debugging and error messages. The
 * output may be directed to stdout, stderr, and/or syslog based on
 * configuration. The contents of buf is printed out has hex dump. This works
 * like wpa_hexdump(), but by default, does not include secret keys (passwords,
 * etc.) in debug output.
 */
void wpa_hexdump_key(int level, const char *title, const u8 *buf, size_t len);

/**
 * wpa_hexdump_ascii - conditional hex dump
 * @level: priority level (MSG_*) of the message
 * @title: title of for the message
 * @buf: data buffer to be dumped
 * @len: length of the buf
 *
 * This function is used to print conditional debugging and error messages. The
 * output may be directed to stdout, stderr, and/or syslog based on
 * configuration. The contents of buf is printed out has hex dump with both
 * the hex numbers and ASCII characters (for printable range) are shown. 16
 * bytes per line will be shown.
 */
void wpa_hexdump_ascii(int level, const char *title, const u8 *buf,
		       size_t len);

/**
 * wpa_hexdump_ascii_key - conditional hex dump, hide keys
 * @level: priority level (MSG_*) of the message
 * @title: title of for the message
 * @buf: data buffer to be dumped
 * @len: length of the buf
 *
 * This function is used to print conditional debugging and error messages. The
 * output may be directed to stdout, stderr, and/or syslog based on
 * configuration. The contents of buf is printed out has hex dump with both
 * the hex numbers and ASCII characters (for printable range) are shown. 16
 * bytes per line will be shown. This works like wpa_hexdump_ascii(), but by
 * default, does not include secret keys (passwords, etc.) in debug output.
 */
void wpa_hexdump_ascii_key(int level, const char *title, const u8 *buf,
			   size_t len);

#endif /* CONFIG_NO_STDOUT_DEBUG */


#define wpa_msg(args...) do { } while (0)
#define wpa_msg_register_cb(f) do { } while (0)


int wpa_snprintf_hex(char *buf, size_t buf_size, const u8 *data, size_t len);
int wpa_snprintf_hex_uppercase(char *buf, size_t buf_size, const u8 *data,
			       size_t len);


#define WPA_ASSERT(a) do { } while (0)




#ifdef CONFIG_ANSI_C_EXTRA

#if !defined(_MSC_VER) || _MSC_VER < 1400
/* snprintf - used in number of places; sprintf() is _not_ a good replacement
 * due to possible buffer overflow; see, e.g.,
 * http://www.ijs.si/software/snprintf/ for portable implementation of
 * snprintf. */
int snprintf(char *str, size_t size, const char *format, ...);

/* vsnprintf - only used for wpa_msg() in wpa_supplicant.c */
int vsnprintf(char *str, size_t size, const char *format, va_list ap);
#endif /* !defined(_MSC_VER) || _MSC_VER < 1400 */

/* getopt - only used in main.c */
int getopt(int argc, char *const argv[], const char *optstring);
extern char *optarg;
extern int optind;

#ifndef CONFIG_NO_SOCKLEN_T_TYPEDEF
#ifndef __socklen_t_defined
typedef int socklen_t;
#endif
#endif

/* inline - define as __inline or just define it to be empty, if needed */
#ifdef CONFIG_NO_INLINE
#define inline
#else
#define inline __inline
#endif

#ifndef __func__
#define __func__ "__func__ not defined"
#endif

#ifndef bswap_16
#define bswap_16(a) ((((u16) (a) << 8) & 0xff00) | (((u16) (a) >> 8) & 0xff))
#endif

#ifndef bswap_32
#define bswap_32(a) ((((u32) (a) << 24) & 0xff000000) | \
		     (((u32) (a) << 8) & 0xff0000) | \
     		     (((u32) (a) >> 8) & 0xff00) | \
     		     (((u32) (a) >> 24) & 0xff))
#endif

#ifndef MSG_DONTWAIT
#define MSG_DONTWAIT 0
#endif


#endif /* CONFIG_ANSI_C_EXTRA */

#define wpa_zalloc(s) os_zalloc((s))

#define wpa_unicode2ascii_inplace(s) do { } while (0)
#define wpa_strdup_tchar(s) strdup((s))

const char * wpa_ssid_txt(u8 *ssid, size_t ssid_len);

#endif /* COMMON_H */
