#include "DotsPlayer.h"
#include <cstring>
#include <condition_variable>
#include <mutex>
#include <string>
#include <unordered_map>
#include <dots.h>
#include "Networking/data.h"
#include "Networking/Player.h"
#include "Networking/sockets.h"

constexpr size_t PURPOSE_LEN = 128;

mutex DotsPlayer::receivedSocketsLock;
unordered_map<string, int> DotsPlayer::receivedSockets;
condition_variable DotsPlayer::socketReceived;

DotsPlayer::DotsPlayer(const string& id) :
        Player(Names(dots_world_rank, dots_world_size)), id(id) {
    sockets.resize(dots_world_size);

    /* Loop through and claim a socket for each other node. */
    unique_lock<mutex> lock(receivedSocketsLock);
    for (size_t i = 0; i < dots_world_size; i++) {
        if (i == dots_world_rank) {
            continue;
        }

        /* Request a socket from DoTS. */
        int socket = dots_open_socket(i);
        if (socket < 0) {
            throw runtime_error("Failed to open subsequent DoTS socket");
        }

        if (i > dots_world_rank) {
            /* For greater ranks, we decide which ID this socket belongs to, so
             * we send the ID to the other side to notify it of the owner. */
            char purpose[PURPOSE_LEN];
            memset(purpose, '\0', PURPOSE_LEN);
            strncpy(purpose, id.c_str(), PURPOSE_LEN);
            send(socket, (octet*) purpose, PURPOSE_LEN);
            sockets[i] = socket;

            cout << "Claiming socket for " << id << endl;
        } else {
            /* For lesser ranks, get the purpose by reading from the socket. */
            char purpose[PURPOSE_LEN];
            receive(socket, (octet*) purpose, PURPOSE_LEN);

            string purpose_str(purpose);
            if (purpose_str == id) {
                /* If the purpose is correct, use the socket. */
                cout << "Received socket for " << id << ", which matches " << id << endl;
                sockets[i] = socket;
            } else {
                cout << "Received socket for " << id << ", but I wanted " << id << endl;
                /* Put the socket in the receivedSockets map, signal that it
                 * it's been filled, and sleep until someone else picks up our
                 * socket. */
                int foundSocket = -1;
                while (foundSocket != -1) {
                    receivedSockets[purpose_str] = socket;
                    socketReceived.notify_all();
                    socketReceived.wait(lock);

                    /* Search the map for our socket. */
                    for (auto& it : receivedSockets) {
                        if (it.first == id) {
                            foundSocket = it.second;
                            break;
                        }
                    }
                }
                sockets[i] = foundSocket;
                receivedSockets.erase(id);
            }
        }
    }
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

    send(sockets[player], lenbuf, sizeof(lenbuf));
    send(sockets[player], o.get_data(), o.get_length());
}

void DotsPlayer::receive_player_no_stats(int player, octetStream& o) const {
    octet lenbuf[LENGTH_SIZE];

    receive(sockets[player], lenbuf, LENGTH_SIZE);

    size_t len = decode_length(lenbuf, LENGTH_SIZE);
    o.reset_write_head();
    o.resize_min(len);
    octet *ptr = o.append(len);

    receive(sockets[player], ptr, len);
}

size_t DotsPlayer::send_no_stats(int player, const PlayerBuffer& buffer,
        bool block __attribute__((unused))) const {
    send(sockets[player], buffer.data, buffer.size);
    return buffer.size;
}

size_t DotsPlayer::recv_no_stats(int player, const PlayerBuffer& buffer,
        bool block __attribute__((unused))) const {
    receive(sockets[player], buffer.data, buffer.size);
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
