// Copyright 2026 The Rooster Authors
// You can use, redistribute, and/or modify this source code under
// the terms of the GPL-3.0 license that can be found in the LICENSE file.

#include "rooster/components/adblock/browser/filter_lists.h"

namespace rooster::adblock {

namespace {

constexpr FilterList kDefaultFilterLists[] = {
    {"easylist", "https://easylist.to/easylist/easylist.txt"},
    {"easyprivacy", "https://easylist.to/easylist/easyprivacy.txt"},
    {"ublock-filters",
     "https://ublockorigin.github.io/uAssets/filters/filters.txt"},
    {"ublock-privacy",
     "https://ublockorigin.github.io/uAssets/filters/privacy.txt"},
    {"ublock-unbreak",
     "https://ublockorigin.github.io/uAssets/filters/unbreak.txt"},
    {"peter-lowe",
     "https://pgl.yoyo.org/adservers/serverlist.php"
     "?hostformat=adblockplus&showintro=0&mimetype=plaintext"},
};

}  // namespace

base::span<const FilterList> DefaultFilterLists() {
  return kDefaultFilterLists;
}

}  // namespace rooster::adblock
