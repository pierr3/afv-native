#include "afv-native/http/CurlPerform.h"
#include "afv-native/http/Request.h"
#include "afv-native/http/Response.h"
#include "http/loopback_server.h"
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <memory>
#include <thread>

using namespace afv_native::http;

namespace {
    void plainOk(Poco::Net::HTTPServerResponse &resp, const std::string &payload) {
        resp.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
        resp.setContentType("text/plain");
        resp.send() << payload;
    }
} // namespace

TEST_CASE("curlPerform returns a 200 body", "[curlPerform]") {
    afv_test::LoopbackServer server([](const std::string &, const std::string &, const std::string &,
                                       Poco::Net::HTTPServerResponse &resp) { plainOk(resp, "pong"); });

    Request  req(server.url("/ping"), Method::GET);
    Response resp = curlPerform(req, nullptr);

    REQUIRE(resp.ok);
    REQUIRE(resp.statusCode == 200);
    REQUIRE(resp.body == "pong");
    REQUIRE(resp.error.empty());
    REQUIRE_FALSE(resp.cancelled);
}

TEST_CASE("curlPerform reports a 404 as ok with the status code", "[curlPerform]") {
    afv_test::LoopbackServer server([](const std::string &, const std::string &, const std::string &,
                                       Poco::Net::HTTPServerResponse &resp) {
        resp.setStatus(Poco::Net::HTTPResponse::HTTP_NOT_FOUND);
        resp.send() << "nope";
    });

    Response resp = curlPerform(Request(server.url("/missing"), Method::GET), nullptr);

    REQUIRE(resp.ok);
    REQUIRE(resp.statusCode == 404);
}

TEST_CASE("curlPerform sends a POST body and headers", "[curlPerform]") {
    std::string             seenBody, seenMethod;
    afv_test::LoopbackServer server([&](const std::string &method, const std::string &,
                                        const std::string &body, Poco::Net::HTTPServerResponse &resp) {
        seenMethod = method;
        seenBody   = body;
        plainOk(resp, "ok");
    });

    Request req(server.url("/post"), Method::POST);
    req.setBody(nlohmann::json {{"user", "abc"}});
    Response resp = curlPerform(req, nullptr);

    REQUIRE(resp.ok);
    REQUIRE(seenMethod == "POST");
    REQUIRE(nlohmann::json::parse(seenBody)["user"] == "abc");
}

TEST_CASE("curlPerform reports a transfer timeout as a transport error", "[curlPerform]") {
    afv_test::LoopbackServer server([](const std::string &, const std::string &, const std::string &,
                                       Poco::Net::HTTPServerResponse &resp) {
        std::this_thread::sleep_for(std::chrono::seconds(3));
        plainOk(resp, "too late");
    });

    Request req(server.url("/slow"), Method::GET);
    req.setTimeouts(10, 1);
    Response resp = curlPerform(req, nullptr);

    REQUIRE_FALSE(resp.ok);
    REQUIRE_FALSE(resp.error.empty());
}

TEST_CASE("curlPerform honours a cancel flag raised mid-transfer", "[curlPerform]") {
    auto cancel = std::make_shared<std::atomic<bool>>(false);

    afv_test::LoopbackServer server([&](const std::string &, const std::string &, const std::string &,
                                        Poco::Net::HTTPServerResponse &resp) {
        cancel->store(true);
        std::this_thread::sleep_for(std::chrono::seconds(2));
        plainOk(resp, "unwanted");
    });

    Request req(server.url("/cancelme"), Method::GET);
    req.setTimeouts(10, 30);
    Response resp = curlPerform(req, cancel);

    REQUIRE(resp.cancelled);
    REQUIRE_FALSE(resp.ok);
}

TEST_CASE("curlPerform refuses a request whose cancel flag is already raised", "[curlPerform]") {
    auto cancel = std::make_shared<std::atomic<bool>>(true);

    Response resp = curlPerform(Request("http://127.0.0.1:1/never", Method::GET), cancel);

    REQUIRE(resp.cancelled);
    REQUIRE_FALSE(resp.ok);
}

TEST_CASE("Response::json parses a JSON body and tolerates garbage", "[curlPerform]") {
    Response good;
    good.body = R"({"a":1})";
    REQUIRE(good.json()["a"] == 1);

    Response bad;
    bad.body = "not json";
    REQUIRE(bad.json().is_discarded());

    Response empty;
    REQUIRE(empty.json().is_null());
}
