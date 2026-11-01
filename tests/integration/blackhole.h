#pragma once

#include <cstdint>
#include <vector>

namespace fulcrum::test {

// A port where TCP connects hang: they never succeed or fail.
//
// How: a listener with backlog 0 that never calls accept(). We fill its accept
// queue with a few connections; after that Linux silently drops new SYNs, so a
// client's connect() keeps retransmitting SYN until its own timeout. This
// imitates a dead host or a firewall that drops packets, without iptables.
class Blackhole {
 public:
  Blackhole();
  ~Blackhole();
  Blackhole(const Blackhole&) = delete;
  Blackhole& operator=(const Blackhole&) = delete;

  std::uint16_t port() const { return port_; }

 private:
  int listener_ = -1;
  std::vector<int> fillers_;
  std::uint16_t port_ = 0;
};

}  // namespace fulcrum::test
