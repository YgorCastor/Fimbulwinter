/*============ Cronus++ developer team presents: ==========*
*	______ _           _           _           _		   *
*	|  ___(_)         | |         | |         | |		   *
*	| |_   _ _ __ ___ | |__  _   _| |_   _____| |_ _ __    *
*	|  _| | | '_ ` _ \| '_ \| | | | \ \ / / _ \ __| '__|   *
*	| |   | | | | | | | |_) | |_| | |\ V /  __/ |_| |      *
*	\_|   |_|_| |_| |_|_.__/ \__,_|_| \_/ \___|\__|_|      *
* -------------------------------------------------------- *
*               An Ragnarok Online Emulator                *
* -------------------------------------------------------- *
*                Licenced under GNU GPL v3                 *
* -------------------------------------------------------- *
*            Character Server Structures and classes	   *
* ======================================================== */

#pragma once

#include <config_file.hpp>
#include <tcp_server.hpp>
#include <string>
#include <soci/soci.h>
#include <ragnarok.hpp>

using namespace std;

enum auth_type
{
	auth_raw,
	auth_md5,
	auth_token,
};

struct CharSessionData
{
	bool auth;
	int account_id, login_id1, login_id2, sex;

	char email[40];
	time_t expiration_time;
	int gmlevel;

	int found_char[MAX_CHARS];

	unsigned int version;
	unsigned char clienttype;

	char birthdate[10+1];

	tcp_connection::pointer cl;
};

struct AuthNode
{
	int login_id1;
	int login_id2;

	char sex;

	unsigned char clienttype;
	unsigned int version;
};

struct OnlineChar 
{
	int account_id;
	int char_id;

	tcp_connection::pointer cl;
	int disconnect_timer;
	int server;
};

class CharDB;
class CharServer
{
public:
	typedef std::map<int, struct AuthNode> auth_node_db;
	typedef std::map<int, struct OnlineChar> online_account_db;

	struct login_config
	{
		// CharServer
		string			server_name;

		// Network
		string			network_bindip;
		unsigned short	network_bindport;

		// IP to sent to AuthServer
		string			network_charip;

		// Interconnection
		string			inter_login_ip;
		unsigned short	inter_login_port;
		string			inter_login_user;
		string			inter_login_pass;
	}; 

	static void run();
	static int parse_from_client(tcp_connection::pointer cl);
	static int parse_from_zone(tcp_connection::pointer cl);
	static int parse_from_login(tcp_connection::pointer cl);
	
	// Auth InterConn
	static void connect_to_auth();
	
	// Auth
	static void auth_ok(tcp_connection::pointer cl, CharSessionData *csd);
	static void set_charsel(int account_id, tcp_connection::pointer cl);
	static void set_char_offline(int account_id, char char_id);

	static void disconnect_char(int timer, int accid);
	static void send_chars(tcp_connection::pointer cl, CharSessionData *csd);
	static int char_to_buf(unsigned char *buf, CharData *ch);
	static int create_char(CharSessionData *csd, char* name_, int str, int agi, int vit, int int_, int dex, int luk, int slot, int hair_color, int hair_style);
	static int check_char_name(char *name);

	static bool auth_conn_ok;
	static tcp_connection::pointer auth_conn;

	// Config
	static config_file *auth_config;
	static config_file *database_config;

	static struct login_config config;

	// Network
	static boost::asio::io_service *io_service;
	static tcp_server *server;

	// Database
	static soci::session *database;
	static CharDB *chars;

	// Auth
	static auth_node_db auth_nodes;
	static online_account_db online_chars;
};

#include "CharDB.hpp"