// Copyright 2026 The Rooster Authors
// You can use, redistribute, and/or modify this source code under
// the terms of the GPL-3.0 license that can be found in the LICENSE file.

#ifndef ROOSTER_COMPONENTS_ADBLOCK_CORE_DOMAIN_RESOLVER_H_
#define ROOSTER_COMPONENTS_ADBLOCK_CORE_DOMAIN_RESOLVER_H_

#include <stddef.h>

#include "third_party/rust/cxx/v1/cxx.h"

namespace rooster::adblock::ffi {

// Called by adblock-rust (see rust/lib.rs) to find the registrable domain
// (eTLD+1) of `host` using Chromium's public suffix list, which decides what
// counts as first- or third-party. Sets [start, end) to its byte range within
// `host`, or to the whole host if it has none (IP addresses, bare suffixes).
void RegistrableDomainRange(rust::Str host, size_t& start, size_t& end);

}  // namespace rooster::adblock::ffi

#endif  // ROOSTER_COMPONENTS_ADBLOCK_CORE_DOMAIN_RESOLVER_H_
