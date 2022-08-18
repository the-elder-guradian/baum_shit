#include "test_listener.hpp"

#include <thread>
#include <iostream>
#include <stdexcept>
#include <functional>

#include <cmath>

using test_listener::listener;

listener::listener(uint8_t _backlog, uint16_t _port, const char _ip[INET_ADDRSTRLEN]) :
		backlog(_backlog), port(_port)
		{
			memcpy(ip, _ip, sizeof(ip));
			
			create_socket();
			set_socket_options();
			bind_socket();
			setup_socket();
			
			setup_polling();
		}
		
		listener::~listener()
		{
			close(socket_descriptor);
			
			client_pool.clear();
		}
		
		void listener::run(bool _is_asynchronous)
		{
			std::thread master_thread([&](auto _client_pool) -> void
			{
				int poll_code;
				
				do
				{
					poll_code = poll(polling_pool, 
										total_polled_connections, 
										polling_timeout);
										
					if(poll_code < 0)
					{
						throw std::runtime_error("polling failed -> " + std::to_string(poll_code));
					}
										
					if(poll_code > 0)
					{
						const uint16_t current_amount = total_polled_connections;
						
						for(uint16_t index = 0; index < current_amount; index++)
						{
							if(polling_pool[index].revents == POLLIN)
							{
								char peer_ip[INET_ADDRSTRLEN];
									
								memset(peer_ip, 0, sizeof(peer_ip));
								
								if(polling_pool[index].fd == socket_descriptor)
								{
									int32_t new_connection_holder;
									
									do
									{
										new_connection_holder = accept(socket_descriptor, nullptr, nullptr);
										
										if(new_connection_holder < 0)
										{
											break;
										}
										
										if(extract_ip(new_connection_holder, 
													peer_ip))
										{
											polling_pool[total_polled_connections].fd = new_connection_holder;
											polling_pool[total_polled_connections].events = POLLIN;
											
											++total_polled_connections;
											
											std::lock_guard<std::mutex> guard(client_pool_mutex);
											
											_client_pool.get().push_back(new client(peer_ip));
										}
									}
									while(new_connection_holder > 0);
								}
								else
								{
									if(extract_ip(polling_pool[index].fd, 
													peer_ip))
									{
										std::lock_guard<std::mutex> guard(client_pool_mutex);
										
										for(client*& item : client_pool)
										{
											if(strcmp(item->get_identifier(), peer_ip) == 0)
											{
												const int a = item->read_stream(polling_pool[index].fd);
												
												std::cout << a << std::endl;
												
												switch(a)
												{
													// failed
													case 1:
													// disconnected
													case 2:
													validate_clients();
													break;
													
													// arguments wrong
													//case 3:
													//break;
													
													// export demand
													case 4:
													{
														const std::vector<std::vector<uint64_t>> iterated = item->iterate();
														
														const auto get_integer_length = [=](const auto _number) -> uint64_t
														{
															uint64_t length = 1;
															uint64_t accumulator = 10;
															
															while(accumulator < _number)
															{
																accumulator *= 10;
																++length;
															}
															
															return length;
														};
														
														const auto find_most_long_number = [=](const std::vector<std::vector<uint64_t>> _container) -> uint64_t
														{
															uint64_t result = 0;
															
															for(const std::vector<uint64_t>& sub_container : _container)
															{
																for(const uint64_t item : sub_container)
																{
																	if(result < item)
																	{
																		result = get_integer_length(item);
																	}
																}
															}
															
															return result;
														};
														
														const auto produce_textual_shift = [=](const uint64_t _length) -> std::string
														{
															std::string result;
															
															for(uint64_t counter = 0; counter < _length + 2; counter++)
															{
																result += " ";
															}
															
															return result;
														};
														
														const uint64_t most_long_number = find_most_long_number(iterated);
														
														std::string message;
														
														uint64_t container_limit = 0;
														
														if(iterated.size() == 0)
														{
															return;
														}
														
														if(iterated.size() == 1)
														{
															container_limit = iterated[0].size();
														}
														else if(iterated.size() == 2)
														{
															container_limit = std::max({iterated[0].size(), iterated[1].size()});
														}
														else
														{
															container_limit = std::max({iterated[0].size(), iterated[1].size(), iterated[2].size()});
														}
														
														for(uint64_t container_index = 0; container_index < container_limit; container_index++)
														{
															uint64_t item = iterated[0][container_index];
															
															uint64_t length = get_integer_length(item);
															
															message += std::to_string(item);
															message += produce_textual_shift(most_long_number - length);
															
															if(iterated.size() >= 2)
															{
																if(container_index < iterated[1].size())
																{
																	item = iterated[1][container_index];
																	length = get_integer_length(item);
																
																	message += std::to_string(item);
																	message += produce_textual_shift(most_long_number - length);
																}
															}
															
															if(iterated.size() == 3)
															{
																if(container_index < iterated[2].size())
																{
																	item = iterated[2][container_index];
																	length = get_integer_length(item);
																
																	message += std::to_string(item);
																	message += produce_textual_shift(most_long_number - length);
																}
															}
															
															message += "\n";
															
															send(polling_pool[index].fd,
																message.c_str(),
																message.length(),
																0);
																
															message.clear();
														}
													}
													break;
													
													// syntax wrong
													//case 5:
													//break;
												}
												
												break;
											}
										}
									}
								}
							}
						}
					}
				}
				while(poll_code >= 0);
				
				throw std::runtime_error("polling failed -> " + std::to_string(poll_code));
			}, std::ref(client_pool));
			
			if(_is_asynchronous)
			{
				master_thread.detach();
			}
			else
			{
				master_thread.join();
			}
		}
		
		void listener::validate_clients()
		{
			for(uint16_t index = 0; index < total_polled_connections; index++)
			{
				if(polling_pool[index].fd < 0)
				{
					for(uint16_t shift_index = index; shift_index < total_polled_connections; shift_index++)
					{
						polling_pool[shift_index].fd = polling_pool[shift_index + 1].fd;
					}
					
					--index;
					--total_polled_connections;
				}
			}
		}
		
		bool listener::extract_ip(const int32_t _descriptor, char* _ip)
		{
			struct sockaddr_in socket_holder;
			socklen_t socket_holder_size;
			
			memset(&socket_holder, 0, sizeof(socket_holder));
			
			socket_holder_size = sizeof(socket_holder);
			
			if(getsockname(
				_descriptor,
				reinterpret_cast<struct sockaddr*>(&socket_holder),
				&socket_holder_size) != 0)
			{
				return false;
			}
			
			inet_ntop(AF_INET, 
				&socket_holder.sin_addr, 
				_ip, 
				INET_ADDRSTRLEN);
				
			return true;
		}
		
		void listener::setup_polling()
		{
			total_polled_connections = 1;
			
			memset(polling_pool, 0, sizeof(polling_pool));
			
			polling_pool[0].fd = socket_descriptor;
			polling_pool[0].events = POLLIN;
			
			polling_timeout = 720000;
		}
		
		void listener::create_socket()
		{
			socket_descriptor = socket (PF_INET, SOCK_STREAM, 0);
			
			if (socket_descriptor < 0)
			{
				print_socket_error_explanation(socket_descriptor);
				
				throw std::runtime_error("socket_descriptor is invalid. halting the listener");
			}
			
			memset(&socket_instance, 0, sizeof(socket_instance));
			
			socket_instance.sin_family = AF_INET;
			socket_instance.sin_port = htons (port);
			
			inet_pton(AF_INET, ip, &socket_instance.sin_addr);
		}
		
		void listener::bind_socket()
		{
			int bind_code = bind(
					socket_descriptor, 
					reinterpret_cast<struct sockaddr*>(&socket_instance), 
					sizeof(socket_instance));
					
			if (bind_code < 0)
			{
				print_bind_error_explanation(bind_code);
				
				throw std::runtime_error("binding failed. halting the listener");
			}
		}
		
		void listener::set_socket_options()
		{
			const int option = 1;
			
			int operation_code = setsockopt(socket_descriptor,
				SOL_SOCKET,
				SO_REUSEADDR,
				reinterpret_cast<const char*>(&option),
				sizeof(int));
				
			if(operation_code < 0)
			{
				print_setsocketopt_error_explanation(operation_code);
				
				throw std::runtime_error("setsockopt failed. halting the listener");
			}
			
			operation_code = ioctl(socket_descriptor, 
				FIONBIO,
				reinterpret_cast<const char*>(&option));
				
			if(operation_code < 0)
			{
				print_ioctl_error_explanation(operation_code);
				
				throw std::runtime_error("ioctl failed. halting the listener");
			}
		}
		
		void listener::setup_socket()
		{
			int listen_code = listen(socket_descriptor, backlog);
			
			if(listen_code < 0)
			{
				print_listen_error_explanation(listen_code);
				
				throw std::runtime_error("listening failed. halting the listener");
			}
		}
		
		void listener::print_listen_error_explanation(int _code)
		{
			std::string message = "listen error:: ";
			
			switch(_code)
			{
				case EADDRINUSE:
				message += "another socket is already listening on the same port";
				break;
				
				case EBADF:
				message += "the argument *sockfd* is not a valid descriptor";
				break;
				
				case ENOTSOCK:
				message += "the argument *sockfd* is not a socket";
				break;
				
				case EOPNOTSUPP:
				message += "the socket is not of a type that supports the *listen()* operation";
				break;
				
				default:
				message += "unknown error code (" + std::to_string(_code) + ")";
				break;
			}
			
			std::cout << message << std::endl;
		}
		
		void listener::print_socket_error_explanation(int _code)
		{
			std::string message = "socket error:: ";
			
			switch(_code)
			{
				case EACCES:
				message += "permission to create a socket of the specified type and/or protocol is denied";
				break;
				
				case EAFNOSUPPORT:
				message += "the implementation does not support the specified address family";
				break;
				
				case EINVAL:
				message += "unknown protocol, protocol family not available or invalid flags in *type*";
				break;
				
				case EMFILE:
				message += "process file table overflow";
				break;
				
				case ENFILE:
				message += "the system limit on the total number of open files has been reached";
				break;
				
				case ENOBUFS:
				case ENOMEM:
				message += "the socket cannot be created until sufficient resources are freed";
				break;
				
				case EPROTONOSUPPORT:
				message += "the protocol type or the specified protocol is not supported within this domain";
				break;
				
				default:
				message += "unknown error code (" + std::to_string(_code) + ")";
				break;
			}
			
			std::cout << message << std::endl;
		}
		
		void listener::print_bind_error_explanation(int _code)
		{
			std::string message = "bind error:: ";
			
			switch(_code)
			{
				case EACCES:
				message += "the address is protected, and the user is not the superuser";
				break;
				
				case EADDRINUSE:
				message += "the given address is already in use";
				break;
				
				case EBADF:
				message += "the argument *sockfd* is not a valid descriptor";
				break;
				
				case EINVAL:
				message += "the socket is already bound to an address";
				break;
				
				case ENOTSOCK:
				message += "the argument *sockfd* is a descriptor for a file, not a socket";
				break;
				
				default:
				message += "unknown error code (" + std::to_string(_code) + ")";
				break;
			}
			
			std::cout << message << std::endl;
		}
		
		void listener::print_connect_error_explanation(int _code)
		{
			std::string message = "connect error:: ";
			
			switch(_code)
			{
				case EACCES:
				message += "write permission is denied on the socket file, or search permission is denied for one of the directories in the path prefix";
				break;
				
				case EPERM:
				message += "the user tried to connect to a broadcast address without having the socket broadcast flag enabled or the connection request failed because of a local firewall rule, or write permission is denied on the socket file, or search permission is denied for one of the directories in the path prefix";
				break;
				
				case EADDRINUSE:
				message += "local address is already in use";
				break;
				
				case EAFNOSUPPORT:
				message += "the passed address didn't have the correct address family in its *sa_family* field";
				break;
				
				case EAGAIN:
				message += "no more free local ports or insufficient entries in the routing cache";
				break;
				
				case EALREADY:
				message += "the socket is nonblocking and a previous connection attempt has not yet been completed";
				break;
				
				case EBADF:
				message += "the file descriptor is not a valid index in the descriptor table";
				break;
				
				case ECONNREFUSED:
				message += "no-one listening on the remote address";
				break;
				
				case EFAULT:
				message += "the socket structure address is outside the user's address space";
				break;
				
				case EINPROGRESS:
				message += "the socket is nonblocking and the connection cannot be completed immediately";
				break;
				
				case EINTR:
				message += "the system call was interrupted by a signal that was caught";
				break;
				
				case EISCONN:
				message += "the socket is already connected";
				break;
				
				case ENETUNREACH:
				message += "network is unreachable";
				break;
				
				case ENOTSOCK:
				message += "the file descriptor is not associated with a socket";
				break;
				
				case ETIMEDOUT:
				message += "timeout while attempting connection. the server may be too busy to accept new connections";
				break;
				
				default:
				message += "unknown error code (" + std::to_string(_code) + ")";
				break;
			}
			
			std::cout << message << std::endl;
		}
		
		void listener::print_accept_error_explanation(int _code)
		{
			std::string message = "accept error:: ";
			
			switch(_code)
			{
				case EAGAIN:
				message += "the socket is marked nonblocking and no connections are present to be accepted";
				break;
				
				case EBADF:
				message += "the descriptor is invalid";
				break;
				
				case ECONNABORTED:
				message += "a connection has been aborted";
				break;
				
				case EFAULT:
				message += "the *addr* argument is not in a writable part of the user address space";
				break;
				
				case EINTR:
				message += "the system call was interrupted by a signal that was caught before a valid connection arrived";
				break;
				
				case EINVAL:
				message += "socket is not listening for connections, or *addrlen* is invalid (e.g., is negative) or invalid value in *flags*";
				break;
				
				case EMFILE:
				message += "the per-process limit of open file descriptors has been reached";
				break;
				
				case ENFILE:
				message += "the system limit on the total number of open files has been reached";
				break;
				
				case ENOBUFS:
				case ENOMEM:
				message += "not enough free memory. this often means that the memory allocation is limited by the socket buffer limits, not by the system memory";
				break;
				
				case ENOTSOCK:
				message += "the descriptor references a file, not a socket";
				break;
				
				case EOPNOTSUPP:
				message += "the referenced socket is not of type *SOCK_STREAM*";
				break;
				
				case EPROTO:
				message += "protocol error";
				break;
				
				case EPERM:
				message += "firewall rules forbid connection";
				break;
				
				default:
				message += "unknown error code (" + std::to_string(_code) + ")";
				break;
			}
			
			std::cout << message << std::endl;
		}
		
		void listener::print_setsocketopt_error_explanation(int _code)
		{
			std::string message = "setsocketopt error:: ";
			
			switch(_code)
			{
				case EBADF:
				message += "the *socket* argument is not a valid file descriptor";
				break;
				
				case EDOM:
				message += "the send and receive timeout values are too big to fit into the timeout fields in the socket structure";
				break;
				
				case EINVAL:
				message += "the specified option is invalid at the specified socket level or the socket has been shut down";
				break;
				
				case EISCONN:
				message += "the socket is already connected, and a specified option cannot be set while the socket is connected";
				break;
				
				case ENOPROTOOPT:
				message += "the option is not supported by the protocol";
				break;
				
				case ENOTSOCK:
				message += "the *socket* argument does not refer to a socket";
				break;
				
				case ENOMEM:
				message += "there was insufficient memory available for the operation to complete";
				break;
				
				case ENOBUFS:
				message += "insufficient resources are available in the system to complete the call";
				break;
				
				default:
				message += "unknown error code (" + std::to_string(_code) + ")";
				break;
			}
			
			std::cout << message << std::endl;
		}
		
		void listener::print_ioctl_error_explanation(int _code)
		{
			std::string message = "ioctl error:: ";
			
			switch(_code)
			{
				case EBADF:
				message += "*fd* is not a valid file descriptor";
				break;
				
				case EFAULT:
				message += "*argp* references an inaccessible memory area";
				break;
				
				case EINVAL:
				message += "*request* or *argp* is not valid";
				break;
				
				case ENOTTY:
				message += "*fd* is not associated with a character special device";
				break;
				
				default:
				message += "unknown error code (" + std::to_string(_code) + ")";
				break;
			}
			
			std::cout << message << std::endl;
		}
