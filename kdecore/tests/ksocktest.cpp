/* This file is part of the KDE libraries
    Copyright (c) 1999 Waldo Bastian <bastian@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License version 2 as published by the Free Software Foundation.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "kuniqueapp.h"
#include "kglobal.h"
#include "kdebug.h"
#include "ksock.h"
#include "ksockaddr.h"
#include "kextsock.h"

#include <kaboutdata.h>
#include <kcmdlineargs.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

bool check(QString txt, QString a, QString b)
{
  if (a.isEmpty())
     a = QString::null;
  if (b.isEmpty())
     b = QString::null;
  if (a == b) {
    kdDebug() << txt << " : checking '" << a << "' against expected value '" << b << "'... " << "ok" << endl;
  }
  else {
    kdDebug() << txt << " : checking '" << a << "' against expected value '" << b << "'... " << "KO !" << endl;
    exit(1);
  }
  return true;
}


int
main(int argc, char *argv[])
{
   KAboutData about("socktest", "SockTest", "version");
   KCmdLineArgs::init(argc, argv, &about);
   KApplication::addCmdLineOptions();

   KApplication app;

   QString host, port;

   KInetSocketAddress host_address("136.243.103.182", 80);

   check("KInetSocketAddress(\"136.243.103.182\", 80)", host_address.pretty(), "136.243.103.182 port 80");

   int result = KExtendedSocket::resolve(&host_address, host, port, NI_NAMEREQD);
   if (result < 0) {
       printf("Resolution failed: %s (%d)\n", strerror(-result), -result);
   }
   printf( "resolve result: %d\n", result );
   check("KExtendedSocket::resolve() host=", host, "nicoda.kde.org");
//   check("KExtendedSocket::resolve() port=", port, "http");
   QList<KAddressInfo> list;
   list = KExtendedSocket::lookup("nicoda.kde.org", "http", KExtendedSocket::inetSocket);

   for(KAddressInfo *info = list.first(); info; info = list.next())
   {
      qWarning("Lookup: %s %s %s", info->address()->pretty().latin1(),
		                   info->address()->isEqual(KInetSocketAddress("136.243.103.182", 80)) ?
				   "is equal to" : "is NOT equal to",
				   "136.243.103.182 port 80");
   }
   check("KExtendedSocket::lookup()", list.last()->address()->pretty(), "136.243.103.182 port 80");



   int err;

   QList<KAddressInfo> cns = KExtendedSocket::lookup("nicoda.kde.org", 0, KExtendedSocket::canonName, &err);
   for (KAddressInfo *x = cns.first(); x; x = cns.next()) {
        const char *canon = x->canonname();
        qWarning( "Lookup: %s", canon ? canon : "<Null>");
   }
   check("KExtendedSocket::lookup() canonical", cns.first()->canonname(), "nicoda.kde.org");

   KExtendedSocket * sock2 = new KExtendedSocket( "nicoda.kde.org", 80 );
   check( "KExtendedSocket ctor / connect", QString::number( sock2->connect() ), "0" );

   printf("FD %d\n", sock2->fd());

   KSocketAddress* addr = KExtendedSocket::peerAddress( sock2->fd() );
   const char *expectedAddr = addr->family() == AF_INET6 ?  "2a01:4f8:171:c9a::5" : "136.243.103.182";

   check( "peerAddress:", addr->nodeName().latin1(),  expectedAddr);

   check( "isEqual port 80:", addr->isEqual(KInetSocketAddress(expectedAddr, 80)) ? "TRUE" : "FALSE", "TRUE");
   check( "isEqual port 8080:", addr->isEqual(KInetSocketAddress(expectedAddr, 8080)) ? "TRUE" : "FALSE", "FALSE");
   check( "isEqual core equal port 8080:", addr->isCoreEqual(KInetSocketAddress(expectedAddr, 8080)) ? "TRUE" : "FALSE", "TRUE");

   delete addr;
   delete sock2;
   for(KAddressInfo *info = list.first(); info; info = list.next())
       delete info;
   for (KAddressInfo *x = cns.first(); x; x = cns.next())
       delete x;
}
