#ifndef _sockets_h
#define _sockets_h

#include "Networking/data.h"

#include <errno.h>      /* Errors */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <arpa/inet.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>   /* Wait for Process Termination */
#include "Processor/PpcConstant.h"
#include <iostream>
#include <fstream>
using namespace std;


void error(const char *str);

void set_up_client_socket(int& mysocket,const char* hostname,int Portnum);
void close_client_socket(int socket);

// send/receive integers
template<class T>
void send(T& socket, size_t a, size_t len);
template<class T>
void receive(T& socket, size_t& a, size_t len);

template<class T>
void send(T socket, octet* msg, size_t len);
template<class T>
void receive(T socket, octet* msg, size_t len);

inline std::string BinaryToHexString(
        unsigned char* msg,
        size_t len
        )
    {
        std::stringstream ss;
        for(int i=0; i<(int)len; ++i)
            ss << std::hex << (int)msg[i];
        std::string mystr = ss.str();

        return mystr;
    }

inline size_t send_non_blocking(int socket, octet* msg, size_t len)
{
  int j = send(socket,msg,len,MSG_DONTWAIT);
  if (j < 0)
    {
      if (errno != EINTR and errno != EAGAIN and errno != EWOULDBLOCK)
        { error("Send error - 1 ");  }
      else
        return 0;
    }
  return j;
}

inline void write_ppc_debug_file(int socket, octet* msg, size_t len, bool debug_flag, std::string role) {
    if(debug_flag) {
        time_t timep;
        time(&timep);
        struct tm *p_time;
        p_time = gmtime(&timep);
        std::ostringstream os_time;
        std::ostringstream os_time_log;
        os_time << "-" <<  1900 + p_time->tm_year << "-" << p_time->tm_mon + 1 << "-" << p_time->tm_mday;
        os_time_log << p_time->tm_hour << ":"<< p_time->tm_min << ":"<< p_time->tm_sec << ":";
        std::string time_prefix = os_time.str();
        std::string iPid = "-PID-" + std::to_string(getpid());
        std::string file_name = PPC_DEBUG_PREFIEX + role + iPid + time_prefix + ".log";
        ofstream ppc_debug_log;
        ppc_debug_log.open(file_name, std::ios_base::app);
        ppc_debug_log << os_time_log.str() << "socketId:" << socket << ":message:"<< BinaryToHexString(msg, len) << "\n";
        ppc_debug_log.close();
    }
}

template<>
inline void send(int socket,octet *msg,size_t len)
{
    bool debug_flag = get_debug_flag();
    write_ppc_debug_file(socket, msg, len, debug_flag, "Sender");
  size_t i = 0;
  while (i < len)
    {
      i += send_non_blocking(socket, msg + i, len - i);
    }
}

template<class T>
inline void send(T& socket, size_t a, size_t len)
{
  octet blen[len];
  encode_length(blen, a, len);
  send(socket, blen, len);
}

template<>
inline void receive(int socket,octet *msg,size_t len)
{
  size_t i=0;
  int fail = 0;
  long wait = 1;
  while (len-i>0)
    { int j=recv(socket,msg+i,len-i,0);
      // success first
      if (j > 0)
        i = i + j;
      else if (j < 0)
        {
          if (errno == EAGAIN or errno == EINTR)
            {
              if (++fail > 25)
                error("Unavailable too many times");
              else
                {
                  usleep(wait *= 2);
                }
            }
          else
            { error("Receiving error - 1"); }
        }
      else
        throw closed_connection();
    }
    bool debug_flag = get_debug_flag();
    write_ppc_debug_file(socket, msg, len, debug_flag, "Receiver");
}

template<class T>
inline void receive(T& socket, size_t& a, size_t len)
{
  octet blen[len];
  receive(socket, blen, len);
  a = decode_length(blen, len);
}

inline size_t check_non_blocking_result(int res)
{
  if (res < 0)
    {
      if (errno != EWOULDBLOCK)
        error("Non-blocking receiving error");
      return 0;
    }
  return res;
}

inline size_t receive_non_blocking(int socket,octet *msg,int len)
{
  int res = recv(socket, msg, len, MSG_DONTWAIT);
  return check_non_blocking_result(res);
}

inline size_t receive_all_or_nothing(int socket,octet *msg,int len)
{
  int res = recv(socket, msg, len, MSG_DONTWAIT | MSG_PEEK);
  check_non_blocking_result(res);
  if (res == len)
    {
      if (recv(socket, msg, len, 0) != len)
        error("All or nothing receiving error");
      return len;
    }
  else
    return 0;
}

#endif
