
#include "sockets.h"
#include "Tools/Exceptions.h"
#include "Tools/time-func.h"
#include "Processor/OnlineOptions.h"

#include <iostream>
#include <fcntl.h>
using namespace std;

void error(const char *str)
{
  int old_errno = errno;
  char err[1000];
  gethostname(err,1000);
  strcat(err," : ");
  strcat(err,str);
  throw runtime_error(string() + err + " : " + strerror(old_errno));
}

void set_up_client_socket(int& mysocket,const char* hostname,int Portnum)
{
   struct addrinfo hints, *ai=NULL,*rp;
   memset (&hints, 0, sizeof(hints));
   hints.ai_family = AF_INET;
   hints.ai_flags = AI_CANONNAME;

   char my_name[512];
   memset(my_name,0,512*sizeof(octet));
   gethostname((char*)my_name,512);

   int erp;
   for (int i = 0; i < 60; i++)
     { erp=getaddrinfo (hostname, NULL, &hints, &ai);
       if (erp == 0)
         { break; }
       else
         { cerr << "getaddrinfo on " << my_name << " has returned '" << gai_strerror(erp) <<
           "' for " << hostname << ", trying again in a second ..." << endl;
           if (ai)
             freeaddrinfo(ai);
           sleep(1);
         }
     }
   if (erp!=0)
     { error("set_up_socket:getaddrinfo");  }

   bool success = false;
   socklen_t len = 0;
   const struct sockaddr* addr = 0;
   for (rp=ai; rp!=NULL; rp=rp->ai_next)
      { addr = ai->ai_addr;

        if (ai->ai_family == AF_INET)
           {
             len = ai->ai_addrlen;
             success = true;
             continue;
           }
      }

   if (not success)
     {
       for (rp = ai; rp != NULL; rp = rp->ai_next)
         cerr << "Family on offer: " << ai->ai_family << endl;
       runtime_error(string("No AF_INET for ") + (char*)hostname + " on " + (char*)my_name);
     }


   Timer timer;
   timer.start();
   struct sockaddr_in* addr4 = (sockaddr_in*) addr;
   addr4->sin_port = htons(Portnum);      // set destination port number
#ifdef DEBUG_IPV4
   cout << "connect to ip " << hex << addr4->sin_addr.s_addr << " port " << addr4->sin_port << dec << endl;
#endif

   int attempts = 0;
   long wait = 1;
   int fl;
   int connect_errno;
   do
   {
       mysocket = socket(AF_INET, SOCK_STREAM, 0);
       if (mysocket < 0)
         error("set_up_socket:socket");

       fl = connect(mysocket, addr, len);
       connect_errno = errno;
       attempts++;
       if (fl != 0)
         {
           close(mysocket);
           usleep(wait *= 2);
#ifdef DEBUG_NETWORKING
           string msg = "Connecting to " + string(hostname) + ":" +
               to_string(Portnum) + " failed";
           errno = connect_errno;
           perror(msg.c_str());
#endif
         }
       errno = connect_errno;
   }
   while (fl == -1
       && (errno == ECONNREFUSED || errno == ETIMEDOUT || errno == EINPROGRESS || errno == EHOSTUNREACH)
       && timer.elapsed() < OnlineOptions::singleton.connection_timeout);

   if (fl < 0)
     {
       throw runtime_error(
           string() + "cannot connect from " + my_name + " to " + hostname + ":"
               + to_string(Portnum) + " after " + to_string(attempts)
               + " attempts in " + to_string(OnlineOptions::singleton.connection_timeout) + " seconds because " +
               strerror(connect_errno) + ". "
               "https://mp-spdz.readthedocs.io/en/latest/troubleshooting.html#"
               "connection-failures has more information on port requirements.");
     }

   freeaddrinfo(ai);

  /* disable Nagle's algorithm */
  int one=1;
  fl= setsockopt(mysocket, IPPROTO_TCP, TCP_NODELAY, (char*)&one, sizeof(int));
  if (fl<0) { error("set_up_socket:setsockopt");  }

#ifdef __APPLE__
  int flags = fcntl(mysocket, F_GETFL, 0);
  fl = fcntl(mysocket, F_SETFL, O_NONBLOCK |  flags);
  if (fl < 0)
    error("set non-blocking");
#endif
}

void close_client_socket(int socket)
{
  if (close(socket))
    {
      char tmp[1000];
      sprintf(tmp, "close(%d)", socket);
      error(tmp);
    }
}
