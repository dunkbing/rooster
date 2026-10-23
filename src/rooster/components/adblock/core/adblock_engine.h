// Copyright 2026 The Rooster Authors
// You can use, redistribute, and/or modify this source code under
// the terms of the GPL-3.0 license that can be found in the LICENSE file.

#ifndef ROOSTER_COMPONENTS_ADBLOCK_CORE_ADBLOCK_ENGINE_H_
#define ROOSTER_COMPONENTS_ADBLOCK_CORE_ADBLOCK_ENGINE_H_

#include <stdint.h>

#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/ref_counted.h"
#include "third_party/rust/cxx/v1/cxx.h"

namespace rooster::adblock {

namespace ffi {
struct Engine;
}  // namespace ffi

// The engine's verdict on one network request.
struct Decision {
  // Cancel the request.
  bool block = false;
  // When `block` is set: a data: URL to serve instead (from a `redirect`
  // rule), or empty to just cancel.
  std::string redirect;
  // When `block` isn't set: the URL with tracking parameters removed (from a
  // `removeparam` rule), or empty to leave it unchanged.
  std::string rewritten_url;
};

// An adblock-rust engine built from filter lists. Immutable and safe to use
// from any thread; updating the lists builds a new Engine to swap in.
class Engine : public base::RefCountedThreadSafe<Engine> {
 public:
  // Compiles the given filter lists (Adblock Plus / uBlock Origin syntax).
  // Takes seconds for the default lists, so call it on a background thread.
  static scoped_refptr<Engine> FromFilterLists(
      const std::vector<std::string>& lists);

  // Restores an engine saved by Serialize(). Returns null if `data` is
  // corrupt or from a different adblock-rust format version.
  static scoped_refptr<Engine> FromSerialized(base::span<const uint8_t> data);

  Engine(const Engine&) = delete;
  Engine& operator=(const Engine&) = delete;

  std::vector<uint8_t> Serialize() const;

  // `request_type` is an adblock-rust request type, see RequestTypeFor().
  // `source_url` is the URL of the page making the request; it decides
  // first- vs third-party and `domain=` options.
  Decision Check(std::string_view url,
                 std::string_view source_url,
                 std::string_view request_type,
                 std::string_view method) const;

 private:
  friend class base::RefCountedThreadSafe<Engine>;

  explicit Engine(rust::Box<ffi::Engine> engine);
  ~Engine();

  rust::Box<ffi::Engine> engine_;
};

// Points adblock-rust at Chromium's public suffix list. Call once, before
// the first Engine is used.
void InitializeAdblockRust();

}  // namespace rooster::adblock

#endif  // ROOSTER_COMPONENTS_ADBLOCK_CORE_ADBLOCK_ENGINE_H_
