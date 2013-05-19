/**
 * @file   network.cpp
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

#include "main.hpp"
#include "network.hpp"

Network::Network(const std::string& host)
	: host(host),
	  okFTP(false)
{
	// Get a list of endpoints corresponding to the server name
	boost::asio::ip::tcp::resolver resolver(this->io_service);
	boost::asio::ip::tcp::resolver::query query(host, "http");
	this->endpoint_iterator_http = resolver.resolve(query);
}

std::vector<std::string> Network::http_headers()
{
	std::vector<std::string> headers;

	if (verbose) std::cerr << "Trying to get HTTP headers..." << std::endl;

	// Try each endpoint until we successfully establish a connection
	boost::asio::ip::tcp::socket socket(this->io_service);
	boost::asio::connect(socket, this->endpoint_iterator_http);

	// Form the request. We specify the "Connection: close" header so that the
	// server will close the socket after transmitting the response. This will
	// allow us to treat all data up until the EOF as the content.
	boost::asio::streambuf request;
	std::ostream request_stream(&request);
	request_stream << "GET / HTTP/1.0\r\n";
	request_stream << "Host: " << this->host << "\r\n";
	request_stream << "Accept: */*\r\n";
	request_stream << "Connection: close\r\n\r\n";

	// Send the request
	boost::asio::write(socket, request);

	// Read the response status line. The response streambuf will automatically
	// grow to accommodate the entire line. The growth may be limited by passing
	// a maximum size to the streambuf constructor.
	boost::asio::streambuf response;
	boost::asio::read_until(socket, response, "\r\n");

	// Check that response is OK.
	std::istream response_stream(&response);
	std::string http_version;
	response_stream >> http_version;
	unsigned int status_code;
	response_stream >> status_code;
	std::string status_message;
	std::getline(response_stream, status_message);
	if (!response_stream || http_version.substr(0, 5) != "HTTP/") {
		if (verbose) std::cerr << " - Invalid HTTP response\n";
		return headers;
	}

	// Read the response headers, which are terminated by a blank line.
	boost::asio::read_until(socket, response, "\r\n\r\n");

	// Process the response headers.
	std::string header;
	while (std::getline(response_stream, header) && header != "\r") {
		std::string::size_type len = header.length();
		if (header[len - 1] == '\r') header = header.substr(0, len - 1);
		if (verbose > 1) std::cerr << " - Got header: " << header << "\n";
		headers.push_back(header);
	}

	return headers;
}

std::string Network::http_get(const std::string& path)
{
	if (verbose) std::cerr << "Trying to download \"" << path << "\"..." << std::endl;

	// Try each endpoint until we successfully establish a connection
	boost::asio::ip::tcp::socket socket(this->io_service);
	boost::asio::connect(socket, this->endpoint_iterator_http);

	// Form the request. We specify the "Connection: close" header so that the
	// server will close the socket after transmitting the response. This will
	// allow us to treat all data up until the EOF as the content.
	boost::asio::streambuf request;
	std::ostream request_stream(&request);
	request_stream << "GET " << path << " HTTP/1.0\r\n";
	request_stream << "Host: " << this->host << "\r\n";
	request_stream << "Accept: */*\r\n";
	request_stream << "Connection: close\r\n\r\n";

	// Send the request
	boost::asio::write(socket, request);

	// Read the response status line. The response streambuf will automatically
	// grow to accommodate the entire line. The growth may be limited by passing
	// a maximum size to the streambuf constructor.
	boost::asio::streambuf response;
	boost::asio::read_until(socket, response, "\r\n");

	// Check that response is OK.
	std::istream response_stream(&response);
	std::string http_version;
	response_stream >> http_version;
	unsigned int status_code;
	response_stream >> status_code;
	std::string status_message;
	std::getline(response_stream, status_message);
	if (!response_stream || http_version.substr(0, 5) != "HTTP/")
	{
		if (verbose) std::cerr << " - Invalid HTTP response\n";
		return std::string();
	}
	if (status_code != 200)
	{
		if (verbose) std::cerr << " - Unexpected HTTP status code: " << status_code << "\n";
		return std::string();
	}

	// Read the response headers, which are terminated by a blank line.
	boost::asio::read_until(socket, response, "\r\n\r\n");

	// Process the response headers.
	std::string header;
	while (std::getline(response_stream, header) && header != "\r")
		if (verbose > 1) std::cerr << " - [header] " << header << "\n";

	// Read until EOF
	boost::system::error_code error;
	boost::asio::read(socket, response, boost::asio::transfer_all(), error);
	if (error != boost::asio::error::eof)
		throw boost::system::system_error(error);
	boost::asio::streambuf::const_buffers_type response_bufs = response.data();
	std::string content(boost::asio::buffers_begin(response_bufs),
		boost::asio::buffers_begin(response_bufs) + response.size());

	if (verbose) std::cerr << "Download successful" << std::endl;
	if (verbose > 1) std::cerr << "Received content:\n" << content << std::endl;

	return content;
}

boost::shared_ptr<boost::asio::ip::tcp::socket> Network::tcp_connect(
	const std::string& service, boost::asio::io_service *tcp_service)
{
	boost::asio::ip::tcp::resolver resolver(*tcp_service);
	boost::asio::ip::tcp::resolver::query query(this->host, service);
	boost::asio::ip::tcp::resolver::iterator it = resolver.resolve(query);
	boost::shared_ptr<boost::asio::ip::tcp::socket> socket(new boost::asio::ip::tcp::socket(*tcp_service));
	if (verbose) std::cerr << "Connecting to " << this->host << " on port "
		<< it->endpoint().port() << "..." << std::endl;
	boost::asio::connect(*socket, it);
	// TODO: check for timeout
	return socket;
}

#define EXPECT_FTP_STATUS(s) \
	for (;;) { \
		boost::asio::read_until(*this->ftp_socket, response, "\r\n"); \
		std::string line; \
		std::getline(response_stream, line); \
		if (line[3] == ' ') { \
			status_code = strtoul(line.c_str(), NULL, 10); \
			if ((s != 0) && (status_code != s)) { \
				if (verbose) std::cerr << "[ftp] Unexpected status code: " << status_code \
					<< std::endl; \
				return false; \
			} \
			break; \
		} \
	}

bool Network::ftp_login(const std::string& user, const std::string& pass)
{
	if (this->okFTP) return true;

	boost::asio::ip::tcp::resolver resolver(this->io_service_ftp);
	boost::asio::ip::tcp::resolver::query query(host, "ftp");
	this->endpoint_iterator_ftp = resolver.resolve(query);

	this->ftp_socket.reset(new boost::asio::ip::tcp::socket(this->io_service_ftp));
	boost::asio::connect(*this->ftp_socket, this->endpoint_iterator_ftp);

	boost::asio::streambuf request;
	std::ostream request_stream(&request);
	boost::asio::streambuf response;
	std::istream response_stream(&response);

	unsigned int status_code;
	if (verbose) std::cerr << "[ftp] Waiting for greeting" << std::endl;
	EXPECT_FTP_STATUS(220);
	if (verbose) std::cerr << "[ftp] Received greeting, logging in" << std::endl;

	request_stream << "USER " << user << "\r\n";
	boost::asio::write(*this->ftp_socket, request);
	EXPECT_FTP_STATUS(331);

	request_stream << "PASS " << pass << "\r\n";
	boost::asio::write(*this->ftp_socket, request);
	EXPECT_FTP_STATUS(230);

	if (verbose) std::cerr << "[ftp] Login successful" << std::endl;

	request_stream << "TYPE I\r\n";
	boost::asio::write(*this->ftp_socket, request);
	EXPECT_FTP_STATUS(200);

	if (verbose) std::cerr << "[ftp] Binary flag set ok" << std::endl;

	this->okFTP = true;
	return true;
}

bool Network::ftp_get(std::ostream& target, const std::string& path,
	const std::string& filename, fn_progress fnProgress)
{
	boost::asio::streambuf request;
	std::ostream request_stream(&request);
	boost::asio::streambuf response;
	std::istream response_stream(&response);

	if (verbose) std::cerr << "[ftp] Setting passive mode" << std::endl;

	request_stream << "PASV\r\n";
	boost::asio::write(*this->ftp_socket, request);

	boost::asio::read_until(*this->ftp_socket, response, "\r\n");
	std::string line;
	std::getline(response_stream, line);

	char *tok = strtok(const_cast<char *>(line.c_str()), " "); // status code
	unsigned int status_code = strtoul(tok, NULL, 10);
	if (status_code != 227) {
		if (verbose) std::cerr << "[ftp] Unable to set passive mode: " << line
			<< std::endl;
		return false;
	}
	tok = strtok(NULL, "("); // rest of message
	tok = strtok(NULL, ","); // ip1
	tok = strtok(NULL, ","); // ip2
	tok = strtok(NULL, ","); // ip3
	tok = strtok(NULL, ","); // ip4
	tok = strtok(NULL, ","); // port high
	unsigned short port = strtoul(tok, NULL, 10) << 8;
	tok = strtok(NULL, ","); // port low
	port += strtoul(tok, NULL, 10);

	// Have to convert to a string because boost::asio can't accept int ports
	std::stringstream ss;
	ss << port;

	if (verbose) std::cerr << "[ftp] Passive ok, connecing to port " << port << std::endl;

	boost::asio::io_service io_service_ftp_data;
	boost::asio::ip::tcp::resolver resolver(io_service_ftp_data);
	boost::asio::ip::tcp::resolver::query query(host, ss.str(), boost::asio::ip::resolver_query_base::numeric_service);
	boost::asio::ip::tcp::resolver::iterator endpoint_iterator_ftp_data
		= resolver.resolve(query);

	boost::asio::ip::tcp::socket socket_data(io_service_ftp_data);
	boost::asio::connect(socket_data, endpoint_iterator_ftp_data);
	boost::asio::streambuf response_data;
	std::istream response_data_stream(&response_data);

	if (verbose) std::cerr << "[ftp] Beginning download" << std::endl;

	request_stream << "CWD " << path << "\r\n";
	boost::asio::write(*this->ftp_socket, request);
	EXPECT_FTP_STATUS(250);

	request_stream << "RETR " << filename << "\r\n";
	boost::asio::write(*this->ftp_socket, request);
	EXPECT_FTP_STATUS(150);

	if (verbose) std::cerr << "[ftp] Receiving data" << std::endl;

	unsigned long amount = 0, total = 0;
	boost::system::error_code error;
	while (boost::asio::read(socket_data, response_data,
			boost::asio::transfer_at_least(1), error)
	) {
		amount += response_data.size();
		target << &response_data;
		fnProgress(amount, total);
	}
	fnProgress(amount, -1); // signal download complete
	if (error != boost::asio::error::eof)
		throw boost::system::system_error(error);

	EXPECT_FTP_STATUS(226);
	socket_data.close();

	if (verbose) std::cerr << "[ftp] Download complete" << std::endl;

	return true;
}

void Network::ftp_close()
{
	boost::asio::streambuf request;
	std::ostream request_stream(&request);

	request_stream << "QUIT\r\n";
	boost::asio::write(*this->ftp_socket, request);
	this->ftp_socket->close();

	this->okFTP = false;
	return;
}

const std::string& Network::hostname()
{
	return this->host;
}
