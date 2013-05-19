/**
 * @file   network.hpp
 * @brief  Common code for TCP/IP access.
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

#ifndef NETWORK_HPP
#define NETWORK_HPP

#include <string>
#include <vector>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include "device-interface.hpp"

class Network {
	public:
		/// Prepare a network connection to the given host.
		/**
		 * @param host
		 *   Hostname or IP address.
		 */
		Network(const std::string& host);

		/// Change the port used for outgoing HTTP connections.
		/**
		 * @param port
		 *   The new port number to use, or 0 for the default.
		 */
		void set_http_port(unsigned short port);

		/// Get the HTTP port in use.
		/**
		 * @return A numeric value for the port currently in use.  Will return the
		 *   actual port number if the default port is in use.
		 */
		unsigned short get_http_port();

		/// Retrieve the HTTP headers from a default query ("/").
		/**
		 * @return A vector of strings, with each string being one header.
		 *   \r\n is trimmed from the end of each line, making them suitable
		 *   for printing or comparison.
		 */
		std::vector<std::string> http_headers();

		/// Download a file over HTTP.
		/**
		 * @param path
		 *   Path to download, e.g. "/index.html".
		 *
		 * @return A string containing the file's content.
		 */
		std::string http_get(const std::string& path);

		boost::shared_ptr<boost::asio::ip::tcp::socket> tcp_connect(
			const std::string& service, boost::asio::io_service *tcp_service);

		bool ftp_login(const std::string& user, const std::string& pass);
		bool ftp_get(std::ostream& target, const std::string& path,
			const std::string& filename, fn_progress fnProgress);
		void ftp_close();

		/// Get the hostname we are connecting to.
		/**
		 * @return The value passed as 'host' to the constructor.
		 */
		const std::string& hostname();

	private:
		const std::string& host;
		unsigned short port_http;
		boost::asio::io_service io_service;
		boost::asio::ip::tcp::resolver::iterator endpoint_iterator_http;

		bool okFTP; // true if FTP is connected
		boost::asio::io_service io_service_ftp;
		boost::asio::ip::tcp::resolver::iterator endpoint_iterator_ftp;
		boost::shared_ptr<boost::asio::ip::tcp::socket> ftp_socket;
};

#endif // NETWORK_HPP
