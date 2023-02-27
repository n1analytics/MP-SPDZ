
#include "Player.h"
#include "ssl_sockets.h"
#include "Tools/Exceptions.h"
#include "Tools/int.h"
#include "Tools/NetworkOptions.h"
#include "Networking/Server.h"
#include "Networking/ServerSocket.h"
#include "Networking/Exchanger.h"

#include <sys/select.h>
#include <utility>
#include <assert.h>

using namespace std;

void Names::init(int player, int pnb, int my_port, const char* servername,
    bool setup_socket)
{
  player_no=player;
  portnum_base=pnb;
  setup_names(servername, my_port);
  if (setup_socket)
    setup_server();
}

Names::Names(int player, int nplayers, const string& servername, int pnb,
    int my_port) :
    Names()
{
  Server::start_networking(*this, player, nplayers, servername, pnb, my_port);
}

void Names::init(int player,int pnb,vector<string> Nms)
{
  player_no=player;
  portnum_base=pnb;
  nplayers=Nms.size();
  names=Nms;
  setup_ports();
  setup_server();
}

// initialize names from file, no Server.x coordination.
void Names::init(int player, int pnb, const string& filename, int nplayers_wanted)
{
  ifstream hostsfile(filename.c_str());
  if (hostsfile.fail())
  {
     stringstream ss;
     ss << "Error opening " << filename << ". See HOSTS.example for an example.";
     throw file_error(ss.str().c_str());
  }
  player_no = player;
  nplayers = 0;
  portnum_base = pnb;
  string line;
  ports.clear();
  while (getline(hostsfile, line))
  {
    if (line.length() > 0 && line.at(0) != '#') {
      auto pos = line.find(':');
      if (pos == string::npos)
      {
        names.push_back(line);
        ports.push_back(default_port(nplayers));
      }
      else
      {
        names.push_back(line.substr(0, pos));
        int port;
        stringstream(line.substr(pos + 1)) >> port;
        ports.push_back(port);
      }
      nplayers++;
      if (nplayers_wanted > 0 and nplayers_wanted == nplayers)
        break;
    }
  }
  if (nplayers_wanted > 0 and nplayers_wanted != nplayers)
    throw runtime_error("not enought hosts in HOSTS");
#ifdef DEBUG_NETWORKING
  cerr << "Got list of " << nplayers << " players from file: " << endl;
  for (unsigned int i = 0; i < names.size(); i++)
    cerr << "    " << names[i] << ":" << ports[i] << endl;
#endif
  setup_server();
}

Names::Names(ez::ezOptionParser& opt, int argc, const char** argv,
    int default_nplayers) : Names()
{
  NetworkOptionsWithNumber network_opts(opt, argc, argv, default_nplayers, true);
  opt.add(
          "", // Default.
          1, // Required?
          1, // Number of args expected.
          0, // Delimiter if expecting multiple args.
          "This player's number (required)", // Help description.
          "-p", // Flag token.
          "--player" // Flag token.
  );
  opt.parse(argc, argv);
  opt.get("-p")->getInt(player_no);
  vector<string> missing;
  if (not opt.gotRequired(missing))
  {
      string usage;
      opt.getUsage(usage);
      cerr << usage;
      exit(1);
  }
  network_opts.start_networking(*this, player_no);
}

void Names::setup_ports()
{
  ports.resize(nplayers);
  for (int i = 0; i < nplayers; i++)
    ports[i] = default_port(i);
}

void Names::setup_names(const char *servername, int my_port)
{
  if (my_port == DEFAULT_PORT)
    my_port = default_port(player_no);

  int socket_num;
  int pn = portnum_base;
  set_up_client_socket(socket_num, servername, pn);
  octetStream("P" + to_string(player_no)).Send(socket_num);
#ifdef DEBUG_NETWORKING
  cerr << "Sent " << player_no << " to " << servername << ":" << pn << endl;
#endif

  // Send my name
  sockaddr_in address;
  socklen_t size = sizeof address;
  getsockname(socket_num, (sockaddr*)&address, &size);
  char* my_name = inet_ntoa(address.sin_addr);
  octetStream(my_name).Send(socket_num);
  send(socket_num,(octet*)&my_port,4);
#ifdef DEBUG_NETWORKING
  fprintf(stderr, "My Name = %s\n",my_name);
  cerr << "My number = " << player_no << endl;
#endif

  // Now get the set of names
  try
  {
    octetStream os;
    os.Receive(socket_num);
    os.get(names);
    os.get(ports);
  }
  catch (exception& e)
  {
    throw runtime_error(string("error in network setup: ") + e.what());
  }

  if (names.size() != ports.size())
    throw runtime_error("invalid network setup");
  nplayers = names.size();
#ifdef VERBOSE
  for (int i = 0; i < nplayers; i++)
    cerr << "Player " << i << " is running on machine " << names[i] << endl;
#endif
  close_client_socket(socket_num);
}


void Names::setup_server()
{
  server = new ServerSocket(ports.at(player_no));
  server->init();
}

void Names::set_server(ServerSocket* socket)
{
  assert(not server);
  server = socket;
}


Names::Names(const Names& other)
{
  if (other.server != 0)
      throw runtime_error("Can copy Names only when uninitialized");
  player_no = other.player_no;
  nplayers = other.nplayers;
  portnum_base = other.portnum_base;
  names = other.names;
  ports = other.ports;
  server = 0;
}

Names::Names(int my_num, int num_players) :
    nplayers(num_players), portnum_base(-1), player_no(my_num), server(0)
{
}

Names::~Names()
{
  if (server != 0)
    delete server;
}


Player::Player(const Names& Nms) :
        PlayerBase(Nms.my_num()), N(Nms)
{
  nplayers=Nms.nplayers;
  player_no=Nms.player_no;
  thread_stats.resize(nplayers);
}


template<class T>
MultiPlayer<T>::MultiPlayer(const Names& Nms, const string& id) :
        Player(Nms), id(id), send_to_self_socket(0)
{
  sockets.resize(Nms.num_players());
}


PlainPlayer::PlainPlayer(const Names& Nms, const string& id) :
        MultiPlayer<int>(Nms, id)
{
  if (Nms.num_players() > 1)
    setup_sockets(Nms.names, Nms.ports, id, *Nms.server);
}


PlainPlayer::PlainPlayer(const Names& Nms, int id_base) :
        PlainPlayer(Nms, to_string(id_base))
{
}

PlainPlayer::~PlainPlayer()
{
  if (num_players() > 1)
    {
      /* Close down the sockets */
      for (auto socket : sockets)
        close_client_socket(socket);
      close_client_socket(send_to_self_socket);
    }
}

template<class T>
MultiPlayer<T>::~MultiPlayer()
{
}

Player::~Player()
{
#ifdef VERBOSE
  for (auto& x : thread_stats)
    x.print();
#endif
}

PlayerBase::~PlayerBase()
{
#ifdef VERBOSE
  comm_stats.print();
  if (timer.elapsed() > 0)
    cerr << "Receiving took " << timer.elapsed() << " seconds" << endl;
#endif
}



// Set up nmachines client and server sockets to send data back and fro
//   A machine is a server between it and player i if i<=my_number
//   Can also communicate with myself, but only with send_to and receive_from
void PlainPlayer::setup_sockets(const vector<string>& names,
        const vector<int>& ports, const string& id_base, ServerSocket& server)
{
    sockets.resize(nplayers);
    // Set up the client side
    for (int i=player_no; i<nplayers; i++) {
        auto pn=id_base+"P"+to_string(player_no);
        if (i==player_no) {
          const char* localhost = "127.0.0.1";
#ifdef DEBUG_NETWORKING
          fprintf(stderr,
              "Setting up send to self socket to %s:%d with id %s\n",
              localhost, ports[i], pn.c_str());
#endif
          set_up_client_socket(sockets[i],localhost,ports[i]);
        } else {
#ifdef DEBUG_NETWORKING
            fprintf(stderr, "Setting up client to %s:%d with id %s\n",
                names[i].c_str(), ports[i], pn.c_str());
#endif
          set_up_client_socket(sockets[i],names[i].c_str(),ports[i]);
        }
        octetStream(pn).Send(sockets[i]);
    }
    send_to_self_socket = sockets[player_no];
    // Setting up the server side
    for (int i=0; i<=player_no; i++) {
        auto id=id_base+"P"+to_string(i);
#ifdef DEBUG_NETWORKING
        fprintf(stderr,
            "As a server, waiting for client with id %s to connect.\n",
            id.c_str());
#endif
        sockets[i] = server.get_connection_socket(id);
    }

    for (int i = 0; i < nplayers; i++) {
        // timeout of 5 minutes
        struct timeval tv;
        tv.tv_sec = 300;
        tv.tv_usec = 0;
        int fl = setsockopt(sockets[i], SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(struct timeval));
        if (fl<0) { error("set_up_socket:setsockopt");  }
    }
}


template<class T>
void MultiPlayer<T>::send_long(int i, long a) const
{
  send(sockets[i], (octet*)&a, sizeof(long));
}

template<class T>
long MultiPlayer<T>::receive_long(int i) const
{
  long res;
  receive(sockets[i], (octet*)&res, sizeof(long));
  return res;
}



void Player::send_to(int player,const octetStream& o) const
{
#ifdef VERBOSE_COMM
  cerr << "sending to " << player << endl;
#endif
  TimeScope ts(comm_stats["Sending directly"].add(o));
  send_to_no_stats(player, o);
  sent += o.get_length();
}

template<class T>
void MultiPlayer<T>::send_to_no_stats(int player,const octetStream& o) const
{
  T socket = socket_to_send(player);
  o.Send(socket);
}


void Player::send_all(const octetStream& o) const
{
  TimeScope ts(comm_stats["Sending to all"].add(o));
  for (int i=0; i<nplayers; i++)
     { if (i!=player_no)
         send_to_no_stats(i, o);
     }
  sent += o.get_length() * (num_players() - 1);
}


void Player::receive_all(vector<octetStream>& os) const
{
  os.resize(num_players());
  for (int j = 0; j < num_players(); j++)
    if (j != my_num())
      receive_player(j, os[j]);
}


void Player::receive_player(int i,octetStream& o) const
{
#ifdef VERBOSE_COMM
  cerr << "receiving from " << i << endl;
#endif
  TimeScope ts(timer);
  receive_player_no_stats(i, o);
  comm_stats["Receiving directly"].add(o, ts);
}

template<class T>
void MultiPlayer<T>::receive_player_no_stats(int i,octetStream& o) const
{
  o.reset_write_head();
  o.Receive(sockets[i]);
}

void Player::receive_player(int i, FlexBuffer& buffer) const
{
  octetStream os;
  receive_player(i, os);
  buffer = os;
}

size_t PlainPlayer::send_no_stats(int player,
        const PlayerBuffer& buffer, bool block) const
{
  if (block)
    {
      send(socket(player), buffer.data, buffer.size);
      return buffer.size;
    }
  else
    return send_non_blocking(socket(player), buffer.data, buffer.size);
}

size_t PlainPlayer::recv_no_stats(int player,
        const PlayerBuffer& buffer, bool block) const
{
    if (block)
      {
        receive(socket(player), buffer.data, buffer.size);
        return buffer.size;
      }
    else
      return receive_non_blocking(socket(player), buffer.data, buffer.size);
}


void Player::send_relative(const vector<octetStream>& os) const
{
  assert((int)os.size() == num_players() - 1);
  for (int offset = 1; offset < num_players(); offset++)
    send_relative(offset, os[offset - 1]);
}

void Player::send_relative(int offset, const octetStream& o) const
{
  send_to(positive_modulo(my_num() + offset, num_players()), o);
}

void Player::receive_relative(vector<octetStream>& os) const
{
  assert((int)os.size() == num_players() - 1);
  for (int offset = 1; offset < num_players(); offset++)
    receive_relative(offset, os[offset - 1]);
}

void Player::receive_relative(int offset, octetStream& o) const
{
  receive_player(positive_modulo(my_num() + offset, num_players()), o);
}

template<class T>
void MultiPlayer<T>::exchange_no_stats(int other, const octetStream& o, octetStream& to_receive) const
{
  o.exchange(sockets[other], sockets[other], to_receive);
}

void Player::exchange(int other, const octetStream& o, octetStream& to_receive) const
{
#ifdef VERBOSE_COMM
  cerr << "Exchanging with " << other << endl;
#endif
  TimeScope ts(comm_stats["Exchanging"].add(o));
  exchange_no_stats(other, o, to_receive);
  sent += o.get_length();
}


void Player::exchange(int player, octetStream& o) const
{
  exchange(player, o, o);
}

void Player::exchange_relative(int offset, octetStream& o) const
{
  exchange(get_player(offset), o);
}


template<class T>
void MultiPlayer<T>::pass_around_no_stats(const octetStream& o, octetStream& to_receive, int offset) const
{
  o.exchange(sockets.at(get_player(offset)), sockets.at(get_player(-offset)), to_receive);
}

void Player::pass_around(octetStream& o, octetStream& to_receive, int offset) const
{
  TimeScope ts(comm_stats["Passing around"].add(o));
  pass_around_no_stats(o, to_receive, offset);
  sent += o.get_length();
}


/* This is deliberately weird to avoid problems with OS max buffer
 * size getting in the way
 */
template<class T>
void MultiPlayer<T>::Broadcast_Receive_no_stats(vector<octetStream>& o) const
{
  if (o.size() != sockets.size())
    throw runtime_error("player numbers don't match");

  vector<Exchanger<T>> exchangers;
  for (int i=1; i<nplayers; i++)
    {
      int send_to = (my_num() + i) % num_players();
      int receive_from = (my_num() + num_players() - i) % num_players();
      exchangers.push_back({sockets[send_to], o[my_num()], sockets[receive_from], o[receive_from]});
    }

  int left = 1;
  while (left > 0)
    {
      left = 0;
      for (auto& exchanger : exchangers)
        left += exchanger.round(false);
    }
}

void Player::unchecked_broadcast(vector<octetStream>& o) const
{
  TimeScope ts(comm_stats["Broadcasting"].add(o[player_no]));
  Broadcast_Receive_no_stats(o);
  sent += o[player_no].get_length() * (num_players() - 1);
}

void Player::Broadcast_Receive(vector<octetStream>& o) const
{
  unchecked_broadcast(o);
    { for (int i=0; i<nplayers; i++)
        { hash_update(&ctx,o[i].get_data(),o[i].get_length()); }
    }
}


void Player::Check_Broadcast() const
{
  if (ctx.size == 0)
    return;
  vector<octetStream> h(nplayers);
  h[player_no].concat(ctx.final());

  unchecked_broadcast(h);
  for (int i=0; i<nplayers; i++)
    { if (i!=player_no)
        { if (!h[i].equals(h[player_no]))
	    { throw broadcast_invalid(); }
        }
    }
  ctx.reset();
}

void Player::send_receive_all(const vector<octetStream>& to_send,
    vector<octetStream>& to_receive) const
{
  send_receive_all(
      vector<vector<bool>>(num_players(), vector<bool>(num_players(), true)),
      to_send, to_receive);
}

void Player::send_receive_all(const vector<bool>& senders,
    const vector<octetStream>& to_send, vector<octetStream>& to_receive) const
{
  vector<vector<bool>> channels;
  for (int i = 0; i < num_players(); i++)
    channels.push_back(vector<bool>(num_players(), senders.at(i)));
  send_receive_all(channels, to_send, to_receive);
}

void Player::send_receive_all(const vector<vector<bool>>& channels,
    const vector<octetStream>& to_send,
    vector<octetStream>& to_receive) const
{
  size_t data = 0;
  for (int i = 0; i < num_players(); i++)
    if (i != my_num() and channels.at(my_num()).at(i))
      {
        data += to_send.at(i).get_length();
#ifdef VERBOSE_COMM
        cerr << "Send " << to_send.at(i).get_length() << " to " << i << endl;
#endif
      }
  TimeScope ts(comm_stats["Sending/receiving"].add(data));
  sent += data;
  send_receive_all_no_stats(channels, to_send, to_receive);
}

void Player::partial_broadcast(const vector<bool>&,
    const vector<bool>&, vector<octetStream>& os) const
{
  unchecked_broadcast(os);
}

template<class T>
void MultiPlayer<T>::send_receive_all_no_stats(
    const vector<vector<bool>>& channels, const vector<octetStream>& to_send,
    vector<octetStream>& to_receive) const
{
  to_receive.resize(num_players());
  for (int offset = 1; offset < num_players(); offset++)
    {
      int receive_from = get_player(-offset);
      int send_to = get_player(offset);
      bool receive = channels[receive_from][my_num()];
      if (channels[my_num()][send_to])
        {
          if (receive)
            pass_around_no_stats(to_send[send_to], to_receive[receive_from], offset);
          else
            send_to_no_stats(send_to, to_send[send_to]);
        }
      else if (receive)
        receive_player_no_stats(receive_from, to_receive[receive_from]);
    }
}


ThreadPlayer::ThreadPlayer(const Names& Nms, const string& id_base) :
    PlainPlayer(Nms, id_base)
{
  for (int i = 0; i < Nms.num_players(); i++)
    {
      receivers.push_back(new Receiver<int>(sockets[i]));
      senders.push_back(new Sender<int>(socket_to_send(i)));
    }
}

ThreadPlayer::~ThreadPlayer()
{
  for (unsigned int i = 0; i < receivers.size(); i++)
    {
      if (receivers[i]->timer.elapsed() > 0)
        cerr << "Waiting for receiving from " << i << ": " << receivers[i]->timer.elapsed() << endl;
      delete receivers[i];
    }

  for (unsigned int i = 0; i < senders.size(); i++)
    {
      if (senders[i]->timer.elapsed() > 0)
        cerr << "Waiting for sending to " << i << ": " << senders[i]->timer.elapsed() << endl;
      delete senders[i];
    }
}

void ThreadPlayer::request_receive(int i, octetStream& o) const
{
  receivers[i]->request(o);
}

void ThreadPlayer::wait_receive(int i, octetStream& o) const
{
  receivers[i]->wait(o);
}

void ThreadPlayer::receive_player_no_stats(int i, octetStream& o) const
{
  request_receive(i, o);
  wait_receive(i, o);
}

void ThreadPlayer::send_all(const octetStream& o) const
{
  for (int i=0; i<nplayers; i++)
     { if (i!=player_no)
         senders[i]->request(o);
     }

  for (int i = 0; i < nplayers; i++)
    if (i != player_no)
      senders[i]->wait(o);
}


RealTwoPartyPlayer::RealTwoPartyPlayer(const Names& Nms, int other_player, const string& id) :
        VirtualTwoPartyPlayer(*(P = new PlainPlayer(Nms, id + "2")), other_player)
{
}

RealTwoPartyPlayer::RealTwoPartyPlayer(const Names& Nms, int other_player,
        int id_base) : RealTwoPartyPlayer(Nms, other_player, to_string(id_base))
{
}

RealTwoPartyPlayer::~RealTwoPartyPlayer()
{
  delete P;
}

void VirtualTwoPartyPlayer::send(octetStream& o) const
{
  TimeScope ts(comm_stats["Sending one-to-one"].add(o));
  P.send_to_no_stats(other_player, o);
  comm_stats.sent += o.get_length();
}

void VirtualTwoPartyPlayer::receive(octetStream& o) const
{
  TimeScope ts(timer);
  P.receive_player_no_stats(other_player, o);
  comm_stats["Receiving one-to-one"].add(o, ts);
}

void VirtualTwoPartyPlayer::send_receive_player(vector<octetStream>& o) const
{
  TimeScope ts(comm_stats["Exchanging one-to-one"].add(o[0]));
  comm_stats.sent += o[0].get_length();
  P.exchange_no_stats(other_player, o[0], o[1]);
}

VirtualTwoPartyPlayer::VirtualTwoPartyPlayer(Player& P, int other_player) :
    TwoPartyPlayer(P.my_num()), P(P), other_player(other_player), comm_stats(
        P.thread_stats.at(other_player))
{
}

size_t VirtualTwoPartyPlayer::send(const PlayerBuffer& buffer, bool block) const
{
  auto sent = P.send_no_stats(other_player, buffer, block);
  lock.lock();
  comm_stats["Sending one-to-one"].add(sent);
  comm_stats.sent += sent;
  lock.unlock();
  return sent;
}

size_t VirtualTwoPartyPlayer::recv(const PlayerBuffer& buffer, bool block) const
{
  auto received = P.recv_no_stats(other_player, buffer, block);
  lock.lock();
  comm_stats["Receiving one-to-one"].add(received);
  lock.unlock();
  return received;
}

void OffsetPlayer::send_receive_player(vector<octetStream>& o) const
{
  P.exchange(P.get_player(offset), o[0], o[1]);
}

void TwoPartyPlayer::Broadcast_Receive(vector<octetStream>& o) const
{
  vector<octetStream> os(2);
  os[0] = o[my_num()];
  send_receive_player(os);
  o[1 - my_num()] = os[1];
}

NamedCommStats::NamedCommStats() : sent(0)
{
}

CommStats& CommStats::operator +=(const CommStats& other)
{
  data += other.data;
  rounds += other.rounds;
  timer += other.timer;
  return *this;
}

NamedCommStats& NamedCommStats::operator +=(const NamedCommStats& other)
{
  sent += other.sent;
  for (auto it = other.begin(); it != other.end(); it++)
    (*this)[it->first] += it->second;
  return *this;
}

NamedCommStats NamedCommStats::operator +(const NamedCommStats& other) const
{
  auto res = *this;
  res += other;
  return res;
}

CommStats& CommStats::operator -=(const CommStats& other)
{
  data -= other.data;
  rounds -= other.rounds;
  timer -= other.timer;
  return *this;
}

NamedCommStats NamedCommStats::operator -(const NamedCommStats& other) const
{
  NamedCommStats res = *this;
  res.sent = sent - other.sent;
  for (auto it = other.begin(); it != other.end(); it++)
    res[it->first] -= it->second;
  return res;
}

void NamedCommStats::print(bool newline)
{
  for (auto it = begin(); it != end(); it++)
    if (it->second.data)
      cerr << it->first << " " << 1e-6 * it->second.data << " MB in "
      << it->second.rounds << " rounds, taking " << it->second.timer.elapsed()
      << " seconds" << endl;
  if (size() and newline)
    cerr << endl;
}

void NamedCommStats::reset()
{
  clear();
  sent = 0;
}

void PlayerBase::reset_stats()
{
  comm_stats.reset();
}

NamedCommStats Player::total_comm() const
{
  auto res = comm_stats;
  for (auto& x : thread_stats)
    res += x;
  return res;
}

template class MultiPlayer<int>;
template class MultiPlayer<ssl_socket*> ;
