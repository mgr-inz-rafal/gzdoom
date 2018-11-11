#include "gienek.h"

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <string>
#include <vector>
using boost::asio::ip::tcp;

#include "actor.h"

extern std::map<std::string, int16_t> typename_to_id_map;

gienek_api::gienek_api()
{
	gienek_full_map_loaded = false;
}

void gienek_api::Gienek_Init(const char* address)
{
	std::vector<std::string> parts;
	boost::split(parts, address, [](char c){return c == ':';});
	if(2 != parts.size())
	{
		throw std::invalid_argument("Correct format for specifying Gienek server is SERVERNAME:PORT");
	}
	tcp::resolver resolver(io_context);
    tcp::resolver::results_type endpoints = resolver.resolve(parts.front(), parts.back());

    boost::asio::connect(gienek_socket, endpoints);
}

void gienek_api::add_thing_to_gienek(AActor* a)
{
	if(gienek_full_map_loaded)
	{
		uint16_t index = a->gienek_index;
		int16_t health = a->health;
		int16_t direction = static_cast<int16_t>(a->Angles.Yaw.Degrees);
		int16_t posx = static_cast<int16_t>(a->X());
		int16_t posy = static_cast<int16_t>(a->Y());
		int16_t posz = static_cast<int16_t>(a->Z());
		int16_t type;
		auto classname = a->GetClass()->TypeName.GetChars();
		if(typename_to_id_map.find(classname) != typename_to_id_map.end())
		{
			type = typename_to_id_map[classname];
		}
		else
		{
			type = 0;
		}

		if(type != 0)
		{
			char buf[15];
			buf[0] = 'c';
			memcpy(&buf[1], &index, 2);
			memcpy(&buf[3], &health, 2);
			memcpy(&buf[5], &direction, 2);
			memcpy(&buf[7], &posx, 2);
			memcpy(&buf[9], &posy, 2);
			memcpy(&buf[11], &posz, 2);
			memcpy(&buf[13], &type, 2);

			boost::system::error_code ignored_error;
			boost::asio::write(gienek_socket, boost::asio::buffer(buf, sizeof(buf)), ignored_error);
		}
	}
}

void gienek_api::remove_thing_from_gienek(uint16_t index)
{
	if(gienek_full_map_loaded)
	{
		char buf[3];
		buf[0] = 'e';
		memcpy(&buf[1], &index, 2);

		boost::system::error_code ignored_error;
		boost::asio::write(gienek_socket, boost::asio::buffer(buf, sizeof(buf)), ignored_error);
	}
}

void gienek_api::update_thing_pos_in_gienek(AActor* a)
{
	if(gienek_full_map_loaded)
	{
		uint16_t index = a->gienek_index;
		if(index == 0)
		{
			// Do not sent items out of Gienek's interest, like temporary BulletPuffs
			return;
		}
		int16_t health = a->health;
		int16_t direction = static_cast<int16_t>(a->Angles.Yaw.Degrees);
		int16_t posx = static_cast<int16_t>(a->X());
		int16_t posy = static_cast<int16_t>(a->Y());
		int16_t posz = static_cast<int16_t>(a->Z());
		int16_t type;
		auto classname = a->GetClass()->TypeName.GetChars();
		if(typename_to_id_map.find(classname) != typename_to_id_map.end())
		{
			type = typename_to_id_map[classname];
		}
		else
		{
			type = 0;
		}

		char buf[15];
		buf[0] = 'd';
		memcpy(&buf[1], &index, 2);
		memcpy(&buf[3], &health, 2);
		memcpy(&buf[5], &direction, 2);
		memcpy(&buf[7], &posx, 2);
		memcpy(&buf[9], &posy, 2);
		memcpy(&buf[11], &posz, 2);
		memcpy(&buf[13], &type, 2);

		boost::system::error_code ignored_error;
		boost::asio::write(gienek_socket, boost::asio::buffer(buf, sizeof(buf)), ignored_error);
	}
}
