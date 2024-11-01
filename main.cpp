//
// Created by Phillip Romig on 7/16/24.
//
#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <system_error>
#include <unistd.h>
#include <array>

#include "timerC.h"
#include "unreliableTransport.h"
#include "logging.h"


#define WINDOW_SIZE 10
int main(int argc, char* argv[]) {

    // Defaults
    // Default values
    std::string hostname;
    uint16_t portNum;
    std::string inputFilename;
    int LOG_LEVEL = 3;
    timerC timer(1000);  // 1-second timer
    timer.setDuration(1000);

    // Parse command-line arguments
    int opt;
    while ((opt = getopt(argc, argv, "h:p:f:d:")) != -1) {
        switch (opt) {
            case 'h': hostname = optarg; break;
            case 'p': portNum = std::stoi(optarg); break;
            case 'f': inputFilename = optarg; break;
            case 'd': LOG_LEVEL = std::stoi(optarg); break;
            default:
                std::cerr << "Usage: " << argv[0] << " -h <hostname> -p <port> -f <filename> [-d <debug level>]\n";
                return 1;
        }
    }

    if (hostname.empty() || portNum == 0 || inputFilename.empty()) {
        std::cerr << "Hostname, port, and filename are required.\n";
        return 1;
    }

    // Initialize unreliable transport connection
    unreliableTransportC transport(hostname, portNum);

    // Open input file for reading
    std::ifstream inputFile(inputFilename, std::ios::binary);
    if (!inputFile.is_open()) {
        FATAL << "Unable to open file: " << inputFilename << ENDL;
        return 1;
    }

    uint16_t base = 1, nextSeqNum = 1;
    bool allSent = false, allAcked = false;
    std::array<datagramS, WINDOW_SIZE> sndpkt;  // Buffer for sent packets

    while (!allSent || !allAcked) {
        // Send packets within window
        while (nextSeqNum < base + WINDOW_SIZE && !allSent) {
            char data[MAX_PAYLOAD_LENGTH] = {};
            inputFile.read(data, MAX_PAYLOAD_LENGTH);
            int bytesRead = inputFile.gcount();

            if (bytesRead > 0) {
                datagramS pkt;
                pkt.seqNum = nextSeqNum;
                pkt.payloadLength = bytesRead;
                std::copy(data, data + bytesRead, pkt.data);
                pkt.checksum = computeChecksum(pkt);

                transport.udt_send(pkt);
                sndpkt[nextSeqNum % WINDOW_SIZE] = pkt;  // Store packet in buffer
                INFO << "Packet sent with seqNum: " << nextSeqNum << ENDL;

                if (base == nextSeqNum) {
                    timer.start();  // Start timer for the first unacknowledged packet
                }

                nextSeqNum++;
            } else {
                allSent = true;  // No more data to send
                break;
            }
        }

        // Receive acknowledgments
        datagramS ackPkt;
        while (transport.udt_receive(ackPkt) > 0) {
            if (validateChecksum(ackPkt)) {
                uint16_t ackNum = ackPkt.ackNum;
                if (ackNum >= base) {
                    base = ackNum + 1;
                    INFO << "Acknowledgment received for packet seqNum: " << ackNum << ENDL;

                    if (base == nextSeqNum) {
                        timer.stop();  // All packets acknowledged
                        if (allSent) allAcked = true;
                    } else {
                        timer.start();  // Restart timer for unacknowledged packets
                    }
                }
            }
        }

        // Handle timeout (retransmit unacknowledged packets)
        if (timer.timeout()) {
            INFO << "Timeout occurred; resending all unacknowledged packets." << ENDL;
            timer.start();

            for (uint16_t i = base; i < nextSeqNum; ++i) {
                transport.udt_send(sndpkt[i % WINDOW_SIZE]);
                INFO << "Resent packet with seqNum: " << i << ENDL;
            }
        }
    }

    // Send an empty packet to indicate the end of the file transfer
    datagramS endPkt;
    endPkt.seqNum = nextSeqNum;
    endPkt.payloadLength = 0;
    endPkt.checksum = computeChecksum(endPkt);
    transport.udt_send(endPkt);

    // Clean up and close the file and network connection
    inputFile.close();
    INFO << "File transfer complete. Closing connection." << ENDL;
    return 0;
}
