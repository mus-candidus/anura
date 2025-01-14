/*
	Copyright (C) 2003-2014 by David White <davewx7@gmail.com>

	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	   1. The origin of this software must not be misrepresented; you must not
	   claim that you wrote the original software. If you use this software
	   in a product, an acknowledgement in the product documentation would be
	   appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	   3. This notice may not be removed or altered from any source
	   distribution.
*/

#include <assert.h>

#include <array>
#include <boost/asio.hpp>
#include <boost/regex.hpp>
// boost::thread < 1.51 conflicts with C++11-capable compilers
#if BOOST_VERSION < 105100
    #include <ctime>
    #undef TIME_UTC
#endif
//#include <boost/thread.hpp>

#include <iostream>
#include <string>
#include <vector>

#include <iostream>
#include <cstdint>

#include "asserts.hpp"
#include "unit_test.hpp"

using boost::asio::ip::tcp;
using boost::asio::ip::udp;

typedef std::shared_ptr<tcp::socket> socket_ptr;
typedef std::shared_ptr<std::array<char, 1024> > buffer_ptr;

namespace multiplayer
{
	class server
	{
	public:
		explicit server(boost::asio::io_service& io_service)
		  : acceptor_(io_service, tcp::endpoint(tcp::v4(), 17002)),
			next_id_(0),
			udp_socket_(io_service, udp::endpoint(udp::v4(), 17001))
		{
			start_accept();
			start_udp_receive();
		}

	private:
		void start_accept()
		{
#if BOOST_ASIO_VERSION >= 101400
			socket_ptr socket(new tcp::socket(acceptor_.get_executor()));
#else
			socket_ptr socket(new tcp::socket(acceptor_.get_io_service()));
#endif
			acceptor_.async_accept(*socket, std::bind(&server::handle_accept, this, socket, std::placeholders::_1));
		}

		void handle_accept(socket_ptr socket, const boost::system::error_code& error)
		{
			if(!error) {
				SocketInfo info;
				info.id = next_id_++;

				std::array<char, 4> buf;
				memcpy(&buf[0], &info.id, 4);

				boost::asio::async_write(*socket, boost::asio::buffer(buf),
										 std::bind(&server::handle_send, this, socket, std::placeholders::_1, std::placeholders::_2));

				sockets_info_[socket] = info;
				id_to_socket_[info.id] = socket;

				sockets_.push_back(socket);
				start_receive(socket);
				start_accept();
			} else {
				LOG_ERROR("ERROR IN ACCEPT!");
			}
		}

		void handle_send(socket_ptr socket, const boost::system::error_code& e, size_t nbytes)
		{
			if(e) {
				disconnect(socket);
			}
		}

		void start_receive(socket_ptr socket)
		{
			buffer_ptr buf(new std::array<char, 1024>);
			socket->async_read_some(boost::asio::buffer(*buf), std::bind(&server::handle_receive, this, socket, buf, std::placeholders::_1, std::placeholders::_2));
		}

		void handle_receive(socket_ptr socket, buffer_ptr buf, const boost::system::error_code& e, size_t nbytes)
		{
			if(!e) {
				std::string str(buf->data(), buf->data() + nbytes);
				LOG_INFO("RECEIVE {{{" << str << "}}}");

				static boost::regex ready("READY/(.+)/(\\d+)/(.* \\d+)");

				boost::cmatch match;
				if(boost::regex_match(str.c_str(), match, ready)) {
					std::string level_id(match[1].first, match[1].second);

					GameInfoPtr& game = games_[level_id];
					if(!game) {
						game.reset(new GameInfo);
					}

					SocketInfo& info = sockets_info_[socket];
					info.local_addr = std::string(match[3].first, match[3].second);
					LOG_INFO("LOCAL ADDRESS: " << info.local_addr);
					if(info.game) {
						//if the player is already in a game, remove them from it.
						std::vector<socket_ptr>& v = info.game->players;
						v.erase(std::remove(v.begin(), v.end(), socket), v.end());
					}

					info.game = game;

					LOG_INFO("ADDING PLAYER TO GAME: " << socket.get());
					game->players.push_back(socket);


					const int nplayers = atoi(match[2].first);
					game->nplayers = nplayers;

				}

				start_receive(socket);
			} else {
				disconnect(socket);
			}
		}

		void disconnect(socket_ptr socket) {

			SocketInfo& info = sockets_info_[socket];
			if(info.game) {
				std::vector<socket_ptr>& v = info.game->players;
				v.erase(std::remove(v.begin(), v.end(), socket), v.end());
			}

			LOG_INFO("CLOSING SOCKET: ");
			socket->close();
			id_to_socket_.erase(sockets_info_[socket].id);
			sockets_.erase(std::remove(sockets_.begin(), sockets_.end(), socket), sockets_.end());

			sockets_info_.erase(socket);
		}

		tcp::acceptor acceptor_;

		std::vector<socket_ptr> sockets_;

		typedef std::shared_ptr<udp::endpoint> udp_endpoint_ptr;

		struct GameInfo;
		typedef std::shared_ptr<GameInfo> GameInfoPtr;

		struct SocketInfo {
			uint32_t id;
			udp_endpoint_ptr udp_endpoint;
			GameInfoPtr game;
			std::string local_addr;
		};

		std::map<socket_ptr, SocketInfo> sockets_info_;
		std::map<uint32_t, socket_ptr> id_to_socket_;

		struct GameInfo {
			GameInfo() : started(false) {}
			std::vector<socket_ptr> players;
			int nplayers;
			bool started;
		};

		std::map<std::string, GameInfoPtr> games_;

		uint32_t next_id_;

		void start_udp_receive() {
			udp_endpoint_ptr endpoint(new udp::endpoint);
			udp_socket_.async_receive_from(
			  boost::asio::buffer(udp_buf_), *endpoint,
			  std::bind(&server::handle_udp_receive, this, endpoint, std::placeholders::_1, std::placeholders::_2));
		}

		void handle_udp_receive(udp_endpoint_ptr endpoint, const boost::system::error_code& error, size_t len)
		{
			LOG_INFO("RECEIVED UDP PACKET: " << static_cast<int>(len));
			if(len >= 5) {
				uint32_t id;
				memcpy(&id, &udp_buf_[1], 4);
				std::map<uint32_t, socket_ptr>::iterator socket_it = id_to_socket_.find(id);
				if(socket_it != id_to_socket_.end()) {
					assert(sockets_info_.count(socket_it->second));
					sockets_info_[socket_it->second].udp_endpoint = endpoint;

					GameInfoPtr& game = sockets_info_[socket_it->second].game;
					if(udp_buf_[0] == 'Z' && game.get() != nullptr && !game->started && game->players.size() >= size_t(game->nplayers)) {
						bool have_sockets = true;
						for(const socket_ptr& sock : game->players) {
							const SocketInfo& info = sockets_info_[sock];
							if(info.udp_endpoint.get() == nullptr) {
								have_sockets = false;
							}
						}

						if(have_sockets) {

							for(socket_ptr socket : game->players) {
								const SocketInfo& send_socket_info = sockets_info_[socket];

								std::ostringstream msg;
								msg << "START " << game->players.size() << "\n";
								for(socket_ptr s : game->players) {
									if(s == socket) {
										msg << "SLOT\n";
										continue;
									}

									const SocketInfo& sock_info = sockets_info_[s];
									if(send_socket_info.udp_endpoint->address().to_string() != sock_info.udp_endpoint->address().to_string()) {
										//the hosts are not from the same address,
										//so send them each other's network address.
										msg << sock_info.udp_endpoint->address().to_string().c_str() << " " << sock_info.udp_endpoint->port() << "\n";
									} else {
										//the hosts are from the same address,
										//which means they are likely behind the
										//same NAT device. Send them their local
										//addresses, behind their devices.
										msg << sock_info.local_addr << "\n";
									}
								}

								const std::string msg_str = msg.str();

								boost::asio::async_write(*socket, boost::asio::buffer(msg_str),
											 std::bind(&server::handle_send, this, socket, std::placeholders::_1, std::placeholders::_2));
							}

							game->started = true;
							for(std::map<std::string, GameInfoPtr>::iterator i = games_.begin(); i != games_.end(); ++i) {
								if(game == i->second) {
									i->second.reset();
								}
							}
						}
					}

					if(udp_buf_[0] != 'Z') {
						GameInfoPtr game = sockets_info_[socket_it->second].game;

						if(game.get() != nullptr) {
							for(int n = 0; n != game->players.size(); ++n) {
								if(game->players[n] == socket_it->second) {
									continue;
								}

								LOG_INFO("GOT FROM: " << endpoint->port() << " RELAYING TO...");
								std::map<socket_ptr, SocketInfo>::iterator socket_info = sockets_info_.find(game->players[n]);
								if(socket_info != sockets_info_.end()) {
									LOG_INFO("  RELAY TO " << socket_info->second.udp_endpoint->port());
									udp_socket_.async_send_to(boost::asio::buffer(&udp_buf_[0], len), *socket_info->second.udp_endpoint,
										std::bind(&server::handle_udp_send, this, socket_info->second.udp_endpoint, std::placeholders::_1, std::placeholders::_2));
								}
							}
						}
					}
				}
			}
			start_udp_receive();
		}

		void handle_udp_send(udp_endpoint_ptr endpoint, const boost::system::error_code& error, size_t len)
		{
			if(error) {
				LOG_INFO("ERROR IN UDP SEND!");
			} else {
				LOG_INFO("UDP: SENT " << static_cast<int>(len) << " BYTES");
			}
		}

		udp::socket udp_socket_;
		std::array<char, 1024> udp_buf_;
	};
}

COMMAND_LINE_UTILITY(multiplayer_server)
{
	boost::asio::io_service io_service;

	multiplayer::server srv(io_service);
	io_service.run();
}
