#include "crypto.hpp"
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

std::string utils::crypto::base32_encode(const std::vector<uint8_t> data) {
    std::cout << "Data size: " << data.size() << std::endl;

    static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    std::string out;
    out.reserve((data.size() * 8 + 4) / 5);

    uint32_t buffer = 0;
    int bits_left = 0;

    for (size_t i = 0; i < data.size(); ++i) {
        buffer = (buffer << 8) | data[i];
        bits_left += 8;

        while (bits_left >= 5) {
            out.push_back(alphabet[(buffer >> (bits_left - 5)) & 0x1F]);
            bits_left -= 5;
        }
    }

    if (bits_left > 0) {
        out.push_back(alphabet[(buffer << (5 - bits_left)) & 0x1F]);
    }

    return out;
}
