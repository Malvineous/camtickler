/**
 * @file   device-interface.hpp
 * @brief  Interface class for device access.
 *
 * Copyright (C) 2013 Adam Nielsen <malvineous@shikadi.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef DEVICE_HPP
#define DEVICE_HPP

#include <iostream>
#include <boost/function.hpp>

/// Callback function for reporting progress.
/**
 * First param is amount read in bytes, second param is total size to read, in
 * bytes.
 */
typedef boost::function<void(unsigned long, unsigned long)> fn_progress;

class Device
{
	public:
		/// Download the device's firmware.
		/**
		 * @param target
		 *   On return, data will have been written to this stream.
		 *
		 * @param fnProgress
		 *   Callback function for displaying the download progress.
		 *
		 * @throw std::string on error, content is error message.
		 */
		virtual void getFirmware(std::ostream& target, fn_progress fnProgress) = 0;

		/// Get information about the device's flash.
		/**
		 * @param length
		 *   On return, size of flash in bytes.
		 *
		 * @throw std::string on error, content is error message.
		 */
		virtual void getFlashInfo(unsigned long *length) = 0;

		/// Get the USB device IDs for the camera device.
		/**
		 * @param idVendor
		 *   On return, contains the vendor ID.
		 *
		 * @param idProduct
		 *   On return, contains the product ID.
		 *
		 * @param bInterfaceClass
		 *   On return, contains the device's USB class ID.
		 */
		virtual void getCameraInfo(unsigned short *idVendor,
			unsigned short *idProduct, unsigned char *bInterfaceClass) = 0;
};

#endif // DEVICE_HPP
