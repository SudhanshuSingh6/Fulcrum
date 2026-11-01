// M0 acceptance criterion 2: the integration harness can start fake_backend
// and talk to it. M1's proxy tests build on these helpers.
#include <gtest/gtest.h>
#include <sys/wait.h>

#include <chrono>
#include <string>

#include "http_client.h"
#include "process.h"

using namespace std::chrono_literals;
using fulcrum::test::free_port;
using fulcrum::test::http_get;
using fulcrum::test::http_request;
using fulcrum::test::Process;
using fulcrum::test::wait_for_port;

class FakeBackendTest : public ::testing::Test {
 protected:
  void SetUp() override {
    port_ = free_port();
    backend_ = Process::spawn(FAKE_BACKEND_BIN, {"--port", std::to_string(port_), "--id", "a"});
    ASSERT_TRUE(wait_for_port(port_, 5s)) << "fake_backend did not start";
  }

  std::uint16_t port_ = 0;
  Process backend_;
};

TEST_F(FakeBackendTest, RootReturnsId) {
  const auto resp = http_get(port_, "/");
  ASSERT_TRUE(resp.has_value());
  EXPECT_EQ(resp->status, 200);
  EXPECT_EQ(resp->body, "a");
}

TEST_F(FakeBackendTest, HealthReturns200) {
  const auto resp = http_get(port_, "/health");
  ASSERT_TRUE(resp.has_value());
  EXPECT_EQ(resp->status, 200);
}

TEST_F(FakeBackendTest, EchoReturnsBody) {
  const std::string body = "hello fulcrum\n";
  const auto resp = http_request(port_, "POST", "/echo", body);
  ASSERT_TRUE(resp.has_value());
  EXPECT_EQ(resp->status, 200);
  EXPECT_EQ(resp->body, body);
}

TEST(FulcrumBinary, PrintsVersion) {
  // Smoke test that the fulcrum binary is built and runs.
  Process p = Process::spawn(FULCRUM_BIN, {"--version"});
  const auto status = p.wait_exit(5s);
  ASSERT_TRUE(status.has_value()) << "fulcrum --version did not exit";
  ASSERT_TRUE(WIFEXITED(*status));
  EXPECT_EQ(WEXITSTATUS(*status), 0);
}
