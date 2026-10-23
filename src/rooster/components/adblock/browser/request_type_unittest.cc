// Copyright 2026 The Rooster Authors
// You can use, redistribute, and/or modify this source code under
// the terms of the GPL-3.0 license that can be found in the LICENSE file.

#include "rooster/components/adblock/browser/request_type.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace rooster::adblock {
namespace {

using Destination = network::mojom::RequestDestination;

TEST(RequestTypeTest, MapsDestinationsToFilterOptions) {
  EXPECT_EQ(RequestTypeFor(Destination::kEmpty), "xmlhttprequest");
  EXPECT_EQ(RequestTypeFor(Destination::kScript), "script");
  EXPECT_EQ(RequestTypeFor(Destination::kWorker), "script");
  EXPECT_EQ(RequestTypeFor(Destination::kImage), "image");
  EXPECT_EQ(RequestTypeFor(Destination::kStyle), "stylesheet");
  EXPECT_EQ(RequestTypeFor(Destination::kFont), "font");
  EXPECT_EQ(RequestTypeFor(Destination::kVideo), "media");
  EXPECT_EQ(RequestTypeFor(Destination::kIframe), "subdocument");
  EXPECT_EQ(RequestTypeFor(Destination::kFencedframe), "subdocument");
  EXPECT_EQ(RequestTypeFor(Destination::kObject), "object");
  EXPECT_EQ(RequestTypeFor(Destination::kReport), "ping");
  EXPECT_EQ(RequestTypeFor(Destination::kDocument), "document");
  EXPECT_EQ(RequestTypeFor(Destination::kManifest), "other");
}

}  // namespace
}  // namespace rooster::adblock
