/**
 * @file   maygion-mips.hpp
 * @brief  Support code for MayGion MIPS IP cameras.
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

#ifndef MAYGION_MIPS_HPP
#define MAYGION_MIPS_HPP

#include "network.hpp"
#include "device-interface.hpp"

class maygion_mips: virtual public Device
{
	public:
		maygion_mips(Network *network);
		virtual ~maygion_mips();

		virtual void getFirmware(std::ostream& target, fn_progress fnProgress);
		virtual void getFlashInfo(unsigned long *length);
		virtual void getCameraInfo(unsigned short *idVendor,
			unsigned short *idProduct, unsigned char *bInterfaceClass);

	private:
		Network *network;
};

#endif // MAYGION_MIPS_HPP
