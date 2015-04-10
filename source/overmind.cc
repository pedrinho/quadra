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

#include "overmind.h"

Overmind overmind;
Inter* ecran = NULL;

Overmind::Overmind() {
	framecount = 0;
	paused = false;
	done = false;
}

Overmind::~Overmind() {
	clean_up();
}

void Overmind::clean_up() {
	while(execs.size()) {
		Executor *e = execs.back();
		if(e && e->self_destruct)
			delete e;
		execs.pop_back();
	}
}

void Overmind::pause() {
	paused = true;
}

void Overmind::unpause() {
	paused = false;
}

void Overmind::step() {
	if(paused)
		return;
	framecount++;
	for (size_t i = 0; i<execs.size(); i++) {
		Executor *e = execs[i];
		if(!e) {
			execs.erase(execs.begin() + i);
			i--;
		} else {
			e->step();
			if(e->done) {
				if(e->self_destruct)
					delete e;
				execs.erase(execs.begin() + i);
				i--;
			}
		}
	}
	if(execs.empty())
		done = true;
}

void Overmind::start(Executor* e) {
	execs.push_back(e);
	done = false;
}

void Overmind::stop(Executor* e) {
	for (size_t i = 0; i<execs.size(); i++) {
		if(e == execs[i]) {
			if(e->self_destruct)
				delete e;
			execs[i] = NULL;
		}
	}
}

Executor::Executor(bool self_des) {
	done = paused = false;
	self_destruct = self_des;
}

Executor::~Executor() {
	while(!modules.empty())
		remove();
}

void Executor::remove() {
	delete modules.back();
	modules.pop_back();
}

void Executor::step() {
	if(paused)
		return;
	if(!modules.empty()) {
		if(!modules.back()->done) {
			if(modules.back()->first_time) {
				modules.back()->first_time = false;
				modules.back()->init();
			} else {
				modules.back()->step();
			}
		}
		while(!modules.empty() && modules.back()->done) {
			remove();
		}
	}
	if(modules.empty())
		done=true;
}

void Executor::add(Module* m) {
	modules.push_back(m);
	m->parent=this;
}

Module::Module(): done(false) {
	first_time = true;
}

Module::~Module() {
}

void Module::step() {
}

void Module::init() {
}

void Module::exec(Module* module) {
	call(module);
	ret();
}

void Module::call(Module* module) {
	parent->add(module);
}

void Module::ret() {
	done=true;
}

Module_thread::Module_thread() {
	parent = new Executor(true);
	parent->add(this);
	overmind.start(parent);
}

Menu::Menu(Inter* base) {
	if(base)
		inter=new Inter(base);
	else
		inter=new Inter();
	result=NULL;
	old_ecran=ecran;
}

Menu::~Menu() {
	delete inter;
	ecran=old_ecran;
	if(ecran)
		ecran->dirt_all();
}

void Menu::init() {
	old_ecran=ecran;
	ecran = inter;
}

void Menu::step() {
	ecran = inter;
	if(ecran) {
		result = ecran->do_frame();
	} else
		result = NULL;
}
