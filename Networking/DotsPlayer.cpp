#include "DotsPlayer.h"
#include "Networking/Player.h"
#include <dots.h>

bool DotsPlayer::envIsInitted = false;

DotsPlayer::DotsPlayer() : Player(Names(dots_world_rank, dots_world_size)) {
    if (envIsInitted) {
        envIsInitted = true;
        isFirstPlayer = true;
    } else {
        isFirstPlayer = false;
        nonFirstSockets.resize(dots_world_size);
        for (size_t i = 0; i < dots_world_size; i++) {
            if (i == dots_world_rank) {
                continue;
            }
            int socket = dots_open_socket(i);
            if (socket < 0) {
                throw runtime_error("Failed to open subsequent DoTS socket");
            }
            nonFirstSockets[i] = socket;
        }
    }
}

string DotsPlayer::get_id() const {
    return to_string(my_num());
}

int DotsPlayer::num_players() const {
    return dots_world_size;
}

int DotsPlayer::my_num() const {
    return dots_world_rank;
}

void DotsPlayer::send_to_no_stats(int player, const octetStream& o) const {
    octet lenbuf[LENGTH_SIZE];
    encode_length(lenbuf, o.get_length(), LENGTH_SIZE);

    if (isFirstPlayer) {
        if (dots_msg_send(lenbuf, sizeof(lenbuf), (size_t) player)) {
            throw runtime_error("Error in dots_msg_send");
        }
        if (dots_msg_send(o.get_data(), o.get_length(), (size_t) player)) {
            throw runtime_error("Error in dots_msg_send");
        }
    } else {
        send(nonFirstSockets[player], lenbuf, sizeof(lenbuf));
        send(nonFirstSockets[player], o.get_data(), o.get_length());
    }
}

void DotsPlayer::receive_player_no_stats(int player, octetStream& o) const {
    octet lenbuf[LENGTH_SIZE];

    if (isFirstPlayer) {
        if (dots_msg_recv(lenbuf, sizeof(lenbuf), (size_t) player, NULL)) {
            throw runtime_error("Error in dots_msg_recv");
        }
    } else {
        receive(nonFirstSockets[player], lenbuf, LENGTH_SIZE);
    }

    size_t len = decode_length(lenbuf, LENGTH_SIZE);
    o.reset_write_head();
    o.resize_min(len);
    octet *ptr = o.append(len);

    if (isFirstPlayer) {
        if (dots_msg_recv(ptr, len, (size_t) player, NULL)) {
            throw runtime_error("Error in dots_msg_send");
        }
    } else {
        receive(nonFirstSockets[player], ptr, len);
    }
}

size_t DotsPlayer::send_no_stats(int player, const PlayerBuffer& buffer,
        bool block __attribute__((unused))) const {
    if (isFirstPlayer) {
        if (dots_msg_send(buffer.data, buffer.size, (size_t) player)) {
            throw runtime_error("Error in dots_msg_send");
        }
    } else {
        send(nonFirstSockets[player], buffer.data, buffer.size);
    }
    return buffer.size;
}

size_t DotsPlayer::recv_no_stats(int player, const PlayerBuffer& buffer,
        bool block __attribute__((unused))) const {
    if (isFirstPlayer) {
        if (dots_msg_recv(buffer.data, buffer.size, (size_t) player, NULL)) {
            throw runtime_error("Error in dots_msg_recv");
        }
    } else {
        receive(nonFirstSockets[player], buffer.data, buffer.size);
    }
    return buffer.size;
}

void DotsPlayer::exchange_no_stats(int other, const octetStream& to_send,
        octetStream& to_receive) const {
    send_to_no_stats(other, to_send);
    receive_player_no_stats(other, to_receive);
}

void DotsPlayer::pass_around_no_stats(const octetStream& to_send,
      octetStream& to_receive, int offset) const {
    send_to_no_stats(get_player(offset), to_send);
    receive_player_no_stats(get_player(-offset), to_receive);
}

void DotsPlayer::Broadcast_Receive_no_stats(vector<octetStream>& o) const {
    for (int i = 0; i < num_players(); i++) {
        if (i == my_num()) {
            continue;
        }
        send_to_no_stats(i, o[my_num()]);
    }
    for (int i = 0; i < num_players(); i++) {
        if (i == my_num()) {
            continue;
        }
        receive_player_no_stats(i, o[i]);
    }
}

void DotsPlayer::send_receive_all_no_stats(const vector<vector<bool>>& channels,
        const vector<octetStream>& to_send,
        vector<octetStream>& to_receive) const {
    for (int i = 0; i < num_players(); i++) {
        if (i == my_num()) {
            continue;
        }
        if (channels[my_num()][i]) {
            send_to_no_stats(i, to_send[i]);
        }
    }
    for (int i = 0; i < num_players(); i++) {
        if (i == my_num()) {
            continue;
        }
        to_receive.resize(num_players());
        if (channels[i][my_num()]) {
            receive_player_no_stats(i, to_receive[i]);
        }
    }
}

DotsPlayer::~DotsPlayer() {}
