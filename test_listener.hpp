#pragma once

#include <limits>
#include <vector>
#include <regex>
#include <mutex>

#include <cstring>
#include <cstdlib>

#include <unistd.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/poll.h>

#include <arpa/inet.h>
#include <netinet/in.h>

namespace test_listener
{
	constexpr uint64_t SEQUENCE_VALUE_HOLDER_LIMIT = std::numeric_limits<uint64_t>::max();
	
	struct sequence
	{
		uint16_t initial_value, step;
	};
	
	class client
	{
		public:
		const std::regex 
		command_split_regex = std::regex("\\s"),
		command_watchword_regex = std::regex("seq[0-3]"),
		command_number_regex = std::regex("[0-9]+");
		
		client(char[INET_ADDRSTRLEN], uint8_t = 3);
		
		~client();
		
		uint8_t read_stream(const int32_t _descriptor);
		
		std::vector<std::vector<uint64_t>> iterate() const;
		
		std::vector<struct sequence> get_sequences() const;
		
		const char* get_identifier() const;
		
		bool is_set_command(const std::vector<std::string>&) const;
		bool is_export_command(const std::vector<std::string>&) const;
		
		protected:
		std::vector<std::string> split(const std::string&) const;
		
		private:
		char identifier[INET_ADDRSTRLEN];
		
		std::vector<struct sequence> sequences;
	};
	
	class listener
	{
		public:
		listener(uint8_t, uint16_t, const char[INET_ADDRSTRLEN]);
		
		~listener();
		
		void run(bool = false);
		
		protected:
		void validate_clients();
		
		bool extract_ip(const int32_t, char*);
		
		void setup_polling();
		
		void create_socket();
		
		void bind_socket();
		
		void set_socket_options();
		
		void setup_socket();
		
		void print_listen_error_explanation(int);
		
		void print_socket_error_explanation(int);
		
		void print_bind_error_explanation(int);
		
		void print_connect_error_explanation(int);
		
		void print_accept_error_explanation(int);
		
		void print_setsocketopt_error_explanation(int);
		
		void print_ioctl_error_explanation(int);
		
		private:
		uint8_t backlog;
		uint16_t port;
		char ip[INET_ADDRSTRLEN];
		
		int32_t socket_descriptor;
		struct sockaddr_in socket_instance;
		
		uint16_t total_polled_connections;
		uint32_t polling_timeout;
		struct pollfd polling_pool[1024];
		
		std::vector<client*> client_pool;
		std::mutex client_pool_mutex;
	};
}
