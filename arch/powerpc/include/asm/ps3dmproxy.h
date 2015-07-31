
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

#ifndef _ASM_POWERPC_PS3DMPROXY_H_
#define _ASM_POWERPC_PS3DMPROXY_H_

#include <linux/types.h>
#include <linux/ioctl.h>

#define PS3DMPROXY_IOCTL_USER_TO_LPAR_ADDR	_IOWR('d', 0, int) /* user to lpar address */
#define PS3DMPROXY_IOCTL_GET_REPO_NODE_VAL	_IOWR('d', 1, int) /* get repository node value */
#define PS3DMPROXY_IOCTL_DO_REQUEST		_IOWR('d', 2, int) /* send a request and receive a response */

struct ps3dmproxy_ioctl_user_to_lpar_addr {
	__u64 user_addr;
	__u64 lpar_addr;
};

struct ps3dmproxy_ioctl_get_repo_node_val {
	__u64 lpar_id;
	__u64 key[4];
	__u64 val[2];
};

struct ps3dmproxy_ioctl_do_request {
	__u64 sendbuf;
	__u64 sendbuf_size;
	__u64 recvbuf;
	__u64 recvbuf_size;
};

#endif /* _ASM_POWERPC_PS3DMPROXY_H_ */
