// Copyright 2026 The Rooster Authors
// You can use, redistribute, and/or modify this source code under
// the terms of the GPL-3.0 license that can be found in the LICENSE file.

#ifndef ROOSTER_COMPONENTS_ADBLOCK_BROWSER_FILTER_LISTS_H_
#define ROOSTER_COMPONENTS_ADBLOCK_BROWSER_FILTER_LISTS_H_

#include <string_view>

#include "base/containers/span.h"

namespace rooster::adblock {

// A filter list Rooster downloads from its publisher.
struct FilterList {
  // Stable name, also the file name of its cached copy.
  std::string_view id;
  std::string_view url;
};

// The lists enabled by default: ads, trackers, and fixes for sites they
// break. They're fetched straight from the publishers, with no Rooster server
// in between.
base::span<const FilterList> DefaultFilterLists();

}  // namespace rooster::adblock

#endif  // ROOSTER_COMPONENTS_ADBLOCK_BROWSER_FILTER_LISTS_H_
