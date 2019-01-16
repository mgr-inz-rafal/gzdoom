#include "gienek.h"

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <string>
#include <vector>
#include <chrono>
using boost::asio::ip::tcp;

#include "actor.h"
#include "a_pickups.h"
#include "g_levellocals.h"
#include "dobject.h"
#include "c_dispatch.h"
#include "p_tags.h"

extern std::map<std::string, int16_t> typename_to_id_map;

gienek_command_acceptor::gienek_command_acceptor(boost::asio::io_context& context, boost::asio::ip::tcp::socket& socket): _context(context), _socket(socket) {}

void gienek_command_acceptor::operator()()
{
	std::string buffer;
	buffer.resize(1);
	using namespace std::chrono_literals;
    tcp::acceptor acceptor(_context, tcp::endpoint(tcp::v4(), 14));
    acceptor.async_accept(_socket, [&](const boost::system::error_code& error) {
        if (!error) {
			for(;;)
			{
				std::this_thread::sleep_for(4ms);
				if (!_socket.available()) {
					continue;
				}
				boost::system::error_code incoming_code;
				boost::asio::read(_socket, boost::asio::buffer(buffer), boost::asio::transfer_at_least(buffer.size()),
								  incoming_code);
				if (incoming_code.value()) {
					throw std::runtime_error(incoming_code.message());
				}

				if(buffer == "U")
				{
					AddCommandString("+use");
				}
				if(buffer == "u")
				{
					AddCommandString("-use");
				}
				if(buffer == "L")
				{
					AddCommandString("+left");
				}
				if(buffer == "l")
				{
					AddCommandString("-left");
				}
				if(buffer == "R")
				{
					AddCommandString("+right");
				}
				if(buffer == "r")
				{
					AddCommandString("-right");
				}
				if(buffer == "F")
				{
					AddCommandString("+forward");
				}
				if(buffer == "f")
				{
					AddCommandString("-forward");
				}
				if(buffer == "B")
				{
					AddCommandString("+back");
				}
				if(buffer == "b")
				{
					AddCommandString("-back");
				}
				if(buffer == "A")
				{
					AddCommandString("+attack");
				}
				if(buffer == "a")
				{
					AddCommandString("-attack");
				}
			}
        }
    });
	for(;;) {
		_context.run();
	}
}

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

void gienek_api::connect_to_gienek(const char* address)
{
	std::vector<std::string> parts;
	boost::split(parts, address, [](char c){return c == ':';});
	if(2 != parts.size())
	{
		throw std::invalid_argument("Correct format for specifying Gienek server is SERVERNAME:PORT");
	}
	tcp::resolver resolver(io_context);
    tcp::resolver::results_type endpoints = resolver.resolve(parts.front(), parts.back());
    boost::asio::connect(gienek_reporting_socket, endpoints);
}

void gienek_api::setup_receiving_socket()
{
    cmdacc_thread = std::thread(cmdacc);
	cmdacc_thread.detach();
}

void gienek_api::Gienek_Init(const char* address)
{
	connect_to_gienek(address);
	setup_receiving_socket();
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
		boost::asio::write(gienek_reporting_socket, boost::asio::buffer(buf, sizeof(buf)), ignored_error);
	}
}

void gienek_api::remove_thing_from_gienek(uint16_t index)
{
	char buf[3];
	buf[0] = 'e';
	memcpy(&buf[1], &index, 2);

	boost::system::error_code ignored_error;
	boost::asio::write(gienek_reporting_socket, boost::asio::buffer(buf, sizeof(buf)), ignored_error);
}

void gienek_api::update_player_angle_in_gienek(double angle)
{
	while(angle < 0.0f)
	{
		angle += 360.0f;
	}
	angle = std::fmod(angle, 360.0f);
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
			boost::asio::write(gienek_reporting_socket, boost::asio::buffer(buf, sizeof(buf)), ignored_error);
		}
	}
}

void gienek_api::update_thing_pos_in_gienek(AActor* a)
{
	auto classname = a->GetClass()->TypeName.GetChars();
	uint16_t index = a->gienek_index;
	int16_t health = a->health;
	double angle = a->Angles.Yaw.Degrees;
	while(angle < 0.0f)
	{
		angle += 360.0f;
	}
	angle = std::fmod(angle, 360.0f);
	int16_t direction = static_cast<int16_t>(angle);
	int16_t posx = static_cast<int16_t>(a->X());
	int16_t posy = static_cast<int16_t>(a->Y());
	int16_t posz = static_cast<int16_t>(a->Z());
	int16_t type;
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
	boost::asio::write(gienek_reporting_socket, boost::asio::buffer(buf, sizeof(buf)), ignored_error);
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
		boost::asio::write(gienek_reporting_socket, boost::asio::buffer(buf, sizeof(buf)), ignored_error);
	}

	for (const auto &l : level->lines)
	{
		int16_t x1 = static_cast<int16_t>(l.v1->p.X);
		int16_t y1 = static_cast<int16_t>(l.v1->p.Y);
		int16_t x2 = static_cast<int16_t>(l.v2->p.X);
		int16_t y2 = static_cast<int16_t>(l.v2->p.Y);
		int16_t tag = tagManager.GetFirstLineID(&l);
		int16_t type = l.special;
		int16_t activation = l.activation;

		if(type != 0)
		{
			// Report line to Gienek
			char buf[15];
			buf[0] = 'i';
			memcpy(&buf[1], &x1, 2);
			memcpy(&buf[3], &y1, 2);
			memcpy(&buf[5], &x2, 2);
			memcpy(&buf[7], &y2, 2);
			memcpy(&buf[9], &tag, 2);
			memcpy(&buf[11], &type, 2);
			memcpy(&buf[13], &activation, 2);
			boost::system::error_code ignored_error;
			boost::asio::write(gienek_reporting_socket, boost::asio::buffer(buf, sizeof(buf)), ignored_error);
		}
	}

	for (const auto& sector: level->sectors)
	{
		char buf[7];
		buf[0] = 'h';

		int16_t floor = static_cast<int16_t>(sector.GetPlaneTexZ(sector_t::floor));
		int16_t ceiling = static_cast<int16_t>(sector.GetPlaneTexZ(sector_t::ceiling));
		int16_t tag = tagManager.GetFirstSectorTag(&sector);

		memcpy(&buf[1], &floor, 2);
		memcpy(&buf[3], &ceiling, 2);
		memcpy(&buf[5], &tag, 2);

		boost::system::error_code ignored_error;
		boost::asio::write(gienek_reporting_socket, boost::asio::buffer(buf, sizeof(buf)), ignored_error);
	}

	for (const auto &ssector: level->subsectors)
	{
		char buf[5];
		buf[0] = 'b';

		int16_t numlines = ssector.numlines;
		int16_t my_sector = ssector.sector->Index();

		memcpy(&buf[1], &numlines, 2);
		memcpy(&buf[3], &my_sector, 2);
		boost::system::error_code ignored_error;
		boost::asio::write(gienek_reporting_socket, boost::asio::buffer(buf, sizeof(buf)), ignored_error);

		int x = ssector.firstline->Index();
		for(std::size_t i = 0; i < ssector.numlines; ++i, ++x)
		{
			int16_t sti = level->segs[x].v1->Index();
			int16_t eni = level->segs[x].v2->Index();
			char buf[4];
			memcpy(&buf[0], &sti, 2);
			memcpy(&buf[2], &eni, 2);
			boost::system::error_code ignored_error;
			boost::asio::write(gienek_reporting_socket, boost::asio::buffer(buf, sizeof(buf)), ignored_error);
		}
	}

	stop_sending_map();
}

void gienek_api::start_sending_map()
{
	char buf[1];
	buf[0] = 'x';
	boost::system::error_code ignored_error;
	boost::asio::write(gienek_reporting_socket, boost::asio::buffer(buf, sizeof(buf)), ignored_error);

	map_delivery_in_progress = true;
}

void gienek_api::stop_sending_map()
{
	char buf[1];
	buf[0] = 'f';
	boost::system::error_code ignored_error;
	boost::asio::write(gienek_reporting_socket, boost::asio::buffer(buf, sizeof(buf)), ignored_error);
	gienek_full_map_loaded = true;

	map_delivery_in_progress = false;
}

int32_t gienek_api::get_next_index()
{
	return gienek_indexer++;
}
