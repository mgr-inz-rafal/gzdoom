#pragma once

#include <boost/asio.hpp>

class AActor;
struct FLevelLocals;

// TODO: Remove all "_to_gienek" suffixex and "gienek_" prefixes, since all is now
// located in the common API
class gienek_api
{
public:
	std::map<std::string, int16_t> typename_to_id_map = {
		{"CellPack",		142},
		{"Cell",			75},
		{"Berserk",			134},
		{"Chaingun",		28},
		{"PlasmaRifle",		30},
		{"PlasmaBall",		51},
		{"Zombieman",		4},
		{"BulletPuff",		131},
		{"Clip",			11}
	};

	boost::asio::io_context io_context;
	boost::asio::ip::tcp::socket gienek_socket {io_context};
	bool gienek_full_map_loaded;
	int32_t gienek_indexer;
public:
	gienek_api();

	void Gienek_Init(const char* address);
	void add_thing_to_gienek(AActor* a);
	void remove_thing_from_gienek(uint16_t index);
	void update_thing_pos_in_gienek(AActor* a);
	// TODO: This should be renamed to something like "send_map_geometry", since it
	// will (probably) not care about things
	void send_map_to_gienek(FLevelLocals* level);
	void start_sending_map();
	void reset();

private:
	bool map_delivery_in_progress;
	void stop_sending_map();
};
