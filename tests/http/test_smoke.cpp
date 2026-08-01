#include "http/loopback_server.h"
#include <catch2/catch_test_macros.hpp>
#include <curl/curl.h>
#include <string>

namespace {
    size_t collect(char *ptr, size_t size, size_t nmemb, void *userdata) {
        static_cast<std::string *>(userdata)->append(ptr, size * nmemb);
        return size * nmemb;
    }
} // namespace

TEST_CASE("loopback server answers a plain GET", "[smoke]") {
    afv_test::LoopbackServer server([](const std::string &method, const std::string &path,
                                       const std::string &, Poco::Net::HTTPServerResponse &resp) {
        REQUIRE(method == "GET");
        REQUIRE(path == "/hello");
        resp.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
        resp.setContentType("text/plain");
        resp.send() << "world";
    });

    std::string body;
    CURL       *h = curl_easy_init();
    REQUIRE(h != nullptr);
    curl_easy_setopt(h, CURLOPT_URL, server.url("/hello").c_str());
    curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, &collect);
    curl_easy_setopt(h, CURLOPT_WRITEDATA, &body);
    REQUIRE(curl_easy_perform(h) == CURLE_OK);
    curl_easy_cleanup(h);

    REQUIRE(body == "world");
}
