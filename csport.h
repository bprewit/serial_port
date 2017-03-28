// -*- C++ -*-
//////////////////////////////////////////////////////////////////////////
//
// $Id: csport.h 325 2014-02-17 00:20:55Z bprewit $
//
// purpose:
//
// NOTES:
//
// Copyright (c) TenStar Technologies 2006
////////////////////////////////////////////////////////////////////////
#ifndef SPORT_H
#define SPORT_H

// #include <string>
#include <iostream>

#include <unistd.h>
#include <termios.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <qstring.h>
#include <qstringlist.h>

using namespace std;

typedef unsigned long 	ULONG;
typedef unsigned int	UINT;

class CSport
{
public:
  typedef enum		 // debug
  {
    PARITY_INVAL,	 // 0
    PARITY_NONE, 	 // 1
    PARITY_ODD, 	 // 2
    PARITY_EVEN, 	 // 3
    PARITY_MARK, 	 // 4
    PARITY_SPACE, 	 // 5
    PARITY_IGNORE	 // 6
  } PARITY_T;

  typedef enum
  {
    SPORT_EOK 		=  0,
    SPORT_EOTHER 	= -1,
    SPORT_EBADNAME 	= -2,
    SPORT_ENODEV 	= -3,
    SPORT_ENOLOCK 	= -4
  } SPORT_ERR;

  CSport();
  ~CSport();

  // port open routines
  int		sport_open(QString);
  int		sport_open(const char *, ULONG, UINT, PARITY_T, UINT, bool = true);

  // port close
  int		sport_close();

  // baudrate
  int		set_baudrate(ULONG);
  long		get_baudrate();

  // data bits
  int		set_databits(UINT);
  int		get_databits();

  // stop bits
  int		set_stopbits(UINT);
  int		get_stopbits();

  // parity bit
  int		set_parity(PARITY_T);
  int		set_parity(char);
  PARITY_T	get_parity();

  // flush output data
  int		sport_flush();

  // discard pending input data
  int		sport_clear();

  // wait for data, w/ timeout
  int		sport_wait(int, float);

  // utilities
  PARITY_T	ctop(char);
  char		ptoc(PARITY_T);

  int		errno();

  // data
  string	sport_name;
  int		sport_fd;
  FILE		*sport_file;

protected:

private:
  int		sport_errno;

  // port parameters
  ULONG		baudrate;
  UINT		databits;
  PARITY_T	parityb;
  UINT		stopbits;

  // private data structs
  struct termios old_ios;
  struct termios new_ios;

  // simple utilities
  ULONG 	btoul(ULONG);
  ULONG 	ultob(ULONG);
};


#endif

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
