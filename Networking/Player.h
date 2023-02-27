#ifndef _Player
#define _Player

/* Class to create a player, for KeyGen, Offline and Online phases.
 *
 * Basically handles connection to the server to obtain the names
 * of the other players. Plus sending and receiving of data
 *
 */

#include <vector>
#include <set>
#include <iostream>
#include <fstream>
using namespace std;

#include "Tools/octetStream.h"
#include "Tools/FlexBuffer.h"
#include "Networking/sockets.h"
#include "Tools/Hash.h"
#include "Tools/int.h"
#include "Networking/Receiver.h"
#include "Networking/Sender.h"
#include "Tools/ezOptionParser.h"
#include "Networking/PlayerBuffer.h"
#include "Tools/Lock.h"

template<class T> class MultiPlayer;
class Server;
class ServerSocket;

/**
 * Network setup (hostnames and port numbers)
 */
class Names
{
  friend class Player;
  friend class PlainPlayer;
  friend class RealTwoPartyPlayer;
  friend class Server;

  vector<string> names;
  vector<int> ports;
  int nplayers;
  int portnum_base;
  int player_no;

  ServerSocket* server;

  int default_port(int playerno) { return portnum_base + playerno; }
  void setup_ports();

  void setup_names(const char *servername, int my_port);

  void setup_server();

  void set_server(ServerSocket* socket);

  public:

  static const int DEFAULT_PORT = -1;

  /**
   * Initialize with central server
   * @param player my number
   * @param pnb base port number (server listens one below)
   * @param my_port my port number (`DEFAULT_PORT` for default,
   *  which is base port number plus player number)
   * @param servername location of server
   * @param setup_socket whether to start listening
   */
  void init(int player, int pnb, int my_port, const char* servername,
      bool setup_socket = true);
  Names(int player,int pnb,int my_port,const char* servername) : Names()
    { init(player,pnb,my_port,servername); }

  /**
   * Initialize with central server running on player 0
   * @param player my number
   * @param nplayers number of players
   * @param servername location of player 0
   * @param pnb base port number
   * @param my_port my port number (`DEFAULT_PORT` for default,
   *  which is base port number plus player number)
   */
  Names(int player, int nplayers, const string& servername, int pnb,
      int my_port = DEFAULT_PORT);

  /**
   * Initialize without central server
   * @param player my number
   * @param pnb base port number
   * @param Nms locations of all parties
   */
  void init(int player,int pnb,vector<string> Nms);
  Names(int player,int pnb,vector<string> Nms) : Names()
    { init(player,pnb,Nms); }

  /**
   * Initialize from file. One party per line, format ``<hostname>[:<port>]``
   * @param player my number
   * @param pnb base port number
   * @param hostsfile filename
   * @param players number of players (0 to take from file)
   */
  void init(int player, int pnb, const string& hostsfile, int players = 0);
  Names(int player, int pnb, const string& hostsfile) : Names()
    { init(player, pnb, hostsfile); }

  /**
   * Initialize from command-line options
   * @param opt option parser instance
   * @param argc number of command-line arguments
   * @param argv command-line arguments
   * @param default_nplayers default number of players
   *  (used if not given in arguments)
   */
  Names(ez::ezOptionParser& opt, int argc, const char** argv,
      int default_nplayers = 2);

  Names(int my_num = 0, int num_players = 1);
  Names(const Names& other);
  ~Names();

  int num_players() const { return nplayers; }
  int my_num() const { return player_no; }
  const string get_name(int i) const { return names[i]; }
  int get_portnum_base() const { return portnum_base; }
};


struct CommStats
{
  size_t data, rounds;
  Timer timer;
  CommStats() : data(0), rounds(0) {}
  Timer& add(size_t length)
    {
#ifdef VERBOSE_COMM
      cout << "add " << length << endl;
#endif
      data += length;
      rounds++;
      return timer;
    }
  Timer& add(const octetStream& os) { return add(os.get_length()); }
  void add(const octetStream& os, const TimeScope& scope) { add(os) += scope; }
  CommStats& operator+=(const CommStats& other);
  CommStats& operator-=(const CommStats& other);
};

class NamedCommStats : public map<string, CommStats>
{
public:
  size_t sent;

  NamedCommStats();

  NamedCommStats& operator+=(const NamedCommStats& other);
  NamedCommStats operator+(const NamedCommStats& other) const;
  NamedCommStats operator-(const NamedCommStats& other) const;
  void print(bool newline = false);
  void reset();
#ifdef VERBOSE_COMM
  CommStats& operator[](const string& name)
  {
    auto& res = map<string, CommStats>::operator[](name);
    cout << name << " after " << res.data << endl;
    return res;
  }
#endif
};

/**
 * Abstract class for two- and multi-player communication
 */
class PlayerBase
{
protected:
  int player_no;

  size_t& sent;
  mutable NamedCommStats comm_stats;

public:
  mutable Timer timer;

  PlayerBase(int player_no) : player_no(player_no), sent(comm_stats.sent) {}
  virtual ~PlayerBase();

  int my_real_num() const { return player_no; }
  virtual int my_num() const = 0;
  virtual int num_players() const = 0;

  virtual void receive_player(int, octetStream&) const
  { throw not_implemented(); }
  virtual void pass_around(octetStream&, int = 1) const
  { throw not_implemented(); }
  virtual void Broadcast_Receive(vector<octetStream>&) const
  { throw not_implemented(); }
  virtual void unchecked_broadcast(vector<octetStream>& o) const
  { Broadcast_Receive(o); }
  virtual void send_receive_all(const vector<octetStream>&,
      vector<octetStream>&) const
  { throw not_implemented(); }

  void reset_stats();
};

/**
 * Abstract class for multi-player communication.
 * ``*_no_stats`` functions are called by their equivalents
 * after accounting for communications statistics.
 */
class Player : public PlayerBase
{
protected:
  int nplayers;

  mutable Hash ctx;

public:
  const Names& N;

  mutable vector<NamedCommStats> thread_stats;

  Player(const Names& Nms);
  virtual ~Player();

  virtual string get_id() const { throw not_implemented(); }

  /**
   * Get number of players
   */
  int num_players() const { return nplayers; }
  /**
   * Get my player number
   */
  int my_num() const { return player_no; }

  int get_offset(int other_player) const { return positive_modulo(other_player - my_num(), num_players()); }
  int get_player(int offset) const { return positive_modulo(offset + my_num(), num_players()); }

  virtual bool is_encrypted() { return false; }

  virtual void send_long(int, long) const { throw not_implemented(); }
  virtual long receive_long(int) const { throw not_implemented(); }

  // The following functions generally update the statistics
  // and then call the *_no_stats equivalent specified by a subclass.

  /**
   * Send the same to all other players
   */
  virtual void send_all(const octetStream& o) const;
  /**
   * Send to a specific player
   */
  void send_to(int player,const octetStream& o) const;
  virtual void send_to_no_stats(int player,const octetStream& o) const = 0;
  /**
   * Receive from all other players.
   * Information from player 0 at ``os[0]`` etc.
   */
  void receive_all(vector<octetStream>& os) const;
  /**
   * Receive from a specific player
   */
  void receive_player(int i,octetStream& o) const;
  virtual void receive_player_no_stats(int i,octetStream& o) const = 0;
  virtual void receive_player(int i,FlexBuffer& buffer) const;

  virtual size_t send_no_stats(int, const PlayerBuffer&, bool) const
  { throw not_implemented(); }
  virtual size_t recv_no_stats(int, const PlayerBuffer&, bool) const
  { throw not_implemented(); }

  /**
   * Send to all other players by offset.
   * ``o[0]`` gets sent to the next player etc.
   */
  void send_relative(const vector<octetStream>& o) const;
  /*
   * Send to other player specified by offset.
   * 1 stands for the next player etc.
   */
  void send_relative(int offset, const octetStream& o) const;
  /**
   * Receive from all other players by offset.
   * ``o[0]`` will contain data from the next player etc.
   */
  void receive_relative(vector<octetStream>& o) const;
  /**
   * Receive from other player specified by offset.
   * 1 stands for the next player etc.
   */
  void receive_relative(int offset, octetStream& o) const;

  /**
   * Exchange information with one other party,
   * reusing the buffer if possible.
   */
  void exchange(int other, const octetStream& to_send, octetStream& ot_receive) const;
  virtual void exchange_no_stats(int, const octetStream&, octetStream&) const
  { throw runtime_error("implement exchange"); }
  /**
   * Exchange information with one other party, reusing the buffer.
   */
  void exchange(int other, octetStream& o) const;
  /**
   * Exchange information with one other party specified by offset,
   * reusing the buffer if possible.
   */
  void exchange_relative(int offset, octetStream& o) const;
  /**
   * Send information to a party while receiving from another by offset,
   * The default is to send to the next party while receiving from the previous.
   * The buffer is reused.
   */
  void pass_around(octetStream& o, int offset = 1) const { pass_around(o, o, offset); }
  /**
   * Send information to a party while receiving from another by offset.
   * The default is to send to the next party while receiving from the previous.
   */
  void pass_around(octetStream& to_send, octetStream& to_receive, int offset) const;
  virtual void pass_around_no_stats(const octetStream&, octetStream&,
      int) const { throw runtime_error("implement passing around"); }

  /**
   * Broadcast and receive data to/from all players.
   * Assumes o[player_no] contains the data to be broadcast by me.
   */
  virtual void unchecked_broadcast(vector<octetStream>& o) const;
  /**
   * Broadcast and receive data to/from all players with eventual verification.
   * Assumes o[player_no] contains the data to be broadcast by me.
   */
  virtual void Broadcast_Receive(vector<octetStream>& o) const;
  virtual void Broadcast_Receive_no_stats(vector<octetStream>&) const
  { throw runtime_error("implement broadcast"); }

  /**
   * Run protocol to verify broadcast is correct
   */
  virtual void Check_Broadcast() const;

  /**
   * Send something different to each player.
   */
  void send_receive_all(const vector<octetStream>& to_send,
      vector<octetStream>& to_receive) const;
  /**
   * Specified senders only send something different to each player.
   * @param senders set whether a player sends or not,
   *   must be equal on all players
   * @param to_send data to send by player number
   * @param to_receive received data by player number
   */
  void send_receive_all(const vector<bool>& senders,
      const vector<octetStream>& to_send, vector<octetStream>& to_receive) const;
  /**
   * Send something different only one specified channels.
   * @param channels ``channel[i][j]`` indicates whether party ``i`` sends
   *   to party ``j``
   * @param to_send data to send by player number
   * @param to_receive received data by player number
   */
  void send_receive_all(const vector<vector<bool>>& channels,
      const vector<octetStream>& to_send,
      vector<octetStream>& to_receive) const;
  virtual void send_receive_all_no_stats(const vector<vector<bool>>& channels,
      const vector<octetStream>& to_send,
      vector<octetStream>& to_receive) const = 0;

  /**
   * Specified senders broadcast information to specified receivers.
   * @param senders specify which parties do send
   * @param receivers specify which parties do send
   * @param os data to send at ``os[my_number]``, received data elsewhere
   */
  virtual void partial_broadcast(const vector<bool>& senders,
      const vector<bool>& receivers,
      vector<octetStream>& os) const;

  // dummy functions for compatibility
  virtual void request_receive(int i, octetStream& o) const { (void)i; (void)o; }
  virtual void wait_receive(int i, octetStream& o) const
  { receive_player(i, o); }

  NamedCommStats total_comm() const;
};

/**
 * Multi-player communication helper class.
 * ``T = int`` for unencrypted BSD sockets and
 * ``T = ssl_socket*`` for Boost SSL streams.
 */
template<class T>
class MultiPlayer : public Player
{
  string id;

protected:
  vector<T> sockets;
  T send_to_self_socket;

  T socket_to_send(int player) const { return player == player_no ? send_to_self_socket : sockets[player]; }
  T socket(int i) const { return sockets[i]; }

  friend class CryptoPlayer;

public:
  MultiPlayer(const Names& Nms, const string& id);

  virtual ~MultiPlayer();

  string get_id() const { return id; }

  // Send/Receive data to/from player i 
  void send_long(int i, long a) const;
  long receive_long(int i) const;

  // Send an octetStream to all other players 
  //   -- And corresponding receive
  virtual void send_to_no_stats(int player,const octetStream& o) const;
  virtual void receive_player_no_stats(int i,octetStream& o) const;

  // exchange data with minimal memory usage
  virtual void exchange_no_stats(int other, const octetStream& to_send,
      octetStream& to_receive) const;

  // send to next and receive from previous player
  virtual void pass_around_no_stats(const octetStream& to_send,
      octetStream& to_receive, int offset) const;

  /* Broadcast and Receive data to/from all players 
   *  - Assumes o[player_no] contains the thing broadcast by me
   */
  virtual void Broadcast_Receive_no_stats(vector<octetStream>& o) const;

  virtual void send_receive_all_no_stats(const vector<vector<bool>>& channels,
      const vector<octetStream>& to_send,
      vector<octetStream>& to_receive) const;
};

/**
 * Plaintext multi-player communication
 */
class PlainPlayer : public MultiPlayer<int>
{
  void setup_sockets(const vector<string>& names, const vector<int>& ports,
      const string& id_base, ServerSocket& server);

public:
  /**
   * Start a new set of unencrypted connections.
   * @param Nms network setup
   * @param id unique identifier
   */
  PlainPlayer(const Names& Nms, const string& id);
  // legacy interface
  PlainPlayer(const Names& Nms, int id_base = 0);
  ~PlainPlayer();

  size_t send_no_stats(int player, const PlayerBuffer& buffer, bool block) const;
  size_t recv_no_stats(int player, const PlayerBuffer& buffer, bool block) const;
};


class ThreadPlayer : public PlainPlayer
{
public:
  mutable vector<Receiver<int>*> receivers;
  mutable vector<Sender<int>*>   senders;

  ThreadPlayer(const Names& Nms, const string& id_base);
  virtual ~ThreadPlayer();

  void request_receive(int i, octetStream& o) const;
  void wait_receive(int i, octetStream& o) const;
  void receive_player_no_stats(int i,octetStream& o) const;

  void send_all(const octetStream& o) const;
};


class TwoPartyPlayer : public PlayerBase
{
public:
  TwoPartyPlayer(int my_num) : PlayerBase(my_num) {}
  virtual ~TwoPartyPlayer() {}

  virtual int my_num() const = 0;
  virtual int other_player_num() const = 0;

  virtual void send(octetStream& o) const = 0;
  virtual void receive(octetStream& o) const = 0;
  virtual void send_receive_player(vector<octetStream>& o) const = 0;
  void Broadcast_Receive(vector<octetStream>& o) const;

  virtual size_t send(const PlayerBuffer&, bool) const
  { throw not_implemented(); }
  virtual size_t recv(const PlayerBuffer&, bool) const
  { throw not_implemented(); }
};

// for different threads, separate statistics
class VirtualTwoPartyPlayer : public TwoPartyPlayer
{
  Player& P;
  int other_player;
  NamedCommStats& comm_stats;

  mutable Lock lock;

public:
  VirtualTwoPartyPlayer(Player& P, int other_player);

  // emulate RealTwoPartyPlayer
  int my_num() const { return P.my_num() > other_player; }
  int other_player_num() const { return other_player; }
  int num_players() const { return 2; }

  void send(octetStream& o) const;
  void receive(octetStream& o) const;
  void send_receive_player(vector<octetStream>& o) const;

  void pass_around(octetStream& o, int _ = 1) const { (void)_, (void) o; throw not_implemented(); }

  size_t send(const PlayerBuffer& buffer, bool block) const;
  size_t recv(const PlayerBuffer& buffer, bool block) const;
};

class RealTwoPartyPlayer : public VirtualTwoPartyPlayer
{
  PlainPlayer* P;

public:
  RealTwoPartyPlayer(const Names& Nms, int other_player, const string& id);
  // legacy
  RealTwoPartyPlayer(const Names& Nms, int other_player, int id_base = 0);
  ~RealTwoPartyPlayer();
};

// for the same thread
class OffsetPlayer : public TwoPartyPlayer
{
private:
  Player& P;
  int offset;

public:
  OffsetPlayer(Player& P, int offset) : TwoPartyPlayer(P.my_num()), P(P), offset(offset) {}

  // emulate RealTwoPartyPlayer
  int my_num() const { return P.my_num() > P.get_player(offset); }
  int other_player_num() const { return P.get_player(offset); }
  int num_players() const { return 2; }
  int get_offset() const { return offset; }
  Player& get_full_player() const { return P; }

  void send(octetStream& o) const { P.send_to(P.get_player(offset), o); }
  void reverse_send(octetStream& o) const { P.send_to(P.get_player(-offset), o); }
  void receive(octetStream& o) const { P.receive_player(P.get_player(offset), o); }
  void reverse_receive(octetStream& o) { P.receive_player(P.get_player(-offset), o); }
  void send_receive_player(vector<octetStream>& o) const;

  void reverse_exchange(octetStream& o) const { P.pass_around(o, P.num_players() - offset); }
  void exchange(octetStream& o) const { P.exchange(P.get_player(offset), o); }
  void pass_around(octetStream& o, int _ = 1) const { (void)_; P.pass_around(o, offset); }
};

#endif
