#include "DotsPlayer.h"
#include <cstring>
#include <condition_variable>
#include <mutex>
#include <string>
#include <unordered_map>
#include <sys/socket.h>
#include <unistd.h>
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
            /* Set up send-to-self socket with socketpair() and /bin/cat. */
            int pair[2];
            if (socketpair(AF_UNIX, SOCK_STREAM, 0, pair)) {
                throw runtime_error("Error opening socketpair");
            }

            pid_t child = fork();
            if (child == -1) {
                throw runtime_error("fork");
            } else if (child == 0) {
                /* Child. Move pair[1] to stdin/stdout and execv /bin/cat. */
                if (dup2(pair[1], STDIN_FILENO) == -1) {
                    abort();
                }
                if (dup2(pair[1], STDOUT_FILENO) == -1) {
                    abort();
                }
                close(pair[0]);
                close(pair[1]);
                char *argv[] = { (char *) "/bin/cat", NULL };
                if (execv(argv[0], argv)) {
                    abort();
                }
            } else {
                close(pair[1]);
                sockets[i] = pair[0];
            }
        } else {
            /* Request a socket from DoTS. */
            int socket = dots_open_socket(i);
            if (socket < 0) {
                throw runtime_error("Failed to open subsequent DoTS socket");
            }

            if (i > dots_world_rank) {
                /* For greater ranks, we decide which ID this socket belongs to,
                 * so we send the ID to the other side to notify it of the
                 * owner. */
                char purpose[PURPOSE_LEN];
                strncpy(purpose, id.c_str(), PURPOSE_LEN - 1);
                purpose[PURPOSE_LEN - 1] = '\0';
                send(socket, (octet*) purpose, PURPOSE_LEN);
                sockets[i] = socket;

#ifdef DEBUG_NETWORKING
                cout << "Claiming socket for " << id << endl;
#endif
            } else {
                /* For lesser ranks, get the purpose by reading from the
                 * socket. */
                char purpose[PURPOSE_LEN];
                receive(socket, (octet*) purpose, PURPOSE_LEN);

                string purpose_str(purpose);
                if (purpose_str == id) {
                    /* If the purpose is correct, use the socket. */
                    sockets[i] = socket;
#ifdef DEBUG_NETWORKING
                    cout << "Received socket for " << id << ", which matches " << id << endl;
#endif
                } else {
#ifdef DEBUG_NETWORKING
                    cout << "Received socket for " << id << ", but I wanted " << id << endl;
#endif

                    /* Put the socket in the receivedSockets map, signal that it
                     * it's been filled, and sleep until someone else picks up
                     * our socket. */
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

#ifdef VERBOSE_COMM
    cout << id << ": Sending " << o.get_length() << " bytes to " << player << endl;
#endif

    send(sockets[player], lenbuf, sizeof(lenbuf));
    send(sockets[player], o.get_data(), o.get_length());

#ifdef VERBOSE_COMM
    cout << id << ": Sent " << o.get_length() << " bytes to " << player << endl;
#endif
}

void DotsPlayer::receive_player_no_stats(int player, octetStream& o) const {
    octet lenbuf[LENGTH_SIZE];

#ifdef VERBOSE_COMM
    cout << id << ": Receiving from " << player << endl;
#endif

    receive(sockets[player], lenbuf, LENGTH_SIZE);

    size_t len = decode_length(lenbuf, LENGTH_SIZE);
    o.reset_write_head();
    o.resize_min(len);
    octet *ptr = o.append(len);

    receive(sockets[player], ptr, len);

#ifdef VERBOSE_COMM
    cout << id << ": Received " << o.get_length() << " bytes from " << player << endl;
#endif
}

size_t DotsPlayer::send_no_stats(int player, const PlayerBuffer& buffer,
        bool block __attribute__((unused))) const {

#ifdef VERBOSE_COMM
    cout << id << ": [Player buffer] Sending " << buffer.size << " bytes to " << player << endl;
#endif
    send(sockets[player], buffer.data, buffer.size);

#ifdef VERBOSE_COMM
    cout << id << ": [Player buffer] Sent " << buffer.size << " bytes to " << player << endl;
#endif

    return buffer.size;
}

size_t DotsPlayer::recv_no_stats(int player, const PlayerBuffer& buffer,
        bool block __attribute__((unused))) const {

#ifdef VERBOSE_COMM
    cout << id << ": [Player buffer] Receiving " << buffer.size << " bytes from " << player << endl;
#endif

    receive(sockets[player], buffer.data, buffer.size);

#ifdef VERBOSE_COMM
    cout << id << ": [Player buffer] Received " << buffer.size << " bytes from " << player << endl;
#endif

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
