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

#include "net.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#ifndef WIN32
#include <unistd.h>
#ifndef __STRICT_ANSI__
#define I_SET_STRICT_ANSI
#define __STRICT_ANSI__
#endif
#include <sys/socket.h>
#ifdef I_SET_STRICT_ANSI
#undef I_SET_STRICT_ANSI
#undef __STRICT_ANSI__
#endif
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <string.h>
#include <fcntl.h>
inline int closesocket(int fd) {
	return close(fd);
}
#else
typedef int socklen_t;
#endif

#include "SDL.h"

#include "error.h"
#include "net_buf.h"
#include "packet.h"

using std::list;
using std::min;

IP_addr::IP_addr(const IP_addr& o) {
	set(o.ip, o.mask);
}

IP_addr::IP_addr(const char *addr) {
	set(addr);
}

IP_addr::IP_addr(uint32_t pip, uint32_t pmask) {
	set(pip, pmask);
}

bool IP_addr::operator>(const IP_addr& o) {
	uint32_t m=mask&o.mask;
	if(m!=mask)
		return false;
	return (ip&m)==(o.ip&m);
}

bool IP_addr::operator>=(const IP_addr& o) {
	return *this>o || (ip==o.ip && mask==o.mask);
}

void IP_addr::print(char *st) {
	st[0]=0;
	char num[4];
	uint32_t ip=this->ip;
	uint32_t mask=this->mask;
	int i;
	for(i=0; i<4; i++) {
		if((mask&0xFF000000)==0)
			strcat(st, "*");
		else {
			sprintf(num, "%i", ip>>24);
			strcat(st, num);
		}
		if(i<3)
			strcat(st, ".");
		ip<<=8;
		mask<<=8;
	}
}

void IP_addr::set(const char *addr) {
	uint8_t a[4];
	uint8_t m[4];
	const char *p=addr;
	int i;
	for(i=0; i<4; i++) {
		const char *dot=strchr(p, '.');
		if(!dot)
			dot=p+strlen(p);
		char num[4];
		if(dot-p>4)
			break;
		memcpy(num, p, dot-p);
		num[dot-p]=0;
		int part=1;
		if(num[0]==0 || !strcmp(num, "*"))
			m[i]=0;
		else {
			m[i]=255;
			part=atoi(num);
		}
		if(part<0 || part>255)
			break;
		a[i]=(uint8_t)part;
		if(*dot)
			p=dot+1;
		else
			p=dot;
	}
	if(i==4) {
		ip=a[0]<<24 | a[1]<<16 | a[2]<<8 | a[3];
		mask=m[0]<<24 | m[1]<<16 | m[2]<<8 | m[3];
	}
	else {
		ip=0;
		mask=0xFFFFFFFF;
	}
}

void IP_addr::set(uint32_t pip, uint32_t pmask) {
	ip=pip;
	mask=pmask;
}

bool Net_param::accept_connection(Net_connection *nc) {
	return net->connections.size()<64;
}

int Net_connection::next_port=65536;

Net_connection::Net_connection() {
	_state=invalid;
	destport=next_port++;
	packet_based=true;
	joined=false;
	trusted=false;
	incoming=NULL;
	buf=new Buf(0, 256);
	connected_to=NULL;
	incoming_inactive=outgoing_inactive=0;
	outgoing_size=0;
	outgoing_min=0;
	outgoing_max=0;
	outgoing_total=0;
	incoming_size=0;
	incoming_min=0;
	incoming_max=0;
	incoming_total=0;
	commit_count_in=0;
	commit_count_out=0;
	commit_count_both=0;
	commit_count_total=0;
}

Net_connection::~Net_connection() {
	disconnect();
	if(buf)
		delete buf;
	if(incoming)
		delete incoming;
}

Net_connection::Netstate Net_connection::state() {
	return _state;
}

void Net_connection::connect(Net_connection *dest) {
	if(connected_to || dest->connected_to) {
		skelton_msgbox("Net_connection::connect: already connected!\n");
		return;
	}
	connected_to=dest;
	dest->connected_to=this;
	_state=connected;
	dest->_state=connected;
}

void Net_connection::connect(uint32_t addr, int port) {
}

void Net_connection::connect(const char* host, int port) {
}

bool Net_connection::checktcp() {
	if(_state!=connected)
		return false;
	if(buf->size()>=sizeof(uint16_t)) {
		uint16_t s = SDL_SwapLE16(*(uint16_t *) buf->get());
		if(buf->size()>=sizeof(uint16_t)+s)
			return true;
	}
	return false;
}

void Net_connection::receivetcp(Net_buf *p) {
	if(!packet_based)
		return;
	p->from = this;
	p->from_addr = INADDR_LOOPBACK;
	uint16_t size = SDL_SwapLE16(*(uint16_t *) buf->get());
	memcpy(p->buf, buf->get()+sizeof(uint16_t), size);
	buf->remove_from_start(size+sizeof(uint16_t));
	incoming_inactive=0;
	incoming_size+=size+sizeof(uint16_t);
}

int Net_connection::receivetcp(uint8_t *buf, uint32_t size) {
	if(_state!=connected || packet_based)
		return false;
	if(size > this->buf->size())
		size=this->buf->size();
	memcpy(buf, this->buf->get(), size);
	this->buf->remove_from_start(size);
	if(size) {
		incoming_inactive=0;
		incoming_size+=size;
	}
	return size;
}

void Net_connection::sendtcp(Packet *p2) {
	if(_state<connected || _state==disconnected || !packet_based)
		return;
	Net_buf p;
	p2->write(&p);
	uint16_t size=p.len();
	static uint8_t outbuf[1026];
	*(uint16_t *) outbuf=SDL_SwapLE16(size);
	memcpy(&outbuf[2], p.buf, size);
	sendtcp(outbuf, size+2);
}

void Net_connection::sendtcp(const uint8_t *buf, uint32_t size) {
	if(connected_to) {
		connected_to->buf->append(buf, size);
		outgoing_inactive=0;
		outgoing_size+=size;
	}
}

void Net_connection::commit() {
	if(outgoing_size && incoming_size)
		commit_count_both++;
	if(outgoing_size)
		commit_count_out++;
	if(outgoing_size>outgoing_max)
		outgoing_max=outgoing_size;
	if(outgoing_size && (outgoing_size<outgoing_min || !outgoing_min))
		outgoing_min=outgoing_size;
	outgoing_total+=outgoing_size;
	outgoing_size=0;
	if(incoming_size)
		commit_count_in++;
	if(incoming_size>incoming_max)
		incoming_max=incoming_size;
	if(incoming_size && (incoming_size<incoming_min || !incoming_min))
		incoming_min=incoming_size;
	incoming_total+=incoming_size;
	incoming_size=0;
	commit_count_total++;
}

void Net_connection::disconnect() {
	_state=disconnected;
	if(connected_to) {
		connected_to->_state=disconnected;
		connected_to->connected_to=NULL;
		connected_to=NULL;
	}
}

uint32_t Net_connection::getbufsize() const {
	return buf->size();
}

/* constructor for client_connection and server_connection */
Net_connection_tcp::Net_connection_tcp(int p, bool ppacket_based) {
	outgoing_buf.reserve(1024);
	packet_based=ppacket_based;
	joined=false;
	trusted=false;
	incoming=NULL;
	_state=invalid;
	desthost[0]=0;
	destaddr=0;
	destport=0;
	tcpbufsize=0;
	tcppacsize=0;

	tcpsock = socket(AF_INET, SOCK_STREAM, 0);
	if(net->getlasterror(tcpsock)) {
		return;
	}

	int val=1;
/*	if(net->getlasterror(setsockopt(tcpsock, SOL_SOCKET, SO_KEEPALIVE, (char *) &val, sizeof(int))))
		return;*/

	val=1;
	if(net->getlasterror(setsockopt(tcpsock, IPPROTO_TCP, TCP_NODELAY, (char *) &val, sizeof(int))))
		return;

#ifdef WIN32
	unsigned long val2=1;
	if(net->getlasterror(ioctlsocket(tcpsock, FIONBIO, &val2)))
		return;
#else
	if(net->getlasterror(fcntl(tcpsock, F_SETFL, fcntl(tcpsock, F_GETFL)|O_NONBLOCK)))
		return;

	if(net->getlasterror(setsockopt(tcpsock, SOL_SOCKET, SO_REUSEADDR, (char*)&val, sizeof(int))))
		return;
#endif

  tcpsin.sin_family = AF_INET;
  tcpsin.sin_addr.s_addr = htonl(0);
  tcpsin.sin_port = htons(p);

	if(p) {
	  if(net->getlasterror(bind(tcpsock, (sockaddr *) &tcpsin, sizeof(tcpsin))))
			return;

		if(net->getlasterror(listen(tcpsock, 5)))
			return;
	}

	from=0;
	_state=valid;
	skelton_msgbox("Opening TCP socket %i\n",tcpsock);
}

/* constructor for accepted connections */
Net_connection_tcp::Net_connection_tcp(int sock, uint32_t adr, int port, bool ppacket_based) {
	outgoing_buf.reserve(1024);
	packet_based=ppacket_based;
	joined=false;
	trusted=false;
	incoming=NULL;
	_state=invalid;
	destaddr=adr;
	destport=port;
	desthost[0]=0;
	skelton_msgbox("Opening TCP socket (accept) %i\n",sock);
	tcpsock=sock;
	#ifdef WIN32
		int val=1;
		if(net->getlasterror(setsockopt(tcpsock, IPPROTO_TCP, TCP_NODELAY, (char *) &val, sizeof(int))))
			skelton_msgbox("Error setting TCP_NODELAY for accepted socket %i, ignoring.\n",tcpsock);
	#endif
	tcpbufsize=0;
	tcppacsize=0;

  tcpsin.sin_family = AF_INET;
  tcpsin.sin_addr.s_addr = htonl(adr);
  tcpsin.sin_port = htons(port);
	from=adr;

	_state=connected;
}

Net_connection_tcp::~Net_connection_tcp() {
	if(_state==dnslookup)
		net->gethostbyname_cancel();
	if(tcpsock != -1) {
		char buf[1024];
		skelton_msgbox("Shutting down...");
		if(shutdown(tcpsock, 1) != SOCKET_ERROR) {
			while(net->checkreceive(tcpsock) > 0)	{
				int i = recv(tcpsock, buf, 1024, 0);
				if(i <= 0)
					break;
				skelton_msgbox("%i bytes,", i);
			}
			skelton_msgbox("Done!\n");
		} else {
			skelton_msgbox("Ignoring shutdown error.\n");
		}
		skelton_msgbox("Closing TCP socket %i...\n", tcpsock);
		closesocket(tcpsock);
	}
}

void Net_connection_tcp::connect(uint32_t adr, int port) {
	desthost[0]=0;
	destaddr=adr;
	destport=port;
	from=adr;
	tcpsin.sin_addr.s_addr = htonl(adr);
	tcpsin.sin_port = htons(port);
	skelton_msgbox("Net_connection_tcp::connect: Connecting socket to %s: %i... ", inet_ntoa(tcpsin.sin_addr), port);
	_state=connecting;
	if(net->getlasterror(::connect(tcpsock, (sockaddr *) &tcpsin, sizeof(tcpsin)))) {
		msgbox("Failed [%s]\n", net->last_error);
		_state=invalid;
		return;
	}
	skelton_msgbox("done!\nChecking socket... ");
	if(net->checkreceive(tcpsock) >= 0) {
		skelton_msgbox("done!\n");
	}
}

void Net_connection_tcp::connect(const char* host, int port) {
	if(strlen(host)<sizeof(desthost))
		strcpy(desthost, host);
	else {
		_state=disconnected;
		return;
	}
	destaddr=0;
	destport=port;
#ifdef WIN32
	if(net->name_handle==0) {
#endif
		destaddr = net->getaddress(host);
		if(net->port_resolve) // if 'host' contains a port, like "host:port"
			destport = net->port_resolve; // override the specified port
		msgbox("Net_connection_tcp::connect: host=%s, destaddr=%x port=%i\n", host, destaddr, destport);
		if(destaddr) { // if immediately resolved
			connect(destaddr, destport);
			return;
		}
		_state=dnslookup;
#ifdef WIN32
	}
	else
		_state=waitingfordns;
#endif
}

bool Net_connection_tcp::checktcp() {
	if(_state<connected || _state==disconnected || !packet_based)
		return false;
	if(tcppacsize)
		return true;
	while(net->checkreceive(tcpsock) > 0 && tcpbufsize<NETBUF_SIZE) {
		int temp = recv(tcpsock, (char *) &tcpbuf[tcpbufsize], NETBUF_SIZE-tcpbufsize, 0);
		// reset by remote side !
		net->getlasterror(temp);
		const char *msg = net->failed();
		if(msg) {
			skelton_msgbox("Net_connection_tcp::checktcp: Socket error on connection #%i. Shutting it.\n[%s]\n", tcpsock, msg);
			_state=disconnected;
			return false;
		}
		tcpbufsize+=temp;
		if(!temp) {
			//Connection closed gracefully
			_state=disconnected;
			return false;
		}
		incoming_inactive=0;
		incoming_size+=temp;
	}
	if(tcpbufsize>=sizeof(uint16_t)) {
		uint16_t pacsize=SDL_SwapLE16(*(uint16_t *)tcpbuf);
		if(!pacsize || (pacsize >= NETBUF_SIZE && pacsize != (('/'*256) +'/'))) {
			skelton_msgbox("Garbage received on connection #%i (%04X). Shutting it.\n", tcpsock, pacsize);
			_state=disconnected; // forcing a graceful shutdown
			return false;
		}
		if(pacsize==(('/'*256) +'/')) {
			skelton_msgbox("Net_connection_tcp::checktcp: connection #%i reverted to packet_based==false.\n", tcpsock);
			packet_based=false;
			incoming=new Buf(0, 256);
			if(tcpbufsize>sizeof(uint16_t))
				incoming->append(tcpbuf+sizeof(uint16_t), tcpbufsize-sizeof(uint16_t));
			tcpbufsize=0;
			char login[512];
			sprintf(login, "%s\r\n\r\n", net->net_param->get_motd());
			sendtcp((uint8_t *)login, strlen(login));
			return false;
		}
		if(tcpbufsize>=pacsize+sizeof(uint16_t)) {
			tcppacsize=pacsize;
			/*
			skelton_msgbox("Recv %i bytes (TCP): ", tcppacsize+sizeof(uint16_t));
			for(uint32_t i=0; i<tcppacsize+sizeof(uint16_t); i++)
				skelton_msgbox("%i, ", (uint8_t)tcpbuf[i]);
			skelton_msgbox("\n");
			*/
		}
	}
	return tcppacsize!=0;
}

void Net_connection_tcp::receivetcp(Net_buf *p) {
	if(!packet_based)
		return;
	p->from  = this;
	p->from_addr = from;
	memcpy(p->buf, tcpbuf+sizeof(uint16_t), tcppacsize);
	memmove(tcpbuf, tcpbuf+sizeof(uint16_t)+tcppacsize, NETBUF_SIZE-(tcppacsize+sizeof(uint16_t)));
	tcpbufsize-=tcppacsize+sizeof(uint16_t);
	tcppacsize=0;
}

int Net_connection_tcp::receivetcp(uint8_t *buf, uint32_t size) {
	if(_state<connected || _state==disconnected || packet_based)
		return 0;
	if(net->checkreceive(tcpsock) > 0) {
		int temp = recv(tcpsock, (char *) buf, size, 0);
		// reset by remote side !
		net->getlasterror(temp);
		const char *msg = net->failed();
		if(msg) {
			char st[64];
			net->stringaddress(st, address(), getdestport());
			skelton_msgbox("Net_connection_tcp::receivetcp: Socket error on connection %s.\n[%s]\n", st, msg);
			_state=disconnected;
			temp = 0;
		}
		if(!temp) {
			//Connection closed gracefully
			_state=disconnected;
		}
		if(temp)
			incoming_inactive=0;
		incoming_size+=temp;
		return temp;
	}
	return 0;
}

void Net_connection_tcp::sendtcp(Packet *p2) {
	if(_state<connected || _state==disconnected || !packet_based)
		return;
	Net_buf p;
	p2->write(&p);
	uint16_t size=p.len();
	static uint8_t outbuf[1026];
	*(uint16_t *) outbuf=SDL_SwapLE16(size);
	memcpy(&outbuf[2], p.buf, size);
	sendtcp(outbuf, size+2);
}

void Net_connection_tcp::sendtcp(const uint8_t *buf, uint32_t size) {
	static Net_connection_tcp *test=NULL;
	test=this;
	outgoing_inactive=0;
	outgoing_buf.append(buf, size);
	outgoing_size+=size;
}

void Net_connection_tcp::commit() {
	if(_state!=connected)
		return;
	Net_connection::commit(); //Only for stats accounting
	uint32_t size=outgoing_buf.size();
	if(!size)
		return;
	int temp = send(tcpsock, (const char *)(outgoing_buf.get()), size, 0);
	if(net->getlasterror(temp)) {
		char st[64];
		net->stringaddress(st, address(), getdestport());
		skelton_msgbox("Net_connection_tcp::commit: '%s', closing %s.\n", net->last_error, st);
		_state=disconnected;
		return;
	}

	if(temp > 0)
		outgoing_buf.remove_from_start(temp);

	if(outgoing_buf.size() > 262144) {
		char st[64];
		net->stringaddress(st, address(), getdestport());
		skelton_msgbox("Net_connection_tcp::commit: outgoing_buf size exceeds maximum: %i! Closing %s.\n", outgoing_buf.size(), st);
		_state=disconnected;
		return;
	}
}

Net_connection::Netstate Net_connection_tcp::state() {
	// retry sending buffered output if applicable
	commit();

	if(_state<waitingfordns || _state==disconnected || _state==connected) {
		return _state;
	}
	if(_state==waitingfordns) {
#ifdef WIN32
		if(net->name_handle==0) {
#endif
			uint32_t a = net->getaddress(desthost);
			if(a) {
				connect(a, destport);
				return _state;
#ifdef WIN32
			}
#endif
			_state=dnslookup;
		}
		return _state;
	}
	if(_state==dnslookup) {
#ifdef WIN32
		if(net->name_handle==0) {
#endif
			if(net->name_resolve==0) {
				net->gethostbyname_cancel();
				_state=disconnected;
				return _state;
			} else {
				connect(net->name_resolve, destport);
				return _state;
			}
#ifdef WIN32
		}
#endif
		return _state;
	}
	fd_set fdsoc;
	FD_ZERO(&fdsoc);
	FD_SET(tcpsock, &fdsoc);

	timeval empty_tv;
	empty_tv.tv_sec = 0;
	empty_tv.tv_usec = 0;
	int winsock_pourriture=0;
	for(;;) {
		int i = select(tcpsock+1, NULL, &fdsoc, NULL, &empty_tv);
		if(i > 0) {
			winsock_pourriture++;
			if(winsock_pourriture > 5) {
				skelton_msgbox("   ok it passed the test!\n");
				_state=connected;
				return _state;
			}
		} else {
			net->getlasterror(i);
			if(net->last_error) {
				skelton_msgbox("Net_connection_tcp::state(): socket failed [%s]\n", net->last_error);
				_state=disconnected;
			}
			if(i==0)
				skelton_msgbox("Net_connection_tcp::state(): socket %i, state=%i, is awaiting writability...\n", tcpsock, _state);
			return _state;
		}
	}
}

int Net_connection_tcp::getFD() const {
	return tcpsock;
}

Net::Net(Net_param *np) {
	active = false;
	net_param=np;
	client_connection=server_connection=NULL;
	last_error = NULL;

	name_resolve = (uint32_t)-1;
	port_resolve = -1;
	host_name[0] = 0;
	udpnum = 0;
	udpport=-1;
#ifdef WIN32
	name_buf[0] = 0;
	name_handle = 0;
	WORD wVersionRequested;
	WSADATA wsaData;

	wVersionRequested = MAKEWORD(1, 1);
	if(checkerror(WSAStartup(wVersionRequested, &wsaData)))
		return;

	if(LOBYTE(wsaData.wVersion) != 1 || HIBYTE(wsaData.wVersion) != 1) {
		last_error = "Wrong version of Windows Sockets DLL";
		return;
	}
#endif

	active = true;
	init_local_addresses();
}

Net::~Net() {
	if(active)
		gethostbyname_cancel();
	close_all_udp();
	stop_client();
	stop_server();
	if(net_param)
		delete net_param;
	if(!callbacks.empty())
		skelton_msgbox("Net::~Net: callback size should be 0, but it's %i\n", callbacks.size());
}

void Net::init_local_addresses() {
	if(!active)
		return;
	skelton_msgbox("Net::init_local_addresses: getting hostname\n");
	if(getlasterror(gethostname(host_name, 1024))) {
		skelton_msgbox("Net::Net: gethostname() failed. Ignoring\n");
		host_name[0] = 0;
	}
	else {
		skelton_msgbox("Net::Net: gethostname() is %s\n", host_name);
		// find the list of IP interfaces
		struct hostent *host;
		host = gethostbyname(host_name);
		if(host) {
			for (uint32_t** curptr = reinterpret_cast<uint32_t**>(host->h_addr_list); *curptr; ++curptr)
				host_adr.push_back(ntohl(**curptr));
			uint32_t fallback = INADDR_LOOPBACK;
			//Even better than what the NetGames guys do! :)
			for (size_t i = 0; i<host_adr.size(); i++) {
				uint32_t a=host_adr[i];
				bool pub = true;
				//192.168/16 is not public
				if(a>>16==192*256+168)
					pub = false;
				//Same thing for 172.16/12
				if(a>>20==172*16+1)
					pub = false;
				//And again for 10/8
				if(a>>24==10)
					pub = false;
				//Oh, and 127/8 while we're at it...
				if(a>>24==127)
					pub = false;
				if(fallback == INADDR_LOOPBACK) {
					//Got something that looks better than whatever we got up to that point
					fallback=a;
				}
				if(pub) {
					//Remember it as an internet address
					host_adr_pub.push_back(a);
				}
			}
			if(host_adr.empty()) {
				//No IP interfaces at all?! We'll assume loopback at least is there...
				host_adr.push_back(fallback);
			}
			if(host_adr_pub.empty()) {
				//Didn't find a public address, at least put in something
				host_adr_pub.push_back(fallback);
			}
		}
		else {
			skelton_msgbox("  gethostbyname() failed. Ignoring.\n");
		}
	}
}

void Net::init_all_udp() {
	if(!active)
		return;
	udpport = net_param->udpport();
	for (size_t i = 0; i<host_adr.size() && udpnum < 32; i++)
		open_udpsock(host_adr[i]);
	if(udpnum == 0) {
		skelton_msgbox("Net::Net: Opening default INADDR_ANY udp...\n");
		open_udpsock(0);
	}
}

void Net::close_all_udp() {
	if(!active)
		return;
	skelton_msgbox("Closing UDP sockets (%i)...\n", udpnum);
	for(int i=0; i<udpnum; i++) {
		closesocket(udpsock[i]);
	}
	udpnum = 0;
}

int Net::open_udpsock(uint32_t adr) {
	if(!active)
		return -1;
	int sock;
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if(getlasterror(sock)) {
		skelton_msgbox("Net::open_udpsock: error creating socket [%s]\n", failed());
		return -1;
	}

	int val = 1;
	if(getlasterror(setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char *) &val, sizeof(int)))) {
		skelton_msgbox("Net::open_udpsock: error setting broadcast [%s]. Ignoring.\n", failed());
	}

  udpsin.sin_family = AF_INET;
  udpsin.sin_addr.s_addr = htonl(adr);
  udpsin.sin_port = htons(udpport);

  if(getlasterror(bind(sock, (sockaddr *)&udpsin, sizeof(udpsin)))) {
		skelton_msgbox("Net::open_udpsock: error binding [%s]. Closing socket.\n", failed());
		closesocket(sock);
		return -1;
	}
	udpsock[udpnum++] = sock;
	skelton_msgbox("Net::open_udpsock: (UDP %i) opened\n", sock);
	return sock;
}

void Net::step(bool loop_only) {
	size_t i;
	for(i=0; i<connections.size(); i++)
		if(connections[i]) {
			connections[i]->incoming_inactive++;
			connections[i]->outgoing_inactive++;
		}
	if(!loop_only) {
		//Accept pending connections
		while(accept())
			;
	}
	while(true) {
		bool done=true;
		fd_set fdsoc;
		FD_ZERO(&fdsoc);
		int maxsock=0;
		#define ADD_SOCK(sock) { \
			FD_SET(sock, &fdsoc); \
			if(sock>maxsock) \
				maxsock=sock; \
		}

		for(i=0; i<connections.size(); i++)
			if(connections[i]->state()==Net_connection::connected) {
				int tcpsock=connections[i]->getFD();
				if(tcpsock==-1) {
					if(connections[i]->checktcp())
						done=false;
				}
				else
					if(!loop_only)
						ADD_SOCK(tcpsock);
			}
		if(client_connection && client_connection->state()==Net_connection::connected) {
			int tcpsock=client_connection->getFD();
			if(tcpsock==-1) {
				if(client_connection->checktcp())
					done=false;
			}
			else
				if(!loop_only)
					ADD_SOCK(tcpsock);
		}
		if(!loop_only)
			for(int j=0; j<udpnum; j++)
				ADD_SOCK(udpsock[i]);

		#undef ADD_SOCK

		if(!maxsock && done)
			break;
		timeval empty_tv;
		empty_tv.tv_sec = 0;
		empty_tv.tv_usec = 0;
		int tube;
		if(maxsock) {
			tube = select(maxsock+1, &fdsoc, NULL, NULL, &empty_tv);
			getlasterror(tube);
			const char *temp = failed();
			if(temp) {
				skelton_msgbox("Net::step: net error in select(): [%s]. Ignored\n", temp);
				break; // flush a potential network error
			}
			if(!tube && done)
				break;
		}

		for(int j=0; j<udpnum; j++)
			if(FD_ISSET(udpsock[j], &fdsoc)) {
				Net_buf nb;
				receiveudp(udpsock[j], &nb);
				packetreceived(&nb, false);
			}
		if(client_connection) {
			int tcpsock=client_connection->getFD();
			if(tcpsock==-1 || FD_ISSET(tcpsock, &fdsoc)) {
				//There's stuff to read from client_connection->tcpsock
				while(client_connection && client_connection->checktcp()) {
					Net_buf nb;
					client_connection->receivetcp(&nb);
					packetreceived(&nb, true);
				}
				verify_server_connection(); // verify if server has been disconnected
			}
		}
		for (size_t co = 0; co<connections.size(); co++) {
			Net_connection *nc=connections[co];
			bool isset=false;
			int tcpsock=nc->getFD();
			if(tcpsock==-1 || FD_ISSET(tcpsock, &fdsoc))
				isset=true;
			if(nc->state()==Net_connection::connected && isset) {
				//There's stuff to read from connections[co]->tcpsock
				if(nc->packet_based) {
					while(nc->checktcp()) {
						Net_buf nb;
						nc->receivetcp(&nb);
						packetreceived(&nb, true);
					}
				}
				else {
					//Not packet based, read everything into incoming
					uint8_t buf[1024];
					int num;
					while((num=nc->receivetcp(buf, 1024))) {
						nc->sendtcp(buf, num); //auto-echo
						Buf *out=nc->incoming;
						//Do something quick about backspace
						uint8_t *p1=buf;
						while(num) {
							if(*p1==8) {
								//Found a backspace, remove if not at line start
								uint32_t outsize=out->size();
								if(outsize) {
									uint8_t last=*(out->get()+outsize-1);
									if(last!='\r' && last!='\n')
										out->resize(outsize-1);
								}
							}
							else {
								out->append(p1, 1);
							}
							p1++;
							num--;
						}
					}
				}
			}
		}
		verify_connections(); //Verify all client connections and notify if something changed
	}
	verify_connections(); //One last time, just to be sure :)
	for(i=0; i<connections.size(); i++)
		connections[i]->commit();
	if(client_connection)
		client_connection->commit();
}

void Net::verify_connections() {
	bool change=false;
	for (size_t i = 0; i<connections.size(); i++) {
		Net_connection *nc=connections[i];
		if(nc->state()==Net_connection::disconnected) {
			net_param->client_deconnect(nc);
			delete nc;
			connections.erase(connections.begin() + i);
			i--;
			change=true;
		}
	}
	if(change)
		notify_all();
}

void Net::verify_server_connection() {
	if(client_connection->state()!=Net_connection::connected) {
		//Graceful and sudden server deconnection
		stop_client();
		net_param->server_deconnect();
	}
}

void Net::addwatch(uint16_t id, Net_callable *nc) {
	for (list<Net_receive_cb*>::const_iterator it = callbacks.begin();
	     it != callbacks.end(); ++it)
		if ((*it)->id == id && (*it)->net_callable == nc)
			return;

	callbacks.push_back(new Net_receive_cb(id, nc));
}

void Net::removewatch(uint16_t id, Net_callable *nc) {
	for (list<Net_receive_cb*>::iterator it = callbacks.begin();
	     it != callbacks.end(); ++it)
		if ((*it)->id == id && (*it)->net_callable == nc) {
			delete *it;
			callbacks.erase(it);
			break;
		}
}

void Net::sendudp(uint32_t to, Packet *p) {
	Net_buf nb;
	p->write(&nb);

  udpsin.sin_addr.s_addr = htonl(to);
	int i;
	for(i=0; i<udpnum; i++) {
		int temp = sendto(udpsock[i], (const char *) nb.buf, nb.len(), 0, (sockaddr *) &udpsin, sizeof(udpsin));
		if(getlasterror(temp))
			return; // failed!
		if(temp != static_cast<int>(nb.len())) {
			last_error = "Protocol UDP did not send all requested data";
			return; // failed!
		}
		/*
		skelton_msgbox("Sent %i bytes to %s (UDP %i): ", temp, inet_ntoa(udpsin.sin_addr), udpsock[i]);
		for(int j=0; j<temp; j++)
			skelton_msgbox("%i,", nb.buf[j]);
		skelton_msgbox("\n");
		*/
	}
}

Net_connection *Net::server_addr() {
	if(client_connection && client_connection->state()==Net_connection::connected)
		return client_connection;
	else
		return NULL;
}

void Net::start_server(bool sock) {
	if(server_connection)
		return;
	skelton_msgbox("Server listening for TCP connection on port %i... ", net_param->tcpport());
	if(active && sock) {
		Net_connection_tcp *sc=new Net_connection_tcp(net_param->tcpport());
		if(sc->state()==Net_connection::invalid) {
			skelton_msgbox("but it failed with [%s]\n", last_error);
			delete sc;
		}
		else {
			skelton_msgbox("Ok\n");
			sockaddr_in sin;
			socklen_t len=sizeof(sin);
			callwsa(getsockname(sc->getFD(), (sockaddr *) &sin, &len));
			sc->from=ntohl(sin.sin_addr.s_addr);
			msgbox("server_connection: %p\n", sc);
			server_connection=sc;
			return;
		}
	}
	//Fail-safe: internal loopback connection
	server_connection=new Net_connection();
}

void Net::stop_server() {
	while (!connections.empty()) {
	  delete connections.back();
    connections.pop_back();
	}
	suspend_server();
	notify_all();
}

void Net::suspend_server() {
	if(server_connection) {
		skelton_msgbox("Server stop listening\n");
		delete server_connection;
		server_connection=NULL;
	}
}

void Net::sendtcp(Net_connection *nc, Packet *p) {
	if(nc && p) {
		nc->sendtcp(p);
	}
}

void Net::dispatch(Packet *p, uint32_t pt, Net_connection *nc) {
	if(!p)
		return;
	p->packet_id=pt;
	for (size_t i = 0; i<connections.size(); i++) {
		Net_connection *nc2=connections[i];
		if(nc2!=nc && nc2->packet_based && net_param->is_dispatchable(nc2, p)) {
			connections[i]->sendtcp(p);
		}
	}
}

Net_connection *Net::start_loopback_client() {
	if(client_connection)
		return NULL;
	client_connection=new Net_connection();
	Net_connection *accepted_connection=new Net_connection();
	connections.push_back(accepted_connection);
	client_connection->connect(accepted_connection);
	return accepted_connection;
}

void Net::start_client(uint32_t adr, int port) {
	if(client_connection)
		return;
	if(!active) {
		msgbox("Net::start_client: can't start client when not active!\n");
		return;
	}
	client_connection=new Net_connection_tcp(0);
	if(client_connection->state()==Net_connection::invalid) {
		delete client_connection;
		client_connection=NULL;
		return;
	}
	if(!port)
		port = net_param->tcpport();
	client_connection->connect(adr, port);
	msgbox("client_connection: %p\n", client_connection);
}

void Net::stop_client() {
	if(client_connection) {
		client_connection->commit();
		delete client_connection;
		client_connection=NULL;
	}
}

Net_connection_tcp *Net::start_other(uint32_t adr, int port) {
	Net_connection_tcp *nc=new Net_connection_tcp(0, false);
	if(nc->state()==Net_connection::invalid) {
		delete nc;
		return NULL;
	}
	nc->connect(adr, port);
	return nc;
}

Net_connection_tcp *Net::start_other(const char *host, int port) {
	Net_connection_tcp *nc=new Net_connection_tcp(0, false);
	if(nc->state()==Net_connection::invalid) {
		delete nc;
		return NULL;
	}
	nc->connect(host, port);
	return nc;
}

Packet *Net::net_buf2packet(Net_buf *nb, bool tcp) {
	Packet p;
	if(!p.read(nb)) {
		//Invalid signature
		return NULL;
	}
	Packet *packet=net_param->alloc_packet(p.packet_id);
	packet->istcp=tcp;
	if(packet) {
		if(!packet->read(nb)) {
			delete packet;
			packet=NULL;
			skelton_msgbox("  bad packet\n  ");
			uint16_t size=min(nb->len(), static_cast<unsigned int>(128));
			nb->reset();
			int i;
			for(i=0; i<size; i++)
				skelton_msgbox("%02x, ", nb->buf[i]);
		}
	}
	else {
		skelton_msgbox("Packet_id %i not allocated by alloc_packet()!\n", p.packet_id);
	}
	return packet;
}

bool Net::connected() {
	if(!client_connection || client_connection->state()!=Net_connection::connected)
		return false;
	return true;
}

void Net::sendtcp(Packet *p)  {
	if(client_connection) {
		client_connection->sendtcp(p);
		verify_server_connection(); // verify if server has been disconnected
	}
}

uint32_t Net::dotted2addr(const char *host) {
  uint32_t lAddr = INADDR_NONE;

  // check that we have a string
  if(*host) {
    // check for a dotted-IP address string
    lAddr = ntohl(inet_addr(host));
	}
	return lAddr;
}

uint32_t Net::getaddress(const char *host) {
  uint32_t lAddr = INADDR_ANY;

  // check that we have a string
  if(*host) {
		char tube[1024];
		strncpy(tube, host, 1023);
		tube[1023] = 0;
		char *adr = strchr(tube, ':');
		if(adr) {
			port_resolve = atoi(adr+1);
			*adr = 0;
		} else {
			port_resolve = 0;
		}
    // check for a dotted-IP address string
    lAddr = ntohl(inet_addr(tube));

    // If not an address, then try to resolve it as a hostname
    if((lAddr == INADDR_NONE) && (strcmp(tube, "255.255.255.255"))) {
			lAddr = 0;
#if 0
			// FIXME: Should make every DNS lookup async.
			gethostbyname_cancel();
			name_handle = WSAAsyncGetHostByName(hwnd, WM_USER, tube, name_buf, MAXGETHOSTSTRUCT);
			if (name_handle == 0) {  // if error return 0 
				name_resolve = 0;  // impossible to resolve DNS because of Winsock and/or Windows
			}
#else
			struct hostent *lpstHost;
			// Blocking call!!
			lpstHost = gethostbyname(tube);
			if (lpstHost)  // success
				lAddr = ntohl(*(uint32_t*)lpstHost->h_addr_list[0]);
#endif
    }
  }
  return lAddr;
}

void Net::stringaddress(char *st, uint32_t adr) {
	struct in_addr in;
	in.s_addr = htonl(adr);
	strcpy(st, inet_ntoa(in));
}

void Net::stringaddress(char *st, uint32_t adr, int port) {
	struct in_addr in;
	in.s_addr = htonl(adr);
	strcpy(st, inet_ntoa(in));
	char st2[16];
	sprintf(st2, ":%i", port);
	strcat(st, st2);
}

void Net::gethostbyname_completed(bool success) {
#ifdef WIN32
	name_handle = 0;
	if(success)
		name_resolve = ntohl(*(uint32_t *)((LPHOSTENT) name_buf)->h_addr);
#endif
	if(!success)
		name_resolve = 0;
}

void Net::gethostbyname_cancel() {
#ifdef WIN32
	if(name_handle)
		WSACancelAsyncRequest(name_handle);
	name_handle = 0;
	name_resolve = -1;
#endif
}

void Net::packetreceived(Net_buf *nb, bool tcp) {
	Packet pac;
	struct in_addr in;
	if(!pac.read(nb)) {
		//Invalid signature
		return;
	}
	in.s_addr = htonl(pac.from_addr);
	list<Net_receive_cb*>::const_iterator it = callbacks.begin();
	while (it != callbacks.end()) {
		list<Net_receive_cb*>::const_iterator cur = it;
		++it;
		if ((*cur)->id == pac.packet_id) {
			Packet* const packet(net_buf2packet(nb, tcp));
			if (packet)
				(*cur)->net_callable->net_call(packet);
			else
				skelton_msgbox("Packet_id %i not allocated by net_buf2packet()!\n", pac.packet_id);
		}
	}
}

void Net::receiveudp(int sock, Net_buf *p) {
	sockaddr_in tsin;
	socklen_t tsin_size = sizeof(tsin);
	int temp = recvfrom(sock, (char *) p->buf, NETBUF_SIZE, 0, (sockaddr *) &tsin, &tsin_size);
	p->from = NULL;
	p->from_addr = ntohl(tsin.sin_addr.s_addr);
	if(getlasterror(temp)) {
		const char *msg = net->failed();
		if(msg) {
			skelton_msgbox("Net::receiveudp: Error. Ignoring it. [%s]\n", msg);
		}
		return;
	}
	/*
	skelton_msgbox("Recv %i bytes from %s (UDP %i): ", p->size, inet_ntoa(tsin.sin_addr), sock);
	for(int i=0; i<p->size; i++)
		skelton_msgbox("%i,", p->buf[i]);
	skelton_msgbox("\n");
	*/
}

bool Net::accept() {
	if(!active)
		return false;
	sockaddr_in bob;
	socklen_t boblen=sizeof(bob);

	if(!server_connection || server_connection->state()==Net_connection::invalid)
		return false;
	int sock=::accept(server_connection->getFD(), (sockaddr *) &bob, &boblen);
	if(sock > 0) {
		uint32_t adr=ntohl(bob.sin_addr.s_addr);
		uint16_t port=ntohs(bob.sin_port);
		skelton_msgbox("Net::accept adding connection from %s:%i\n", inet_ntoa(bob.sin_addr), port);
		Net_connection_tcp *nc=new Net_connection_tcp(sock, adr, port);
		if(net_param->accept_connection(nc)) {
			connections.push_back(nc);
		}
		else {
			skelton_msgbox("...fail because too many connections or connection refused\n");
			delete nc;
			return false;
		}
		net_param->client_connect(nc);
		notify_all();
		return true;
	}
	return false;
}

int Net::checkreceive(int s) {
	fd_set fdsoc;
	FD_ZERO(&fdsoc);
	FD_SET(s, &fdsoc);

	timeval empty_tv;
	empty_tv.tv_sec = 0;
	empty_tv.tv_usec = 0;

	int tube = select(s+1, &fdsoc, NULL, NULL, &empty_tv);
	getlasterror(tube);
	const char *msg = failed();
	if(msg) {
		skelton_msgbox("Net::checkreceive error [%s], ignoring it\n", msg);
	}
	return tube;
}

bool Net::checkerror(int quel) {
	if(quel == 0)
		return false;
#ifdef WIN32
	switch(quel) {
//	case WSAEWOULDBLOCK:	last_error = "The socket is marked as non-blocking and this operation would block."; break;
	case WSAEWOULDBLOCK: skelton_msgbox("Net::checkerror: WSAEWOULDBLOCK ignored.\n"); return false; // special: not really an error
	case WSASYSNOTREADY: last_error = "The underlying network subsystem is not ready for network communication."; break;
	case WSAVERNOTSUPPORTED: last_error = "The version of Winsock API support requested is not provided by this implementation."; break;
	case WSAEINVAL:	last_error = "Operation invalid or not supported by this Windows Sockets DLL"; break;
	case WSANOTINITIALISED:	last_error = "A successful WSAStartup() must occur before using this API."; break;
	case WSAENETDOWN:	last_error = "The Windows Sockets implementation has detected that the network subsystem has failed."; break;
	case WSAEADDRINUSE:	last_error = "The specified address is already in use."; break;
	case WSAEINTR: last_error = "The (blocking) call was canceled via WSACancelBlockingCall()"; break;
	case WSAEINPROGRESS: last_error = "A blocking Windows Sockets call is in progress."; break;
	case WSAEAFNOSUPPORT:	last_error = "The specified address family is not supported by this protocol."; break;
	case WSAENOBUFS: last_error = "Not enough buffers available, too many connections."; break;
	case WSAENOTSOCK: last_error = "The descriptor is not a socket."; break;
	case WSAHOST_NOT_FOUND:	last_error = "Authoritative Answer Host not found."; break;
	case WSATRY_AGAIN:	last_error = "Non-Authoritative Host not found, or SERVERFAIL."; break;
	case WSANO_RECOVERY:	last_error = "Non recoverable errors, FORMERR, REFUSED, NOTIMP."; break;
	case WSANO_DATA:	last_error = "Valid name, no data record of requested type."; break;
	case WSAEACCES:	last_error = "The requested address is a broadcast address."; break;
	case WSAEFAULT:	last_error = "Invalid or too small argument"; break;
	case WSAENETRESET:	last_error = "The connection must be reset because Winsock implementation dropped it."; break;
	case WSAENOTCONN:	last_error = "The socket is not connected (SOCK_STREAM only)."; break;
	case WSAEOPNOTSUPP:	last_error = "MSG_OOB was specified, but the socket is not of type SOCK_STREAM."; break;
	case WSAESHUTDOWN:	last_error = "The socket has been shutdown."; break;
	case WSAEMSGSIZE:	last_error = "The datagram is larger than the maximum supported by Winsock."; break;
	case WSAECONNABORTED:	last_error = "The virtual circuit was aborted due to timeout or other failure."; break;
	case WSAECONNRESET:	last_error = "The virtual circuit was reset by the remote side."; break;
	case WSAEADDRNOTAVAIL:	last_error = "The specified address is not available from the local machine."; break;
	case WSAEDESTADDRREQ:	last_error = "A destination address is required."; break;
	case WSAENETUNREACH:	last_error = "The network can't be reached from this host at this time."; break;
	case WSAECONNREFUSED: last_error = "The attempt to connect was forcefully rejected."; break;
	case WSAEISCONN: last_error = "The socket is already connected."; break;
	case WSAEMFILE: last_error = "No more file descriptors are available."; break;
	case WSAETIMEDOUT: last_error = "Attempt to connect timed out without establishing a connection."; break;
	case WSAEHOSTUNREACH: last_error = "The destination address is unreachable."; break;
	default: last_error = "Unknown error"; break;
	}
#else
	switch(errno) {
	case EWOULDBLOCK:
	case EINTR:
	case EINPROGRESS:
	case EHOSTUNREACH:
	  return false;
	}
	last_error = strerror(errno);
	skelton_msgbox("errno = %i (%s)\n", errno, last_error);
#endif
	return true;
}

bool Net::getlasterror(int quel) {
	if(quel >= 0)
		return false;
#ifdef WIN32
	return checkerror(WSAGetLastError());
#else
	return checkerror(errno);
#endif
}

void Net::callwsa(int quel) {
	if(getlasterror(quel))
		fatal_msgbox(last_error);
}

const char *Net::failed() {
	const char *tube;
	if(last_error) {
		tube = last_error;
		last_error = NULL;
	} else
		tube = NULL;
	return tube;
}

Net *net=NULL;
