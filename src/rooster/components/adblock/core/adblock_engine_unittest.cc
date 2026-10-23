// Copyright 2026 The Rooster Authors
// You can use, redistribute, and/or modify this source code under
// the terms of the GPL-3.0 license that can be found in the LICENSE file.

#include "rooster/components/adblock/core/adblock_engine.h"

#include <stdint.h>

#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace rooster::adblock {
namespace {

constexpr char kRules[] =
    "||ads.example^\n"
    "@@||ads.example/allowed/\n"
    "/banner/*$image\n"
    "||tracker.example.co.uk^$third-party\n"
    "||important.example^$important\n"
    "@@||important.example^\n";

class AdblockEngineTest : public testing::Test {
 protected:
  static void SetUpTestSuite() { InitializeAdblockRust(); }

  void SetUp() override {
    engine_ = Engine::FromFilterLists({kRules});
    ASSERT_TRUE(engine_);
  }

  bool Blocks(const Engine& engine,
              const std::string& url,
              const std::string& source_url,
              const std::string& type = "script") {
    return engine.Check(url, source_url, type, "GET").block;
  }

  bool Blocks(const std::string& url,
              const std::string& source_url,
              const std::string& type = "script") {
    return Blocks(*engine_, url, source_url, type);
  }

  scoped_refptr<Engine> engine_;
};

TEST_F(AdblockEngineTest, BlocksMatchingRequests) {
  EXPECT_TRUE(Blocks("https://ads.example/ad.js", "https://news.example/"));
  EXPECT_TRUE(
      Blocks("https://cdn.ads.example/ad.js", "https://news.example/"));
  EXPECT_FALSE(Blocks("https://news.example/app.js", "https://news.example/"));
}

TEST_F(AdblockEngineTest, ExceptionsWin) {
  EXPECT_FALSE(
      Blocks("https://ads.example/allowed/x.js", "https://news.example/"));
}

TEST_F(AdblockEngineTest, ImportantBeatsExceptions) {
  EXPECT_TRUE(Blocks("https://important.example/x.js", "https://news.example/"));
}

TEST_F(AdblockEngineTest, ResourceTypeOptions) {
  EXPECT_TRUE(Blocks("https://cdn.example/banner/1.png",
                     "https://news.example/", "image"));
  EXPECT_FALSE(Blocks("https://cdn.example/banner/1.js",
                      "https://news.example/", "script"));
}

// example.co.uk and other.co.uk are different sites only if the public
// suffix list (co.uk) is used, so this also covers the domain resolver.
TEST_F(AdblockEngineTest, ThirdPartyUsesPublicSuffixList) {
  EXPECT_FALSE(Blocks("https://tracker.example.co.uk/t.js",
                      "https://www.example.co.uk/"));
  EXPECT_TRUE(Blocks("https://tracker.example.co.uk/t.js",
                     "https://www.other.co.uk/"));
}

TEST_F(AdblockEngineTest, UnparseableUrlsAreAllowed) {
  EXPECT_FALSE(Blocks("not a url", "https://news.example/"));
  EXPECT_FALSE(Blocks("", ""));
}

TEST_F(AdblockEngineTest, SerializationRoundTrip) {
  const std::vector<uint8_t> data = engine_->Serialize();
  ASSERT_FALSE(data.empty());

  scoped_refptr<Engine> restored = Engine::FromSerialized(data);
  ASSERT_TRUE(restored);
  EXPECT_TRUE(Blocks(*restored, "https://ads.example/ad.js",
                     "https://news.example/"));
  EXPECT_FALSE(Blocks(*restored, "https://ads.example/allowed/x.js",
                      "https://news.example/"));
  EXPECT_TRUE(Blocks(*restored, "https://tracker.example.co.uk/t.js",
                     "https://www.other.co.uk/"));
}

TEST_F(AdblockEngineTest, CorruptSerializedDataIsRejected) {
  const std::vector<uint8_t> garbage = {1, 2, 3, 4, 5};
  EXPECT_FALSE(Engine::FromSerialized(garbage));
  EXPECT_FALSE(Engine::FromSerialized({}));
}

}  // namespace
}  // namespace rooster::adblock
