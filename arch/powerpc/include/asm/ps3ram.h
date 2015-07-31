
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

#ifndef _ASM_POWERPC_PS3RAM_H_
#define _ASM_POWERPC_PS3RAM_H_

#include <linux/types.h>
#include <linux/ioctl.h>

#define PS3RAM_IOCTL_USER_TO_LPAR_ADDR		_IOWR('r', 0, int) /* user to lpar address */

struct ps3ram_ioctl_user_to_lpar_addr {
	__u64 user_addr;
	__u64 lpar_addr;
};

#endif /* _ASM_POWERPC_PS3RAM_H_ */
