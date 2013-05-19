/**
 * @file   main.cpp
 * @brief  Command-line utility for identifying an IP camera.
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

#include <fstream>
#include <iomanip>
#include <boost/program_options.hpp>
#include <boost/asio.hpp>
#include <boost/regex.hpp>
#include <boost/bind.hpp>

#include "device-interface.hpp"
#include "maygion-mips.hpp"

namespace po = boost::program_options;

#define PROGNAME "camtickler"

#define RET_OK                 0 ///< Return value: all is good
#define RET_BADARGS            1 ///< Return value: bad/missing arguments
#define RET_SHOWSTOPPER        2 ///< Return value: failure of the single action requested

int verbose = 0; ///< Verbosity level of stdout messages

Device *openDevice(std::string strType, Network *network,
	boost::asio::serial_port *serial)
{
	if (strType.compare("maygion-mips") == 0) {
		return new maygion_mips(network);
	}
	return NULL;
}

void showProgress(const std::string& msg, unsigned long amount, unsigned long total)
{
	if (total == (unsigned long)-1) {
		// complete
		std::cerr << std::endl;
		return;
	}
	std::cerr << "\r" << msg << ": " << amount << " bytes read";
	if (total) {
		std::cerr << " (" << (amount * 100 / total) << "%)";
	}
	std::cerr << std::flush;
	return;
}

#define possible_match(conf, tname) \
	if (verbose) std::cerr << "Possible match: " << tname \
		<< " (confidence: " << conf; \
	if (confidence < conf) { \
		if (verbose) std::cerr << ")\n"; \
		type = tname; \
		confidence = conf; \
	} else { \
		if (verbose) std::cerr << "; too low, ignoring)\n"; \
	}

class Identify
{
	public:
		Identify(Network *network, boost::asio::serial_port *serial)
			: network(network),
			  serial(serial),
			  httpPort(0) // auto
		{
		}

		std::string getType()
		{
			bool okHTTP = this->tryHTTP();
			bool okFTP = this->tryFTP();
			if (okFTP && !okHTTP && !this->dev_pass.empty()) {
				// Try HTTP again now we have some credentials
				okHTTP = this->tryHTTP();
			}

			// If we don't know what port HTTP is and the default didn't work, try a
			// few alternatives.
			if ((!okHTTP) && (this->httpPort == 0)) {
				this->httpPort = 81;
				okHTTP = this->tryHTTP();
				if (!okHTTP) {
					this->httpPort = 8080;
					okHTTP = this->tryHTTP();
					if (!okHTTP) {
						this->httpPort = 0; // couldn't find it
					}
				}
			}

			int maxConfidence = 50; // must be at least this confident for a result
			std::string bestType = "unknown";
			if (verbose) std::cerr << "Confidence levels:\n";
			for (std::map<std::string, int>::const_iterator i = confidence.begin(); i != confidence.end(); i++) {
				if (verbose) std::cerr << "  " << i->first << ": " << i->second << "%\n";
				if (i->second > maxConfidence) {
					maxConfidence = i->second;
					bestType = i->first;
				}
			}
			if (!this->dev_user.empty() && !this->dev_pass.empty()) {
				std::cout << "admin_username=" << this->dev_user
					<< "\nadmin_password=" << this->dev_pass << std::endl;
			}
			return bestType;
		}

		bool tryHTTP()
		{
			this->network->set_http_port(this->httpPort);
			std::cerr << "[http] Attempting to connect to " << network->hostname()
				<< " port " << network->get_http_port() << std::endl;
			std::vector<std::string> headers;
			try {
				headers = network->http_headers();
			} catch (const boost::system::system_error& e) {
				// Assume HTTP is unavailable on this port
				std::cerr << "[http] Connection failed." << std::endl;
				return false;
			}

			for (std::vector<std::string>::iterator i = headers.begin(); i != headers.end(); i++) {
				if (i->substr(0, 7).compare("Server:") == 0) {
					// This is the Server: header
					std::string server = i->substr(8);
					if (verbose) std::cerr << "[http] Server is \"" << server << "\"\n";
					if (server.compare("WebServer(IPCamera_Logo)") == 0) {
						confidence["maygion-mips"] += 10;
					}
				}
			}

			// Use the discovered credentials if present, otherwise fall back to the
			// default ones.
			std::string url = "/sysinfo.xml?user=";
			if (this->dev_user.empty()) url += "admin"; else url += this->dev_user;
			url += "&password=";
			if (this->dev_pass.empty()) url += "admin"; else url += this->dev_pass;

			std::string httpData = network->http_get(url);
			boost::regex result_regex("<Success>(.*)</Success>");
			boost::match_results<std::string::const_iterator> result_matches;
			std::string::const_iterator start = httpData.begin();
			std::string::const_iterator end = httpData.end();
			boost::regex_search(start, end, result_matches, result_regex, boost::match_default);
			std::string result(result_matches[1].first, result_matches[1].second);
			if (result.compare("0") == 0) {
				if (verbose) std::cerr << "[http] Possible MayGion MIPS with non-default admin password\n";
				confidence["maygion-mips"] += 20;

				boost::regex error_regex("<ErrorCode>(.*)</ErrorCode>");
				boost::match_results<std::string::const_iterator> error_matches;
				std::string::const_iterator start = httpData.begin();
				std::string::const_iterator end = httpData.end();
				boost::regex_search(start, end, error_matches, error_regex, boost::match_default);
				std::string error_code(error_matches[1].first, error_matches[1].second);
				if (error_code.compare("eHttpError_No_Auth") == 0) {
					// Newer firmware
					confidence["maygion-mips"] += 20;
				} else if (error_code.compare("5") == 0) {
					// Older firmware
					confidence["maygion-mips"] += 20;
				} else {
					if (verbose) std::cerr << "[http] Unknown error trying to get "
						"device info: " << error_code << std::endl;
				}
				return false;
			} else if (result.compare("1") != 0) {
				// Unknown response
				confidence["maygion-mips"] -= 10;
				return false;
			}

			// Got acceptable HTTP response
			if (verbose) std::cerr << "[http] Appears to be a MayGion MIPS\n";
			confidence["maygion-mips"] += 10;

			if (this->dev_user.empty() && this->dev_pass.empty()) {
				if (verbose) std::cerr << "[http] Default user/pass works\n";
				this->dev_user = "admin";
				this->dev_pass = "admin";
			}

			boost::regex board_regex("<Board>(.*)</Board>");
			boost::match_results<std::string::const_iterator> matches;
			start = httpData.begin();
			end = httpData.end();
			boost::regex_search(start, end, matches, board_regex, boost::match_default);
			std::string board(matches[1].first, matches[1].second);
			if (verbose) std::cerr << "[http] MayGion board ID: " << board << std::endl;
			if (board.compare("MIPS") == 0) {
				confidence["maygion-mips"] = 100;
			} // else could be MIPS with old firmware

			return true;
		}

		bool tryFTP()
		{
			// Get password
			std::string cred_enc;
			if (network->ftp_login("MayGion", "maygion.com")) {
				this->confidence["maygion-mips"] = 100;

				std::stringstream config;
				network->ftp_get(config, "/tmp/eye/app", "cs.ini",
					boost::bind(showProgress, "Retrieving config", _1, _2));
				config.seekg(0);
				enum section {SECTION_NONE, SECTION_HTTP, SECTION_USR};
				enum section curSection = SECTION_NONE;
				while (!config.eof()) {
					std::string line;
					std::getline(config, line);
					if (verbose > 1) std::cerr << "[config] " << line << std::endl;
					const char *cline = line.c_str();
					if (strncmp(cline, "[http]", 6) == 0) {
						curSection = SECTION_HTTP;
					} else if (strncmp(cline, "[usr]", 5) == 0) {
						curSection = SECTION_USR;
					} else if ((curSection == SECTION_USR) && (strncmp(cline, "ui=", 3) == 0)) {
						// This is the UI line

						// From http://base64.sourceforge.net/b64.c
						static const char cd64[]="|$$$}rstuvwxyz{$$$$$$$>?@ABCDEFGHIJKLMNOPQRSTUVW$$$$$$XYZ[\\]^_`abcdefghijklmnopq";
						for (std::string::const_iterator i = line.begin() + 3; i != line.end(); i++) {
							unsigned char v = ((*i < 43 || *i > 122) ? 0 : (int) cd64[ *i - 43 ]);
							if (v != 0) {
								v = ((v == '$') ? 0 : v - 61);
							}
							cred_enc += v-1;
						}
					} else if ((curSection == SECTION_HTTP) && (strncmp(cline, "port=", 5) == 0)) {
						unsigned int port = strtoul(line.c_str() + 5, NULL, 10);
						if ((port != 80) && (port != 0)) {
							if (verbose) std::cerr << "[ftp] Web interface is operating on port "
								<< port << std::endl;
							this->httpPort = port;
							std::cout << "http_port=" << this->httpPort << "\n";
						}
					}
				}
			} else {
				// FTP unavailable, try telnet
				return false;
			}

			if (!cred_enc.empty()) {
				std::string cred_dec;

				// From http://base64.sourceforge.net/b64.c
				for (std::string::const_iterator i = cred_enc.begin(); i != cred_enc.end(); i++) {
					unsigned char a = *i;
					if (++i == cred_enc.end()) break;
					unsigned char b = *i;
					cred_dec += (char)(a << 2 | b >> 4);
					if (++i == cred_enc.end()) break;
					unsigned char c = *i;
					cred_dec += (char)(b << 4 | c >> 2);
					if (++i == cred_enc.end()) break;
					unsigned char d = *i;
					cred_dec += (char)(((c << 6) & 0xc0) | d);
				}
				if (verbose > 1) std::cout << "base64 decoded data: " << cred_dec << std::endl;
				std::string::size_type usr_start = cred_dec.find("usr=") + 4;
				std::string::size_type usr_end = cred_dec.find("\r\n", usr_start);
				this->dev_user = cred_dec.substr(usr_start, usr_end - usr_start);

				std::string::size_type pwd_start = cred_dec.find("pwd=") + 4;
				std::string::size_type pwd_end = cred_dec.find("\r\n", pwd_start);
				this->dev_pass = cred_dec.substr(pwd_start, pwd_end - pwd_start);

			} else {
				// Unable to get credentials
				this->dev_user = "";
				this->dev_pass = "";
			}
			return true;
		}

	private:
		Network *network;
		boost::asio::serial_port *serial;
		std::map<std::string, int> confidence;
		std::string dev_user, dev_pass;
		unsigned int httpPort;
};

int main(int argc, char *argv[])
{
#ifdef __GLIBCXX__
	// Set a better exception handler
	std::set_terminate(__gnu_cxx::__verbose_terminate_handler);
#endif

	// Disable stdin/printf/etc. sync for a speed boost
	std::ios_base::sync_with_stdio(false);

	// Declare the supported options.
	po::options_description poActions("Actions");
	poActions.add_options()
		("identify,i",
			"identify device")

		("query,q",
			"query details about a known device")

		("dump-firmware,d", po::value<std::string>(),
			"copy firmware from device's flash into this file")
	;

	po::options_description poOptions("Options");
	poOptions.add_options()
		("type,t", po::value<std::string>(),
			"specify the device type (required unless using --identify)")
		("host,h", po::value<std::string>(),
			"hostname or IP address of device")
		("serial,s", po::value<std::string>(),
			"serial port device is connected to (COM1, /dev/ttyUSB0, etc.)")
		("verbose,v",
			"show more detail (can specify twice for even more detail)")
	;

	po::options_description poHidden("Hidden parameters");
	poHidden.add_options()
		("help", "produce help message")
	;

	po::options_description poVisible("");
	poVisible.add(poActions).add(poOptions);

	po::options_description poComplete("Parameters");
	poComplete.add(poActions).add(poOptions).add(poHidden);
	po::variables_map mpArgs;

	std::string strType, strHost, strSerial;

	try {
		po::parsed_options pa = po::parse_command_line(argc, argv, poComplete);

		// Parse the global command line options
		for (std::vector<po::option>::iterator i = pa.options.begin(); i != pa.options.end(); i++) {
			if (i->string_key.empty()) {
				std::cerr << "Error: unexpected extra parameter" << std::endl;
				return RET_BADARGS;
			} else if (i->string_key.compare("help") == 0) {
				std::cout <<
					"Copyright (C) 2013 Adam Nielsen <malvineous@shikadi.net>\n"
					"This program comes with ABSOLUTELY NO WARRANTY.  This is free software,\n"
					"and you are welcome to change and redistribute it under certain conditions;\n"
					"see <http://www.gnu.org/licenses/> for details.\n"
					"\n"
					"Utility to identify network/serial attached devices and manipulate firmware.\n"
					"Build date " __DATE__ " " __TIME__ << "\n"
					"\n"
					"Usage: " PROGNAME " <action> [action...]\n" << poVisible <<
					"\n"
					"Example:\n"
					"  " PROGNAME " --host 1.2.3.4 --identify  # Get value to use in --type\n"
					"  " PROGNAME " --host 1.2.3.4 --type device-type --query\n"
					<< std::endl;
				return RET_OK;

			} else if (
				(i->string_key.compare("list-types") == 0)
			) {
				std::cout
					<< "maygion-mips\tMayGion MIPS camera\n"
					<< std::flush;
				return RET_OK;

			} else if (
				(i->string_key.compare("t") == 0) ||
				(i->string_key.compare("type") == 0)
			) {
				assert(i->value.size() != 0);
				strType = i->value[0];

			} else if (
				(i->string_key.compare("h") == 0) ||
				(i->string_key.compare("host") == 0)
			) {
				assert(i->value.size() != 0);
				strHost = i->value[0];

			} else if (
				(i->string_key.compare("s") == 0) ||
				(i->string_key.compare("serial") == 0)
			) {
				assert(i->value.size() != 0);
				strSerial = i->value[0];

			} else if (
				(i->string_key.compare("v") == 0) ||
				(i->string_key.compare("verbose") == 0)
			) {
				verbose++;

			}
		}

		if (strHost.empty() && strSerial.empty()) {
			std::cerr << PROGNAME << ": a hostname or serial port must be specified." << std::endl;
			return RET_BADARGS;
		}

		// Attempt to open the serial port if one was given
		boost::asio::io_service serial_io;
		boost::asio::serial_port serial(serial_io);
		if (!strSerial.empty()) {
			serial.open(strSerial);
			serial.set_option(boost::asio::serial_port::baud_rate(115200));
		}
		Network network(strHost);

		// Run through the actions on the command line
		for (std::vector<po::option>::iterator i = pa.options.begin(); i != pa.options.end(); i++) {
			if (i->string_key.compare("identify") == 0) {
				Identify id(&network, &serial);
				strType = id.getType();
				std::cout << "device_type=";
				if (strType.empty()) {
					std::cout << "unknown" << std::endl;
					std::cerr << "Unable to identify device!" << std::endl;
				} else {
					std::cout << strType << std::endl;
				}

			} else if (i->string_key.compare("dump-firmware") == 0) {
				Device *dev = openDevice(strType, &network, &serial);
				if (!dev) {
					std::cerr << PROGNAME ": --type missing or invalid." << std::endl;
					return RET_BADARGS;
				}
				const std::string& strFilename = i->value[0];
				std::ofstream outfile(strFilename.c_str(),
					std::ios::out | std::ios::trunc | std::ios::binary);

				std::vector<uint8_t> firmware;
				fn_progress fnProg
					= boost::bind(showProgress, "Downloading firmware", _1, _2);
				try {
					dev->getFirmware(outfile, fnProg);
				} catch (const std::string& err) {
					std::cerr << "Download failed: " << err
						<< std::endl;
				}
				outfile.close();
				std::cout << "Saved to " << strFilename << std::endl;

			} else if (i->string_key.compare("query") == 0) {
				Device *dev = openDevice(strType, &network, &serial);
				if (!dev) {
					std::cerr << PROGNAME ": --type missing or invalid." << std::endl;
					return RET_BADARGS;
				}
				bool known_model = false;
				try {
					unsigned long lenFlash;
					dev->getFlashInfo(&lenFlash);
					std::cout << "flash_size=" << lenFlash << std::endl;

					unsigned short idVendor, idProduct;
					unsigned char bInterfaceClass;
					dev->getCameraInfo(&idVendor, &idProduct, &bInterfaceClass);
					std::cout << std::hex
						<< "camera_usb_vendor=" << std::setw(4) << std::setfill('0') << idVendor
						<< "\ncamera_usb_product=" << std::setw(4) << std::setfill('0') << idProduct
						<< "\ncamera_usb_class=" << std::setw(2) << std::setfill('0') << (unsigned int)bInterfaceClass
						<< std::endl;

					std::cout << "model=" << strType << "-";
					if ((lenFlash == 0x400000) && (idVendor == 0x0c45) && (idProduct == 0x6360)) {
						std::cout << "1.0";
						known_model = true;
					} else {
						std::cout << "ver_unknown";
					}
					std::cout << "\n";

					std::cout << "fwid=" << strType << "-" << (lenFlash >> 20) << "mb-";
					if (bInterfaceClass == 0x0e) std::cout << "uvc";
					else std::cout << "unknown_image_sensor";
					std::cout << "\n";

					if (!known_model) {
						std::cerr << "\n\n >>> This camera is an unknown model!  Please get in "
							"touch!\nhttp://www.openipcam.com/forum/\n" << std::endl;
					}
				} catch (const std::string& err) {
					std::cerr << "Device query failed: " << err
						<< std::endl;
				}

			}
		} // for (all command line elements)

	} catch (const po::unknown_option& e) {
		std::cerr << PROGNAME ": " << e.what()
			<< ".  Use --help for help." << std::endl;
		return RET_BADARGS;

	} catch (const po::invalid_command_line_syntax& e) {
		std::cerr << PROGNAME ": " << e.what()
			<< ".  Use --help for help." << std::endl;
		return RET_BADARGS;

	} catch (const boost::system::system_error& e) {
		std::cerr << PROGNAME ": " << e.what()
			<< ".  Use --help for help." << std::endl;
		return RET_BADARGS;
	}

	return RET_OK;
}
