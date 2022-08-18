#include "test_listener.hpp"

#include <iostream>

using test_listener::sequence;
using test_listener::client;

client::client(char _identifier[INET_ADDRSTRLEN], uint8_t _size) :
		sequences(_size)
		{
			memcpy(identifier, _identifier, sizeof(identifier));
		}
		
		client::~client()
		{
			sequences.clear();
		}
		
		uint8_t client::read_stream(const int32_t _descriptor)
		{
			char stream_buffer[256];
			int32_t stream_code;
			
			memset(stream_buffer, 0, sizeof(stream_buffer));
			
			do
			{
				stream_code = recv(_descriptor, 
										stream_buffer, sizeof(stream_buffer), 
										0);
				
				if(stream_code < 0)
				{
					return 1;
				}
				else if(stream_code == 0)
				{
					return 2;
				}
				
				const std::vector<std::string> command = split(stream_buffer);

				if(is_set_command(command))
				{
					if(!std::regex_match(command[1], command_number_regex)
						||
						!std::regex_match(command[2], command_number_regex))
					{
						return 3;
					}
					
					const uint32_t index = std::stoul(std::string(1, command[0].back()));
					
					const uint64_t 
					initial_value = std::stoull(command[1]),
					step = std::stoull(command[2]);
					
					sequences[index].initial_value = initial_value;
					sequences[index].step = step;
				}
				else if(is_export_command(command))
				{
					return 4;
				}
				else
				{
					return 5;
				}
			}
			while(stream_code > 0);
			
			return 0;
		}
		
		std::vector<std::vector<uint64_t>> client::iterate() const
		{
			std::vector<std::vector<uint64_t>> container;
			
			for(const struct sequence& item : sequences)
			{
				if(item.initial_value > 0 && item.step > 0)
				{
					std::vector<uint64_t> sequence_count_holder;
				
					uint64_t previous_value = 0;
					uint64_t current_value = item.initial_value;
					
					while(previous_value <= current_value && current_value)
					{
						sequence_count_holder.push_back(current_value);
						
						previous_value = current_value;
						current_value += item.step;
					}
					
					container.push_back(sequence_count_holder);
				}
			}
			
			return container;
		}
		
		std::vector<struct sequence> client::get_sequences() const
		{
			return sequences;
		}
		
		const char* client::get_identifier() const
		{
			return identifier;
		}
		
		bool client::is_set_command(const std::vector<std::string>& _sequence) const
		{
			return std::regex_search(_sequence.front(), 
									command_watchword_regex)
					&&
					_sequence.size() == 3;
		}
		
		bool client::is_export_command(const std::vector<std::string>& _sequence) const
		{
			return _sequence.back() == "seq" && _sequence.size() == 2;
		}
		
		std::vector<std::string> client::split(const std::string& _value) const
		{
			std::vector<std::string> result;
			
			std::sregex_token_iterator 
			begin
			{
				_value.begin(),
				_value.end(),
				command_split_regex,
				-1
			},
			end;
			
			for(const std::string& item : std::vector(begin, end))
			{
				if(!item.empty())
				{
					result.push_back(item);
				}
			}
			
			return result;
		}
