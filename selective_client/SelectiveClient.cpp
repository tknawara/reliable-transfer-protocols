/**
 *  File: SelectiveClient.cpp
 *  Description: Implementation for the selective repeat receiver
 *  Created at: 2017-12-8
 */

#include <socket_utils.h>
#include "SelectiveClient.h"

SelectiveClient::SelectiveClient(int server_socket, sockaddr_in &server_addr, double plp, unsigned int seed) {
    this->server_socket = server_socket;
    this->server_addr = server_addr;
    srand(seed);
    this->plp = plp;
}

void
SelectiveClient::request_file(std::string &filename) {
    socklen_t server_addr_size = sizeof(server_addr);

    send_request_file_packet(filename);

    std::ofstream output_stream;
    output_stream.open(filename);

    auto *header_packet = receive_header_packet();
    auto chunk_count = header_packet->seqno;
    this->window_size = header_packet->len;
    this->window = new PacketPtr[this->window_size];

    for (size_t i = 0; i < this->window_size; ++i) {
        this->window[i] = nullptr;
    }

    while (chunk_count > 0) {
        auto *data_packet = new utils::Packet();
        ssize_t recv_code = recvfrom(server_socket, data_packet, sizeof(*data_packet), 0,
                                    (struct sockaddr *) &server_addr, &server_addr_size);
        if (recv_code > 0) {
            send_ack(data_packet->seqno);
           // if (std::abs(data_packet->seqno - (double)recv_base) < window_size) {
                window[data_packet->seqno] = data_packet;
                std::cout << "[request_file]---Receiving new packet with seqno=" << data_packet->seqno << '\n';
                --chunk_count;
           // }
        }
	std::cout << "[request_file]---Chunk count=" << chunk_count << '\n';
        packet_clean_up(output_stream);
    }
    output_stream << std::flush;
}

void
SelectiveClient::send_request_file_packet(std::string &filename) {
    auto *packet = new utils::Packet();
    packet->seqno = 0;
    packet->len = filename.length();
    for (size_t i = 0; i < filename.length(); ++i) {
        packet->data[i] = filename[i];
    }
    sendto_wrapper(server_socket, packet, sizeof(packet) + sizeof(packet->data),
                   (struct sockaddr *) &server_addr, sizeof(server_addr));
    std::cout << "[send_request_file_packet]---Send request packet successfully" << '\n';
}

void
SelectiveClient::packet_clean_up(std::ofstream &output_stream) {
    std::cout << "[packet_clean_up]" << '\n';
    while (window[recv_base] != nullptr) {
        write_packet(output_stream, window[recv_base]);
        window[recv_base] = nullptr;
        recv_base = (recv_base + 1) % window_size;
    }
}

void
SelectiveClient::send_ack_with_prob(uint32_t ack_no) {
    if (should_send_packet()) {
        std::cout << "[send_ack_with_prob]--- Sending ack no=" << ack_no << '\n';
        send_ack(ack_no);
    }
}

void
SelectiveClient::send_ack(uint32_t ack_no) {
    auto *ack_packet = new utils::AckPacket();
    ack_packet->seqno = ack_no;
    utils::sendto_wrapper(server_socket, ack_packet, sizeof(ack_packet),
                          (struct sockaddr *) &server_addr, sizeof(server_addr));
    std::cout << "[send_ack]---Send ack packet with ackno=" << ack_no << '\n';
}

void
SelectiveClient::write_packet(std::ofstream &output_stream, utils::Packet *packet) {
    for (int i = 0; i < packet->len; ++i) {
        output_stream << packet->data[i];
    }
}

utils::Packet *
SelectiveClient::receive_header_packet() {
    auto *header_packet = new utils::Packet();
    socklen_t server_addr_size = sizeof(server_addr);
    while (true) {
        ssize_t recv_res = recvfrom(server_socket, header_packet, sizeof(*header_packet), 0,
                                    (struct sockaddr *) &server_addr, &server_addr_size);
        if (recv_res > 0) {
            return header_packet;
        }
    }
}

bool
SelectiveClient::should_send_packet() {
    if (this->plp <= 1e-5) {
        return true;
    }
    return ((double) rand() / RAND_MAX) > this->plp;
}
