#ifndef NETWORKING_DOTSPLAYER_H_
#define NETWORKING_DOTSPLAYER_H_

#include "Networking/Player.h"

class DotsPlayer : public Player {
  private:
    static bool envIsInitted;
    bool isFirstPlayer;
    vector<int> nonFirstSockets;

  public:
    DotsPlayer();
    ~DotsPlayer() override;

    virtual int num_players() const override;
    virtual int my_num() const override;

    virtual void send_to_no_stats(int player,
            const octetStream& o) const override;
    virtual void receive_player_no_stats(int i, octetStream& o) const override;

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
