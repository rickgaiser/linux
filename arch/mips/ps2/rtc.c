/*
 * PS2 time and rtc routines.
 *
 * Copyright 2011 Mega Man
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/platform_device.h>

int __init ps2rtc_init(void)
{
	struct platform_device *pdev;

	/* Register PS2 RTC driver. */
	pdev = platform_device_register_simple("rtc-ps2", -1, NULL, 0);
	if (IS_ERR(pdev))
		return PTR_ERR(pdev);

	return 0;
}
