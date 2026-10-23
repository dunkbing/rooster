// Copyright 2026 The Rooster Authors
// You can use, redistribute, and/or modify this source code under
// the terms of the GPL-3.0 license that can be found in the LICENSE file.

#ifndef ROOSTER_COMPONENTS_ADBLOCK_BROWSER_REQUEST_TYPE_H_
#define ROOSTER_COMPONENTS_ADBLOCK_BROWSER_REQUEST_TYPE_H_

#include <string_view>

#include "services/network/public/mojom/fetch_api.mojom-shared.h"

namespace rooster::adblock {

// The adblock-rust request type ("script", "image", ...) that filter options
// like `$script` and `$image` match for a request with `destination`.
std::string_view RequestTypeFor(network::mojom::RequestDestination destination);

}  // namespace rooster::adblock

#endif  // ROOSTER_COMPONENTS_ADBLOCK_BROWSER_REQUEST_TYPE_H_
