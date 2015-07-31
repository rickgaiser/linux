
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

#ifndef _ASM_POWERPC_PS3ENCDEC_H_
#define _ASM_POWERPC_PS3ENCDEC_H_

#include <linux/types.h>
#include <linux/ioctl.h>

#define PS3ENCDEC_IOCTL_DO_REQUEST		_IOWR('e', 0, int) /* send a request and receive a response */

struct ps3encdec_ioctl_do_request {
	__u64 cmd;
	__u64 cmdbuf;
	__u64 cmdbuf_size;
	__u64 respbuf;
	__u64 respbuf_size;
};

#endif /* _ASM_POWERPC_PS3ENCDEC_H_ */
