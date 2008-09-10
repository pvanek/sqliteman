/*
This library will provide the ipv4 ISINNET, IP2INT, INT2IP, NET2INT, NET2LENGTH, NETMASK2LENGTH functions in
SQL queries.

The functions was coded by Alexey Pechnikov (pechnikov@mobigroup.ru) and tested on linux only. Tests are writed and performed by Alexander Romanov (romanov@mobigroup.ru).
The code is public domain. Author use these functions for store ip addresses as integers and networks as intervals of integers and search as

	select * from table_addr
	where IP2INT('172.16.1.193') between ip_from and ip_to;

For example, 
	ip_from = ('172.16.1.193/255.255.255.0')
	ip_to = ('172.16.1.193/255.255.255.0') + NET2LENGTH('172.16.1.193/255.255.255.0')
or
	ip_to = ('172.16.1.193/24') + NET2LENGTH('172.16.1.193/24')
or 
	ip_to = ('172.16.1.193/24') + NETMASK2LENGTH('24');


The description of IP2INT function:

	IP2INT( ip )

	IP2INT returns NULL if there is any kind of error, mainly :
		- strings are not valid IPV4 addresses or
		- number of bits is not a number or is out of range
	IP2INT returns integer number of IPV4 address otherwise

	SELECT IP2INT('192.168.1.1');
	        ==>3232235777
	SELECT IP2INT('255.255.255.255');
	        ==>4294967295
	SELECT IP2INT('0.0.0.0');
	        ==>0


The description of INT2IP function:

	INT2IP( int_number )
	integer number may be a string ('3232235777' for
	example) or a number (3232235777 for example).
	Number 3232235777 is an integer number of the
	IPV4 address 192.168.1.1

	IP2INT returns NULL if int_number is not an integer number.
	IP2INT returns IPV4 address otherwise.
	
	SELECT INT2IP(3232235777);
	SELECT INT2IP('3232235777');
	        ==>192.168.1.1
	SELECT INT2IP(4294967295);
	SELECT INT2IP('4294967295');
	        ==>255.255.255.255
	SELECT INT2IP(0);
	SELECT INT2IP('0');
	        ==>0.0.0.0	
	        

The description of NET2INT function:

	NET2INT( network, mask ) or
	NET2INT( network/mask )
	mask may be specified the CIDR way as a number of bits,
	or as a standard 4 bytes notation.
	if CIDR notation is used, mask may be a string ('24' for
	example) or a number (24 for example).

	NET2INT returns NULL if there is any kind of error, mainly :
		- strings are not valid standard 4 bytes notation or
		- number of bits is not a number or is out of range
	NET2INT returns integer number of mask otherwise.

	SELECT NET2INT('192.168.1.1/255.255.255.0');
	SELECT NET2INT('192.168.1.1/24');
	SELECT NET2INT('192.168.1.1','255.255.255.0');
	SELECT NET2INT('192.168.1.1','24');
	SELECT NET2INT('192.168.1.1',24);
	        ==>3232235776
	SELECT NET2INT('192.168.1.1/255.255.255.255');
	SELECT NET2INT('192.168.1.1/32');
	SELECT NET2INT('192.168.1.1','255.255.255.255');
	SELECT NET2INT('192.168.1.1','32');
	SELECT NET2INT('192.168.1.1',32);
	        ==>3232235777
	SELECT NET2INT('192.168.1.1/255.255.128.0');
	SELECT NET2INT('192.168.1.1/17');
	SELECT NET2INT('192.168.1.1','255.255.128.0');
	SELECT NET2INT('192.168.1.1','17');
	SELECT NET2INT('192.168.1.1',17);
	        ==>3232235520


The description of NET2LENGTH function:

	NET2LENGTH( network, mask ) or
	NET2LENGTH( network/mask )
	mask may be specified the CIDR way as a number of bits,
	or as a standard 4 bytes notation.

	NET2LENGTH returns NULL if there is any kind of error, mainly :
		- strings are not valid standard 4 bytes notation or
		- number of bits is not a number or is out of range
	NET2LENGTH returns integer number of mask length otherwise.

	SELECT NET2LENGTH('192.168.1.1','255.255.255.0');
	SELECT NET2LENGTH('192.168.1.1,'24');
	SELECT NET2LENGTH('192.168.1.1,24);
	SELECT NET2LENGTH('192.168.1.1/255.255.255.0');
	SELECT NET2LENGTH('192.168.1.1/24');
	        ==>256
	SELECT NET2LENGTH('192.168.1.1','255.255.255.255');
	SELECT NET2LENGTH('192.168.1.1,'32');
	SELECT NET2LENGTH('192.168.1.1,32);
	SELECT NET2LENGTH('192.168.1.1/255.255.255.255');
	SELECT NET2LENGTH('192.168.1.1/32');
	        ==>1
	SELECT NET2LENGTH('192.168.1.1','255.255.128.0');
	SELECT NET2LENGTH('192.168.1.1,'17');
	SELECT NET2LENGTH('192.168.1.1,17);
	SELECT NET2LENGTH('192.168.1.1/255.255.128.0');
	SELECT NET2LENGTH('192.168.1.1/17');
	        ==>32768


The description of NETMASK2LENGTH function:

	NETMASK2LENGTH( mask )
	mask should be specified the CIDR way as a number of bits,
	in CIDR notation mask may be a string ('24' for
	example) or a number (24 for example). In CIDR notation
	mask should be in range from 8 to 32.

	NET2LENGTH returns integer number of mask length.
	
	SELECT NETMASK2LENGTH('24');
	SELECT NETMASK2LENGTH(24);
	        ==>256
	SELECT NETMASK2LENGTH('32');
	SELECT NETMASK2LENGTH(32);
	        ==>1
	SELECT NETMASK2LENGTH('17');
	SELECT NETMASK2LENGTH(17);
	        ==>32768


The ISINNET function reimplemented by Alexey Pechnikov (pechnikov@mobigroup.ru). Tests is saved as original author provide it. Thanks for idea! The code is public domain.


	ISINNET( ip, network, mask )
	mask may be specified the CIDR way as a number of bits,
	or as a standard 4 bytes notation.
	if CIDR notation is used, mask may be a string ('13' for
	example) or a number (13 for example)

	ISINNET returns NULL if there is any kind of error, mainly :
		- strings are not valid IPV4 addresses or
		- number of bits is not a number or is out of range
	ISINNET returns 1 if (ip & mask) = (net & mask)
	ISINNET returns 0 otherwise

	SELECT ISINNET( '172.16.1.193', '172.16.1.0', 24 );
	SELECT ISINNET( '172.16.1.193', '172.16.1.0/24' );
	        ==> 1
	SELECT ISINNET( '172.16.1.193', '172.16.1.0', 25 );
	SELECT ISINNET( '172.16.1.193', '172.16.1.0/25' );
	        ==> 0
	SELECT ISINNET( '172.16.1.193', '172.16.1.0', '255.255.255.0' );
	SELECT ISINNET( '172.16.1.193', '172.16.1.0/255.255.255.0' );
	        ==> 1
	SELECT ISINNET( '172.16.1.193', '172.16.1.0', '255.255.255.128' );
	SELECT ISINNET( '172.16.1.193', '172.16.1.0/255.255.255.128' );
	        ==> 0

	CREATE TABLE ip_add (
		ip	varchar( 16 )
	);
	INSERT INTO ip_add VALUES('172.16.1.40');
	INSERT INTO ip_add VALUES('172.16.1.93');
	INSERT INTO ip_add VALUES('172.16.1.204');
	INSERT INTO ip_add VALUES('172.16.4.203');
	INSERT INTO ip_add VALUES('172.16.4.205');
	INSERT INTO ip_add VALUES('172.16.4.69');
	INSERT INTO ip_add VALUES('10.0.1.204');
	INSERT INTO ip_add VALUES('10.0.1.16');
	INSERT INTO ip_add VALUES('10.1.0.16');
	INSERT INTO ip_add VALUES('192.168.1.5');
	INSERT INTO ip_add VALUES('192.168.1.7');
	INSERT INTO ip_add VALUES('192.168.1.19');

	SELECT ip FROM ip_add WHERE ISINNET( ip, '172.16.1.0', 16 );
	SELECT ip FROM ip_add WHERE ISINNET( ip, '172.16.1.0/16' );
	172.16.1.40
	172.16.1.93
	172.16.1.204
	172.16.4.203
	172.16.4.205
	172.16.4.69

	SELECT ip FROM ip_add WHERE ISINNET( ip, '172.16.1.0', 24 );
	SELECT ip FROM ip_add WHERE ISINNET( ip, '172.16.1.0/24' );
	172.16.1.40
	172.16.1.93
	172.16.1.204

	SELECT * FROM ip_add WHERE NOT ISINNET( ip, '128.0.0.0', 1 );
	SELECT * FROM ip_add WHERE NOT ISINNET( ip, '128.0.0.0/1' );
	10.0.1.204
	10.0.1.16
	10.1.0.16

	DELETE FROM ip_add WHERE NOT ISINNET( ip, '128.0.0.0', 1 );
	DELETE FROM ip_add WHERE NOT ISINNET( ip, '128.0.0.0/1' );

	SELECT * FROM ip_add;
	172.16.1.40
	172.16.1.93
	172.16.1.204
	172.16.4.203
	172.16.4.205
	172.16.4.69
	192.168.1.5
	192.168.1.7
	192.168.1.19


programm template was taken from
	http://sqlite.org/contrib/
	http://sqlite.org/contrib/download/extension-functions.c?get=25


Mer 23 jul 2008 16:24:01 CEST
Schplurtz le deboulonne.


Instructions (mostly from extension-functions.c):
1) Compile with
   Linux:
     gcc -fPIC -lm -shared ipv4-ext.c -o libsqliteipv4.so
   Mac OS X:
     gcc -fno-common -dynamiclib ipv4-ext.c -o libsqliteipv4.dylib
   (You may need to add flags
    -I /opt/local/include/
    if your sqlite3 is installed from Mac ports, or
    -I /sw/include/
    if installed with Fink.)
            Please, note that sqlite3 from macport 1.6.0 is not compiled with
            --enable-load-extension. So you cannot try this extension from
            within the sqlite3 shell.
            The same applies to leopard's /usr/bin/sqlite3
2) In your application, call sqlite3_enable_load_extension(db,1) to
   allow loading external libraries.  Then load the library libsqliteipv4
   using sqlite3_load_extension; the third argument should be 0.
   See http://www.sqlite.org/cvstrac/wiki?p=LoadableExtensions.
3) Use, for example:
   SELECT ISINNET( '10.0.0.1', '10.0.0.0', 8 );

Note: Loading extensions is by default prohibited as a
security measure; see "Security Considerations" in
http://www.sqlite.org/cvstrac/wiki?p=LoadableExtensions.
If the sqlite3 program and library are built this
way, you cannot use these functions from the program, you
must write your own program using the sqlite3 API, and call
sqlite3_enable_load_extension as described above.

If the program is built so that loading extensions is permitted,
the following will work:
sqlite> SELECT load_extension('./libsqliteipv4.so');
sqlite> select isinnet( '123.234.210.109', '123.123.23.18', '255.248.0.0' );
0

Alterations:
The instructions are for Linux or Mac OS X; users of other OSes may
need to modify this procedure.   If you do not
wish to make a loadable module, #define SQLITE_ENABLE_INET

Liam Healy (with little modifications by Schplurtz le deboulonne)

*/

#if !defined(SQLITE_CORE) || defined(SQLITE_ENABLE_INET)

#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <arpa/inet.h>

#include <assert.h>

#ifndef SQLITE_CORE
  #include "sqlite3ext.h"
  SQLITE_EXTENSION_INIT1
#else
  #include "sqlite3.h"
#endif

/*
 * The isinnet() SQL function returns true if ip is in network/netmask.
 * isinnet( '172.16.1.23', '172.16.1.0', 18 )
 * isinnet( '172.16.1.23', '172.16.1.0', '18' )
 * isinnet( '172.16.1.23', '172.16.1.0', '255.255.192.0' )
 */
static void isinnet3Func(
	sqlite3_context *context,
	int argc,
	sqlite3_value **argv
) {
	u_int32_t ad, net, mask;

	int rval, maskLen;

	if( sqlite3_value_type(argv[0]) == SQLITE_NULL || sqlite3_value_type(argv[1]) == SQLITE_NULL || sqlite3_value_type(argv[2]) == SQLITE_NULL ){
		sqlite3_result_null(context);
		return;
	}

	if( (rval = inet_pton(AF_INET,(char*)sqlite3_value_text(argv[0]),&ad) < 1) ||
	    (rval = inet_pton(AF_INET,(char*)sqlite3_value_text(argv[1]),&net) < 1)
	) {
		sqlite3_result_null(context);
		return;
	}
	ad = htonl(ad);
	net = htonl(net);

	maskLen =strlen((char*)sqlite3_value_text(argv[2]));
	/* put mask in hex form */
	if (maskLen < 3) {
		mask = atoi((char*)sqlite3_value_text(argv[2]));
		mask = ~ ( (((u_int32_t)1) << (32 - mask)) -1 );
	} else {
		/* mask is in dotted form */
		if( (rval = inet_pton(AF_INET,(char*)sqlite3_value_text(argv[2]),&mask) < 1) ) {
			sqlite3_result_null(context);
			return;
		}
		mask = htonl(mask);
	}
	sqlite3_result_int( context, ((ad & mask) == (net & mask )) );
}

/*
 * The isinnet() SQL function returns true if ip is in network/netmask.
 * isinnet( '172.16.1.23', '172.16.1.0/18' )
 * isinnet( '172.16.1.23', '172.16.1.0/255.255.192.0' )
 */
static void isinnet2Func(
	sqlite3_context *context,
	int argc,
	sqlite3_value **argv
) {
	u_int32_t ad, net, mask;

	int rval, maskLen;
	char *slashPos, *stringMask, *stringIP;

	if( sqlite3_value_type(argv[0]) == SQLITE_NULL || sqlite3_value_type(argv[1]) == SQLITE_NULL ){
		sqlite3_result_null(context);
		return;
	}
	if( (rval = inet_pton(AF_INET,(char*)sqlite3_value_text(argv[0]),&ad) < 1) ) {
		sqlite3_result_null(context);
		return;
	}
	ad = htonl(ad);

	/*  split the ip address and mask */
	slashPos = strchr((char*)sqlite3_value_text(argv[1]), (int) '/');
	if (slashPos == NULL) {
		/*  straight ip address without mask */
		mask = (u_int32_t)1;
	} else {
		/* ipaddress has the mask, handle the mask and seperate out the  */
		/*  ip address */
		stringMask = slashPos +1;
		maskLen =strlen(stringMask);
		/* put mask in hex form */
		if (maskLen < 3) {
			mask = atoi(stringMask);
			mask = ~ ( (((u_int32_t)1) << (32 - mask)) -1 );
		} else {
			/* mask is in dotted form */
			if ((rval = inet_pton(AF_INET,stringMask,&mask)) < 1 ) {
				sqlite3_result_null(context);
				return;
			}
			mask = htonl(mask);
		}
		int ipLen = (uintptr_t)slashPos  - (uintptr_t)(char*)sqlite3_value_text(argv[1]);
		/* divide the string into ip and mask portion */
		stringIP = sqlite3_malloc( ipLen +1 );
		strncpy( stringIP, (char*)sqlite3_value_text(argv[1]), ipLen );
		stringIP[ipLen] = '\0';
	}

	if ( (rval = inet_pton(AF_INET,(char*)stringIP,&net)) < 1) {
		sqlite3_result_null(context);
		return;
	};
	net = htonl(net);

	sqlite3_result_int( context, ((ad & mask) == (net & mask)) );
	sqlite3_free(stringIP);
}

static void ip2intFunc(
	sqlite3_context *context,
	int argc,
	sqlite3_value **argv
) {
	u_int32_t ad;
	int rval;

	if( sqlite3_value_type(argv[0]) == SQLITE_NULL ){
		sqlite3_result_null(context);
	} else {
  		if( (rval = inet_pton(AF_INET,(char*)sqlite3_value_text(argv[0]),&ad) < 1) ) {
			sqlite3_result_null(context);
			return;
		}
		ad = htonl(ad);
		sqlite3_result_int64( context, ad );
	}

}

static void int2ipFunc(
	sqlite3_context *context,
	int argc,
	sqlite3_value **argv
) {
	u_int32_t ip;
	unsigned char ad[32];

	if( sqlite3_value_type(argv[0]) == SQLITE_NULL ){
		sqlite3_result_null(context);
	} else {
		ip = sqlite3_value_int64(argv[0]);
		ip = ntohl(ip);
  		if( inet_ntop(AF_INET, &ip, ad, 32) == NULL ) {
			sqlite3_result_null(context);
			return;
		}
		sqlite3_result_text( context, (char*)ad, -1, SQLITE_TRANSIENT);
	}
}

static void net2int1Func(
	sqlite3_context *context,
	int argc,
	sqlite3_value **argv
) {
	u_int32_t net, mask;

	int rval, maskLen;
	char *slashPos, *stringMask, *stringIP;

	if( sqlite3_value_type(argv[0]) == SQLITE_NULL ){
		sqlite3_result_null(context);
		return;
	}

	/*  split the ip address and mask */
	slashPos = strchr((char*)sqlite3_value_text(argv[0]), (int) '/');
	if (slashPos == NULL) {
		/*  straight ip address without mask */
		mask = (u_int32_t)1;
	} else {
		/* ipaddress has the mask, handle the mask and seperate out the  */
		/*  ip address */
		stringMask = slashPos +1;
		maskLen =strlen(stringMask);
		/* put mask in hex form */
		if (maskLen < 3) {
			mask = atoi(stringMask);
			mask = ~ ( (((u_int32_t)1) << (32 - mask)) -1 );
		} else {
			/* mask is in dotted form */
			if ((rval = inet_pton(AF_INET,stringMask,&mask)) < 1 ) {
				sqlite3_result_null(context);
				return;
			}
			mask = htonl(mask);
		}
		int ipLen = (uintptr_t)slashPos  - (uintptr_t)(char*)sqlite3_value_text(argv[0]);
		/* divide the string into ip and mask portion */
		stringIP = sqlite3_malloc( ipLen +1 );
		strncpy( stringIP, (char*)sqlite3_value_text(argv[0]), ipLen );
		stringIP[ipLen] = '\0';
	}

	if ( (rval = inet_pton(AF_INET,(char*)stringIP,&net)) < 1) {
		sqlite3_result_null(context);
		return;
	};
	net = htonl(net);

	sqlite3_result_int64( context, ((net & mask)) );
	sqlite3_free(stringIP);


}

static void net2int2Func(
	sqlite3_context *context,
	int argc,
	sqlite3_value **argv
) {
	u_int32_t net, mask;

	int rval, maskLen;

	if( sqlite3_value_type(argv[0]) == SQLITE_NULL || sqlite3_value_type(argv[1]) == SQLITE_NULL ){
		sqlite3_result_null(context);
		return;
	}

	if( (rval = inet_pton(AF_INET,(char*)sqlite3_value_text(argv[0]),&net) < 1) ) {
		sqlite3_result_null(context);
		return;
	}
	net = htonl(net);

	maskLen =strlen((char*)sqlite3_value_text(argv[1]));
	/* put mask in hex form */
	if (maskLen < 3) {
		mask = atoi((char*)sqlite3_value_text(argv[1]));
		mask = ~ ( (((u_int32_t)1) << (32 - mask)) -1 );
	} else {
		/* mask is in dotted form */
		if( (rval = inet_pton(AF_INET,(char*)sqlite3_value_text(argv[1]),&mask) < 1) ) {
			sqlite3_result_null(context);
			return;
		}
		mask = htonl(mask);
	}
	sqlite3_result_int64( context, ((net & mask )) );
}


static void net2length1Func(
	sqlite3_context *context,
	int argc,
	sqlite3_value **argv
) {
	u_int32_t mask;

	int rval, maskLen;
	char *slashPos, *stringMask;

	if( sqlite3_value_type(argv[0]) == SQLITE_NULL ){
		sqlite3_result_null(context);
		return;
	}

	/*  split the ip address and mask */
	slashPos = strchr((char*)sqlite3_value_text(argv[0]), (int) '/');
	if (slashPos == NULL) {
		/*  straight ip address without mask */
		mask = (u_int32_t)1;
	} else {
		/* ipaddress has the mask, handle the mask and seperate out the  */
		/*  ip address */
		stringMask = slashPos +1;
		maskLen =strlen(stringMask);
		/* put mask in hex form */
		if (maskLen < 3) {
			mask = atoi(stringMask);
			mask = ( (u_int32_t)1 << (32 - mask) );
		} else {
			/* mask is in dotted form */
			if ((rval = inet_pton(AF_INET,stringMask,&mask)) < 1 ) {
				sqlite3_result_null(context);
				return;
			}
			mask = htonl(mask);
			mask = (~(u_int32_t)mask) + 1;
		}

	}
	sqlite3_result_int64( context, mask );
}

static void net2length2Func(
	sqlite3_context *context,
	int argc,
	sqlite3_value **argv
) {
	u_int32_t net, mask;

	int rval, maskLen;

	if( sqlite3_value_type(argv[0]) == SQLITE_NULL || sqlite3_value_type(argv[1]) == SQLITE_NULL ){
		sqlite3_result_null(context);
		return;
	}

	if( (rval = inet_pton(AF_INET,(char*)sqlite3_value_text(argv[0]),&net) < 1) ) {
		sqlite3_result_null(context);
		return;
	}
	net = htonl(net);

	maskLen =strlen((char*)sqlite3_value_text(argv[1]));
	/* put mask in hex form */
	if (maskLen < 3) {
		mask = atoi((char*)sqlite3_value_text(argv[1]));
		mask = ( (u_int32_t)1 << (32 - mask) );
	} else {
		/* mask is in dotted form */
		if( (rval = inet_pton(AF_INET,(char*)sqlite3_value_text(argv[1]),&mask) < 1) ) {
			sqlite3_result_null(context);
			return;
		}
		mask = htonl(mask);
		mask = (~(u_int32_t)mask) + 1;
	}
	sqlite3_result_int64( context, mask );
}

static void netmask2lengthFunc(
	sqlite3_context *context,
	int argc,
	sqlite3_value **argv
) {
	u_int32_t mask;

	if( sqlite3_value_type(argv[0]) == SQLITE_NULL ){
		sqlite3_result_null(context);
		return;
	}

	mask = atoi((char*)sqlite3_value_text(argv[0]));
	mask = ( (u_int32_t)1 << (32 - mask) );

	sqlite3_result_int64( context, mask );
}

/* SQLite invokes this routine once when it loads the extension.
** Create new functions, collating sequences, and virtual table
** modules here.  This is usually the only exported symbol in
** the shared library.
*/

int sqlite3InetInit(sqlite3 *db){
  static const struct {
     char *zName;
     signed char nArg;
     int argType;           /* 1: 0, 2: 1, 3: 2,...  N:  N-1. */
     int eTextRep;          /* 1: UTF-16.  0: UTF-8 */
     void (*xFunc)(sqlite3_context*,int,sqlite3_value **);
  } aFuncs[] = {
	{ "ip2int",             1, 0, SQLITE_UTF8,    ip2intFunc },
	{ "int2ip",             1, 0, SQLITE_UTF8,    int2ipFunc },
	{ "net2int",            1, 0, SQLITE_UTF8,    net2int1Func },
	{ "net2int",            2, 0, SQLITE_UTF8,    net2int2Func },
	{ "net2length",         1, 0, SQLITE_UTF8,    net2length1Func },
	{ "net2length",         2, 0, SQLITE_UTF8,    net2length2Func },
	{ "netmask2length",     1, 0, SQLITE_UTF8,    netmask2lengthFunc },
	{ "isinnet",            3, 0, SQLITE_UTF8,    isinnet3Func },
	{ "isinnet",            2, 0, SQLITE_UTF8,    isinnet2Func },
  };

  int i;
  for(i=0; i<sizeof(aFuncs)/sizeof(aFuncs[0]); i++){
    void *pArg;
    int argType = aFuncs[i].argType;
    pArg = (void*)(int)argType;
    sqlite3_create_function(db, aFuncs[i].zName, aFuncs[i].nArg,
        aFuncs[i].eTextRep, pArg, aFuncs[i].xFunc, 0, 0);
  }

  return 0;
}

#if !SQLITE_CORE
int sqlite3_extension_init(
  sqlite3 *db, 
  char **pzErrMsg,
  const sqlite3_api_routines *pApi
){
  SQLITE_EXTENSION_INIT2(pApi)
  return sqlite3InetInit(db);
}
#endif

#endif
