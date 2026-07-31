// Lab 8: custom client for the vendor's message board web service.
//
// The protocol was recovered by capturing the official app's traffic with
// Wireshark, filtering on "tcp port 80". The full request URI showed that the
// app sends a plain HTTP GET to:
//
//   http://api.thingspeak.com/update?api_key=<key>&field1=<user>&field2=<message>
//
// The secret key is assembled at run time inside the official binary, so it
// does not appear in the output of strings. It is still sent in clear text over
// HTTP, which is how it was recovered from the capture.
//
// Usage: ./lab8_client <username> <message>

#include <curl/curl.h>
#include <iostream>
#include <string>

// The secret key, read out of the captured request.
const std::string API_KEY = "ZKE95ZURWV7DW8B0";
const std::string UPDATE_URL = "http://api.thingspeak.com/update";

// Called by libcurl as the response arrives. The data is not null terminated,
// so the length has to be used when appending it to the response string.
size_t collect_response(void *buffer, size_t size, size_t count, void *userdata)
{
    size_t bytes = size * count;
    std::string *response = static_cast<std::string *>(userdata);
    response->append(static_cast<char *>(buffer), bytes);
    return bytes;
}

// Percent encodes a string so that spaces and other special characters can be
// carried safely inside a URL parameter.
std::string url_encode(CURL *curl, const std::string &text)
{
    char *encoded = curl_easy_escape(curl, text.c_str(), 0);
    if (!encoded) {
        return "";
    }

    std::string result(encoded);
    curl_free(encoded);
    return result;
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <username> <message>\n";
        std::cerr << "Wrap the message in quotes if it contains spaces.\n";
        return 1;
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Failed to initialise the curl library\n";
        return 1;
    }

    std::string url = UPDATE_URL + "?api_key=" + API_KEY +
                      "&field1=" + url_encode(curl, argv[1]) +
                      "&field2=" + url_encode(curl, argv[2]);

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, collect_response);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode result = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (result != CURLE_OK) {
        std::cerr << "Could not reach the server: " << curl_easy_strerror(result) << "\n";
        return 1;
    }

    // The service replies with the entry number that was created, or with "0"
    // if it refused the update. The most common reason for a refusal is posting
    // again before the rate limit has expired.
    if (response.empty() || response == "0") {
        std::cerr << "The server rejected the message. Wait a few seconds and try again.\n";
        return 1;
    }

    std::cout << "Message posted as entry " << response << "\n";
    return 0;
}
