// relay() over real loopback TCP sockets, without the fulcrum binary.
//
//   client_app <-> [client_proxy  relay  backend_proxy] <-> backend_app
#include "proxy/connection.h"

#include <gtest/gtest.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <future>
#include <string>
#include <thread>

#include "core/listener.h"
#include "core/socket.h"
#include "core/socket_util.h"

using namespace std::chrono_literals;
using fulcrum::Fd;

namespace {

// Returns two connected TCP sockets {connector, acceptor} on 127.0.0.1.
std::pair<Fd, Fd> tcp_pair() {
  std::string error;
  Fd listener = fulcrum::listen_tcp({"127.0.0.1", 0}, 1, error);
  EXPECT_TRUE(listener.valid()) << error;
  Fd out = fulcrum::connect_tcp({"127.0.0.1", fulcrum::local_port(listener)}, 1s, error);
  EXPECT_TRUE(out.valid()) << error;
  Fd in(::accept4(listener.get(), nullptr, nullptr, SOCK_CLOEXEC));
  EXPECT_TRUE(in.valid());
  return {std::move(out), std::move(in)};
}

void send_str(const Fd& fd, const std::string& s) {
  ASSERT_EQ(::send(fd.get(), s.data(), s.size(), MSG_NOSIGNAL), static_cast<ssize_t>(s.size()));
}

// Reads exactly n bytes (or until EOF/error).
std::string recv_n(const Fd& fd, std::size_t n) {
  std::string out;
  char buf[4096];
  while (out.size() < n) {
    const ssize_t r = ::recv(fd.get(), buf, std::min(sizeof(buf), n - out.size()), 0);
    if (r <= 0) break;
    out.append(buf, static_cast<std::size_t>(r));
  }
  return out;
}

// Reads until EOF; returns everything read. Error counts as end too.
std::string recv_all(const Fd& fd) {
  std::string out;
  char buf[4096];
  for (;;) {
    const ssize_t r = ::recv(fd.get(), buf, sizeof(buf), 0);
    if (r <= 0) break;
    out.append(buf, static_cast<std::size_t>(r));
  }
  return out;
}

struct Harness {
  Fd client_app, backend_app;
  std::future<fulcrum::ConnectionResult> relay;

  Harness() {
    auto [c_app, c_proxy] = tcp_pair();
    auto [b_proxy, b_app] = tcp_pair();
    client_app = std::move(c_app);
    backend_app = std::move(b_app);
    relay = std::async(std::launch::async, fulcrum::relay, std::move(c_proxy), std::move(b_proxy));
  }
};

}  // namespace

TEST(Relay, CopiesBothDirections) {
  Harness h;
  send_str(h.client_app, "ping");
  EXPECT_EQ(recv_n(h.backend_app, 4), "ping");
  send_str(h.backend_app, "pong");
  EXPECT_EQ(recv_n(h.client_app, 4), "pong");

  h.client_app.reset();
  h.backend_app.reset();
  ASSERT_EQ(h.relay.wait_for(5s), std::future_status::ready);
  const auto r = h.relay.get();
  EXPECT_EQ(r.upstream.bytes, 4u);
  EXPECT_EQ(r.downstream.bytes, 4u);
}

TEST(Relay, HalfCloseStillDeliversTheResponse) {
  Harness h;
  // Client sends its request and closes its write side.
  send_str(h.client_app, "request");
  ASSERT_EQ(::shutdown(h.client_app.get(), SHUT_WR), 0);

  // Backend sees the request, then EOF (the half-close was passed on).
  EXPECT_EQ(recv_all(h.backend_app), "request");

  // Backend still answers (1 MiB, so the relay has to loop), then closes.
  const std::string response(1024 * 1024, 'r');
  std::thread backend([&] {
    send_str(h.backend_app, response);
    h.backend_app.reset();
  });
  EXPECT_EQ(recv_all(h.client_app), response) << "client lost part of the response";
  backend.join();

  ASSERT_EQ(h.relay.wait_for(5s), std::future_status::ready);
  const auto r = h.relay.get();
  EXPECT_EQ(r.upstream.err, 0);
  EXPECT_EQ(r.downstream.err, 0);
  EXPECT_EQ(r.downstream.bytes, response.size());
}

TEST(Relay, BackendResetEndsTheConnection) {
  Harness h;
  send_str(h.client_app, "hello");
  EXPECT_EQ(recv_n(h.backend_app, 5), "hello");

  // SO_LINGER with a zero timeout makes close() send RST instead of FIN.
  const linger lg{1, 0};
  ASSERT_EQ(::setsockopt(h.backend_app.get(), SOL_SOCKET, SO_LINGER, &lg, sizeof(lg)), 0);
  h.backend_app.reset();

  // The client sees the connection end (EOF or error), not a hang...
  EXPECT_EQ(recv_all(h.client_app), "");
  // ...and relay() returns even though the client never closed its side.
  ASSERT_EQ(h.relay.wait_for(5s), std::future_status::ready);
  EXPECT_EQ(h.relay.get().downstream.err, ECONNRESET);
}
