#pragma once
#include <boost/asio.hpp>

class AActor;
struct FLevelLocals;
using boost::asio::ip::tcp;

class gienek_command_acceptor {
	boost::asio::io_context& _context;
	boost::asio::ip::tcp::socket& _socket;
public:
	gienek_command_acceptor(boost::asio::io_context& context, boost::asio::ip::tcp::socket& socket);
    void operator()();
};

// TODO: Remove all "_to_gienek" suffixex and "gienek_" prefixes, since all is now
// located in the common API
class gienek_api
{

public:
	std::map<std::string, int16_t> typename_to_id_map = {
		{"DoomPlayer",		29999},
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

private:
	boost::asio::io_context io_context;
	boost::asio::ip::tcp::socket gienek_reporting_socket {io_context};
	boost::asio::io_context io_context2;
	boost::asio::ip::tcp::socket gienek_remote_control_socket {io_context2};
	gienek_command_acceptor cmdacc{io_context2, gienek_remote_control_socket};
	std::thread cmdacc_thread;

	bool gienek_full_map_loaded;
	int16_t last_reported_angle { 1 };	// No std::optional, choose the value that is less likely than 0, 90, etc. Still unsafe, though.

	void connect_to_gienek(const char* address);
	void setup_receiving_socket();

public:
	gienek_api();

	void Gienek_Init(const char* address);
	void add_thing_to_gienek(AActor* a);
	void remove_thing_from_gienek(uint16_t index);
	void update_thing_pos_in_gienek(AActor* a);
	void update_player_angle_in_gienek(double angle);
	// TODO: This should be renamed to something like "send_map_geometry", since it
	// will (probably) not care about things
	void send_map_to_gienek(FLevelLocals* level);
	void start_sending_map();
	void reset();
	int32_t get_next_index();
	void report_plane_height_change(int16_t sector_index, int16_t new_height, char plane);

private:
	int32_t gienek_indexer;
	bool map_delivery_in_progress;
	void stop_sending_map();
};
