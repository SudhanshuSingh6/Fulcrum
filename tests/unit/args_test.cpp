#include "control/args.h"

#include <gtest/gtest.h>

#include <initializer_list>
#include <vector>

namespace {

fulcrum::ParseResult parse(std::initializer_list<const char*> args) {
  std::vector<const char*> argv{"fulcrum"};
  argv.insert(argv.end(), args);
  return fulcrum::parse_args(static_cast<int>(argv.size()), argv.data());
}

using Kind = fulcrum::ParseResult::Kind;

}  // namespace

TEST(Args, ValidListenAndBackend) {
  const auto r = parse({"--listen", "127.0.0.1:8080", "--backend", "10.0.0.1:9001"});
  ASSERT_EQ(r.kind, Kind::kRun) << r.error;
  EXPECT_EQ(r.args.listen, (fulcrum::HostPort{"127.0.0.1", 8080}));
  ASSERT_EQ(r.args.backends.size(), 1u);
  EXPECT_EQ(r.args.backends[0], (fulcrum::HostPort{"10.0.0.1", 9001}));
}

TEST(Args, SeveralBackendsKeepOrder) {
  const auto r = parse({"--backend", "127.0.0.1:1", "--listen", "0.0.0.0:0", "--backend",
                        "127.0.0.1:2", "--backend", "127.0.0.1:3"});
  ASSERT_EQ(r.kind, Kind::kRun) << r.error;
  ASSERT_EQ(r.args.backends.size(), 3u);
  EXPECT_EQ(r.args.backends[2].port, 3);
  EXPECT_EQ(r.args.listen.port, 0);  // port 0 is allowed for --listen
}

TEST(Args, MissingListen) {
  const auto r = parse({"--backend", "127.0.0.1:9001"});
  EXPECT_EQ(r.kind, Kind::kError);
  EXPECT_NE(r.error.find("--listen"), std::string::npos);
}

TEST(Args, MissingBackend) {
  const auto r = parse({"--listen", "127.0.0.1:8080"});
  EXPECT_EQ(r.kind, Kind::kError);
  EXPECT_NE(r.error.find("--backend"), std::string::npos);
}

TEST(Args, BadPorts) {
  for (const char* bad : {"127.0.0.1:0", "127.0.0.1:70000", "127.0.0.1:abc",
                          "127.0.0.1:", "127.0.0.1:80x", "127.0.0.1:-1"}) {
    const auto r = parse({"--listen", "127.0.0.1:8080", "--backend", bad});
    EXPECT_EQ(r.kind, Kind::kError) << bad;
  }
}

TEST(Args, BadHosts) {
  for (const char* bad : {"9001", "localhost:9001", "1.2.3:9001", ":9001", "256.0.0.1:9001"}) {
    const auto r = parse({"--listen", "127.0.0.1:8080", "--backend", bad});
    EXPECT_EQ(r.kind, Kind::kError) << bad;
  }
}

TEST(Args, MissingValue) {
  const auto r = parse({"--backend", "127.0.0.1:9001", "--listen"});
  EXPECT_EQ(r.kind, Kind::kError);
}

TEST(Args, UnknownFlag) {
  const auto r = parse({"--listen", "127.0.0.1:8080", "--backend", "127.0.0.1:1", "--nope"});
  EXPECT_EQ(r.kind, Kind::kError);
  EXPECT_NE(r.error.find("--nope"), std::string::npos);
}

TEST(Args, HelpAndVersion) {
  EXPECT_EQ(parse({"--help"}).kind, Kind::kHelp);
  EXPECT_EQ(parse({"-h"}).kind, Kind::kHelp);
  EXPECT_EQ(parse({"--version"}).kind, Kind::kVersion);
}
