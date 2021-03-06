/*==================================================================*
*     ___ _           _           _          _       _				*
*    / __(_)_ __ ___ | |__  _   _| |_      _(_)_ __ | |_ ___ _ __	*
*   / _\ | | '_ ` _ \| '_ \| | | | \ \ /\ / / | '_ \| __/ _ \ '__|	*
*  / /   | | | | | | | |_) | |_| | |\ V  V /| | | | | ||  __/ |		*
*  \/    |_|_| |_| |_|_.__/ \__,_|_| \_/\_/ |_|_| |_|\__\___|_|		*
*																	*
* ------------------------------------------------------------------*
*							 Emulator   			                *
* ------------------------------------------------------------------*
*                     Licenced under GNU GPL v3                     *
* ----------------------------------------------------------------- *
*                         Character Server 	               	        *
* ==================================================================*/
#include "CharServer.hpp"

#include  "../Common/show_message.hpp"
#include  "../Common/database_helper.h"
#include  "../Common/ragnarok.hpp"
#include  "../Common/packets.hpp"
#include  "../Common/timers.hpp"
#include  "../Common/strfuncs.hpp"
#include  "../Common/core.hpp"

#include <boost/thread.hpp>
#include <boost/foreach.hpp>
#include <iostream>

// Login InterConn
tcp_connection::pointer CharServer::auth_conn;

// Config
config_file *CharServer::char_config;

struct CharServer::login_config CharServer::config;

// Network
boost::asio::io_service *CharServer::io_service;
tcp_server *CharServer::server;

// Database
soci::session *CharServer::database;
CharDB *CharServer::chars;

// Auth
CharServer::auth_node_db CharServer::auth_nodes;
CharServer::online_char_db CharServer::online_chars;
bool CharServer::auth_conn_ok;

// Maps
map_index CharServer::maps;
map<int, int> CharServer::map_to_zone;

// Zone
CharServer::zone_server_db CharServer::servers;

/*! 
 *  \brief     Start Char Server
 *  \details   Start the char-server and load the confs
 *  \author    Fimbulwinter Development Team
 *  \author    GreenBox
 *  \date      08/12/11
 *
 **/
void CharServer::run()
{
	io_service = new boost::asio::io_service();

	// Read Config Files
	try
	{
		char_config = new config_file("./Config/charserver.conf");
		{
			config.server_name = char_config->read<string>("server.name", "Cronus++");

			config.network_bindip = char_config->read<string>("network.bindip", "0.0.0.0");
			config.network_bindport = char_config->read<unsigned short>("network.bindport", 6121);

			config.network_charip = char_config->read<string>("network.charip", "");

			config.inter_login_ip = char_config->read<string>("inter.login.ip", "127.0.0.1");
			config.inter_login_port = char_config->read<unsigned short>("inter.login.port", 6900);
			config.inter_login_user = char_config->read<string>("inter.login.user", "s1");
			config.inter_login_pass = char_config->read<string>("inter.login.pass", "p1");

			if (config.network_charip == "")
			{
				ShowInfo("Auto-detecting my IP Address...\n");
				
				tcp::resolver resolver(*io_service);
				tcp::resolver::query query(boost::asio::ip::host_name(), "");
				tcp::resolver::iterator iter = resolver.resolve(query);
				tcp::resolver::iterator end;

				while (iter != end)
				{
					tcp::endpoint ep = *iter++;

					if (!ep.address().is_v4())
						continue;
					
					config.network_charip = ep.address().to_string();

					break;
				}

				ShowStatus("Defaulting our IP Address to %s...\n", config.network_charip.c_str());
			}
		}
		ShowStatus("Finished reading charserver.conf.\n");
	}
	catch (config_file::file_not_found *fnf)
	{
		ShowFatalError("Config file not found: %s.\n", fnf->filename);
		return;
	}

	TimerManager::Initialize(io_service);
	
	if (!maps.load("./Data/map_index"))
	{
		getchar();
		abort();
	}

	// Initialize Database System
	{
		ShowInfo("Opening connection to database...\n");

		try
		{
			database = database_helper::get_session(char_config);
		}
		catch (soci::soci_error err)
		{
			ShowFatalError("Error opening database connection: %s\n", err.what());
			return;
		}

		chars = new CharDB(database);

		ShowSQL("Successfully opened database connection.\n");
	}

	connect_to_auth();

	// Initialize Network System
	{
		boost::system::error_code err;
		address_v4 bindip = address_v4::from_string(config.network_bindip, err);

		if (err)
		{
			ShowFatalError("%s\n", err.message().c_str());
			return;
		}

		server = new tcp_server(*io_service, (address)bindip, config.network_bindport);
		server->set_default_parser(CharServer::parse_from_client);

		ShowStatus("CharServer is ready and listening on %s:%d.\n", config.network_bindip.c_str(), config.network_bindport);
	}

	// Run IO service service and start pooling events
	io_service->run();
}

int main(int argc, char *argv[])
{
	srand((unsigned int)time(NULL));

	core_display_title();

	CharServer::run();

	getchar();
	return 0;
}


/*! 
 *  \brief     Set a character to offline mode
 *  
 *  \author    Fimbulwinter Development Team
 *  \author    GreenBox
 *  \date      08/12/11
 *
 **/
void CharServer::set_char_offline(int account_id, char char_id)
{
	if (char_id > 0)
	{
		statement s = (database->prepare << "UPDATE `char` SET `online` = 0 WHERE `char_id` = :c", use(char_id));
		s.execute();
	}
	else
	{
		statement s = (database->prepare << "UPDATE `char` SET `online` = 0 WHERE `account_id` = :a", use(account_id));
		s.execute();
	}

	if (online_chars.count(account_id))
	{
		if (online_chars[account_id].server > -1)
		{
			if (servers[online_chars[account_id].server].users > 0)
				servers[online_chars[account_id].server].users--;
		}

		if (online_chars[account_id].disconnect_timer)
			TimerManager::FreeTimer(online_chars[account_id].disconnect_timer);

		if (online_chars[account_id].char_id == char_id)
		{
			online_chars[account_id].char_id = -1;
			online_chars[account_id].server = -1;
		}
	}

	if (auth_conn_ok && (char_id == -1 || !online_chars.count(account_id) || online_chars[account_id].cl->flags.eof))
	{
		WFIFOHEAD(auth_conn,6);
		WFIFOW(auth_conn,0) = INTER_CA_SET_ACC_OFF;
		WFIFOL(auth_conn,2) = account_id;
		auth_conn->send_buffer(6);
	}
}

/*! 
 *  \brief     Set a Character to online mode
 *  
 *  \author    Fimbulwinter Development Team
 *  \author    GreenBox
 *  \date      08/12/11
 *
 **/
void CharServer::set_char_online(int server, int char_id, int account_id)
{
	statement s = (database->prepare << "UPDATE `char` SET `online` = 1 WHERE `char_id` = :c", use(char_id));
	s.execute();

	online_chars[account_id].server = server;
	online_chars[account_id].char_id = char_id;

	if (online_chars[account_id].disconnect_timer)
		TimerManager::FreeTimer(online_chars[account_id].disconnect_timer);
}
