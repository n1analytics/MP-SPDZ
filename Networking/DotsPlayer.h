#ifndef NETWORKING_DOTSPLAYER_H_
#define NETWORKING_DOTSPLAYER_H_

#include "Networking/Player.h"

class DotsPlayer : public MultiPlayer<int> {
  public:
    DotsPlayer(Names& names, string id, vector<int> socks);
    ~DotsPlayer() override;
};

#endif
