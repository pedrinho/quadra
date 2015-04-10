/* -*- Mode: C++; c-basic-offset: 2; tab-width: 2; indent-tabs-mode: nil -*-
 * 
 * Quadra, an action puzzle game
 * Copyright (C) 1998-2000  Ludus Design
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "pane.h"

#include "input.h"
#include "zone_text_clock.h"
#include "quadra.h"
#include "cfgfile.h"
#include "multi_player.h"
#include "chat_text.h"
#include "bloc.h"
#include "game.h"
#include "net_server.h"
#include "packets.h"
#include "texte.h"
#include "global.h"
#include "sons.h"
#include "recording.h"
#include "canvas.h"
#include "crypt.h"
#include "unicode.h"

using std::vector;

Pane_info::Pane_info(Bitmap *bit, Font *f2, Inter *in, int j, Multi_player *pmp) {
	fond = bit;
	font2 = f2;
	inter = in;
	x = j*214;
	y = 37;
	w = 212;
	h = 480-y;
	mp = pmp;
	quel_pane = j;
	back = new Bitmap((*bit)[y]+x, w, 18*20, bit->realwidth);
	back_bottom = new Bitmap((*bit)[y+18*20]+x, w, 480-y-18*20, bit->realwidth);
}

Pane_info::~Pane_info() {
	delete back_bottom;
	delete back;
}

Pane::Pane(const Pane_info &p, bool dback, bool dbottom):
	Zone(p.inter, p.x, p.y, p.w, p.h),
	pi(p) {
	hiden = false;
	screen = video->new_bitmap(pi.x, pi.y, pi.w, pi.h);
	draw_background = dback;
	draw_bottom = dbottom;
}

Pane::~Pane() {
	delete screen;
	video->need_paint = 2;
}

void Pane::hidecall(Module *m) {
	hide();
	call(m);
}

void Pane::hideexec(Module *m) {
	hide();
	exec(m);
}

void Pane::set_net_pane(int i) {
	if(!playback && !game->single)
		config.info.pane[pi.quel_pane] = i;
}

void Pane::ifdone() {
	if(done)
		set_net_pane(0);
}

void Pane::draw() {
	Zone::draw();
	if(draw_background)
		pi.back->draw(screen,0,0);
	if(draw_bottom)
		pi.back_bottom->draw(screen,0,18*20);
}

void Pane::hide() {
	if(hiden == true)
		return;
	hide_item();
	hiden = true;
}

void Pane::hide_item() {
	for(size_t i=0; i<zone.size(); i++)
		zone[i]->disable();
	disable();
}

void Pane::show() {
	if(hiden == false)
		return;
	show_item();
	hiden = false;
}

void Pane::show_item() {
	for (size_t i = 0; i < zone.size(); i++)
		zone[i]->enable();
	enable();
}

void Pane::step() {
	clicked = inter->clicked;
	show();
}

Pane_option::Pane_option(const Pane_info &p): Pane(p) {
	int x2=x+15;
	int y2=69, y_height=22;
	int i;
	if(!playback) {
		for(i=0; i<3; i++) {
			sprintf(st, ST_STARTBOB, config.player[i].name);
			player[i] = new Zone_text_button2(inter, pi.fond, pi.font2, st, x2, y2, w-44);
			y2+=y_height;
			zone.push_back(player[i]);
		}
	}
	y2+=y_height;
	player_info = new Zone_text_button2(inter, pi.fond, pi.font2, ST_SHOWPLAYER, x2, y2, w-44); y2+=y_height;
	zone.push_back(player_info);
	block_info = new Zone_text_button2(inter, pi.fond, pi.font2, ST_BLOCKINFO, x2, y2, w-44); y2+=y_height;
	zone.push_back(block_info);
	combo_info = new Zone_text_button2(inter, pi.fond, pi.font2, ST_LINESINFO, x2, y2, w-44); y2+=y_height;
	zone.push_back(combo_info);
	select_scheme = new Zone_text_button2(inter, pi.fond, pi.font2, ST_SELECTSCHEME, x2, y2, w-44); y2+=y_height;
	zone.push_back(select_scheme);

	chat_window = server = quit = NULL;

	chat_window = new Zone_text_button2(inter, pi.fond, pi.font2, ST_CHATWINDOW, x2, y2, w-44); y2+=y_height;
	zone.push_back(chat_window);
	y2+=y_height;
	if(game->network) {
		if(!playback) {
			server = new Zone_text_button2(inter, pi.fond, pi.font2, ST_SERVERTOOL, x2, y2, w-44); y2+=y_height;
			zone.push_back(server);
		}
	}
	if(pi.quel_pane == 2 && !game->single && !playback) {
		quit = new Zone_text_button2(inter, pi.fond, pi.font2, ST_QUIT, 560, 450);
		zone.push_back(quit);
	}
	game->net_list.add_watch(this);
}

Pane_option::~Pane_option() {
	game->net_list.remove_watch(this);
}

void Pane_option::init() {
	Pane::init();
	uint8_t t = config.info.pane[pi.quel_pane];
	if(!game->network) {
		if(t==6) // no Pane_server in local mode
			t = 0;
	}
	if(playback && !playback->single()) {
		switch(pi.quel_pane) {
			case 0: t=2; break;
			case 1: t=2; break;
			case 2: t=3; break;
		}
	}
	switch(t) {
		case 1: hidecall(new Pane_selectscheme(pi)); break;
		case 2: hidecall(new Pane_playerinfo(pi)); break;
		case 3: hidecall(new Pane_chat(pi)); break;
		case 4: hidecall(new Pane_blockinfo(pi)); break;
		case 5: hidecall(new Pane_comboinfo(pi)); break;
		case 6:	hidecall(new Pane_server(pi)); break;
	}
}

void Pane_option::step() {
	Pane::step();
	if(!Pane::clicked)
		return;
	if(Pane::clicked == select_scheme) {
		hidecall(new Pane_selectscheme(pi));
	}
	if(Pane::clicked == player_info) {
		hidecall(new Pane_playerinfo(pi));
	}
	if(Pane::clicked == chat_window) {
		hidecall(new Pane_chat(pi));
	}
	if(Pane::clicked == block_info) {
		hidecall(new Pane_blockinfo(pi));
	}
	if(Pane::clicked == combo_info) {
		hidecall(new Pane_comboinfo(pi));
	}
	if(Pane::clicked == server) {
		hidecall(new Pane_server(pi));
	}
	if(Pane::clicked == quit) {
		game->abort = true;
	}
	for(int i=0; i<3; i++) {
		if(Pane::clicked == player[i]) {
			hidecall(new Pane_pre_start(pi, i));
			break;
		}
	}
}

void Pane_option::notify() {
	if(playback)
		return;
	int i, j;
	for(j=0; j<3; j++) {
		bool found = false;
		for(i=0; i<MAXPLAYERS; i++) {
			Canvas *c = game->net_list.get(i);
			if(c && c->islocal() && c->player == j) {
				found = true;
				break;
			}
		}
		if(found)
			sprintf(st, ST_RESUMEBOB, config.player[j].name);
		else
			sprintf(st, ST_STARTBOB, config.player[j].name);

		player[j]->set_text(st);
	}
}

Pane_pre_start::Pane_pre_start(const Pane_info &p, int q): Pane(p) {
	qplayer = q;
	for(int i=0; i<3; i++)
		if(pi.mp->pane[i]) {
			Pane_option *po = (Pane_option *) pi.mp->pane[i];
			po->player[qplayer]->disable();
		}
}

Pane_pre_start::~Pane_pre_start() {
	for(int i=0; i<3; i++)
		if(pi.mp->pane[i]) {
			Pane_option *po = (Pane_option *) pi.mp->pane[i];
			po->player[qplayer]->enable();
		}
}

void Pane_pre_start::init() {
	Pane::init();
	Canvas *canvas = NULL;
	for(int j=0; j<MAXPLAYERS; j++) {
		Canvas *c = game->net_list.get(j);
		if(c && c->islocal() && c->player == qplayer) {
			canvas = c;
		}
	}
	if(canvas == NULL)
		exec(new Pane_playerstartup(pi, qplayer));
	else
		exec(new Pane_startgame(pi, qplayer, canvas));
}

Pane_singleplayer::Pane_singleplayer(const Pane_info &p): Pane(p) {
	if(!playback) {
		game->paused = true; //-roncli 4/29/01 Pause the game so that the timer doesn't start.
		int x2=x+15;
		for(int i=0; i<3; i++) {
			sprintf(st, ST_STARTBOB,config.player[i].name);
			player[i] = new Zone_text_button2(inter, pi.fond, pi.font2, st, x2, i*22+ 69);
			zone.push_back(player[i]);
		}
	}
}

void Pane_singleplayer::step() {
	Pane::step();
	int i;
	if(!playback) {
		for(i=0; i<3; i++) {
			if(Pane::clicked == player[i]) {
				game->paused = false; //-roncli 4/29/01 Unpause the game so that the timer starts when it should
				hideexec(new Pane_playerjoin(pi, i));
			}
		}
	}
	else {
		if(game)
			for(i=0; i<MAXPLAYERS; i++) {
				Canvas *c=game->net_list.get(i);
				if(c)
					hideexec(new Pane_startgame(pi, -1, c));
			}
	}
}

bool Pane_close::global_clock_visible = false;

Pane_close::Pane_close(const Pane_info &p, bool dback, bool dbottom):
	Pane(p, dback, dbottom) {
	close = NULL;
	if(!game->single)
		if(!(playback && playback->auto_demo))
			close = new Zone_text_button2(pi.inter, pi.fond, pi.font2, ST_CLOSE, pi.x+146, 10);
	clock = NULL;
	clock_visible = false;
	seconds = 0;
}

Pane_close::~Pane_close() {
	if(close)
		delete close;
	if(clock) {
		delete clock;
		if(clock_visible)
			global_clock_visible = false;
	}
}

void Pane_close::hide_item() {
	Pane::hide_item();
	if(close)
		close->disable();
	if(clock && clock_visible) {
		clock->disable();
		clock_visible = global_clock_visible = false;
	}
}

void Pane_close::show_item() {
	Pane::show_item();
	if(close)
		close->enable();
	if(clock)
		show_clock();
}

void Pane_close::show_clock() {
	if(!clock_visible && !global_clock_visible) {
		clock->enable();
		clock_visible = global_clock_visible = true;
	}
}

void Pane_close::step() {
	Pane::step();
	if(clicked) {
		if(clicked == close) {
			ret();
		}
	}
	if(clock) {
		show_clock();
		if(clock_visible)
			update_clock();
	}
}

void Pane_close::update_clock() {
	uint32_t timer=game->net_list.gettimer();
	if(timer < (uint32_t)game->game_end_value)
		seconds=(game->game_end_value - timer+99)/100;
	else
		seconds=0;
}

void Pane_close::allow_clock() {
	if(game->game_end == 2) {
		update_clock();
		clock = new Zone_text_clock(inter, &seconds, x+10, 10, 90, false, pi.mp->courrier);
		clock->disable();
		show_clock();
	}
}

Pane_selectscheme::Pane_selectscheme(const Pane_info &p):
	Pane_close(p) {
	int i;
	for(i=0; i<3; i++)
		((Pane_option *) pi.mp->pane[i])->select_scheme->disable();

	bit = pi.mp->bit;
	pal = &pi.mp->pal;
	zone.push_back(new Zone_text(pi.inter, ST_SELECTCOLORSCHEME, pi.x+19, 50));
	for(i=0; i<=config.info.unlock_theme; i++) {
		sprintf(st, ST_FROMLEVEL,i+1);
		level[i] = new Zone_text_button2(pi.inter, pi.fond, pi.font2, st, pi.x+39, 70+i*22);
		zone.push_back(level[i]);
	}
	if(_debug) {
		debugscheme = new Zone_text_button2(pi.inter, pi.fond, pi.font2, "Black", pi.x+39, 70+i*22);
		zone.push_back(debugscheme);
	}
	else
		debugscheme = NULL;
	set_net_pane(1);
	allow_clock();
}

Pane_selectscheme::~Pane_selectscheme() {
	for(int i=0; i<3; i++)
		if(pi.mp->pane[i])
			((Pane_option *) pi.mp->pane[i])->select_scheme->enable();
}

void Pane_selectscheme::step() {
	Pane_close::step();
	ifdone();
	int i;
	for(i=0; i<=config.info.unlock_theme; i++) {
		if(clicked && clicked == level[i]) {
			Canvas::change_level(i+1, pal, bit);
			pi.inter->font->colorize(*pal, 255, 255, 255);
			pi.mp->courrier->colorize(*pal, 255, 255, 255);
			pal->set();
			if(!playback) {
				config.info.multi_level = i+1;
				config.write();
			}
		}
	}
	if(clicked && clicked == debugscheme) {
		Canvas::change_level(-1, pal, bit);
	}
}

Pane_server::Zone_update_rate::Zone_update_rate(Inter* in, const Palette &pal, int px, int py, int pw):
	Zone_text_input(in, pal, (sprintf(port_st, "%i",config.info.update_rate), port_st), 3, px, py, pw) {
}

void Pane_server::Zone_update_rate::lost_focus(int cancel) {
	if(!cancel) {
		int port_num;
		if(sscanf(Zone_text_input::st, "%i", &port_num) != 1)
			port_num = 0;
		if(port_num >= 0 && port_num <=99 && !playback) {
			config.info.update_rate = port_num;
			config.write();
		} else {  // if invalid, cancel the input
			cancel = 1;
		}
	}
	Zone_text_input::lost_focus(cancel);
}

Pane_server::Pane_server(const Pane_info &p):
	Pane_close(p) {
	int i;
	for(i=0; i<3; i++)
		((Pane_option *) pi.mp->pane[i])->server->disable();

	zone.push_back(new Zone_text(pi.inter, ST_SERVERTOOL, pi.x+19, 50));

	int x2=x+15;
	int y2=80, y_height=40;
	Zone *temp;

	if(game->server) {
		drop_player = new Zone_text_button2(inter, pi.fond, pi.font2, ST_DROPPLAYER, x2, y2, w-44); y2+=y_height;
		zone.push_back(drop_player);
		drop_connection = new Zone_text_button2(inter, pi.fond, pi.font2, ST_DROPCONNECTION, x2, y2, w-44); y2+=y_height;
		zone.push_back(drop_connection);

		{
			Zone_state_text2 *temp = new Zone_state_text2(inter, &game->server_accept_player, x2 + 120, y2+17);
			temp->add_string(ST_YES);
			temp->add_string(ST_NO);
			accept_player = temp;
		}
		zone.push_back(accept_player);
		temp = new Zone_text(pi.inter, ST_ACCEPTPLAYER, x2, y2);
		accept_player->set_child(temp);
		zone.push_back(temp);
		y2+=y_height;

		{
			Zone_state_text2 *temp = new Zone_state_text2(inter, &game->server_accept_connection, x2 + 120, y2+17);
			temp->add_string(ST_YES);
			temp->add_string(ST_NO);
			accept_connection = temp;
		}
		zone.push_back(accept_connection);
		temp = new Zone_text(pi.inter, ST_ACCEPTCONNECTION, x2, y2);
		accept_connection->set_child(temp);
		zone.push_back(temp);
		y2+=y_height+5;
	} else {
		drop_player = drop_connection = accept_player = accept_connection = NULL;
	}
	test_ping = new Zone_text_button2(inter, pi.fond, pi.font2, ST_TESTPING, x2, y2, w-44); y2+=y_height;
	zone.push_back(test_ping);

	zone.push_back(new Zone_text(pi.inter, ST_SETUPDATESPEED, x2, y2));
	zone.push_back(new Zone_text(pi.inter, ST_SETUPDATESPEED2, x2, y2+20));
	zone.push_back(new Zone_update_rate(inter, pi.mp->pal, x2 + 20, y2+37, 60));
	y2 += 37+y_height;

	check_ip = new Zone_text_button2(inter, pi.fond, pi.font2, ST_IPINFO, x2, y2, w-44); y2+=y_height;
	zone.push_back(check_ip);

	set_net_pane(6);
	allow_clock();
}

Pane_server::~Pane_server() {
	for(int i=0; i<3; i++)
		if(pi.mp->pane[i])
			((Pane_option *) pi.mp->pane[i])->server->enable();
}

void Pane_server::step() {
	Pane_close::step();
	ifdone();
	if(clicked) {
		if(clicked == drop_player) {
			hidecall(new Pane_server_drop_player(pi));
		}
		if(clicked == drop_connection) {
			hidecall(new Pane_server_drop_connection(pi));
		}
		if(clicked == test_ping) {
			hidecall(new Pane_server_ping(pi));
		}
		if(clicked == check_ip) {
			hidecall(new Pane_server_ip(pi));
		}
	}
}

Pane_server_drop_player::Pane_server_drop_player(const Pane_info &p):
	Pane_close(p) {

	b_drop = new Zone_text_button2(inter, pi.fond, pi.font2, ST_DROPTHISPLAYER, x+15, 340, w-44);
	zone.push_back(b_drop);
	zone.push_back(new Zone_text(inter, ST_DROPPLAYER, x+9, 50, w-30));
	list_player = new Zone_listbox2(inter, pi.fond, inter->font, &selected_player, x+19, 80, 160, 220);
	zone.push_back(list_player);
	game->net_list.add_watch(this);
	notify();
	allow_clock();
}

Pane_server_drop_player::~Pane_server_drop_player() {
	game->net_list.remove_watch(this);
}

void Pane_server_drop_player::notify() {
	list_player->clear();
	for(int i=0; i<MAXPLAYERS; i++) {
		Canvas *c = game->net_list.get(i);
		if(c) {
			List_player *l = new List_player(c->name, i, fteam[c->color]);
			list_player->add_item(l);
		}
	}
	list_player->dirt();
}

void Pane_server_drop_player::step() {
	Pane_close::step();
	bool drop = false;
	if(clicked == b_drop)
		drop = true;
	if(inter->double_clicked && list_player->in_listbox(inter->double_clicked))
		drop = true;
	if(drop && selected_player != -1) {
		List_player *lp = (List_player *) list_player->get_selected();
		game->net_list.server_drop_player(lp->player, DROP_MANUAL);
	}
}

Pane_server_drop_connection::Pane_server_drop_connection(const Pane_info &p):
	Pane_close(p) {
	b_drop = new Zone_text_button2(inter, pi.fond, pi.font2, ST_DROPTHISCONNECTION, x+15, 340, w-44);
	zone.push_back(b_drop);
	zone.push_back(new Zone_text(inter, ST_DROPCONNECTION, x+9, 50, w-30));
	list_connection = new Zone_listbox2(inter, pi.fond, pi.font2, &selected, x+19, 80, 160, 220);
	zone.push_back(list_connection);
	game->net_list.add_watch(this);
	net->add_watch(this);
	notify();
	allow_clock();
}

Pane_server_drop_connection::~Pane_server_drop_connection() {
	net->remove_watch(this);
	game->net_list.remove_watch(this);
}

void Pane_server_drop_connection::notify() {
	list_connection->clear();
	for(size_t i=0; i<net->connections.size(); i++) {
		Net_connection *nc=net->connections[i];
		if(nc == game->loopback_connection)
			continue; // skip the local address
		char st2[256];
		Net::stringaddress(st2, nc->address());
		sprintf(st, "%s:%i", st2, nc->getdestport());
		List_connection *l = new List_connection(st, nc);
		list_connection->add_item(l);
		for(int j=0; j<MAXPLAYERS; j++) {
			Canvas *c = game->net_list.get(j);
			if(c && c->remote_adr == nc) {
				sprintf(st, "[%s]", c->name);
				List_connection *l2 = new List_connection(st, nc, fteam[c->color]);
				list_connection->add_item(l2);
			}
		}
	}
	list_connection->dirt();
}

void Pane_server_drop_connection::step() {
	Pane_close::step();
	bool drop = false;
	if(clicked == b_drop)
		drop = true;
	if(inter->double_clicked && list_connection->in_listbox(inter->double_clicked))
		drop = true;
	if(drop && selected != -1) {
		List_connection *l = (List_connection *) (list_connection->get_selected());
		msgbox("Pane_server_drop_connection: dropping address %x (%s)...\n", l->c->address(), l->list_name);
		l->c->disconnect();
		notify(); // force an update of the window (to remove the destroyed item)
	}
}

Pane_server_ping::Pane_server_ping(const Pane_info &p): Pane_close(p) {
	zone.push_back(new Zone_text(inter, ST_TESTPING, x+9, 50, w-30));
	zone.push_back(new Zone_text(inter, ST_PINGDELAY, x+9, 100, w-30));
	pingtime = moyenne = total = nombre = 0;
	zone.push_back(new Zone_text_field(inter, &pingtime, x+19, 130, 90));
	zone.push_back(new Zone_text(inter, ST_PINGMOYEN, x+9, 190, w-30));
	zone.push_back(new Zone_text_field(inter, &moyenne, x+19, 220, 90));
	net->addwatch(P_TESTPING, this);
	test_delay = 60;
	allow_clock();
}

Pane_server_ping::~Pane_server_ping() {
	net->removewatch(P_TESTPING, this);
}

void Pane_server_ping::step() {
	Pane_close::step();
	if(test_delay == 1) {
		send_test();
	}
	if(test_delay > 1)
		test_delay--;

}

void Pane_server_ping::send_test() {
	Packet_clienttestping p;
	last_frame = p.frame = overmind.framecount;
	net->sendtcp(&p);
	test_delay = 0;
}

void Pane_server_ping::net_call(Packet *p2) {
	Packet_testping *p = (Packet_testping *) p2;
	uint32_t delay = overmind.framecount - p->frame;
	pingtime = delay*10;
	nombre++;
	total += delay;
	moyenne = (total*10)/nombre;
	test_delay = 200;
}

Pane_server_ip::Pane_server_ip(const Pane_info &p): Pane_close(p) {
	zone.push_back(new Zone_text(inter, ST_IPINFO, x+9, 50, w-30));
	zone.push_back(new Zone_text(inter, ST_HOSTNAME, x+9, 100));
	zone.push_back(new Zone_text_field(inter, net->host_name, x+29, 120, 140));
	zone.push_back(new Zone_text(inter, ST_HOSTLIST, x+9, 140));
	Zone_listbox *list;
	list = new Zone_listbox2(inter, pi.fond, pi.font2, NULL, x+29, 160, 140, 200);
	for(size_t i=0; i<net->host_adr.size(); i++) {
		Net::stringaddress(st, net->host_adr[i]);
		list->add_item(st);
	}
	zone.push_back(list);
	allow_clock();
}

Pane_playerinfo::Pane_playerinfo(const Pane_info &p): Pane_close(p, true, true) {
	auto_watch=false;
	game->net_list.add_watch(this);
	show_quoi = 0;
	o_show_val = 0;
	show_button = auto_button = NULL;
	clear_tag();
	refresh();
	set_net_pane(2);
	allow_clock();
	if(playback && playback->auto_demo)
		activate_auto_watch();
	else
		deactivate_auto_watch();
}

Pane_playerinfo::~Pane_playerinfo() {
	game->net_list.remove_watch(this);
}

void Pane_playerinfo::activate_auto_watch() {
	auto_watch=true;
	if(auto_button)
		auto_button->set_font(fteam[5]);
}

void Pane_playerinfo::deactivate_auto_watch() {
	auto_watch=false;
	if(auto_button)
		auto_button->set_font(inter->font);
}

void Pane_playerinfo::notify() {
	refresh();
}

void Pane_playerinfo::process() {
	Pane_close::process();
	int i,j;
	for(i=0; i<MAXTEAMS; i++)
		total0[i] = total1[i] = 0;
	for(i=0; i<MAXPLAYERS; i++) {
		Canvas* c=game->net_list.get(i);
		if(c) {
			if(show_quoi == 0)
				total1[c->color] += c->stats[CS::DEATH].get_value();
			total0[c->color] += c->stats[quel_stat()].get_value();
		} else {
			for(j=0; j<4; j++)
				if(tagged[j] == i) {
					tagged[j] = -1;
					break;
				}
		}
	}
}

void Pane_playerinfo::refresh() {
	deleteall();
	o_show_val=0;
	int x2=x+11;
	int y2=42, y_height=20;
	size_t i;

	for(i=0; i<MAXPLAYERS; i++)
		player[i]=NULL;
	for(i=0; i<MAXTEAMS; i++)
		total0[i] = total1[i] = 0;
	zone.push_back(new Zone_text(inter, ST_SHOWWHAT, x2, 405));
	Zone_state_text2 *temp = new Zone_state_text2(inter, &show_quoi, x2+69, 405);
	temp->add_string(ST_SHOWFRAG);
	temp->add_string(ST_SHOWSCORE);
	temp->add_string(ST_SHOWLINE);
	temp->add_string(ST_SHOWBLOC);
	temp->add_string(ST_SHOWBPM);
	temp->add_string(ST_SHOWPPM);
	zone.push_back(temp);
	o_show_val=show_quoi;
	bool count=false;

	for(int team=0; team<MAXTEAMS; team++) {
		int nb = 0;
		for(i=0; i<MAXPLAYERS; i++) {
			Canvas* c=game->net_list.get(i);
			if(c && c->color == team) {
				if(!c->islocal())
					count = true;
				if(nb == 0)
					y2 += 6;
				add_name(c, i, x2, y2);
				y2+=y_height;
				nb++;
			}
		}
		if(nb > 1) {
			add_total(team, x2, y2);
			y2+=y_height;
		}
	}

	for(i=0; i<4; i++) // removes those 'tagged' that are now NULL (because they're dropped)
		if(tagged[i] != -1) {
			Canvas *c = game->net_list.get(tagged[i]);
			if(!c)
				tagged[i] = -1;
		}

	show_button = auto_button = NULL;
	if(game->network && !(playback && playback->auto_demo)) {
		y2 += 6;
		if(count) {
			show_button = new Zone_text_button2(inter, pi.fond, pi.font2, ST_SHOWSELECTED, x2+12, y2);
			zone.push_back(show_button);
			y2 += 24;
		}
		auto_button = new Zone_text_button2(inter, pi.fond, pi.font2, ST_AUTOWATCH, x2+24, y2);
		zone.push_back(auto_button);
		if(auto_watch)
			activate_auto_watch(); // so that the button is green at the start
	}

	if(hiden) {
		for(i=0; i<zone.size(); i++)
			if(zone[i]->enabled == 0)
				zone[i]->disable();
	} else {
		dirt();
	}

}

void Pane_playerinfo::tag(int q) {
	int i;
	player[q]->dirt();

	for(i=0; i<4; i++)
		if(tagged[i] == q) {
			tagged[i] = -1;
			player[q]->set_font(fteam[game->net_list.get(q)->color]);
			return;
		}

	for(i=0; i<4; i++)
		if(tagged[i] == -1) {
			break;
		}
	if(i == 4) { // if no space left, flush the fourth selected
		tag(tagged[3]);
		for(int j=3; j>0; j--)
			tagged[j] = tagged[j-1]; // rotate the tagged
		i = 0;
	}
	tagged[i] = q;
	player[q]->set_font(inter->font);
}

void Pane_playerinfo::clear_tag() {
	for(int i=0; i<4; i++)
		tagged[i] = -1;
}

void Pane_playerinfo::add_name(Canvas *c, int i, int x2, int y2) {
	Zone_text *play;
	Font *f = fteam[c->color];
	if(c->islocal())
		play = new Zone_text(inter, c->long_name(true, false), x2, y2);
	else {
		play = new Zone_text_button2(inter, pi.fond, f, c->long_name(), x2, y2, 102);
		for(int j=0; j<4; j++)
			if(tagged[j] == i) {
				f = inter->font;
				break;
			}
	}
	play->set_font(f);
	zone.push_back(play);
	Zone *show;
	int width = 70;
	if(show_quoi == 0) {
		width = 33;
		Zone *death = new Zone_text_field(inter, c->stats[CS::DEATH].get_address(), x+w-58, y2, width, pi.mp->courrier);
		zone.push_back(death);
		total1[c->color] += c->stats[CS::DEATH].get_value();
	}
	show = new Zone_text_field(inter, c->stats[quel_stat()].get_address(), x+w-95, y2, width, pi.mp->courrier);
	zone.push_back(show);
	total0[c->color] += c->stats[quel_stat()].get_value();

	player[i] = play;
}

void Pane_playerinfo::add_total(int team, int x2, int y2) {
	Zone_text *play;
	play = new Zone_text(inter, ST_TOTAL, x2+20, y2);
	play->set_font(fteam[team]);
	zone.push_back(play);
	Zone *show;
	int width = 90, x_start = x+w-25-width;
	if(show_quoi == 0) {
		width = 33;
		Zone *death = new Zone_text_field(inter, &total1[team], x+w-58, y2, width, pi.mp->courrier);
		zone.push_back(death);
		x_start = x+w-52-width-10;
	}

	show = new Zone_text_field(inter, &total0[team], x_start, y2, width, pi.mp->courrier);
	zone.push_back(show);
}

int Pane_playerinfo::quel_stat() const {
	switch(show_quoi) {
		case 1:	return CS::SCORE;
		case 2:	return CS::LINESTOT;
		case 3:	return CS::COMPTETOT;
		case 4: return CS::BPM;
		case 5: return CS::PPM;
		default: return CS::FRAG;
	}
}

void Pane_playerinfo::draw() {
	Pane_close::draw();
	if(game->net_list.size() == 0)
		inter->font->draw(ST_NOPLAYERJOINED, screen, 19, 50);
}

void Pane_playerinfo::step() {
	int i,j;
	Pane_close::step();
	ifdone();
	if(show_quoi!=o_show_val) {
		o_show_val=show_quoi;
		refresh();
	}
	for(j=0; j<4; j++) // removes those 'tagged', but already displayed in another pane (or gone)
		if(tagged[j] != -1) {
			Canvas *c = game->net_list.get(tagged[j]);
			if(c) {
				if(c->inter != NULL || c->idle==3)
					tag(tagged[j]); // un-select (since already displayed or disconnected)
			}
			else
				tagged[j] = -1;
		}

	if(!Pane::clicked)
		if(!auto_watch)
			return;
	if(Pane::clicked && Pane::clicked == auto_button) {
		if(auto_watch)
			deactivate_auto_watch();
		else
			activate_auto_watch();
	}
	if(auto_watch)
		clear_tag();

	if((playback && playback->auto_demo) || !auto_watch) {
		int auto_watch_team = -1;
		for(i=0; i<MAXPLAYERS; i++) {
			Canvas *c = game->net_list.get(i);
			if(c && ((player[i] && player[i]==Pane::clicked) || auto_watch)) {
				if(!c->islocal() && c->inter == NULL) {
					if(c->idle != 3) {
						if(auto_watch_team==-1)
							auto_watch_team=c->color;
						if(playback && playback->auto_demo) {
							if(auto_watch_team==c->color && game->delay_start!=500)
								tag(i);
						}
						else
							tag(i);
						if(!auto_watch) // one tag at a time only if not auto_watch
							break;
					}
				}
			}
		}
	}
	else {
		bool to_tag[MAXPLAYERS];
		int count=0;
		for(i=0; i<MAXPLAYERS; i++) {
			Canvas *c=game->net_list.get(i);
			if(c && c->idle<2 && !c->islocal() && c->inter == NULL) {
				to_tag[i]=true;
				count++;
			}
			else
				to_tag[i]=false;
		}
		for(i=0; i<MAXPLAYERS && count<4; i++) {
			Canvas *c=game->net_list.get(i);
			if(c && c->idle==2 && !c->islocal() && c->inter == NULL) {
				to_tag[i]=true;
				count++;
			}
		}
		int team;
		for(team=0; team<MAXTEAMS; team++)
			for(i=0; i<MAXPLAYERS; i++)
				if(to_tag[i] && game->net_list.get(i)->color==team)
					tag(i);
	}
	if((Pane::clicked && Pane::clicked == show_button) || auto_watch) {
		if(Pane::clicked && Pane::clicked == show_button)
			deactivate_auto_watch();
		int count = 0, solo = 0;
		for(j=0; j<4; j++)
			if(tagged[j] != -1) {
				count++;
				solo = tagged[j];
			}

		if(count == 1) {
			hidecall(new Pane_startwatch(pi, solo, this)); // fullpane if only one watch
		} else {
			for(int j=0; j<4; j++)
				if(tagged[j] != -1) {
					hidecall(new Pane_smallwatch(pi, tagged, this)); // starts if more than one tagged
					break;
				}
		}
	}
}

void Pane_playerinfo::auto_watch_closed() {
	activate_auto_watch();
}

bool Pane_playerinfo::auto_watch_started() {
	bool t = auto_watch;
	deactivate_auto_watch(); // disable temporarily, in case the user click on "close"
	return t;
}

Chat_interface::Zone_chat_input::Zone_chat_input(Chat_interface *p, const Palette &pal, Inter* in, char* s, int mlen, int px, int py, int pw):
	Zone_text_input(in, pal, s, mlen, px, py, pw) {
	parent = p;
	kb_focusable = true;
}

void Chat_interface::Zone_chat_input::lost_focus(int cancel) {
	Zone_text_input::lost_focus(cancel);
	if(!cancel && strlen(st) > 0) {
		Packet_clientchat p;
		if(parent->buf[0]=='/') {
			//No player name when sending commands
			strcpy(p.text, parent->buf);
			p.team = -1;
			p.to_team = -1;
		}
		else {
			sprintf(p.text, "%s: %s", config.player[chat_text->quel_player].name, parent->buf);
			p.team = config.player[chat_text->quel_player].color;
			p.to_team = chat_text->to_player-1;
		}
		net->sendtcp(&p);
		val[0] = 0;
		set_val(val);
	}
}

Chat_interface::Zone_to_team::Zone_to_team(Inter *in, int *val, int px, int py):
	Zone_state_text(in, val, px, py) {
	add_string(ST_ALLTEAM);
	for(int i=0; i<MAXTEAMS; i++)
		add_string(team_name[i], fteam[i]);
}

void Chat_interface::Zone_to_team::clicked(int quel) {
	bool vide = true;
	do {
		Zone_state_text::clicked(quel);
		if(*val > 0) {
			int team = *val - 1;
			for(int i=0; i<MAXPLAYERS; i++) {
				Canvas *c = game->net_list.get(i);
				if(c && c->color == team) {
					vide = false;
					break;
				}
			}
		} else {
			vide = false;
		}
	} while(vide);

	sons.enter->play(-800, 0, 26000+ugs_random.rnd(1023));
}

Chat_interface::Chat_interface(Inter *in, const Palette &pal, Bitmap *bit, int px, int py, int pw, int ph, Video_bitmap *scr): Zone(in) {
	buf[0] = 0;
	if(!playback) {
		zinput = new Zone_chat_input(this, pal, inter, buf, 200, px, 410, pw);
		zone.push_back(zinput);
		zone.push_back(new Zone_text(inter, ST_FROM, px, 435));

		z_from = new Zone_state_text2(inter, &chat_text->quel_player, px+70, 435);
		notify();
		zone.push_back(z_from);
		zone.push_back(new Zone_text(inter, ST_TO, px, 455));

		zone.push_back(new Zone_to_team(inter, &chat_text->to_player, px+70, 455));
	} else {
		z_from = NULL;
		zinput = NULL;
	}
	if(scr) {
		set_screen_offset(0, scr);
		delete_screen = false;
	} else {
		set_screen_offset(0, video->new_bitmap(px, py, pw, ph));
		delete_screen = true;
	}
	back = new Bitmap((*bit)[py]+px, pw, 18*20, bit->realwidth);
	if(game)
		game->net_list.add_watch(this);
}

Chat_interface::~Chat_interface() {
	if(game)
		game->net_list.remove_watch(this);
	if(delete_screen)
		delete screen;
	delete back;
}

void Chat_interface::set_screen_offset(int o, Video_bitmap *vb) {
	y_offset = o;
	screen = vb;
	dirt();
}

void Chat_interface::draw() {
	back->draw(screen, 0, -y_offset);
	int ty, i;
	for(i=0; i<CHAT_NBLINE; i++) {
		ty = i*16 - y_offset;
		Font *fcp, *scp;
		if(chat_text->list[i].team != -1)
			fcp = fteam[chat_text->list[i].team];
		else
			fcp = inter->font;
		scp = inter->font;
		char tmp[256];
		strcpy(tmp, chat_text->list[i].text);
		int color_cut=chat_text->list[i].color_cut;
		if(color_cut!=-1)
			tmp[color_cut]=0;
		fcp->draw(tmp, screen, 1, ty);
		if(color_cut!=-1) {
			int x=fcp->width(tmp)+1;
			scp->draw(&chat_text->list[i].text[color_cut], screen, x, ty);
		}
	}
/*	if(y_offset != 0) {
		screen->hline(0, 0, screen->width, 0);
		screen->hline(1, 0, screen->width, 255);
		screen->hline(2, 0, screen->width, 0);
	}*/
}

void Chat_interface::process() {
	Zone::process();
	if(chat_text->new_text) {
		dirt();
		chat_text->new_text = false;
	}
	if(!playback) {
		// detects the Enter key (in Windows and/or in Unix)
		if(input->last_keysym.sym == SDLK_RETURN && !inter->focus
		   && inter->focus != zinput) {
			inter->select_zone(zinput, 0);
			input->clear_last_keysym();
		}
	}
}

void Chat_interface::notify() {
	if(z_from) {
		z_from->nstate = 0; // not polite, but fuck off
		for(int i=0; i<3; i++) 
			z_from->add_string(config.player[i].name, fteam[config.player[i].color]);
	}
}

Pane_scoreboard::Pane_scoreboard(const Pane_info &p, bool control_button, bool dback, bool dbottom):	Pane_close(p, dback, dbottom) {
	old_size=0;
	potato_team=255;
	b_show_frag = NULL;
	if(!game->single) {
		if(!(playback && playback->auto_demo) && control_button) {
			b_show_frag = new Zone_text_button2(pi.inter, pi.fond, pi.font2, "\2674", pi.x+110, 10);
			zone.push_back(b_show_frag);
		}
	}
	game->net_list.add_watch(this);
	score=game->net_list.score;
	deactivate_frag(false);
}

Pane_scoreboard::~Pane_scoreboard() {
	game->net_list.remove_watch(this);
}

void Pane_scoreboard::step() {
	Pane_close::step();
	if(clicked && clicked==b_show_frag) {
		show_frag = !show_frag;
		if(show_frag)
			activate_frag();
		else
			deactivate_frag(false);
	}
}

void Pane_scoreboard::process() {
	Pane_close::process();
	if(show_frag) {
		if(!(overmind.framecount&127)) {
			score.updateFromGame();
			if(score.team_order_changed || potato_team!=game->potato_team) {
				//Order changed, refresh the whole thing
				Pane_scoreboard::notify();
			}
		}
	}
}

void Pane_scoreboard::activate_frag() {
	char st[1024];
	st[0]=0;
	score.updateFromGame();
	int team, team2;
	if(b_show_frag)
		b_show_frag->set_text("\2673");
	show_frag=true;
	int x2=x+11;
	int y2=33, y_height=20;

	Zone_panel *zp = new Zone_panel(inter, x2-7, y2, w-11-25+14, 2);

	Zone *z;
	if(game->game_end==END_FRAG || game->game_end==END_POINTS || game->game_end==END_LINES) {
		const char *unit=NULL;
		switch(game->net_list.goal_stat) {
			case CS::FRAG: unit=ST_FRAG; break;
			case CS::SCORE: unit=ST_POINT; break;
			case CS::LINESTOT: unit=ST_LINE; break;
			default: unit="frog"; break;
		}
		sprintf(st, ST_GOAL, unit, game->game_end_value!=1? "s":"");
		z = new Zone_text(inter, st, x2, y2+2);
		zone.push_back(z);
		zlist_frag.push_back(z);
		z = new Zone_text_field(inter, &game->game_end_value, x+w-95, y2+1, 70, pi.mp->courrier);
		zone.push_back(z);
		zlist_frag.push_back(z);
		y2 += y_height;
	}
	for(team2=0; team2<MAXTEAMS; team2++) {
		team=score.team_order[team2];
		Canvas *a_canvas=NULL;
		int nb = 0, nb_not_gone = 0;
		int tot_handicap = 0;
		for(int i=0; i<MAXPLAYERS; i++) {
			Canvas* c=game->net_list.get(i);
			if(c && c->color == team) {
				nb++;
				if(c->idle<3) {
					nb_not_gone++;
					tot_handicap+=c->handicap;
				}
				a_canvas = c;
			}
		}
		if(nb > 0) {
			if(game->hot_potato && team==game->potato_team)
				strcpy(st, "\2672 ");
			else
				st[0]=0;
			if(nb==1)
				strcat(st, a_canvas->name);
			else
				strcat(st, team_name[team]);
			int avg_handicap=2;
			if(nb_not_gone)
				avg_handicap=tot_handicap/nb_not_gone;
			const char *handi="";
			switch(avg_handicap) {
				case 0: handi=" (-)"; break;
				case 1: handi=" (A)"; break;
				case 3: handi=" (M)"; break;
				case 4: handi=" (+)"; break;
			}
			strcat(st, handi);
			if(nb==1) {
				if(a_canvas->idle==3)
					strcat(st, " *");
			}
			else {
				char st2[6];
				sprintf(st2, " (%i)", nb_not_gone);
				strcat(st, st2);
			}
			int *statp;
			z = new Zone_text(fteam[team], inter, st, x2, y2+2);
			zone.push_back(z);
			zlist_frag.push_back(z);
			uint32_t width=70;
			if(game->net_list.goal_stat==CS::FRAG)
				width=33;
			statp=score.team_stats[team].stats[game->net_list.goal_stat].get_address();
			z = new Zone_text_field(inter, statp, x+w-95, y2+1, width, pi.mp->courrier);
			zone.push_back(z);
			zlist_frag.push_back(z);
			if(game->net_list.goal_stat==CS::FRAG) {
				statp=score.team_stats[team].stats[CS::DEATH].get_address();
				z = new Zone_text_field(inter, statp, x+w-58, y2+1, width, pi.mp->courrier);
				zone.push_back(z);
				zlist_frag.push_back(z);
			}
			y2 += y_height;
		}
	}
	if(y2 != 33) {
		zone.push_back(zp);
		zlist_frag.push_back(zp);
		y2+=2;
		zp->h = y2-33;
	}
	else {
		delete zp;
		y2=0;
	}
	size=y2;
	if(size!=old_size)
		video->need_paint = 2;
	old_size=size;
}

void Pane_scoreboard::deactivate_frag(bool temp) {
	//temp: tells if we should deactivate only temporarily
	//  (see notify)
	if(b_show_frag && !temp)
		b_show_frag->set_text("\2674");
	show_frag=false;
	for(size_t i=0; i<zlist_frag.size(); i++) {
		for (vector<Zone*>::iterator it = zone.begin(); it != zone.end();
		     ++it)
			if (*it == zlist_frag[i]) {
				zone.erase(it);
				break;
			}
		delete zlist_frag[i];
	}
	zlist_frag.clear();
	size=0;
	if(!temp) {
		video->need_paint = 2;
		old_size=0;
	}
}

void Pane_scoreboard::notify() {
	potato_team=game->potato_team;
	if(show_frag) {
		deactivate_frag(true);
		activate_frag();
	}
}

void Pane_scoreboard::scoreboard_invisible() {
	if(b_show_frag) {
		b_show_frag->disable();
	}
}

Pane_chat::Pane_chat(const Pane_info &p): Pane_scoreboard(p, true, false, false) {
	old_y=0;
	int i;
	for(i=0; i<3; i++)
		((Pane_option *) pi.mp->pane[i])->chat_window->disable();
	chat = new Chat_interface(inter, pi.mp->pal, pi.fond, x, y, w, h, screen);
	zone.push_back(chat);
	set_net_pane(3);
	allow_clock();
	if(playback && !playback->auto_demo) {
		zone.push_back(new Zone_slow_play(inter, pi.mp->bit, pi.mp->font2, ST_SLOWPLAY, x+2, 420));
		zone.push_back(new Zone_fast_play(inter, pi.mp->bit, pi.mp->font2, ST_FASTPLAY, x+2, 450));
		b_quit = new Zone_text_button2(inter, pi.mp->bit, pi.mp->font2, ST_BACK, x+132, 455);
		zone.push_back(b_quit);
	}
	else {
		b_quit=NULL;
	}
	activate_frag();
}

Pane_chat::~Pane_chat() {
	for(int i=0; i<3; i++)
		if(pi.mp->pane[i])
			((Pane_option *) pi.mp->pane[i])->chat_window->enable();
}

void Pane_chat::activate_frag() {
	Pane_scoreboard::activate_frag();
	if(size!=old_y) {
		int diff = size-pi.y;
		delete screen;
		screen = video->new_bitmap(pi.x, size, pi.w, pi.h-diff);
		chat->set_screen_offset(diff, screen);
	}
	old_y=size;
}

void Pane_chat::deactivate_frag(bool temp) {
	Pane_scoreboard::deactivate_frag(temp);
	if(!temp) {
		delete screen;
		screen = video->new_bitmap(pi.x, pi.y, pi.w, pi.h);
		chat->set_screen_offset(0, screen);
		old_y=0;
	}
}

void Pane_chat::step() {
	Pane_scoreboard::step();
	ifdone();
	if(clicked && clicked==b_quit)
		pi.mp->stop=true;
}

Pane_blockinfo::Pane_blockinfo(const Pane_info &p):
	Pane_close(p, true, true) {
	int i;
	old_gauche = gauche = 0;
	old_droite = droite = 1;
	add_info();
	for(i=0; i<7; i++)
		bloc[i] = new Bloc(i);
	set_net_pane(4);
	allow_clock();
	game->net_list.add_watch(this);
}

Pane_blockinfo::~Pane_blockinfo() {
	for(int i=0; i<7; i++)
		delete bloc[i];
	game->net_list.remove_watch(this);
}

void Pane_blockinfo::notify() {
	deleteall();
	add_info();
	dirt();
}

void Pane_blockinfo::draw() {
	Pane_close::draw();
	for(int i=0; i<7; i++) {
		bloc[i]->draw(video->vb, px, 2+(2*18+2)*i + y);
	}
}

void Pane_blockinfo::add_info() {
	Canvas *can=NULL, *can2=NULL;
	if(game->net_list.size() > 1) {
		Zone_state_text2 *temp = new Zone_state_text2(inter, &gauche, x+4, 420, 110);
		zone.push_back(temp);
		Zone_state_text2 *temp2 = new Zone_state_text2(inter, &droite, x+100, 445, 110);
		zone.push_back(temp2);
		int num = 0;
		if(gauche >= (int)game->net_list.size())
			gauche = game->net_list.size()-1;
		old_gauche=gauche;
		if(droite >= (int)game->net_list.size())
			droite = game->net_list.size()-1;
		old_droite=droite;
		for(int i=0; i<MAXPLAYERS; i++) {
			Canvas *c = game->net_list.get(i);
			if(c) {
				temp->add_string(c->name, fteam[c->color]);
				temp2->add_string(c->name, fteam[c->color]);
				if(num == gauche)
					can = c;
				if(num == droite)
					can2 = c;
				num++;
			}
		}
	}
	if(game->net_list.size() == 1) {
		for(int i=0; i<MAXPLAYERS; i++) {
			can = game->net_list.get(i);
			if(can)
				break;
		}
	}

	if(can2) {
		px = 65+x;
		block_info(can, 2);
		block_info(can2, 130);
	} else {
		px = 24+x;
		if(can)
			block_info(can, 90);
	}
	zone.push_back(new Zone_text(inter, ST_TOTAL, px+19, 300+y));
}

void Pane_blockinfo::step() {
	Pane_close::step();
	ifdone();
	if(old_gauche!=gauche || old_droite!=droite)
		notify();
}

void Pane_blockinfo::block_info(Canvas *can, int dx) {
	for(int i=0; i<7; i++) {
		zone.push_back(new Zone_text_field(inter, can->stats[CS::COMPTE0+i].get_address(), dx+x+9, 30+(2*18+2)*i+y, 50, pi.mp->courrier));
	}
	zone.push_back(new Zone_text_field(inter, can->stats[CS::COMPTETOT].get_address(), dx+x+9, 300+y, 50, pi.mp->courrier));
}

Pane_comboinfo::Pane_comboinfo(const Pane_info &p):
	Pane_close(p, true, true) {
	old_gauche = gauche = 0;
	old_droite = droite = 1;
	game->net_list.add_watch(this);

	add_info();
	set_net_pane(5);
	allow_clock();
}

Pane_comboinfo::~Pane_comboinfo() {
	game->net_list.remove_watch(this);
}

void Pane_comboinfo::step() {
	Pane_close::step();
	ifdone();
	if(old_gauche!=gauche || old_droite!=droite)
		notify();
}

void Pane_comboinfo::combo_info(Canvas *can, int dx) {
	int py = 15+y;
	for(int i=0; i<15; i++) {
		zone.push_back(new Zone_text_field(inter, can->stats[CS::CLEAR00+i].get_address(), dx+x+9, py, 50, pi.mp->courrier));
		py += 20;
	}
	py += 10;
	zone.push_back(new Zone_text_field(inter, can->stats[CS::LINESTOT].get_address(), dx+x+9, py, 50, pi.mp->courrier));
}

void Pane_comboinfo::notify() {
	deleteall();
	add_info();
	dirt();
}

void Pane_comboinfo::add_info() {
	Canvas *can=NULL, *can2=NULL;
	int i;
	if(game->net_list.size() > 1) {
		Zone_state_text2 *temp = new Zone_state_text2(inter, &gauche, x+4, 420, 110);
		Zone_state_text2 *temp2 = new Zone_state_text2(inter, &droite, x+100, 445, 110);
		int num = 0;
		if(gauche >= (int)game->net_list.size())
			gauche = game->net_list.size()-1;
		old_gauche=gauche;
		if(droite >= (int)game->net_list.size())
			droite = game->net_list.size()-1;
		old_droite=droite;
		for(int i=0; i<MAXPLAYERS; i++) {
			Canvas *c = game->net_list.get(i);
			if(c) {
				temp->add_string(c->name, fteam[c->color]);
				temp2->add_string(c->name, fteam[c->color]);
				if(num == gauche)
					can = c;
				if(num == droite)
					can2 = c;
				num++;
			}
		}
		zone.push_back(temp);
		zone.push_back(temp2);
	}
	if(game->net_list.size() == 1) {
		old_gauche=gauche=old_droite=droite=0;
		for(i=0; i<MAXPLAYERS; i++) {
			can = game->net_list.get(i);
			if(can)
				break;
		}
	}

	i = y+15;
	int px;

	if(can2) {
		px = 65+x;
		if(can)
			combo_info(can, 2);
		combo_info(can2, 130);
	} else {
		px = 24+x;
		if(can)
			combo_info(can, 90);
	}
	zone.push_back(new Zone_text(inter, ST_CLEARINFO1, px, i)); i+=20;
	zone.push_back(new Zone_text(inter, ST_CLEARINFO2, px, i)); i+=20;
	zone.push_back(new Zone_text(inter, ST_CLEARINFO3, px, i)); i+=20;
	zone.push_back(new Zone_text(inter, ST_CLEARINFO4, px, i)); i+=20;
	zone.push_back(new Zone_text(inter, ST_CLEARINFO5, px, i)); i+=20;
	zone.push_back(new Zone_text(inter, ST_CLEARINFO6, px, i)); i+=20;
	zone.push_back(new Zone_text(inter, ST_CLEARINFO7, px, i)); i+=20;
	zone.push_back(new Zone_text(inter, ST_CLEARINFO8, px, i)); i+=20;
	zone.push_back(new Zone_text(inter, ST_CLEARINFO9, px, i)); i+=20;
	zone.push_back(new Zone_text(inter, ST_CLEARINFO10, px, i)); i+=20;
	zone.push_back(new Zone_text(inter, ST_CLEARINFO11, px, i)); i+=20;
	zone.push_back(new Zone_text(inter, ST_CLEARINFO12, px, i)); i+=20;
	zone.push_back(new Zone_text(inter, ST_CLEARINFO13, px, i)); i+=20;
	zone.push_back(new Zone_text(inter, ST_CLEARINFO14, px, i)); i+=20;
	zone.push_back(new Zone_text(inter, ST_CLEARINFOMORE, px, i)); i+=20;
	i += 10;
	zone.push_back(new Zone_text(inter, ST_TOTAL, px, i));
}

Pane_playerstartup::Pane_playerstartup(const Pane_info &p, int q):
	Pane_close(p) {
	int y=50;
	qplayer = q;

	b_start = new Zone_text_button2(inter, pi.fond, pi.font2, ST_START, x+15, y, w-44);
	zone.push_back(b_start);
	y+=35;
	if(game && game->allow_handicap) {
		zone.push_back(new Zone_text(inter, ST_SELECTHANDICAP, x+9, y, w-30));
		y+=20;
		handicap=config.player2[qplayer].handicap;
		list_handicap = new Zone_state_text2(inter, &handicap, x+42, y);
		list_handicap->add_string(ST_BEGINNER);
		list_handicap->add_string(ST_APPRENTICE);
		list_handicap->add_string(ST_INTERMEDIATE);
		list_handicap->add_string(ST_MASTER);
		list_handicap->add_string(ST_GRANDMASTER);
		zone.push_back(list_handicap);
		y+=35;
	}
	else {
		handicap=2;
		list_handicap=NULL;
	}
	zone.push_back(new Zone_text(inter, ST_SELECTTEAM, x+9, y, w-30));
	y+=20;
	color = config.player[qplayer].color;
	list_team = new Zone_state_text2(inter, &color, x+29, y);
	int t;
	for(t=0; t<MAXTEAMS; t++)
		list_team->add_string(team_name[t], fteam[t]);
	zone.push_back(list_team);
	uint8_t col[MAXTEAMS];
	for(t=0; t<MAXTEAMS-1; t++)
		col[t] = 184+t*8+7;
	col[MAXTEAMS-1] = 184+8*8+4;

	color_team = new Zone_color_select_noclick(inter, &color, x+119, y, col);
	color_team->kb_focusable = false;
	zone.push_back(color_team);
	y+=35;
	list_team->set_child(color_team);

	zone.push_back(new Zone_text(inter, ST_PLAYERINTEAM, x+9, y, w-30));
	y+=20;
	list_player = new Zone_listbox2(inter, pi.fond, pi.font2, NULL, x+19, y, 160, 170);
	zone.push_back(list_player);
	game->net_list.add_watch(this);
	update_player(); // force the first update
}

Pane_playerstartup::~Pane_playerstartup() {
	game->net_list.remove_watch(this);
}

void Pane_playerstartup::notify() {
	update_player();
}

void Pane_playerstartup::update_player() {
	list_player->clear();
	for(int i=0; i<MAXPLAYERS; i++) {
		Canvas *c = game->net_list.get(i);
		if(c)
			if(c->color == list_team->last_val)
				list_player->add_item(new Listable(c->name, fteam[c->color]));
	}
}

void Pane_playerstartup::step() {
	Pane_close::step();
	if(clicked == list_team || clicked == color_team) {
		update_player();
	}
	if(clicked == b_start) { // if start is clicked or the player is already started (but hidden)
		config.player[qplayer].color = color; // assign the color according to choice
		config.player2[qplayer].handicap = handicap;
		Module *m;
		m = new Pane_playerjoin(pi, qplayer);
		game->net_list.remove_watch(this);
		hideexec(m);
	}
}

Pane_playerjoin::Pane_playerjoin(const Pane_info &p, int q):
	Pane_close(p) {
	Packet_playerwantjoin pjoin;
	qplayer = q;
	status = new Zone_text(inter, ST_WAITINGTOJOIN, x+9, 100, w-30);
	zone.push_back(status);
	pjoin.team=config.player[qplayer].color;
	strcpy(pjoin.name, config.player[qplayer].name);
	config.get_player_hash(pjoin.player_hash, qplayer);
	pjoin.player=qplayer;
	pjoin.shadow=config.player[qplayer].shadow;
	pjoin.smooth=config.player[qplayer].smooth;
	pjoin.h_repeat=config.player2[qplayer].h_repeat;
	pjoin.v_repeat=config.player2[qplayer].v_repeat;
	pjoin.handicap=config.player2[qplayer].handicap;
	strcpy(pjoin.team_name, config.player2[qplayer].ngTeam);
	config.get_team_hash(pjoin.team_hash, qplayer);
	eping=new Exec_ping(&pjoin, P_PLAYERACCEPTED, this);
	got_answer=false;
	if(close)
		close->disable();
}

Pane_playerjoin::~Pane_playerjoin() {
	if(eping)
		delete eping;
}

void Pane_playerjoin::step() {
	Pane_close::step();
	//Re-enable close when we get an answer
	if(got_answer && close) {
		//Return got_answer to false so we don't enable close multiple
		//  times (that's an evil thing to do). If a fake server
		//  sends more than one answer, we're screwed but then again,
		//  fake servers can screw us in 1000s of ways already...
		got_answer=false;
		close->enable();
	}
}

void Pane_playerjoin::net_call(Packet *p2) {
	got_answer=true;
	Packet_playeraccepted *p=(Packet_playeraccepted *) p2;
	if(p->accepted == 0) { // if the join has been accepted
		hideexec(new Pane_startgame(pi, qplayer, NULL, p->pos));
	} else if(p->accepted == 4) { // if there is already a 'gone' player and that we replace him
		hideexec(new Pane_startgame(pi, qplayer, game->net_list.get(p->pos), p->pos));
	} else {
		const char *s1 = NULL, *s2 = NULL;
		switch(p->accepted) {
			case 1: // server refuses all joins
				s1 = ST_PLAYERJOINREFUSED;
				s2 = ST_PLAYERJOINREFUSED2;
				break;
			case 2: // already someone with this name
				s1 = ST_PLAYERJOINALREADY;
				s2 = ST_PLAYERJOINALREADY2;
				break;
			case 3: // game is finished, no join
				s1 = ST_PLAYERJOINREFUSED3;
				s2 = ST_PLAYERJOINREFUSED4;
				break;
			case 5: // full game (MAX_PLAYERS)
				s1 = ST_PLAYERJOINFULL1;
				s2 = ST_PLAYERJOINFULL2;
				break;
		};
		dirt();
		status->set_text(s1);
		zone.push_back(new Zone_text(inter, s2, x+9, 130, w-30));
	}
}

Pane_startgame::Pane_startgame(const Pane_info &p, int q, Canvas *c, int pos):
	Pane_close(p, false) {
	delete_zone = true;
	qplayer = q;
	canvas = c;
	if(canvas == NULL) { // if not already there, add new player
		canvas = new Canvas(qplayer, game->seed, &pi.mp->pal);
		config.get_player_hash(canvas->player_hash, qplayer);
		config.get_team_hash(canvas->team_hash, qplayer);
		if(pos==-1) {
			// for local game
			game->net_list.add_player(canvas);
		}
		else {
			// for Internet game
			game->net_list.set_player(canvas, pos, true);
		}
	}
	else { // if resumed or displayed in full screen
		if(qplayer!=-1) { // if resuming a local player
			canvas->clear_key_all();
			if(canvas->idle == 3)
				delete_zone = false; // prevent the refresh that will indicate that this player rejoin since this deletes the zones
		}
	}
	//What the fuck does a canvas need a Palette and Bitmap for,
	//  anyway?
	canvas->pal = &pi.mp->pal;
	canvas->bit = pi.fond;
	if(qplayer!=-1) {
		for(int i=0; i<5; i++)
			pi.inter->kb_alloc_key(config.player[qplayer].key[i]);
		pi.inter->kb_alloc_key(config.player2[qplayer].key[0]);
		pi.inter->kb_alloc_key(config.player2[qplayer].key[1]);
	}

	num_player = canvas->num_player;
	if(delete_zone)
		create_zone();
	game->net_list.add_watch(this);
}

Pane_startgame::~Pane_startgame() {
	if(canvas)
		canvas->hide();
	game->net_list.remove_watch(this);
	if(qplayer!=-1) {
		for(int i=0; i<5; i++)
			pi.inter->kb_free_key(config.player[qplayer].key[i]);
		pi.inter->kb_free_key(config.player2[qplayer].key[0]);
		pi.inter->kb_free_key(config.player2[qplayer].key[1]);
	}
}

void Pane_startgame::create_zone() {
	zone.push_back(new Zone_canvas(inter, *pi.fond, x+9, 37, canvas));
	zone.push_back(new Zone_canvas_bloc(inter, canvas));
	zone.push_back(new Zone_text(inter, ST_GAMESCORE, x+9 - 5, 403));
	zone.push_back(new Zone_text_field(inter, canvas->stats[CS::SCORE].get_address(), x+9+55, 403, 120, pi.mp->courrier));
	zone.push_back(new Zone_text(inter, ST_GAMELINES, x+9 - 5, 422));
	canvas->z_lines=new Zone_text_field(inter, canvas->stats[CS::LINESCUR].get_address(), x+9+55, 422, 65, pi.mp->courrier);
	canvas->z_potatolines=new Zone_text_field(inter, &canvas->team_potato_lines, x+9+55, 422, 65, pi.mp->courrier);
	if(canvas->color==game->potato_team)
		canvas->z_lines->disable();
	else
		canvas->z_potatolines->disable();
	zone.push_back(canvas->z_lines);
	zone.push_back(canvas->z_potatolines);
	zone.push_back(new Zone_text(inter, ST_GAMELEVEL, x+9 - 5, 460));
	zone.push_back(new Zone_text_field(inter, &canvas->level, x+9+55, 460, 35, pi.mp->courrier));
	char *the_guys_name=canvas->name;
	if(!game->single)
		the_guys_name=canvas->long_name(true, false);
	Zone_text *name = new Zone_text(inter, the_guys_name, x+9 + 95, 460);
	if(!game->single)
		name->set_font(fteam[canvas->color]);
	zone.push_back(name);
	if(!game->single) {
		zone.push_back(new Zone_text(inter, ST_GAMEFRAGS, x+9 - 5, 441));
		zone.push_back(new Zone_text_field(inter, canvas->stats[CS::FRAG].get_address(), x+9+55, 441, 35, pi.mp->courrier));
		zone.push_back(new Zone_text(inter, ST_GAMEDEATHS, x+9 + 95, 441));
		zone.push_back(new Zone_text_field(inter, canvas->stats[CS::DEATH].get_address(), x+9+155, 441, 35, pi.mp->courrier));
		canvas->z_linestot=new Zone_text_field(inter, canvas->stats[CS::LINESTOT].get_address(), x+9+55+70, 422, 65, pi.mp->courrier);
		canvas->z_potatolinestot=new Zone_text_field(inter, &canvas->team_potato_linestot, x+9+55+70, 422, 65, pi.mp->courrier);
		if(canvas->color==game->potato_team)
			canvas->z_linestot->disable();
		else
			canvas->z_potatolinestot->disable();
		zone.push_back(canvas->z_linestot);
		zone.push_back(canvas->z_potatolinestot);
	}
}

void Pane_startgame::step() {
	Pane_close::step();
//	if(done)
//		canvas->hide();
}

void Pane_startgame::notify() {
	if(game->net_list.get(num_player) == NULL) {
		if(delete_zone) {
			msgbox("Pane_startgame::notify: Indeed, player %i was dropped.\n", num_player);
			while (!zone.empty()) {
				delete zone.back();
				zone.pop_back();
			}
			canvas = NULL;
			ret();
		} else {
			msgbox("Pane_startgame::notify: Player %i is re-joining and becoming a local canvas.\n", num_player);
		  canvas->pal = &pi.mp->pal;
			canvas->local_player = true;
			canvas->remote_adr = NULL;
			canvas->player = qplayer;
			delete_zone = true;
			create_zone();
		}
	}
}

Pane_startwatch::Pane_startwatch(const Pane_info &p, int player, Pane_playerinfo *ppinfo):
	Pane_startgame(p, -1, game->net_list.get(player)) {
	pinfo = ppinfo;
	auto_watch=pinfo->auto_watch_started();
	watch = new Watch_canvas(player);
	watch->start();
}

Pane_startwatch::~Pane_startwatch() {
	delete watch;
}

void Pane_startwatch::step() {
	Pane_startgame::step();
	if(done) {
		watch->stop();
	}
}

void Pane_startwatch::notify() {
	Pane_startgame::notify();
	if(auto_watch) { // if one player is dropped or a new one arrive, close
		ret(); // since auto_watch might open in a small watch (or re-open the same)
		pinfo->auto_watch_closed();
	}
}

Pane_smallwatch::Pane_smallwatch(const Pane_info &p, int tagged[], Pane_playerinfo *ppinfo):
 Pane_scoreboard(p, true) {
	pinfo = ppinfo;
	auto_watch=pinfo->auto_watch_started();
	int i;
	for(i=0; i<4; i++)
		watch[i] = NULL;
	int nb_temp = 0;
	for(i=0; i<4; i++)
		if(tagged[i] != -1)
			nb_temp++;

	compte = 0;
	for(i=0; i<4; i++) {
		if(tagged[i] != -1) {
			int tx, ty;
			if(compte == 0 || compte == 2)
				tx = 16;
			else
				tx = 114;
			if(compte == 0 || compte == 1)
				ty = 219;
			else
				ty = 35;
			watch[compte] = new Watch_canvas(tagged[i], true);
			watch[compte]->start();
			watch[compte]->small_canvas(pi, tx, ty);
			compte++;
		}
	}
	if(compte==1 || compte==2) {
		activate_frag();
	} else {
		deactivate_frag(false);
		scoreboard_invisible();
	}
	allow_clock();
}

Pane_smallwatch::~Pane_smallwatch() {
	for(int i=0; i<4; i++)
		if(watch[i]) {
			watch[i]->stop();
			watch[i]->c->hide();
			delete watch[i];
		}
}

void Pane_smallwatch::notify() {
	Pane_scoreboard::notify();
	bool deleted=false;
	compte = 0;
	for(int i=0; i<4; i++)
		if(watch[i]) {
			Canvas *c=game->net_list.get(watch[i]->play);
			if(!c || c->idle==3) {
				msgbox("Pane_smallwatch::notify: Indeed, player %i is gone.\n", watch[i]->play);
				watch[i]->stop();
				delete watch[i];
				watch[i] = NULL;
				deleted=true;
				dirt();
			}
			else
				compte++;
		}
	if(deleted || compte == 0 || auto_watch) { // if no small_watch left open
		ret();
		pinfo->auto_watch_closed();
	}
	msgbox("Pane_smallwatch::notify: done\n");
}

void Pane_smallwatch::draw() {
	Pane_scoreboard::draw();
	for(int q=0; q<4; q++) {
		if(watch[q]) {
			int x = watch[q]->x;
			int y = watch[q]->y;
			screen->hline(y-1, x, 68, 255);
			screen->vline(x-1, y, 120, 255);
			screen->hline(y+120, x, 68, 255);
			screen->vline(x+60, y, 120, 255);
			screen->vline(x+67, y, 120, 255);
		}
	}
}

Watch_canvas::Watch_canvas(int player, bool s) {
	play = player;
	small_watch = s;
	c = game->net_list.get(play);
}

void Watch_canvas::stop() {
	Packet_clientstartwatch pac;
	pac.player = play;
	pac.stop = true;
	net->sendtcp(&pac);
}

void Watch_canvas::start() {
	Packet_clientstartwatch pac;
	pac.player = play;
	pac.stop = false;
	pac.update = config.info.update_rate;
	if(small_watch)
		pac.update = pac.update>>1;
	net->sendtcp(&pac);
}

void Watch_canvas::small_canvas(const Pane_info &pi, int tx, int ty) {
	x = tx;
	y = ty;
	tx += pi.x;
	ty += pi.y;
	zone.push_back(new Zone_small_canvas(pi.inter, *pi.fond, tx, ty, c));
	zone.push_back(new Zone_small_canvas_bloc(pi.inter, c));
	Zone_text *name = new Zone_text(pi.inter, c->long_name(true, false), tx, ty+122, 66);
	name->set_font(fteam[c->color]);
	zone.push_back(name);
}
