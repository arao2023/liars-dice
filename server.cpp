#include <atomic>
#include <cstddef>
#include <string>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include "debug.hpp"

namespace beast = boost::beast;
namespace asio = boost::asio;

class Server {
    private:
        struct PlayerStream {
            beast::websocket::stream<beast::ssl_stream<
                    beast::tcp_stream>> socket;
            beast::flat_buffer message {};
        };

        asio::io_context ioContext {};
        asio::ip::tcp::acceptor tcpAcceptor {ioContext};
        asio::ip::tcp::endpoint tcpEndpoint {asio::ip::tcp::v4(), 443};
        asio::ssl::context sslContext {asio::ssl::context::tlsv12};
        std::atomic<bool> accepting {false};

        std::vector<PlayerStream> playerStreams;

    public:
        Server(const bool shouldStartAccepting = true) {
            sslContext.set_options(
                    asio::ssl::context::default_workarounds
                  | asio::ssl::context::no_sslv2
                  | asio::ssl::context::single_dh_use);
            sslContext.use_certificate_chain_file("assets/pem/cert.pem");
            sslContext.use_rsa_private_key_file(
                    "assets/pem/key.pem", 
                    asio::ssl::context::pem);
            sslContext.use_tmp_dh_file("assets/pem/dh.pem");

            tcpAcceptor.open(tcpEndpoint.protocol());
            tcpAcceptor.set_option(asio::socket_base::reuse_address(true));
            tcpAcceptor.bind(tcpEndpoint);
            tcpAcceptor.listen();
        }

        static void playerStreamOnHandshake(
                PlayerStream& playerStream,
                const boost::system::error_code& error) {
            Debug::log("handshake error, if any: %s\n", error.category().message(error.value()).c_str());
            playerStream.socket.set_option(
                    beast::websocket::stream_base::decorator(
                    [](beast::websocket::response_type& response) {
                        response.set(
                                beast::http::field::server,
                                "Liar's Dice Server");
                    }));
            playerStream.socket.async_accept(std::bind(
                    &Server::playerStreamOnAccept,
                    std::ref(playerStream),
                    std::placeholders::_1));
        }
        static void playerStreamOnAccept(
                PlayerStream& playerStream,
                const boost::system::error_code& error) {
            Debug::log("socket accept error, if any: %s\n", error.category().message(error.value()).c_str());
            {
                playerStream.socket.async_read(
                        playerStream.message,
                        std::bind(
                        &Server::playerStreamOnRead,
                        std::ref(playerStream),
                        std::placeholders::_1,
                        std::placeholders::_2));
            }
        }
        static void playerStreamOnRead(
                PlayerStream& playerStream,
                const boost::system::error_code& error,
                std::size_t transferSize) {
            Debug::log("socket read error, if any: %s\n", error.category().message(error.value()).c_str());
            Debug::log("got %d bytes, are text: %d\n", transferSize, playerStream.socket.got_text());
            reinterpret_cast<char*>(
                    playerStream.message.data().data())[
                            transferSize - 1] = '\0';
            Debug::log(
                    "received message: %s\n", 
                    playerStream.message.cdata().data());

            {
                std::string message {"hello, world!"};
                playerStream.socket.text(true);
                playerStream.socket.async_write(
                        asio::dynamic_buffer(message).data(),
                        std::bind(
                                &Server::playerStreamOnWrite,
                                std::ref(playerStream),
                                std::placeholders::_1,
                                std::placeholders::_2));
            }
        }
        static void playerStreamOnWrite(
                PlayerStream& playerStream,
                const boost::system::error_code& error,
                std::size_t transferSize) {
            playerStream.message.clear();
            Debug::log("socket write error, if any: %s\n", error.category().message(error.value()).c_str());
        }

        void serverOnAccept(
                const boost::system::error_code& error,
                asio::ip::tcp::socket socket) {
            Debug::log(
                    "%s%s%s%s", 
                    "accepting connection",
                    (error ? " with error message " : ""),
                    (error ? error.category().message(
                            error.value()).c_str() : ""),
                    "\n");
            playerStreams.push_back({.socket{std::move(socket), sslContext}});
            playerStreams.back().socket.next_layer().async_handshake(
                    asio::ssl::stream_base::server,
                    std::bind(
                            &Server::playerStreamOnHandshake,
                            std::ref(playerStreams.back()),
                            std::placeholders::_1));
            Debug::log("%s", "finished accepting connection\n");
            if (accepting) {
                Debug::log("%s", "queueing accept\n");
                tcpAcceptor.async_accept(ioContext, std::bind(
                        &Server::serverOnAccept,
                        this,
                        std::placeholders::_1,
                        std::placeholders::_2));
            }
        }

        void startAccepting() {
            ioContext.stop();

            accepting = true;
            Debug::log("%s", "queueing accept\n");
            tcpAcceptor.async_accept(ioContext, std::bind(
                    &Server::serverOnAccept,
                    this,
                    std::placeholders::_1,
                    std::placeholders::_2));

            ioContext.restart();
            ioContext.run();
        }
        void stopAccepting() {
            accepting = false;
        }
};

int main(int argc, char* argv[]) {
    Server server {};
    server.startAccepting();
    return 0;
}
