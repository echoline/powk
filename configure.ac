dnl Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org> (GPL v2)

AC_INIT(pork, 0.99.8.1, ryan@numb.org)
AM_INIT_AUTOMAKE(pork, 0.99.8.1)

AC_DEFINE(PACKAGE_EXTRAVERSION, "\"I'm an idiot\" edition", [extraversion])
AC_DEFINE(PACKAGE_WEBSITE, ["http://dev.ojnk.net"], [website])
AC_DEFINE(PACKAGE_AUTHOR, ["Ryan McCabe"], [author])

AM_CONFIG_HEADER(config.h)

AC_PROG_CC
AC_PROG_CPP
AC_PROG_RANLIB
AC_PROG_INSTALL
AC_PATH_PROG(sedpath, sed)

AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_C_CONST
AC_C_INLINE
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_STRUCT_TM
AC_CANONICAL_HOST

DEBUG_CFLAGS=""
DEBUG_LIBS=""

enableval=""
AC_ARG_ENABLE(debug,
[  --enable-debug		  Enable debugging])
if test "$enableval" = "yes"; then
	AC_DEFINE(ENABLE_DEBUGGING, 1, [Define to enable debugging])
	DEBUG_CFLAGS="$DEBUG_CFLAGS -O0 -g3"
fi

enableval=""
AC_ARG_ENABLE(warn,
[  --enable-warn		  Enable a bunch of compiler warnings])
if test "$enableval" = "yes"; then
	DEBUG_CFLAGS="$DEBUG_CFLAGS -W -Wall -Wno-unused-parameter -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations"
fi

withval=""
irc="yes"
AC_ARG_WITH(irc,
[  --with-irc  		  Include IRC support])
if test "$withval" = "no"; then
	irc="no"
else
	AC_DEFINE(IRC_SUPPORT, 1, [Define to enable IRC support])
fi

withval=""
AC_ARG_WITH(efence,
[  --with-efence=PREFIX	Use electric fence])
if test "$withval" = "yes"; then
	have_efence=""
	AC_CHECK_LIB(efence, malloc, have_efence="yes")
	if test "$have_efence" = "yes"; then
		DEBUG_LIBS="-lefence"
	fi
fi

AC_ARG_WITH(ncurses-libs,
[  --with-ncurses-libs=DIR         Specify location of the ncurses libraries],[
	LDFLAGS="$LDFLAGS -L$withval"
])  
    
AC_ARG_WITH(ncurses-includes,
[  --with-ncurses-includes=DIR     Specify location of the ncurses header files],[
	CPPFLAGS="$CPPFLAGS -I$withval"
])  

withval=""
AC_ARG_WITH(dmalloc,
[  --with-dmalloc=PREFIX   Use the dmalloc library])
if test "$withval" = "yes"; then
	have_dmalloc=""
	AC_CHECK_LIB(dmalloc, malloc, have_dmalloc="yes")
	if test "$have_dmalloc" = "yes"; then
		DEBUG_CFLAGS="$DEBUG_CFLAGS -DDEBUG_DMALLOC -DDMALLOC_FUNC_CHECK"
		DEBUG_LIBS="-ldmalloc"
	fi
fi

if test "$have_efence" = "yes" -a "$have_dmalloc" = "yes"; then
	AC_MSG_ERROR([
*** You've requested support for both dmalloc and electric fence, however
*** only one may be linked in at one time.
])
	exit -1
fi

AC_ARG_ENABLE(perl,
[  --disable-perl		  Disable use of perl scripting],
		perl=$enableval, perl=yes)

AC_SUBST(DEBUG_CFLAGS)
AC_SUBST(DEBUG_LIBS)

AC_CHECK_HEADERS(fcntl.h sys/time.h unistd.h ncurses.h termios.h sys/termios.h)

have_ncurses="no"
AC_CHECK_LIB(ncurses, initscr, have_ncurses="yes")
if test "$have_ncurses" = "no"; then
	echo "ncurses is required to build this program."
	echo "install it and try again."
	echo "try passing configure --with-ncurses-libs=<path to ncurses libraries> --with-ncurses-includes=<path to ncurses includes> if you believe ncurses is installed."
	exit -1
fi

AC_CHECK_LIB(ncurses, resizeterm, AC_DEFINE(HAVE_RESIZETERM, 1, [Define to 1 if you have the 'resizeterm' function in -lncurses.]))

AC_CHECK_TYPE(u_int32_t, uint32_t)
if test "$ac_cv_type_u_int32_t" = "no"; then
	AC_CHECK_TYPE(uint32_t, unsigned int)
fi

AC_CHECK_TYPE(u_int16_t, uint16_t)
if test "$ac_cv_type_u_int16_t" = "no"; then
	AC_CHECK_TYPE(uint16_t, unsigned short)
fi

AC_CHECK_TYPE(u_int8_t, uint8_t)
if test "$ac_cv_type_u_int8_t" = "no"; then
	AC_CHECK_TYPE(uint8_t, unsigned char)
fi

AC_MSG_CHECKING(for AF_INET6)
AC_TRY_COMPILE([#include <unistd.h>
				#include <sys/types.h>
				#include <sys/socket.h>
				#include <netdb.h>
				#include <netinet/in_systm.h>
				#include <netinet/in.h>],
					[AF_INET6+1;],
			[AC_MSG_RESULT(yes)],
			[AC_DEFINE(AF_INET6, 10, [Define if AF_INET6 is missing])
			AC_MSG_RESULT(no)])

AC_MSG_CHECKING(for socklen_t)
AC_TRY_COMPILE([#include <unistd.h>
				#include <sys/types.h>
				#include <sys/socket.h>],
					[sizeof(socklen_t);],
			[AC_MSG_RESULT(yes)],
			[AC_DEFINE(socklen_t, int, [Define if socklen_t is missing])
			AC_MSG_RESULT(no)])

AC_MSG_CHECKING(for in_addr_t)
AC_TRY_COMPILE([#include <unistd.h>
				#include <sys/types.h>
				#include <sys/socket.h>
				#include <netdb.h>
				#include <netinet/in_systm.h>
				#include <netinet/in.h>],
					[sizeof(in_addr_t);],
			[AC_MSG_RESULT(yes)],
			[AC_DEFINE(in_addr_t, unsigned int, [Define if in_addr_t is missing])
			AC_MSG_RESULT(no)])

AC_MSG_CHECKING(for in_port_t)
AC_TRY_COMPILE([#include <unistd.h>
				#include <sys/types.h>
				#include <sys/socket.h>
				#include <netdb.h>
				#include <netinet/in_systm.h>
				#include <netinet/in.h>],
					[sizeof(in_port_t);],
			[AC_MSG_RESULT(yes)],
			[AC_DEFINE(in_port_t, unsigned short, [Define if in_port_t is missing])
			AC_MSG_RESULT(no)])

AC_CHECK_SIZEOF(void *)
AC_DEFINE_UNQUOTED(SIZEOF_VOID_P, $ac_cv_sizeof_void_p, [Size of the 'void *' type])

AC_MSG_CHECKING(for __attribute__)
AC_TRY_COMPILE([#include <stdlib.h>],
			[static void foo(void) __attribute__ ((unused));
			static void foo(void) { exit(1); }],
			[AC_MSG_RESULT(yes)],
			[AC_DEFINE(HAVE___ATTRIBUTE__, 1, [Define if your compiler has __attribute__])
			AC_MSG_RESULT(no)])

AC_TYPE_SIGNAL

PERL_CFLAGS=""
PERL_LDFLAGS=""

dnl
dnl This (Perl test) was taken from gaim.
dnl
if test "$perl" = "yes"; then
	AC_PATH_PROG(perlpath, perl)
	AC_MSG_CHECKING(for Perl compile flags)
	PERL_CFLAGS=`$perlpath -MExtUtils::Embed -e ccopts 2>/dev/null`

	if test "_$PERL_CFLAGS" = _ ; then
		AC_MSG_RESULT([not found, building without Perl.])
		perl=no
	else
		PERL_LIBS=`$perlpath -MExtUtils::Embed -e ldopts 2>/dev/null |$sedpath 's/-lgdbm //'`
		PERL_LIBS=`echo $PERL_LIBS |$sedpath 's/-ldb //'`
		PERL_LIBS=`echo $PERL_LIBS |$sedpath 's/-lndbm //'`

		if test "$system" = "Linux"; then
			PERL_LIBS=`echo $PERL_LIBS |$sedpath 's/-lnsl //'`
			PERL_LIBS=`echo $PERL_LIBS |$sedpath 's/-lposix //'`
		fi

		PERL_LIBS=`echo $PERL_LIBS |$sedpath 's/-lc //'`

		AC_MSG_RESULT(ok)

		TEMPCPP="$CPPFLAGS"
		TEMPLDF="$LDFLAGS"
		CPPFLAGS="$CPPFLAGS $PERL_CFLAGS"
		LDFLAGS="$LDFLAGS $PERL_LIBS"

		AC_CHECK_FUNCS(Perl_eval_pv)
		CPPFLAGS="$TEMPCPP"
		LDFLAGS="$TEMPLDF"

		AC_MSG_CHECKING(for old perl)
		PERL_OLD=`$perlpath -e 'if($]<5.006){printf"yes\n";}else{printf"no\n";}'`
		if test "$PERL_OLD" = "yes"; then
			AC_DEFINE(OLD_PERL, 1, [Define if Perl's version is < 5.006])
			AC_MSG_RESULT(yes)
		else
			AC_MSG_RESULT(no)
		fi
	fi
fi

AC_CHECK_FUNCS(snprintf vsnprintf getopt_long strsep vasprintf)
AC_CHECK_FUNCS(gettimeofday select strdup strstr strtol uname)
AC_CHECK_FUNCS(memcmp strftime vprintf gethostbyname)

AC_SEARCH_LIBS(socket, socket, , [AC_CHECK_LIB(socket, socket, LIBS="$LIBS -lsocket -lnsl", , -lsocket)])

AC_CHECK_FUNC(getaddrinfo, AC_DEFINE(HAVE_GETADDRINFO, 1, [Define to 1 if you have the 'getaddrinfo' function.]),
	AC_CHECK_LIB(socket, getaddrinfo, AC_DEFINE(HAVE_GETADDRINFO, 1, [Define to 1 if you have the 'getaddrinfo' function.])))

AC_CHECK_FUNC(freeaddrinfo, AC_DEFINE(HAVE_FREEADDRINFO, 1, [Define to 1 if you have the 'freeaddrinfo' function.]),
	AC_CHECK_LIB(socket, freeaddrinfo), AC_DEFINE(HAVE_FREEADDRINFO, 1, [Define to 1 if you have the 'freeaddrinfo' function.]))

AC_CHECK_FUNC(gai_strerror, AC_DEFINE(HAVE_GAI_STRERROR, 1, [Define to 1 if you have the 'gai_strerror' function.]),
	AC_CHECK_LIB(socket, gai_strerror, AC_DEFINE(HAVE_GAI_STRERROR, 1, [Define to 1 if you have the 'gai_strerror' function.])))

AC_CHECK_FUNC(getnameinfo, AC_DEFINE(HAVE_GETNAMEINFO, 1, [Define to 1 if you have the 'getnameinfo' function.]),
	AC_CHECK_LIB(socket, gai_strerror, AC_DEFINE(HAVE_GETNAMEINFO, 1, [Define to 1 if you have the 'getnameinfo' function.])))

AC_CHECK_FUNC(inet_ntop, AC_DEFINE(HAVE_INET_NTOP, 1, [Define to 1 if you have the 'inet_ntop' function.]),
	AC_CHECK_LIB(nsl, inet_ntop, AC_DEFINE(HAVE_INET_NTOP, 1, [Define to 1 if you have the 'inet_ntop' function.])))

AC_MSG_CHECKING(for struct sockaddr_storage)
AC_TRY_COMPILE(	[	#include <sys/types.h>
					#include <sys/socket.h>],
				[ struct sockaddr_storage ss; ],
				[ have_sockaddr_storage=yes ],
				[ have_sockaddr_storage=no ])
AC_MSG_RESULT($have_sockaddr_storage)

if test "$have_sockaddr_storage" = "yes"; then
	AC_DEFINE(HAVE_STRUCT_SOCKADDR_STORAGE, 1, [Define to 1 if you have struct sockaddr_storage])

	AC_MSG_CHECKING(for ss_family member in struct sockaddr_storage)
	AC_TRY_COMPILE(	[	#include <sys/types.h>
						#include <sys/socket.h>],
					[ size_t test =
						sizeof(((struct sockaddr_storage *)0)->ss_family);],
					[ have_ss_family=yes ],
					[ have_ss_family=no ])
	AC_MSG_RESULT($have_ss_family)

	if test "$have_ss_family" = "no"; then
		AC_MSG_CHECKING(for __ss_family member in struct sockaddr_storage)
		AC_TRY_COMPILE(	[	#include <sys/types.h>
							#include <sys/socket.h>],
						[ size_t test =
							sizeof(((struct sockaddr_storage *)0)->__ss_family);],
						[ have_ss_family=yes ],
						[ have_ss_family=no ])
		AC_MSG_RESULT($have_ss_family)

		if test "$have_ss_family" = "yes"; then
			AC_DEFINE(HAVE___SS_FAMILY, 1, [Define if the sockaddr storage struct has the ss_family member])
		fi
	fi
fi

AC_MSG_CHECKING(for struct sockaddr_in6)
AC_TRY_COMPILE(	[	#include <sys/types.h>
					#include <netinet/in.h>],
				[ struct sockaddr_in6 s; ],
				[ have_sockaddr_in6=yes ],
				[ have_sockaddr_in6=no ])
AC_MSG_RESULT($have_sockaddr_in6)

if test "$have_sockaddr_in6" = "yes"; then
	AC_DEFINE(HAVE_STRUCT_SOCKADDR_IN6, 1, [Define to 1 if you have struct sockaddr_in6])
fi

AC_MSG_CHECKING(for struct in6_addr)
AC_TRY_COMPILE(	[	#include <sys/types.h>
					#include <netinet/in.h>],
				[ struct in6_addr s; ],
				[ have_in6_addr=yes ],
				[ have_in6_addr=no ])
AC_MSG_RESULT($have_in6_addr)

if test "$have_in6_addr" = "yes"; then
	AC_DEFINE(HAVE_STRUCT_IN6_ADDR, 1, [Define to 1 if you have struct in6_addr])
fi

AC_MSG_CHECKING(for struct addrinfo)
AC_TRY_COMPILE( [	#include <sys/types.h>
					#include <sys/socket.h>
					#include <netdb.h>],
				[ size_t test =
					sizeof(((struct addrinfo *)0)->ai_flags);],
				[ have_struct_addrinfo=yes ],
				[ have_struct_addrinfo=no ])
AC_MSG_RESULT($have_struct_addrinfo)

if test "$have_struct_addrinfo" = "yes"; then
	AC_DEFINE(HAVE_STRUCT_ADDRINFO, 1, [Define to 1 if you have struct addrinfo])
fi

AC_SUBST(LIBS)
AC_SUBST(PERL_CFLAGS)
AC_SUBST(CFLAGS)
AC_SUBST(LDFLAGS)
AC_SUBST(CPPFLAGS)
AC_SUBST(PERL_LIBS)

AM_CONDITIONAL(COMPILE_PERL, test "$perl" = "yes")
AM_CONDITIONAL(IRC_SUPPORT, test "$irc" = "yes")

AC_OUTPUT([
	Makefile
	src/Makefile
	src/missing/Makefile
	src/protocols/Makefile
	src/protocols/aim/Makefile
	src/protocols/irc/Makefile
	doc/Makefile
	doc/help/Makefile
	doc/help/blist/Makefile
	doc/help/buddy/Makefile
	doc/help/chat/Makefile
	doc/help/event/Makefile
	doc/help/file/Makefile
	doc/help/history/Makefile
	doc/help/input/Makefile
	doc/help/main/Makefile
	doc/help/perl/Makefile
	doc/help/scroll/Makefile
	doc/help/timer/Makefile
	doc/help/win/Makefile
	examples/Makefile
	examples/perl/Makefile])
