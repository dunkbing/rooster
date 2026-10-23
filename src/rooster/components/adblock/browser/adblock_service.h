// Copyright 2026 The Rooster Authors
// You can use, redistribute, and/or modify this source code under
// the terms of the GPL-3.0 license that can be found in the LICENSE file.

#ifndef ROOSTER_COMPONENTS_ADBLOCK_BROWSER_ADBLOCK_SERVICE_H_
#define ROOSTER_COMPONENTS_ADBLOCK_BROWSER_ADBLOCK_SERVICE_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "rooster/components/adblock/core/adblock_engine.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace rooster::adblock {

// Browser-wide owner of the ad-blocking engine. Loads the engine cached on
// disk at startup, then keeps the filter lists current: it downloads them
// from their publishers once a day and swaps in a freshly compiled engine.
//
// Started and stopped on the UI thread; engine() can be called from any
// thread.
class AdblockService {
 public:
  static AdblockService* GetInstance();

  AdblockService(const AdblockService&) = delete;
  AdblockService& operator=(const AdblockService&) = delete;

  // `data_dir` holds the cached lists and engine.
  void Start(const base::FilePath& data_dir,
             scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  void Shutdown();

  // The engine to check requests with, or null until one has loaded.
  scoped_refptr<Engine> engine() const;

 private:
  friend class base::NoDestructor<AdblockService>;

  // What startup found on disk.
  struct CachedState {
    CachedState();
    CachedState(CachedState&&);
    CachedState& operator=(CachedState&&);
    ~CachedState();

    scoped_refptr<Engine> engine;
    // When the lists were last downloaded, if ever.
    std::optional<base::Time> last_update;
  };

  AdblockService();
  ~AdblockService();

  static CachedState LoadCachedState(const base::FilePath& data_dir);
  // Saves newly downloaded lists, then compiles and caches an engine from
  // every list (falling back to the cached copy of any list that failed).
  static scoped_refptr<Engine> SaveListsAndCompile(
      const base::FilePath& data_dir,
      std::vector<std::optional<std::string>> downloaded);

  void OnCachedStateLoaded(CachedState state);
  void ScheduleUpdate(base::TimeDelta delay);
  void UpdateLists();
  void OnListDownloaded(size_t index, std::optional<std::string> body);
  void OnEngineCompiled(scoped_refptr<Engine> engine);
  void SetEngine(scoped_refptr<Engine> engine);

  mutable base::Lock lock_;
  scoped_refptr<Engine> engine_ GUARDED_BY(lock_);

  base::FilePath data_dir_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;
  base::OneShotTimer update_timer_;

  // One loader per list while an update is running.
  std::vector<std::unique_ptr<network::SimpleURLLoader>> loaders_;
  std::vector<std::optional<std::string>> downloaded_;
  size_t pending_downloads_ = 0;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<AdblockService> weak_factory_{this};
};

}  // namespace rooster::adblock

#endif  // ROOSTER_COMPONENTS_ADBLOCK_BROWSER_ADBLOCK_SERVICE_H_
