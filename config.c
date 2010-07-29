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
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/sockios.h>
#include <net/if.h>
#include "common.h"
#include "eloop.h"
#include "lldpad.h"
#include "lldp.h"
#include "lldp_util.h"
#include "lldp_mod.h"
#include "lldp_mand_clif.h"
#include "dcb_protocol.h"
#include "dcb_persist_store.h"
#include "messages.h"
#include "libconfig.h"
#include "drv_cfg.h"
#include "config.h"
#include "clif_msgs.h"
#include "lldp/l2_packet.h"

config_t lldpad_cfg;
static int cfg_loaded = false;

/*
 * init_cfg - initialze the global lldpad_cfg via config_init
 *
 * Returns true (1) for succes and false (0) for failed
 *
 * check to see if lldpad_cfs is already initailzied
 */
int init_cfg(void)
{
	if (cfg_loaded == false) {
		config_init(&lldpad_cfg);
		if (!config_read_file(&lldpad_cfg, cfg_file_name))
			printf("config file failed to load\n");
		else
			cfg_loaded = true;
	} else {
		if (check_cfg_file() ||
			(!config_read_file(&lldpad_cfg, cfg_file_name)))
			cfg_loaded = false;
	}
	return cfg_loaded;
}

/*
 * destroy_cfg - destroy the initialzed global lldpad_cfg
 *
 * Sets the cfgReturn true (1) for succes and false (0) for failed
 *
 * check to see if lldpad_cfg is already initialized
 */
void destroy_cfg(void)
{
	if (cfg_loaded == true) {
		config_destroy(&lldpad_cfg);
		cfg_loaded = false;
	}
}

void scan_port(void *eloop_data, void *user_ctx)
{
	struct port *port;
	struct if_nameindex *nameidx, *p;
	struct lldp_module *np;
	const struct lldp_mod_ops *ops;
	int err;

	printf("%s: NLMSG dropped, scan ports.\n", __func__);

	nameidx = if_nameindex();
	if (nameidx == NULL) {
		TRACE("if_nameindex error try again later\n");
		goto error_out;
	}
	p = nameidx;

	if (!init_cfg())
		goto err_out;

	/* Walk port list looking for devices that are not in if_nameindex.
	 * If the device is in the port list but not in the if_nameindex
	 * list then we missed a RTM_DELLINK event and the device is no
	 * longer available, possibly because the module has been unloaded.
	 * For this case lets remove the device from the ports list if it
	 * comes back online we should receive a RTM_NEWLINK event and can
	 * readd it there.
	 */
	port = porthead;
	while (port != NULL) {
		int found = 0;
		struct port *del;
		p = nameidx;
		while (p->if_index != 0) {
			if (!strncmp(p->if_name, port->ifname,
				     MAX_DEVICE_NAME_LEN)) {
				/* Good device exists continue port walk */
				found = 1;
				break;
			}
			p++;
		}
		del = port;
		port = port->next;
		if (!found)
			remove_adapter(del->ifname);
	}

	/* Walk port list looking for devices that should have been added
	 * to our port list but have not most likely due to a dropped nlmsg.
	 * At this point we need to add the device and call ops ifup routines.
	 * Additional we call set_hw_all() for all devices already in the port
	 * list this should force the hw settings to get update if needed.
	 * This is required because we currently do not know if we missed
	 * IF_OPER_UP or IF_OPER_DORMANT.  And an extra set_hw_all()
	 * shouldn't hurt anything.
	 */
	p = nameidx;
	while (p->if_index != 0) {
		if (is_valid_lldp_device(p->if_name) &&
		    check_link_status(p->if_name) > 0) {
			oper_add_device(p->if_name);
			set_hw_all(p->if_name);
		}
		p++;
	}

err_out:
	if_freenameindex(nameidx);
	return;
error_out:
	eloop_register_timeout(INI_TIMER, 0, scan_port, NULL, NULL);
	return;
}

void create_default_cfg_file(void)
{
	if (cfg_loaded || init_cfg())
		config_write_file(&lldpad_cfg, cfg_file_name);
}

/* check for existence of cfg file.  If it does not exist,
 * create it.
 * input:  cfg_file:  if NULL, use default cfg file name
 *                    else, use passed in name
 * output: 0:         no error
 *         !0:        errno of the call which failed
*/
int check_cfg_file(void)
{
	int fd;
	int retval = 0;

	if (access(cfg_file_name, R_OK | W_OK)) {
		if (access(cfg_file_name, F_OK)) {
			printf("config file failed to load, ");
			printf("create a new file.\n");
			fd = open(cfg_file_name,
				O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
			if (fd < 0) {
				retval = errno;
				log_message(MSG_ERR_SERVICE_START_FAILURE,
					"error creating %s", cfg_file_name);
			} else {
				close(fd);
				create_default_cfg_file();
			}
		} else {
			retval = errno;
			log_message(MSG_ERR_SERVICE_START_FAILURE,
				"%s is not readable and writeable",
				cfg_file_name);
		}
	}

	return (retval);
}

static bool  check_bool(int bool_setting)
{
	if (bool_setting ==1 || bool_setting==0)
		return true;
	else
		return false;
}

static bool check_percentage(int percentage_setting)
{
	if (percentage_setting < 0 ||percentage_setting > 100)
		return false;
	else
		return true;
}

static bool check_char(int int_setting)
{
	if (int_setting < 0 || int_setting > 255)
		return false;
	else
		return true;
}

static bool check_int(int int_setting)
{
	if (int_setting < 0 || int_setting > 1000000)
		return false;
	else
		return true;
}

static bool check_priority(int priority_setting)
{
	if (priority_setting < dcb_none || priority_setting >= dcb_invalid)
		return false;
	else
		return true;
}

static bool check_double(double double_setting)
{
	if (double_setting < 0 )
		return false;
	else
		return true;
}

int get_int_config(config_setting_t *s, char *attr, int int_type,
				int *result)
{
	char buf[64];
	config_setting_t *setting = NULL;
	int rval = false;

	setting = config_setting_get_member(s, attr);
	if (setting) {
		*result = (int)config_setting_get_int(setting);
		switch(int_type) {
		case TYPE_BOOL:
			rval = check_bool(*result);
			break;
		case TYPE_PERCENT:
			rval = check_percentage(*result);
			break;
		case TYPE_CHAR:
			rval = check_char(*result);
			break;
		case TYPE_INT:
			rval = check_int(*result);
			break;
		default:
			break;
		}
	}

	if (!rval) {
		snprintf(buf, sizeof(buf), "invalid value for %s", attr);
		log_message(MSG_ERR_DCB_INVALID_CONFIG_FILE, "%s", buf);
	}

	return rval;
}

int get_array_config(config_setting_t *s, char *attr, int int_type,
			int *result)
{
	char buf[64];
	config_setting_t *setting = NULL;
	config_setting_t *setting_value = NULL;
	int rval = false;
	int i;

	setting = config_setting_get_member(s, attr);

	if (setting) {
		for (i = 0; i<config_setting_length(setting); i++) {
			setting_value = config_setting_get_elem(setting, i);
			*(result+i) =
				(char)config_setting_get_int(setting_value);

			switch(int_type) {
			case TYPE_BOOL:
				rval = check_bool(*(result+i));
				break;
			case TYPE_PERCENT:
				rval = check_percentage(*(result+i));
				break;
			case TYPE_CHAR:
				rval = check_char(*(result+i));
				break;
			case TYPE_INT:
				rval = check_int(*(result+i));
				break;
			case TYPE_PRIORITY:
				rval = check_priority(*(result+i));
				break;
			default:
				break;
			}
		}
	}

	if (!rval) {
		snprintf(buf, sizeof(buf), "invalid setting for %s", attr);
		log_message(MSG_ERR_DCB_INVALID_CONFIG_FILE, "%s", buf);
	}

	return rval;
}

void init_ports(void)
{
	struct lldp_module *np;
	struct if_nameindex *nameidx, *p;

	nameidx = if_nameindex();
	if (nameidx == NULL) {
		TRACE("error calling if_nameindex()\n")
		return;
	}

	p = nameidx;
	while (p->if_index != 0) {
		if (!init_cfg())
			break;

		if (is_loopback(p->if_name)) {
			;
		} else if (is_bond(p->if_name)) {
			if (add_bond_port(p->if_name) < 0)
				syslog(LOG_ERR, "failed to register port %s",
					p->if_name);
		} else if (is_vlan(p->if_name)) {
			;
		} else if (is_bridge(p->if_name)) {
			; /* ignore bridge device */
		} else if (check_link_status(p->if_name)) {
			add_adapter(p->if_name);

			LIST_FOREACH(np, &lldp_head, lldp)
				np->ops->lldp_mod_ifup(p->if_name);
			set_lldp_port_enable_state(p->if_name, 1);
		}
		p++;
	}

	if_freenameindex(nameidx);
}

static int set_config_value(config_setting_t *setting, void *value, int type)
{
	switch (type) {
	case CONFIG_TYPE_INT:
		return config_setting_set_int(setting, *((long *)value));
	case CONFIG_TYPE_INT64:
		return config_setting_set_int64(setting, *((long long *)value));
	case CONFIG_TYPE_FLOAT:
		return config_setting_set_float(setting, *((double *)value));
	case CONFIG_TYPE_STRING:
		return config_setting_set_string(setting,
						*((const char **)value));
	case CONFIG_TYPE_BOOL:
		return config_setting_set_bool(setting, *((int *)value));
	default:
		return CONFIG_FALSE;
	}
}

static int lookup_config_value(char *path, void *value, int type)
{
	switch (type) {
	case CONFIG_TYPE_INT:
		return config_lookup_int(&lldpad_cfg, path, (long *)value);
	case CONFIG_TYPE_INT64:
		return config_lookup_int64(&lldpad_cfg, path, (long long *)value);
	case CONFIG_TYPE_FLOAT:
		return config_lookup_float(&lldpad_cfg, path, (double *)value);
	case CONFIG_TYPE_STRING:
		return config_lookup_string(&lldpad_cfg, path,
					   (const char **)value);
	case CONFIG_TYPE_BOOL:
		return config_lookup_bool(&lldpad_cfg, path, (int *)value);
	default:
		return CONFIG_FALSE;
	}
}

/*
 * get_config_setting - get the setting from the given config file path by type
 * @ifname: interface name
 * @path: relative to LLDP_COMMON or ifname section of LLDP configuration.
 * @value: pointer to the value to be retrieved
 * @type: libconfig value types
 *
 * Returns cmd_success(0) for success, otherwise for failure.
 *
 * This function assumes init_cfg() has been called.
 */
int get_config_setting(const char *ifname, char *path, void *value, int type)
{
	char p[1024];
	int rval = CONFIG_FALSE;

	/* look for setting in ifname area first */
	if (ifname) {
		snprintf(p, sizeof(p), "%s.%s.%s",
			 LLDP_SETTING, ifname, path);
		rval = lookup_config_value(p, value, type);
	}

	/* if not found look for setting in common area */
	if (rval == CONFIG_FALSE) {
		snprintf(p, sizeof(p), "%s.%s.%s",
			 LLDP_SETTING, LLDP_COMMON, path);
		rval = lookup_config_value(p, value, type);
	}
	return (rval == CONFIG_FALSE) ? cmd_failed : cmd_success;
}

/* calling get_config_setting() w/ init_cfg()/destroy_cfg() */
int get_cfg(const char *ifname, char *path, void *value, int type)
{
	int rval = cmd_failed;

	if (init_cfg()) {
		rval = get_config_setting(ifname, path, value, type);
	}
	return rval;
}

/* must be initially invoked with a scalar or array type
 * drills down path until it finds a part that exists (or not) and then
 * creates the settings on the way back up
 */
config_setting_t *find_or_create_setting(char *p, int type)
{
	config_setting_t *setting = NULL;
	char *s;
	int i;

	setting = config_lookup(&lldpad_cfg, p);

	/* if setting does not exist, then need to create it */
	if (setting == NULL) {
		for (i = strlen(p); i > 0; i--)
			if (*(p+i) == '.')
				break;
		if (i) {
			*(p+i) = '\0';
			setting = find_or_create_setting(p, CONFIG_TYPE_GROUP);
			*(p+i) = '.';
		} else {
			setting = config_root_setting(&lldpad_cfg);
		}

		if (setting) {
			for (i = strlen(p); i > 0; i--)
				if (*(p+i) == '.')
					break;
			if (i)
				s = p+i+1;
			else
				s = p;
			return config_setting_add(setting, s, type);
		}
	}

	return setting;
}

/*
 * set_config_setting - set the setting to the given config file path by type
 * @ifname: interface name
 * @path: relative to LLDP_COMMON or ifname section of LLDP configuration.
 * @value: pointer to the value to be retrieved
 * @type: libconfig value types
 *
 * Returns cmd_success(0) for success, otherwise for failure.
 *
 * This function assumes init_cfg() has been called.
 */
int set_config_setting(const char *ifname, char *path, void *value, int type)
{
	config_setting_t *setting = NULL;
	char p[1024];
	int rval = cmd_success;

	if (strlen(ifname))
		snprintf(p, sizeof(p), "%s.%s.%s",
			 LLDP_SETTING, ifname, path);
	else
		snprintf(p, sizeof(p), "%s.%s.%s",
			 LLDP_SETTING, LLDP_COMMON, path);
	setting = find_or_create_setting(p, type);

	if (setting) {
		if (!set_config_value(setting, value, type)) {
			rval = cmd_failed;
		} else if (!config_write_file(&lldpad_cfg, cfg_file_name)) {
			printf("config write failed\n");
			rval = cmd_failed;
		}
	}
	return rval;
}

int set_cfg(const char *ifname, char *path, void *value, int type)
{
	int rval = cmd_failed;
	if (init_cfg()) {
		rval = set_config_setting(ifname, path, value, type);
	}
	return rval;
}

/* get_config_tlvfield - read one field from the tlvid
 * @ifname: the port name
 * @tlvid: oui + subtype
 * @field: name of the field to query
 * @value: output buffer
 * @size: size in bytes in the output buffer
 * @type: value type
 *
 * Returns 0 on success and -1 on failure
 *
 * Note: must have called init_cfg() before calling this.
 */
int get_config_tlvfield(const char *ifname, u32 tlvid, const char *field, void *val, int type)
{
	int rc;
	char path[256];

	memset(path, 0, sizeof(path));
	rc = snprintf(path, sizeof(path), "tlvid%08x.%s", tlvid, field);
	if (get_config_setting(ifname, path, val, type)) {
		rc = EIO;
		goto out_err;
	}
	rc = 0;

out_err:
	return rc;
}

/* libconfig uses long for int */
int get_config_tlvfield_int(const char *ifname, u32 tlvid, const char *field, int *value)
{
	int rc = 0;
	long int val = (long int) *value;

	rc = get_config_tlvfield(ifname, tlvid, field, &val, CONFIG_TYPE_INT);
	*value = (int) val;
	return rc;
}

int get_config_tlvfield_bool(const char *ifname, u32 tlvid, const char *field, int *value)
{
	return get_config_tlvfield(ifname, tlvid, field, value, CONFIG_TYPE_BOOL);
}

int get_config_tlvfield_bin(const char *ifname, u32 tlvid, const char *field, void *value, size_t size)
{
	int rc = 0;
	char *str = NULL;

	rc = get_config_tlvfield(ifname, tlvid, field, (void *)&str, CONFIG_TYPE_STRING);
	if ((rc == 0) && (str != NULL))
		rc = hexstr2bin(str, value, size);
	return rc;
}

int get_config_tlvfield_str(const char *ifname, u32 tlvid, const char *field, void *value, size_t size)
{
	int rc = 0;
	char *str = NULL;

	rc = get_config_tlvfield(ifname, tlvid, field, (void *)&str, CONFIG_TYPE_STRING);
	if ((rc == 0) && (str != NULL))
		strncpy(value, str, size);
	return rc;
}

int get_config_tlvinfo_bin(const char *ifname, u32 tlvid, void *value, size_t size)
{
	return	get_config_tlvfield_bin(ifname, tlvid, ARG_TLVINFO, value, size);
}

int get_config_tlvinfo_str(const char *ifname, u32 tlvid, void *value, size_t size)
{
	return	get_config_tlvfield_str(ifname, tlvid, ARG_TLVINFO, value, size);
}

int set_config_tlvfield(const char *ifname, u32 tlvid, const char *field, void *val, int type)
{
	char path[256];

	memset(path, 0, sizeof(path));
	snprintf(path, sizeof(path), "tlvid%08x.%s", tlvid, field);
	return set_config_setting(ifname, path, val, type);
}

int set_config_tlvfield_str(const char *ifname, u32 tlvid, const char *field, void *value, size_t size)
{
	int rc = EINVAL;
	char *str = NULL;

	if (value) {
		str = (char *)value;
		rc = set_config_tlvfield(ifname, tlvid, field, (void *)&str, CONFIG_TYPE_STRING);
	}
	return rc;
}

int set_config_tlvfield_bin(const char *ifname, u32 tlvid, const char *field, void *value, size_t size)
{
	int rc = EINVAL;
	char *str = NULL;
	size_t bsize = 0;

	if (!value)
		goto out_err;

	/* 2 char per byte + '\0' */
	bsize = size * 2 + 1;
	str = malloc(bsize);
	if (!str)
		goto out_err;
	memset(str, 0, bsize);
	rc = bin2hexstr((u8 *)value, size, str, bsize);
	if (rc)
		goto out_free;
	str[bsize - 1] = '\0';
	rc = set_config_tlvfield(ifname, tlvid, field, (void *)&str, CONFIG_TYPE_STRING);
out_free:
	free(str);
out_err:
	return rc;
}

int set_config_tlvinfo_bin(const char *ifname, u32 tlvid, void *value, size_t size)
{
	return	set_config_tlvfield_bin(ifname, tlvid, "info", value, size);
}

int set_config_tlvinfo_str(const char *ifname, u32 tlvid, void *value, size_t size)
{
	return	set_config_tlvfield_str(ifname, tlvid, "info", value, size);
}

int set_config_tlvfield_int(const char *ifname, u32 tlvid, const char *field, int *value)
{
	long int val = (long int )*value;
	return set_config_tlvfield(ifname, tlvid, field, &val, CONFIG_TYPE_INT);
}

int set_config_tlvfield_bool(const char *ifname, u32 tlvid, const char *field, int *value)
{
	return set_config_tlvfield(ifname, tlvid, field, value, CONFIG_TYPE_BOOL);
}

int is_tlv_txenabled(const char *ifname, u32 tlvid)
{
	char arg[64];
	int enabletx = false;

	snprintf(arg, sizeof(arg), "%s%08x.%s", TLVID_PREFIX,
		 tlvid, ARG_TLVTXENABLE);

	get_config_setting(ifname, arg, (void *)&enabletx, CONFIG_TYPE_BOOL);

	return enabletx;
}

int tlv_enabletx(const char *ifname, u32 tlvid)
{
	int enabletx = true;
	return set_config_tlvfield_bool(ifname, tlvid,
					ARG_TLVTXENABLE, (void *)&enabletx);
}

int tlv_disabletx(const char *ifname, u32 tlvid)
{
	int enabletx = false;
	return set_config_tlvfield_bool(ifname, tlvid,
					ARG_TLVTXENABLE, (void *)&enabletx);
}

void set_med_devtype(const char *ifname, int devtype)
{
	if (LLDP_MED_DEVTYPE_INVALID(devtype))
		return;
	set_config_tlvfield_int(ifname, TLVID_MED(LLDP_MED_RESERVED),
				"devtype", &devtype);
}

int get_med_devtype(const char *ifname)
{
	int devtype;

	get_config_tlvfield_int(ifname, TLVID_MED(LLDP_MED_RESERVED),
				"devtype", &devtype);
	return devtype;
}
