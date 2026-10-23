// Copyright 2026 The Rooster Authors
// You can use, redistribute, and/or modify this source code under
// the terms of the GPL-3.0 license that can be found in the LICENSE file.

#include "rooster/components/adblock/core/domain_resolver.h"

#include <string>
#include <string_view>

#include "base/strings/string_view_rust.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace rooster::adblock::ffi {

void RegistrableDomainRange(rust::Str host, size_t& start, size_t& end) {
  const std::string_view host_view = base::RustStrToStringView(host);
  start = 0;
  end = host_view.size();

  const std::string domain =
      net::registry_controlled_domains::GetDomainAndRegistry(
          host_view,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  if (!domain.empty() && host_view.ends_with(domain)) {
    start = host_view.size() - domain.size();
  }
}

}  // namespace rooster::adblock::ffi
