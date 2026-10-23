// Copyright 2026 The Rooster Authors
// You can use, redistribute, and/or modify this source code under
// the terms of the GPL-3.0 license that can be found in the LICENSE file.

#ifndef ROOSTER_BROWSER_ROOSTER_BROWSER_MAIN_EXTRA_PARTS_H_
#define ROOSTER_BROWSER_ROOSTER_BROWSER_MAIN_EXTRA_PARTS_H_

#include "chrome/browser/chrome_browser_main_extra_parts.h"

namespace rooster {

// Starts and stops Rooster's browser-wide services.
class RoosterBrowserMainExtraParts : public ChromeBrowserMainExtraParts {
 public:
  RoosterBrowserMainExtraParts();
  RoosterBrowserMainExtraParts(const RoosterBrowserMainExtraParts&) = delete;
  RoosterBrowserMainExtraParts& operator=(const RoosterBrowserMainExtraParts&) =
      delete;
  ~RoosterBrowserMainExtraParts() override;

  // ChromeBrowserMainExtraParts:
  void PostBrowserStart() override;
  void PostMainMessageLoopRun() override;
};

}  // namespace rooster

#endif  // ROOSTER_BROWSER_ROOSTER_BROWSER_MAIN_EXTRA_PARTS_H_
