
/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published
 * by the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _ASM_POWERPC_PS3STORMGR_H_
#define _ASM_POWERPC_PS3STORMGR_H_

#include <linux/types.h>
#include <linux/ioctl.h>

#define PS3STORMGR_IOCTL_CREATE_REGION		_IOWR('s', 0, int) /* create region */
#define PS3STORMGR_IOCTL_DELETE_REGION		_IOW('s', 1, int) /* delete region */
#define PS3STORMGR_IOCTL_SET_REGION_ACL		_IOW('s', 2, int) /* set region ACL */
#define PS3STORMGR_IOCTL_GET_REGION_ACL		_IOWR('s', 3, int) /* get region ACL */

struct ps3stormgr_ioctl_create_region {
	__u64 dev_id;
	__u64 start_sector;
	__u64 sector_count;
	__u64 laid;
	__u64 region_id;
};

struct ps3stormgr_ioctl_delete_region {
	__u64 dev_id;
	__u64 region_id;
};

struct ps3stormgr_ioctl_set_region_acl {
	__u64 dev_id;
	__u64 region_id;
	__u64 laid;
	__u64 access_rights;
};

struct ps3stormgr_ioctl_get_region_acl {
	__u64 dev_id;
	__u64 region_id;
	__u64 entry_index;
	__u64 laid;
	__u64 access_rights;
};

#endif /* _ASM_POWERPC_PS3STORMGR_H_ */
