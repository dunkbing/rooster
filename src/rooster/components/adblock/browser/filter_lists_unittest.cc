// Copyright 2026 The Rooster Authors
// You can use, redistribute, and/or modify this source code under
// the terms of the GPL-3.0 license that can be found in the LICENSE file.

#include "rooster/components/adblock/browser/filter_lists.h"

#include <set>
#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace rooster::adblock {
namespace {

TEST(FilterListsTest, DefaultListsAreFetchedOverHttps) {
  ASSERT_FALSE(DefaultFilterLists().empty());
  for (const FilterList& list : DefaultFilterLists()) {
    const GURL url(list.url);
    EXPECT_TRUE(url.is_valid()) << list.id;
    EXPECT_TRUE(url.SchemeIs(url::kHttpsScheme)) << list.id;
  }
}

// Ids name the cached files, so they must be unique and safe as file names.
TEST(FilterListsTest, IdsAreUniqueFileNames) {
  std::set<std::string> ids;
  for (const FilterList& list : DefaultFilterLists()) {
    EXPECT_TRUE(ids.emplace(list.id).second) << list.id;
    EXPECT_FALSE(list.id.empty());
    for (char c : list.id) {
      EXPECT_TRUE((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')
          << list.id;
    }
  }
}

}  // namespace
}  // namespace rooster::adblock
