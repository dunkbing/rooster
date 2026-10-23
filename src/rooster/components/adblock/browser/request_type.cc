// Copyright 2026 The Rooster Authors
// You can use, redistribute, and/or modify this source code under
// the terms of the GPL-3.0 license that can be found in the LICENSE file.

#include "rooster/components/adblock/browser/request_type.h"

namespace rooster::adblock {

std::string_view RequestTypeFor(
    network::mojom::RequestDestination destination) {
  using Destination = network::mojom::RequestDestination;
  switch (destination) {
    // fetch() and XMLHttpRequest have no destination.
    case Destination::kEmpty:
      return "xmlhttprequest";
    case Destination::kAudio:
    case Destination::kTrack:
    case Destination::kVideo:
      return "media";
    case Destination::kAudioWorklet:
    case Destination::kPaintWorklet:
    case Destination::kScript:
    case Destination::kServiceWorker:
    case Destination::kSharedWorker:
    case Destination::kWorker:
      return "script";
    case Destination::kDocument:
      return "document";
    case Destination::kEmbed:
    case Destination::kObject:
      return "object";
    case Destination::kFont:
      return "font";
    case Destination::kFencedframe:
    case Destination::kFrame:
    case Destination::kIframe:
      return "subdocument";
    case Destination::kImage:
      return "image";
    case Destination::kReport:
      return "ping";
    case Destination::kStyle:
      return "stylesheet";
    default:
      return "other";
  }
}

}  // namespace rooster::adblock
