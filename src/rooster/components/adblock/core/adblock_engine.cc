// Copyright 2026 The Rooster Authors
// You can use, redistribute, and/or modify this source code under
// the terms of the GPL-3.0 license that can be found in the LICENSE file.

#include "rooster/components/adblock/core/adblock_engine.h"

#include <utility>

#include "base/containers/span_rust.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_view_rust.h"
#include "rooster/components/adblock/core/rust/lib.rs.h"

namespace rooster::adblock {

// static
scoped_refptr<Engine> Engine::FromFilterLists(
    const std::vector<std::string>& lists) {
  std::vector<rust::String> rust_lists;
  rust_lists.reserve(lists.size());
  for (const std::string& list : lists) {
    rust_lists.emplace_back(list);
  }
  return base::WrapRefCounted(new Engine(ffi::engine_from_filter_lists(
      rust::Slice<const rust::String>(rust_lists.data(), rust_lists.size()))));
}

// static
scoped_refptr<Engine> Engine::FromSerialized(base::span<const uint8_t> data) {
  bool ok = false;
  rust::Box<ffi::Engine> engine =
      ffi::engine_from_serialized(base::SpanToRustSlice(data), ok);
  if (!ok) {
    return nullptr;
  }
  return base::WrapRefCounted(new Engine(std::move(engine)));
}

Engine::Engine(rust::Box<ffi::Engine> engine) : engine_(std::move(engine)) {}

Engine::~Engine() = default;

std::vector<uint8_t> Engine::Serialize() const {
  rust::Vec<uint8_t> data = engine_->serialize();
  return std::vector<uint8_t>(data.begin(), data.end());
}

Decision Engine::Check(std::string_view url,
                       std::string_view source_url,
                       std::string_view request_type,
                       std::string_view method) const {
  ffi::RequestDecision result = engine_->check_request(
      base::StringViewToRustStrUTF8(url),
      base::StringViewToRustStrUTF8(source_url),
      base::StringViewToRustStrUTF8(request_type),
      base::StringViewToRustStrUTF8(method));
  return Decision{
      .block = result.block,
      .redirect = std::string(result.redirect),
      .rewritten_url = std::string(result.rewritten_url),
  };
}

void InitializeAdblockRust() {
  ffi::install_chromium_domain_resolver();
}

}  // namespace rooster::adblock
