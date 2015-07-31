
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

#ifndef _ASM_POWERPC_PS3HVC_H_
#define _ASM_POWERPC_PS3HVC_H_

#include <linux/types.h>
#include <linux/ioctl.h>

#define PS3HVC_HVCALL_UNDOCUMENTED_FUNCTION_8			8
#define PS3HVC_HVCALL_CONNECT_IRQ_PLUG_EXT			12
#define PS3HVC_HVCALL_DISCONNECT_IRQ_PLUG_EXT			17
#define PS3HVC_HVCALL_CONSTRUCT_EVENT_RECEIVE_PORT		18
#define PS3HVC_HVCALL_DESTRUCT_EVENT_RECEIVE_PORT		19
#define PS3HVC_HVCALL_SHUTDOWN_LOGICAL_PARTITION		44
#define PS3HVC_HVCALL_GET_LPAR_ID				74
#define PS3HVC_HVCALL_CREATE_REPO_NODE				90
#define PS3HVC_HVCALL_GET_REPO_NODE_VAL				91
#define PS3HVC_HVCALL_MODIFY_REPO_NODE_VAL			92
#define PS3HVC_HVCALL_RM_REPO_NODE				93
#define PS3HVC_HVCALL_SET_DABR					96
#define PS3HVC_HVCALL_UNDOCUMENTED_FUNCTION_102			102
#define PS3HVC_HVCALL_UNDOCUMENTED_FUNCTION_107			107
#define PS3HVC_HVCALL_UNDOCUMENTED_FUNCTION_109			109
#define PS3HVC_HVCALL_GET_VERSION_INFO				127
#define PS3HVC_HVCALL_UNDOCUMENTED_FUNCTION_182			182
#define PS3HVC_HVCALL_UNDOCUMENTED_FUNCTION_183			183
#define PS3HVC_HVCALL_NET_CONTROL				194
#define PS3HVC_HVCALL_UNDOCUMENTED_FUNCTION_231			231
#define PS3HVC_HVCALL_GET_RTC					232
#define PS3HVC_HVCALL_PANIC					255

#define PS3HVC_IOCTL_HVCALL					_IOWR('h', 0, int) /* hvcall */

struct ps3hvc_ioctl_hvcall {
	__u64 number;
	__s64 result;
	__u64 in_args;
	__u64 out_args;
	__u64 args[0];
};

#endif /* _ASM_POWERPC_PS3HVC_H_ */
