#ifndef NETWORKING_DOTSPLAYER_H_
#define NETWORKING_DOTSPLAYER_H_

#include "Networking/Player.h"

class DotsPlayer : public Player {
  private:
    vector<int> sockets;

  public:
    DotsPlayer();
    ~DotsPlayer() override;

    virtual inline string get_id() const override;
    virtual int num_players() const override;
    virtual int my_num() const override;

    virtual void send_to_no_stats(int player,
            const octetStream& o) const override;
    virtual void receive_player_no_stats(int i, octetStream& o) const override;

    virtual size_t send_no_stats(int, const PlayerBuffer&, bool) const override;
    virtual size_t recv_no_stats(int, const PlayerBuffer&, bool) const override;

    virtual void exchange_no_stats(int other, const octetStream& to_send,
            octetStream& to_receive) const override;

    virtual void pass_around_no_stats(const octetStream& to_send,
            octetStream& to_receive, int offset) const override;

    virtual void Broadcast_Receive_no_stats(
            vector<octetStream>& o) const override;

    virtual void send_receive_all_no_stats(const vector<vector<bool>>& channels,
            const vector<octetStream>& to_send,
            vector<octetStream>& to_receive) const override;
};

#endif
