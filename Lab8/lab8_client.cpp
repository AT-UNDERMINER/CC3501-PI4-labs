// Lab 8 - Custom client to post messages to the vendor's web service
//
// Usage: ./lab8_client <username> <message>
//
// Reverse-engineered from Wireshark capture of the official app:
//   Host: api.thingspeak.com
//   GET /update?api_key=<KEY>&field1=<username>&field2=<message>

#include <curl/curl.h>
#include <iostream>
#include <string>

// Secret key recovered from the captured HTTP request.
#define API_KEY "ZKE95ZURWV7DW8B0"

int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <username> <message>\n";
        return 1;
    }

    std::string username = argv[1];
    std::string message = argv[2];

    CURL *curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Error: curl_easy_init() failed\n";
        return 1;
    }

    // URL-encode the username and message so spaces/special characters
    // survive being placed into the query string.
    char *encoded_username = curl_easy_escape(curl, username.c_str(), 0);
    char *encoded_message = curl_easy_escape(curl, message.c_str(), 0);

    if (!encoded_username || !encoded_message) {
        std::cerr << "Error: curl_easy_escape() failed\n";
        curl_easy_cleanup(curl);
        return 1;
    }

    // Build the full request URL, matching the format seen in Wireshark.
    std::string url = "http://api.thingspeak.com/update?api_key=" API_KEY
                       "&field1=" + std::string(encoded_username) +
                       "&field2=" + std::string(encoded_message);

    curl_free(encoded_username);
    curl_free(encoded_message);

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

    CURLcode result = curl_easy_perform(curl);

    if (result != CURLE_OK) {
        std::cerr << "Error: curl_easy_perform() failed: "
                   << curl_easy_strerror(result) << "\n";
        curl_easy_cleanup(curl);
        return 1;
    }

    std::cout << "Message posted successfully.\n";

    curl_easy_cleanup(curl);
    return 0;
}
