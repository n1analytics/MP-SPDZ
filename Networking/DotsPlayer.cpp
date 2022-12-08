#include "DotsPlayer.h"

DotsPlayer::DotsPlayer(Names& names, string id, vector<int> socks) :
    MultiPlayer<int>(names, id) {
    sockets = socks;
}

DotsPlayer::~DotsPlayer() {}
