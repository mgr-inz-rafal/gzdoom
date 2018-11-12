#include "gienek.h"

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <string>
#include <vector>
using boost::asio::ip::tcp;

#include "actor.h"
#include "a_pickups.h"
#include "g_levellocals.h"
#include "dobject.h"

extern std::map<std::string, int16_t> typename_to_id_map;

gienek_api::gienek_api()
{
	reset();
}

void gienek_api::reset()
{
	gienek_full_map_loaded = false;
	map_delivery_in_progress = false;
	gienek_indexer = 0;
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
	auto classname = a->GetClass()->TypeName.GetChars();
	a->gienek_index = get_next_index();

	uint16_t index = a->gienek_index;
	int16_t health = a->health;
	int16_t posx = static_cast<int16_t>(a->X());
	int16_t posy = static_cast<int16_t>(a->Y());
	int16_t posz = static_cast<int16_t>(a->Z());
	int16_t direction = static_cast<int16_t>(a->Angles.Yaw.Degrees);
	int16_t type;
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

void gienek_api::remove_thing_from_gienek(uint16_t index)
{
	char buf[3];
	buf[0] = 'e';
	memcpy(&buf[1], &index, 2);

	boost::system::error_code ignored_error;
	boost::asio::write(gienek_socket, boost::asio::buffer(buf, sizeof(buf)), ignored_error);
}

void gienek_api::update_player_angle_in_gienek(double angle)
{
	if(gienek_full_map_loaded)
	{
		int16_t direction = static_cast<int16_t>(angle);
		if(last_reported_angle != direction)
		{
			last_reported_angle = direction;
			char buf[3];
			buf[0] = 'g';
			memcpy(&buf[1], &direction, 2);

			boost::system::error_code ignored_error;
			boost::asio::write(gienek_socket, boost::asio::buffer(buf, sizeof(buf)), ignored_error);
		}
	}
}

void gienek_api::update_thing_pos_in_gienek(AActor* a)
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

void gienek_api::send_map_to_gienek(FLevelLocals* level)
{
	if(!map_delivery_in_progress)
	{
		start_sending_map();
	}

	gienek_full_map_loaded = false;

	for (const auto &v : level->vertexes)
	{
		int16_t x = static_cast<int16_t>(v.p.X);
		int16_t y = static_cast<int16_t>(v.p.Y);

		// Report vertex to Gienek
		// TODO: Take care of the network byte order!
		char buf[5];
		buf[0] = 'a';
		memcpy(&buf[1], &x, 2);
		memcpy(&buf[3], &y, 2);
		boost::system::error_code ignored_error;
		boost::asio::write(gienek_socket, boost::asio::buffer(buf, sizeof(buf)), ignored_error);
	}

	for (const auto &ssector: level->subsectors)
	{
		char buf[3];
		buf[0] = 'b';
		memcpy(&buf[1], &ssector.numlines, 2);
		boost::system::error_code ignored_error;
		boost::asio::write(gienek_socket, boost::asio::buffer(buf, sizeof(buf)), ignored_error);

		int x = ssector.firstline->Index();
		for(std::size_t i = 0; i < ssector.numlines; ++i, ++x)
		{
			int16_t sti = level->segs[x].v1->Index();
			int16_t eni = level->segs[x].v2->Index();
			char buf[4];
			memcpy(&buf[0], &sti, 2);
			memcpy(&buf[2], &eni, 2);
			boost::system::error_code ignored_error;
			boost::asio::write(gienek_socket, boost::asio::buffer(buf, sizeof(buf)), ignored_error);
		}
	}

	// Report things
	/*
	AActor*	t;
	for (auto &sec : level->sectors)
	{
		t = sec.thinglist;
		while (t)
		{
			uint16_t index = t->gienek_index;
			int16_t health = t->health;
			int16_t direction = static_cast<int16_t>(t->Angles.Yaw.Degrees);
			int16_t posx = static_cast<int16_t>(t->X());
			int16_t posy = static_cast<int16_t>(t->Y());
			int16_t posz = static_cast<int16_t>(t->Z());
			int16_t type;
			auto classname = t->GetClass()->TypeName.GetChars();
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
				// TODO: This is copy&pasted to p_mobj.cpp
				// Introduce kind of a gienek-client app with
				// appropriate interface, eg. send_thing()

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
				boost::asio::write(*gienek_global_socket, boost::asio::buffer(buf, sizeof(buf)), ignored_error);
			}

			t = t->snext;
		}
	}
	*/

	stop_sending_map();
}

void gienek_api::start_sending_map()
{
	char buf[1];
	buf[0] = 'x';
	boost::system::error_code ignored_error;
	boost::asio::write(gienek_socket, boost::asio::buffer(buf, sizeof(buf)), ignored_error);

	map_delivery_in_progress = true;
}

void gienek_api::stop_sending_map()
{
	char buf[1];
	buf[0] = 'f';
	boost::system::error_code ignored_error;
	boost::asio::write(gienek_socket, boost::asio::buffer(buf, sizeof(buf)), ignored_error);
	gienek_full_map_loaded = true;

	map_delivery_in_progress = false;
}

int32_t gienek_api::get_next_index()
{
	return gienek_indexer++;
}
