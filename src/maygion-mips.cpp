/**
 * @file   maygion-mips.cpp
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

#include <boost/bind.hpp>
#include "main.hpp"
#include "maygion-mips.hpp"

maygion_mips::maygion_mips(Network *network)
	: network(network)
{
}

maygion_mips::~maygion_mips()
{
}

void showProgress(unsigned long realTotal, fn_progress fnProgress,
	unsigned long amount, unsigned long total)
{
	if (total != (unsigned long)-1) total = realTotal;
	fnProgress(amount, total);
	return;
}

void maygion_mips::getFirmware(std::ostream& target, fn_progress fnProgress)
{
	if (!this->network->ftp_login("MayGion", "maygion.com")) {
		throw std::string("Unable to log in to device via FTP.");
	}
	unsigned long lenFlash = 0;
	this->getFlashInfo(&lenFlash);
	fn_progress fnFixedSizeProgress = boost::bind(showProgress, lenFlash, fnProgress, _1, _2);
	this->network->ftp_get(target, "/dev", "mtdblock0", fnFixedSizeProgress);
	return;
}

void maygion_mips::getFlashInfo(unsigned long *length)
{
	boost::asio::io_service tcp_service;
	boost::shared_ptr<boost::asio::ip::tcp::socket> telnet
		= this->network->tcp_connect("telnet", &tcp_service);

	boost::asio::streambuf response;
	std::istream response_stream(&response);
	boost::asio::streambuf request;
	std::ostream request_stream(&request);

	size_t read;
	if (verbose > 1) std::cerr << "Waiting for prompt..." << std::flush;
	read = boost::asio::read_until(*telnet, response, "# ");
	response.consume(read);
	if (verbose > 1) std::cerr << "ok.\n";


	if (verbose > 1) std::cerr << "Sending cat command" << std::endl;
	request_stream << "cat /proc/mtd\r\n";
	boost::asio::write(*telnet, request);

	// Read back what we just typed as that's just before the content
	if (verbose > 1) std::cerr << "Waiting for ack..." << std::flush;
	read = boost::asio::read_until(*telnet, response, "/proc/mtd\r\n");
	response.consume(read);
	if (verbose > 1) std::cerr << "ok.\nChecking result..." << std::flush;

	boost::asio::read_until(*telnet, response, "# ");
	std::string token;
	response_stream >> token;
	if (token.compare("dev:") != 0) {
		throw std::string("Unable to get MTD info.");
	}
	if (verbose > 1) std::cerr << "ok.\nExamining data..." << std::endl;
	std::getline(response_stream, token);

	response_stream >> token;
	if (token.compare("mtd0:") != 0) {
		throw std::string("mtdblock0 doesn't exist!");
	}
	response_stream >> token;
	*length = strtoul(token.c_str(), NULL, 16);
	if (verbose) std::cerr << "mtd0 size of " << token << " == " << *length
		<< " bytes" << std::endl;
	response.consume(response.size());

	if (verbose > 1) std::cerr << "Done." << std::endl;

	// Logout to avoid lingering shells
	request_stream.write("\x03\x1A", 2);
	boost::asio::write(*telnet, request);

	telnet->close();
	return;
}

void maygion_mips::getCameraInfo(unsigned short *idVendor,
	unsigned short *idProduct, unsigned char *bInterfaceClass)
{
	boost::asio::io_service tcp_service;
	boost::shared_ptr<boost::asio::ip::tcp::socket> telnet
		= this->network->tcp_connect("telnet", &tcp_service);

	boost::asio::streambuf response;
	std::istream response_stream(&response);
	boost::asio::streambuf request;
	std::ostream request_stream(&request);

	size_t read;
	if (verbose > 1) std::cerr << "Waiting for prompt..." << std::flush;
	read = boost::asio::read_until(*telnet, response, "# ");
	response.consume(read);
	if (verbose > 1) std::cerr << "ok.\n";


	if (verbose > 1) std::cerr << "Sending cat command" << std::endl;
	request_stream << "cat /sys/class/video4linux/video0/device/../idVendor ; "
		"cat /sys/class/video4linux/video0/device/../idProduct ; "
		"cat /sys/class/video4linux/video0/device/bInterfaceClass\r\n";
	boost::asio::write(*telnet, request);

	// Read back what we just typed as that's just before the content
	if (verbose > 1) std::cerr << "Waiting for ack..." << std::flush;
	read = boost::asio::read_until(*telnet, response, "Class\r\n");
	response.consume(read);
	if (verbose > 1) std::cerr << "ok.\nChecking result..." << std::flush;

	boost::asio::read_until(*telnet, response, "# ");
	std::string token;
	response_stream >> token;
	*idVendor = strtoul(token.c_str(), NULL, 16);

	response_stream >> token;
	*idProduct = strtoul(token.c_str(), NULL, 16);

	response_stream >> token;
	*bInterfaceClass = strtoul(token.c_str(), NULL, 16);

	response.consume(response.size());

	if (verbose > 1) std::cerr << "Done." << std::endl;

	// Logout to avoid lingering shells
	request_stream.write("\x03\x1A", 2);
	boost::asio::write(*telnet, request);

	telnet->close();
	return;
}
