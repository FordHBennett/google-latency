#include <iostream>
#include <ctime>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/ip_icmp.h>
#include <sys/socket.h>

const int PACKET_SIZE = 64;
const int MAX_TRIES = 4;  // Number of ping attempts

// Function to calculate the checksum of a packet
uint16_t calculateChecksum(uint16_t* buffer, int length) {
    uint32_t sum = 0;
    for (int i = 0; i < length; ++i)
    {
        sum += buffer[i];
    }
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    return static_cast<uint16_t>(~sum);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <IP address>" << std::endl;
        return 1;
    }

    const char* targetIP = argv[1];
    struct sockaddr_in targetAddress;

    // Create a raw socket for ICMP
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock < 0) {
        perror("Socket creation failed");
        return 1;
    }
    else
    {
        std::cout << "Socket created successfully" << std::endl;
    }

    targetAddress.sin_family = AF_INET;
    if (inet_pton(AF_INET, targetIP, &(targetAddress.sin_addr)) == 0) {
        std::cerr << "Invalid IP address: " << targetIP << std::endl;
        return 1;
    }
    else
    {
        std::cout << "IP address " << targetIP << " is valid" << std::endl;
    }

    // Prepare the ICMP packet
    char packet[PACKET_SIZE];
    struct icmp* icmpPacket = reinterpret_cast<struct icmp*>(packet);
    icmpPacket->icmp_type = ICMP_ECHO;  // Echo request
    icmpPacket->icmp_code = 0;
    icmpPacket->icmp_id = getpid();
    icmpPacket->icmp_seq = 0;
    memset(icmpPacket->icmp_data, 0xa5, PACKET_SIZE - sizeof(struct icmp));
    icmpPacket->icmp_cksum = 0;
    icmpPacket->icmp_cksum = calculateChecksum(reinterpret_cast<uint16_t*>(icmpPacket), PACKET_SIZE);

    // Send ICMP echo requests and measure response time
    double elapsed_time = 0.0;
    for (int tries = 0; tries < MAX_TRIES; ++tries) {
        // Record the start time
        std::clock_t start_time = std::clock();

        // Send the ICMP packet
        ssize_t sent_bytes = sendto(sock, packet, PACKET_SIZE, 0, reinterpret_cast<struct sockaddr*>(&targetAddress), sizeof(targetAddress));
        if (sent_bytes < 0) {
            perror("Error sending ICMP packet");
            close(sock);
            return 1;
        }
        else
        {
            std::cout << "ICMP packet sent successfully" << std::endl;
        }

        // Receive the ICMP reply
        char recv_packet[PACKET_SIZE];
        struct sockaddr_in senderAddress;
        socklen_t senderAddressLen = sizeof(senderAddress);
        ssize_t recv_bytes = recvfrom(sock, recv_packet, PACKET_SIZE, 0, reinterpret_cast<struct sockaddr*>(&senderAddress), &senderAddressLen);
        if (recv_bytes < 0) {
            perror("Error receiving ICMP reply");
            close(sock);
            return 1;
        }
        else
        {
            std::cout << "ICMP reply received successfully" << std::endl;
            std::cout << "Received " << recv_bytes << " bytes" << std::endl;
        }

        // Calculate and print the round-trip time
        if (static_cast<ssize_t>(recv_bytes) >= static_cast<ssize_t>(sizeof(struct ip) + sizeof(struct icmp)))
        {
            std::clock_t end_time = std::clock();
            double current_ping = static_cast<double>(end_time - start_time) / CLOCKS_PER_SEC * 1000.0;  // Convert to milliseconds
            std::cout << "Received ICMP reply from " << inet_ntoa(senderAddress.sin_addr) << ": time=" << current_ping << "ms" << std::endl;
            elapsed_time = (elapsed_time + current_ping) / (tries + 1);
            std::cout << "Running average is " << elapsed_time << "ms" << std::endl;
        }
        else
        {
            std::cerr << "No reply received." << std::endl;
        }
        sleep(10);  // Wait before sending the next packet
    }

    close(sock);
    std::cout << "Average round-trip time:" << elapsed_time << "ms" << std::endl;
    return 0;
}
