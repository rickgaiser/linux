
/*
 *  PS3 DM backend support.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _ASM_POWERPC_PS3DM_H_
#define _ASM_POWERPC_PS3DM_H_

struct ps3dm_hdr {
	u32 request_id;
	u32 function_id;
	u32 request_size;
	u32 response_size;
};

#define PS3DM_HDR_SIZE		sizeof(struct ps3dm_hdr)

extern int ps3dm_write(const void *buf, unsigned int size);

extern int ps3dm_read(void *buf, unsigned int size);

extern int ps3dm_do_request(struct ps3dm_hdr *sendbuf, unsigned int sendbuf_size,
	struct ps3dm_hdr *recvbuf, unsigned int recvbuf_size);

#endif	/* _ASM_POWERPC_PS3DM_H_ */
