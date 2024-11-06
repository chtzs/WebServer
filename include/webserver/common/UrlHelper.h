//
// Created by Haotian on 2024/10/17.
//

#ifndef URL_DECODER_H
#define URL_DECODER_H
#include <string>
#include <sstream>

class UrlHelper {
public:
    static std::string decode(const std::string &encoded_url) {
        std::string decoded_url;
        std::istringstream stream(encoded_url);
        char ch;

        while (stream.get(ch)) {
            if (ch == '%') {
                // If current is %, read two next hex number.
                char hex1, hex2;
                stream.get(hex1);
                stream.get(hex2);
                const int value = std::stoi(std::string(1, hex1) + std::string(1, hex2), nullptr, 16);
                ch = static_cast<char>(value);
            }
            decoded_url += ch;
        }

        return decoded_url;
    }
};

#endif //URL_DECODER_H
