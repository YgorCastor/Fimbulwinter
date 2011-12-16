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
*                     Charserver from ZoneServer          	        *
* ==================================================================*/

#include "CharServer.hpp"

#include <show_message.hpp>
#include <database_helper.h>
#include <boost/thread.hpp>
#include <ragnarok.hpp>
#include <core.hpp>
#include <timers.hpp>
#include <iostream>
#include <boost/foreach.hpp>
#include <strfuncs.hpp>


/*==============================================================*
* Function:	Parse from Zone Server								*                                                     
* Author: TODO													*
* Date: TODO 													*
* Description: TODO									            *
**==============================================================*/
int CharServer::parse_from_zone(tcp_connection::pointer cl)
{
	if (cl->flags.eof)
	{
		typedef map<int, int> tmpdef;

		// Remove all maps from map_to_zone associated with this server
		int rindex = -1;
		BOOST_FOREACH(tmpdef::value_type &kvp, map_to_zone)
		{
			if (rindex != -1)
				map_to_zone.erase(rindex);

			if (kvp.second == cl->tag())
				rindex = kvp.first;
			else
				rindex = -1;
		}

		rindex = -1;
		BOOST_FOREACH(online_char_db::value_type &pair, online_chars)
		{
			if (rindex != -1)
				set_char_offline(rindex, -1);

			if (pair.second.server == cl->tag())
			{
				if (pair.second.disconnect_timer)
					TimerManager::FreeTimer(pair.second.disconnect_timer);

				rindex = pair.first;
			}
			else
				rindex = -1;
		}

		return 0;
	}
	
	while(RFIFOREST(cl) >= 2)
	{
		unsigned short cmd = RFIFOW(cl, 0);

		switch (cmd)
		{
		case INTER_ZC_MAPS:
			if (RFIFOREST(cl) < 4 || RFIFOREST(cl) < RFIFOW(cl, 2))
				return 0;
			{
				int num_maps = (RFIFOW(cl, 2) - 4) / sizeof(int);
				int accepted = 0;

				for (int i = 0; i < num_maps; i++)
				{
					int m = RFIFOL(cl, 4 + (i * sizeof(int)));

					if (!map_to_zone.count(m))
					{
						map_to_zone[m] = cl->tag();

						accepted++;
					}
				}

				ShowStatus("Received %d maps from ZoneServer %d.\n", accepted, cl->tag());

				cl->skip(RFIFOW(cl, 2));
			}
			break;
		default:
			ShowWarning("Unknown packet 0x%x sent from ZoneServer, closing connection.\n", cmd, cl->socket().remote_endpoint().address().to_string().c_str());
			cl->set_eof();
			return 0;
		}
	}

	return 0;
}

