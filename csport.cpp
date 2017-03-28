// -*- C++ -*-
//////////////////////////////////////////////////////////////////////////
//
// $Id: csport.cpp 325 2014-02-17 00:20:55Z bprewit $
//
// purpose:
//
// NOTES:
//
//
// Copyright (c) TenStar Technologies 2006
////////////////////////////////////////////////////////////////////////
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

#ifdef USE_LOCKING
#include <lockdev.h>
#endif

#include <termios.h>
#include <unistd.h>


#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <debug.h>

#include "csport.h"

////////////////////////////////////////////////////////////////////////
//
// name: 			CSport - default constructor
//
// purpose:
//
// calling parms:
//
// return parms:
//
// NOTES:
//
//
////////////////////////////////////////////////////////////////////////
CSport::CSport()
{
	sport_fd 	= -1;
}

////////////////////////////////////////////////////////////////////////
//
// name: ~CSport - default destructor
//
// purpose:
//
// calling parms:
//
// return parms:
//
// NOTES:
//

//
////////////////////////////////////////////////////////////////////////
CSport::~CSport()
{
    close(sport_fd);			      // close
    sport_fd = -1;			      // set fd to invalid
}

////////////////////////////////////////////////////////////////////////
//
// name: sport_open
//
// purpose: convenience function for opening serial port.
//
// Takes single QString parameter vs. list; String is formatted like
// "portname,speed,databits,paritychar,stopbits", ie:
// "/dev/ttyS0,9600,8,N,1"
//
// calling parms: QString port_str - port data string (see above)
//
//
// return parms: fd on success or -1 on failure
//
// NOTES:
//
//
////////////////////////////////////////////////////////////////////////
int CSport::sport_open(QString port_str)
{
	QStringList	fields;
	string		port_name;
	ULONG		br;
	UINT		db;
	char		pc;
	PARITY_T 	pb;
	UINT		sb;

	fields = QStringList::split(",",port_str); // separate fields

	if(fields[0].isNull() || fields[0].isEmpty())
	{
		return(-1);
	}

	port_name = string(fields[0].ascii());     // port name
	sport_name = port_name;					   // save to data

	br = fields[1].toULong();				   // baudrate
	db = fields[2].toUInt();				   // data bits

	pc = fields[3][0].latin1();				   // parity character N/E/O
	pb = ctop(pc);							   // parity bits

	sb = fields[4].toUInt();				   // stop bits

	return(sport_open(port_name.c_str(), br, db, pb, sb));
}

////////////////////////////////////////////////////////////////////////
//
// name: 			CSport::sport_open
//
// purpose: 		open serial port w/ params
//
// calling parms:	const char	*port_name	/dev/name_of_port
//					ULONG		speed		baud rate
//					UINT		dbits		data bits 6/7/8
//					PARITY_T	par		parity n/e/o/m/s
//					UINT		sbits		stop bits 1/2
//
// return parms: sport_fd on success, EOF (-1) on failure
//
// NOTES:
//

//
////////////////////////////////////////////////////////////////////////
int CSport::sport_open(const char	*port_name,
					   ULONG 		speed,
					   UINT			dbits,
					   PARITY_T 	par,
					   UINT			sbits,
					   bool			nblock)
{

	struct termios tios;
	int flags;

	if(strlen(port_name) == 0)
	{
		GRIPE("INVALID PORT SETTING");
		return(-1);
	}

	if(dbg_lvl > 0)
	{
		cerr << "Opening " << port_name << " (" << speed << "/" << dbits << "/" << par << "/" << sbits << ")" << endl;
	}

	//
	// open port_name with mode flags
	// O_NOCTTY - don't become controlling tty
	// O_NDELAY - non-blocking I/O
	// ** ADDING O_NDELAY WILL BREAK SELECT() ON DEMAND SCALES (uses select to determine if data avail)
	//
	flags = (O_RDWR | O_NOCTTY);
	if(!nblock) flags |= O_NDELAY;

	sport_fd = open(port_name,flags);
	if(sport_fd == -1)
	{
		GRIPE("COULD NOT OPEN PORT " + string(port_name));
		perror("sport_open (open failed)");
		return(-1);
	}

	// TODO: lock serial port

	// save old port i/o attribs (so they can be put back)
	if(tcgetattr(sport_fd, &old_ios) != 0)
	{
		perror("sport_open (tcgetattr)");
		return(-1);
	}

	// general port initialization
	if(tcgetattr(sport_fd, &tios) != 0)
	{
		perror("sport_open (tcgetattr)");
		return(-1);
	}

	// cfmakeraw sets tios as follows: (from manpage)
	//
	//   termios_p->c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);
	//   termios_p->c_oflag &= ~OPOST;
	//   termios_p->c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
	//   termios_p->c_cflag &= ~(CSIZE|PARENB);
	//   termios_p->c_cflag |= CS8;

	cfmakeraw(&tios);

	// enable receiver & disregard control lines
	tios.c_cflag |= (CREAD|CLOCAL);
	tios.c_cflag &= ~CRTSCTS;

	// translate CR to NL on input
	tios.c_iflag |= ICRNL;

	// tios.c_iflag |= IGNCR;

	// disable canonical input processing
	tios.c_lflag &= ~ICANON;

	if(tcsetattr(sport_fd, TCSANOW, &tios) != 0)
	{
		perror("sport_open (tcsetattr)");
		return(-1);
	}

	// set requested serial parameters
	if(set_baudrate(speed) ||
	   set_databits(dbits) ||
	   set_parity(par)	 ||
	   set_stopbits(sbits))
	{
		perror("sport_open (setting port params)");
		return(-1);
	}
	return(sport_fd);
}


////////////////////////////////////////////////////////////////////////
//
// name:
//
// purpose: close serial port
//
// calling parms:
//
// return parms:
//
// NOTES:
//
//
////////////////////////////////////////////////////////////////////////
int CSport::sport_close()
{
	if(close(sport_fd))
	{
		perror("sport_close");
		return(-1);
	}
	return(0);
}

////////////////////////////////////////////////////////////////////////
//
// name: CSport::set_baudrate
//
// purpose:  set serial port baudrate
//
// calling parms: long baudrate
//
// return parms: int = 0 on success, non-zero on failure
//
// NOTES:
//
//
////////////////////////////////////////////////////////////////////////
int CSport::set_baudrate(ULONG baud)
{
	ULONG baud_val;
	struct termios tios;

	// save data
	baudrate = baud;

	// check port open & fetch struct ios
	if((sport_fd == -1) || (tcgetattr(sport_fd, &tios) != 0))
	{
		perror("set_baudrate (tcgetattr)");
		return(-1);
	}

	// convert numeric baudrate to bitmask
	baud_val = ultob(baud);

	// set port input & output speeds as requested
	if(cfsetispeed(&tios, baud_val) != 0 || cfsetospeed(&tios, baud_val) != 0)
	{
		perror("set_baudrate (cfset[io]speed)");
		return(-1);
	}

	// set ios to keep changes
	if(tcsetattr(sport_fd, TCSANOW, &tios) != 0)
	{
		perror("set_baudrate (tcsetattr)");
		return(-1);
	}
	return(0);
}


////////////////////////////////////////////////////////////////////////
//
// name: get_baudrate
//
// purpose: return serial port baudrate
//
// calling parms: none
//
// return parms: long int = baudrate
//
// NOTES:
//
//
////////////////////////////////////////////////////////////////////////
long CSport::get_baudrate()
{
	ULONG baudrate;

	tcgetattr(sport_fd, &old_ios);
	baudrate = cfgetospeed(&old_ios);
	return(btoul(baudrate));
}

////////////////////////////////////////////////////////////////////////
//
// name: set_databits
//
// purpose: set serial port data bits (7/8)
//
// calling parms: int = data bits
//
// return parms: zero on success, non-zero on failure
//
// NOTES:
//
//
////////////////////////////////////////////////////////////////////////
int CSport::set_databits(UINT datab)
{
	ULONG cs_mask;
	struct termios tios;

	// save data
	databits = datab;

	// check port open && fetch ios
	if((sport_fd == -1) || tcgetattr(sport_fd, &tios))
	{
		perror("set_databits (tcgetattr)");
		return(-1);
	}

	// set char size mask
	switch(datab)
	{
	case 5:
		cs_mask = CS5;
		break;

	case 6:
		cs_mask = CS6;
		break;

	case 7:
		cs_mask = CS7;
		break;

	case 8:
		cs_mask = CS8;
		break;

	default:
		cerr << "set_databits: invalid value (" << datab << ")" << endl;
		return(-1);
		break;
	}

	tios.c_cflag &= ~CSIZE;	// clear existing char-size
	tios.c_cflag |= cs_mask;	// set new char-size mask

	if(tcsetattr(sport_fd, TCSANOW, &tios) != 0)
	{
		perror("set databits (tcsetattr)");
		return(-1);
	}
	return(0);
}

////////////////////////////////////////////////////////////////////////
//
// name: get_databits
//
// purpose: return serial port databits
//
// calling parms: none
//
// return parms: int = data bits
//
// NOTES:
//
//
////////////////////////////////////////////////////////////////////////
int CSport::get_databits()
{
	int db;
	struct termios tios;

	if(sport_fd == -1 || tcgetattr(sport_fd, &tios))
	{
		perror("get_databits (tcgetattr)");
		return(-1);
	}

	switch(tios.c_cflag & CSIZE)
	{
	case CS5:
		db = 5;
		break;

	case CS6:
		db = 6;
		break;

	case CS7:
		db = 7;
		break;

	case CS8:
		db = 8;
		break;

	default:
		db = -1;
		break;
	}
	return(db);
}

////////////////////////////////////////////////////////////////////////
//
// name: set_stopbits
//
// purpose: set serial port stop bits
//
// calling parms: int = number of stop bits (1 or 2)
//
// return parms: as usual
//
// NOTES:
//
//
////////////////////////////////////////////////////////////////////////
int CSport::set_stopbits(UINT stop)
{
	struct termios tios;

	// save data
	stopbits = stop;

	// fetch port attributes
	if(sport_fd == -1 || tcgetattr(sport_fd, &tios))
	{
		perror("set_stopbits (tcgetattr failed?)");
		return(-1);
	}

	// set bitmask
	switch(stop)
	{
	case 1:
		tios.c_cflag &= ~CSTOPB;	// clear flag => 1 stop bit
		break;
	case 2:
		tios.c_cflag |=  CSTOPB;	// set flag => 2 stop bits
		break;
	default:
		cerr << "set_stopbits: invalid value (" << stop << ")" << endl;
		return(-1);
		break;
	}

	// set port attrib
	if(tcsetattr(sport_fd, TCSANOW, &tios) != 0)
	{
		perror("set_stopbits (tcsetattr)");
		return(-1);
	}
	return(0);
}

////////////////////////////////////////////////////////////////////////
//
// name: get_stopbits
//
// purpose: return int = number of stop bits (1 or 2)
//
// calling parms: none
//
// returns:
//
// NOTES:
//
//
////////////////////////////////////////////////////////////////////////
int CSport::get_stopbits()
{
	struct termios tios;

	// retrieve current parameters
	if(sport_fd == -1 || tcgetattr(sport_fd, &tios))
	{
		perror("get_stopbits (tcgetattr)");
		return(-1);
	}

	// CSTOPB flag set => 2 stop bits
	if(tios.c_cflag & CSTOPB)
	{
		return(2);
	}
	return(1);			// CSTOPB clear => 1 stop bit
}


////////////////////////////////////////////////////////////////////////
//
// name: set_parityb
//
// purpose: set serial port parity
//
// calling parms: CSport::PARITY_T
//
// return parms: non-zero on failure, zero on success
//
// NOTES:
//
//
////////////////////////////////////////////////////////////////////////
int CSport::set_parity(PARITY_T parity)
{
	struct termios tios;

	parityb = parity;

	// retrieve current parameters
	if(sport_fd == -1 || tcgetattr(sport_fd, &tios))
	{
		perror("set_parityb (tcgetattr)");
		return(-1);
	}

	switch(parity)
	{
	case PARITY_SPACE:				// 'space' parity - parity bit always clear
	case PARITY_NONE:				// no parity
		tios.c_cflag &= ~PARENB;	// clear parity enable bit
		tios.c_iflag &= ~INPCK;		// clear input parity checking
		break;

	case PARITY_EVEN:
		tios.c_cflag &= ~PARODD;	// clear odd-parity-check-and-generate bit
		tios.c_cflag |= PARENB;		// set parity enable bit
		// tios.c_iflag |= INPCK;	// set input parity checking
		tios.c_iflag &= ~INPCK;		// clear input parity checking
		break;

	case PARITY_ODD:
		tios.c_cflag |= PARODD;		// set odd-parity bit
		tios.c_cflag |= PARENB;		// parity enable
		// tios.c_iflag |= INPCK;	// enable input parity checking
		tios.c_iflag &= ~INPCK;		// clear input parity checking
		break;

	case PARITY_MARK:				// 'mark' parity - parity bit always set
		// TODO: figure out how to set mark parity
		break;

	case PARITY_IGNORE:
		return(0);
		break;

	case PARITY_INVAL:
	default:
		return(-1);
		break;
	}

	if(tcsetattr(sport_fd, TCSANOW, &tios) != 0)
	{
		perror("set_parityb (tcsetattr)");
		return(-1);
	}
	return(0);
}

////////////////////////////////////////////////////////////////////////
//
// name: set_parityb
//
// purpose: overloaded from above
//
// calling parms: char c like 'NEO'
//
// return parms: same as above
//
// NOTES:
//
//
////////////////////////////////////////////////////////////////////////
int CSport::set_parity(char c)
{
	PARITY_T p = ctop(c);
	return(set_parity(p));
}


////////////////////////////////////////////////////////////////////////
//
// name:
//
// purpose: return parity settings
//
// calling parms:
//
// return parms:
//
// NOTES:
//
//
////////////////////////////////////////////////////////////////////////
CSport::PARITY_T CSport::get_parity()
{
	PARITY_T parityb;
	struct termios tios;

	// retrieve current parameters
	if((sport_fd == -1) || tcgetattr(sport_fd, &tios))
	{
		perror("get_parityb (tcgetattr)");
		return(PARITY_NONE);
	}


	if(tios.c_cflag & PARENB)	// parity enabled?
	{
		if(tios.c_cflag & PARODD)	// odd parity bit set?
		{
			parityb = PARITY_ODD;
		}
		else
		{
			parityb = PARITY_EVEN;	// parity set & not odd, has to be even
		}
	}
	else				// not enabled -> no parity
	{
		parityb = PARITY_NONE;
	}
	return(parityb);
}

////////////////////////////////////////////////////////////////////////
//
// name: CSport::btoul
//
// purpose: convert bitmask values to unsigned long values
//
// calling parms: bitmask b
//
// return parms: unsigned long numeric value
//
// NOTES:
//
//
////////////////////////////////////////////////////////////////////////
ULONG CSport::btoul(ULONG b)
{
	long baud;

	switch(b)
	{
	default:
	case B0:
		baud = 0;
		break;

	case B50:
		baud = 50;
		break;

	case B75:
		baud = 75;
		break;

	case B110:
		baud = 110;
		break;

	case B134:
		baud = 134;
		break;

	case B150:
		baud = 150;
		break;

	case B200:
		baud = 200;
		break;

	case B300:
		baud = 300;
		break;

	case B600:
		baud = 600;
		break;

	case B1200:
		baud = 1200;
		break;

	case B1800:
		baud = 1800;
		break;

	case B2400:
		baud = 2400;
		break;

	case B4800:
		baud = 4800;
		break;

	case B9600:
		baud = 9600;
		break;

	case B19200:
		baud = 19200;
		break;

	case B38400:
		baud = 38400;
		break;

	case B57600:
		baud = 57600;
		break;

	case B115200:
		baud = 115200;
		break;

	case B230400:
		baud = 230400;
		break;
	}

	return(baud);
}

////////////////////////////////////////////////////////////////////////
//
// name: CSport::ultob
//
// purpose: convert numeric (unsigned long) values to manifest consts
//
// calling parms: unsigned long ul - value to convert
//
// return parms: unsigned long bitmask
//
// NOTES:
//
//
////////////////////////////////////////////////////////////////////////
ULONG CSport::ultob(ULONG ul)
{
	ULONG baud;

	switch(ul)
	{
	default:
	case 0:
		baud = B0;
		break;

	case 50:
		baud = B50;
		break;

	case 75:
		baud = B75;
		break;

	case 110:
		baud = B110;
		break;

	case 134:
		baud = B134;
		break;

	case 150:
		baud = B150;
		break;

	case 200:
		baud = B200;
		break;

	case 300:
		baud = B300;
		break;

	case 600:
		baud = B600;
		break;

	case 1200:
		baud = B1200;
		break;

	case 1800:
		baud = B1800;
		break;

	case 2400:
		baud = B2400;
		break;

	case 4800:
		baud = B4800;
		break;

	case 9600:
		baud = B9600;
		break;

	case 19200:
		baud = B19200;
		break;

	case 38400:
		baud = B38400;
		break;

	case 57600:
		baud = B57600;
		break;

	case 115200:
		baud = B115200;
		break;

	case 230400:
		baud = B230400;
		break;
	}

	return(baud);
}

////////////////////////////////////////////////////////////////////////
//
// name: ctop
//
// purpose: convert 'character' parity to enum value
//
// calling parms: char 'NEO'
//
// return parms: PARITY_T
//
// NOTES:
//
//
////////////////////////////////////////////////////////////////////////
CSport::PARITY_T CSport::ctop(char c)
{
	PARITY_T p;

	switch(toupper(c))
	{

	case 'E':
		p = PARITY_EVEN;
		break;

	case 'O':
		p = PARITY_ODD;
		break;

	case 'M':
		p = PARITY_MARK;
		break;

	case 'S':
		p = PARITY_SPACE;
		break;

	case 'N':
		p = PARITY_NONE;
		break;

	case 'X':
		p = PARITY_IGNORE;
		break;

	default:
		p = PARITY_INVAL;
	}
	return(p);
}


////////////////////////////////////////////////////////////////////////
//
// name: ptoc
//
// purpose: return 'char' representing PARITY_T
//
// calling parms: PARITY_T
//
// return parms: char 'c'
//
// NOTES: included for completeness
//
//
////////////////////////////////////////////////////////////////////////
char CSport::ptoc(PARITY_T p)
{
	char c;

	switch(p)
	{
	case PARITY_NONE:
		c = 'N';
		break;

	case PARITY_ODD:
		c = 'O';
		break;

	case PARITY_EVEN:
		c = 'E';
		break;

	case PARITY_MARK:
		c = 'M';
		break;

	case PARITY_SPACE:
		c = 'S';
		break;

	case PARITY_IGNORE:
		c = 'X';
		break;

	case PARITY_INVAL:
	default:
		c = 'I';
		break;
	}

	return(c);
}

////////////////////////////////////////////////////////////////////////
//
// name:
//
// purpose:		wait for output to complete
//
// calling parms:
//
// return parms:
//
// NOTES:
//
//
////////////////////////////////////////////////////////////////////////
int CSport::sport_flush()
{
	int status = tcdrain(sport_fd);
	if(status != 0)
	{
		perror("sport_flush");
	}
	return(status);
}

////////////////////////////////////////////////////////////////////////
//
// name:
//
// purpose:		clear any pending input from serial port
//
// calling parms:
//
// return parms:
//
// NOTES:
//
//
////////////////////////////////////////////////////////////////////////
int CSport::sport_clear()
{
	int status = tcflush(sport_fd,TCIOFLUSH);
	if(status != 0)
	{
		perror("sport_clear");
	}
	return(status);
}

////////////////////////////////////////////////////////////////////////
//
// name:
//
// purpose:			block read on 'fd' for up to 'timeout' secs
//
// calling parms:	int fd		- fd to be read
//					int timeout	- seconds to wait
//
// return parms:	status of opn
//					-1		=> select failed
//		 			0		=> no data available
//					else	=> call OK && data available
//
//
// NOTES:
//
////////////////////////////////////////////////////////////////////////
int CSport::sport_wait(int fd, float timeout)
{
	int		retval;
	fd_set 	rfds;
	struct 	timeval 	tv;

	// setup for select(3) call
	FD_ZERO(&rfds);					// clear fd_set
	FD_SET(fd, &rfds);				// enable desired fd

	if(timeout >= 1)
	{
		tv.tv_sec  = (int)timeout;		// seconds to wait
		tv.tv_usec = 0;				// usec (unused)
	}
	else
	{
		tv.tv_sec = 0;
		tv.tv_usec = (int) (timeout * 1000000);
	}

	retval = select(fd+1, &rfds, NULL, NULL, &tv);

	switch(retval)
	{
	case -1:				// error
		GRIPE("SELECT FAILED");
		perror("select failed");
		// fall through (on purpose)

	case  0:				// no data available
		GRIPE("SELECT: NO DATA AVAILABLE");
		break;

	default:	// select succeeded && data avail => good to go
		break;
	}
	return(retval);
}

/*
  ;;; Local Variables: ***
  ;;; mode:c++ ***
  ;;; c-basic-offset:4 ***
  ;;; c-indentation-style:bsd ***
  ;;; comment-column:40 ***
  ;;; comment-start: "// "  ***
  ;;; comment-end:"" ***
  ;;; abbrev-mode:nil ***
  ;;; tab-width:4 ***
  ;;; time-stamp-active:t ***
  ;;; End: ***
*/

