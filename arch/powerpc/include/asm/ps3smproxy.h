
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

#ifndef _ASM_POWERPC_PS3SMPROXY_H_
#define _ASM_POWERPC_PS3SMPROXY_H_

#include <linux/types.h>
#include <linux/ioctl.h>

#define PS3SMPROXY_IOCTL_DO_REQUEST		_IOWR('s', 0, int) /* send a request and receive a response */

struct ps3smproxy_ioctl_do_request {
	__u64 sendbuf;
	__u64 sendbuf_size;
	__u64 recvbuf;
	__u64 recvbuf_size;
};

#endif /* _ASM_POWERPC_PS3SMPROXY_H_ */
