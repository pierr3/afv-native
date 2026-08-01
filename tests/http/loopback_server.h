#pragma once

#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPRequestHandlerFactory.h>
#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/HTTPServerParams.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/Net/ServerSocket.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

namespace afv_test {

    using Handler = std::function<void(const std::string &method, const std::string &path,
                                       const std::string &body, Poco::Net::HTTPServerResponse &resp)>;

    class LoopbackHandler: public Poco::Net::HTTPRequestHandler {
      public:
        explicit LoopbackHandler(Handler h):
            mHandler(std::move(h)) {
        }

        void handleRequest(Poco::Net::HTTPServerRequest &req, Poco::Net::HTTPServerResponse &resp) override {
            std::ostringstream bodyBuf;
            bodyBuf << req.stream().rdbuf();
            mHandler(req.getMethod(), req.getURI(), bodyBuf.str(), resp);
        }

      private:
        Handler mHandler;
    };

    class LoopbackFactory: public Poco::Net::HTTPRequestHandlerFactory {
      public:
        explicit LoopbackFactory(Handler h):
            mHandler(std::move(h)) {
        }

        Poco::Net::HTTPRequestHandler *createRequestHandler(const Poco::Net::HTTPServerRequest &) override {
            return new LoopbackHandler(mHandler);
        }

      private:
        Handler mHandler;
    };

    /** A real HTTP/1.1 server on 127.0.0.1 with an OS-assigned port. */
    class LoopbackServer {
      public:
        explicit LoopbackServer(Handler h) {
            Poco::Net::ServerSocket socket(0); // port 0 => the OS picks a free one
            mPort        = socket.address().port();
            auto *params = new Poco::Net::HTTPServerParams();
            params->setMaxQueued(128);
            params->setMaxThreads(16);
            mServer = std::make_unique<Poco::Net::HTTPServer>(new LoopbackFactory(std::move(h)), socket, params);
            mServer->start();
        }

        ~LoopbackServer() {
            mServer->stopAll(true);
        }

        LoopbackServer(const LoopbackServer &)            = delete;
        LoopbackServer &operator=(const LoopbackServer &) = delete;

        uint16_t port() const {
            return mPort;
        }

        std::string url(const std::string &path) const {
            return "http://127.0.0.1:" + std::to_string(mPort) + path;
        }

      private:
        uint16_t                               mPort = 0;
        std::unique_ptr<Poco::Net::HTTPServer> mServer;
    };

} // namespace afv_test
