// Copyright 2026 The Rooster Authors
// You can use, redistribute, and/or modify this source code under
// the terms of the GPL-3.0 license that can be found in the LICENSE file.

#include "rooster/browser/rooster_browser_main_extra_parts.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_paths.h"
#include "rooster/components/adblock/browser/adblock_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace rooster {

RoosterBrowserMainExtraParts::RoosterBrowserMainExtraParts() = default;

RoosterBrowserMainExtraParts::~RoosterBrowserMainExtraParts() = default;

void RoosterBrowserMainExtraParts::PostBrowserStart() {
  base::FilePath user_data_dir;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir)) {
    return;
  }
  adblock::AdblockService::GetInstance()->Start(
      user_data_dir.Append(FILE_PATH_LITERAL("Adblock")),
      g_browser_process->shared_url_loader_factory());
}

void RoosterBrowserMainExtraParts::PostMainMessageLoopRun() {
  adblock::AdblockService::GetInstance()->Shutdown();
}

}  // namespace rooster
