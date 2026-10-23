// Copyright 2026 The Rooster Authors
// You can use, redistribute, and/or modify this source code under
// the terms of the GPL-3.0 license that can be found in the LICENSE file.

#include "rooster/components/adblock/browser/adblock_service.h"

#include <algorithm>
#include <string_view>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "rooster/components/adblock/browser/filter_lists.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "url/gurl.h"

namespace rooster::adblock {

namespace {

constexpr base::TimeDelta kUpdateInterval = base::Days(1);
constexpr base::TimeDelta kRetryDelay = base::Hours(1);
// With a usable engine already loaded, don't compete with startup work.
constexpr base::TimeDelta kStartupUpdateDelay = base::Seconds(30);
// EasyList and friends are a few MB each.
constexpr size_t kMaxListSize = 32 * 1024 * 1024;

constexpr base::FilePath::CharType kEngineFileName[] =
    FILE_PATH_LITERAL("engine.dat");
constexpr base::FilePath::CharType kListsDirName[] = FILE_PATH_LITERAL("lists");

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("rooster_adblock_filter_lists", R"(
        semantics {
          sender: "Rooster ad blocker"
          description:
            "Downloads the filter lists (EasyList, EasyPrivacy, uBlock "
            "Origin's lists and similar) that the built-in ad and tracker "
            "blocker uses."
          trigger: "Once a day, and at startup if the lists are outdated."
          data: "None; a plain GET of each list without cookies."
          destination: OTHER
          destination_other: "The publishers of the filter lists."
        }
        policy {
          cookies_allowed: NO
          setting: "None yet."
          policy_exception_justification: "Not implemented."
        })");

base::FilePath ListPath(const base::FilePath& data_dir, std::string_view id) {
  return data_dir.Append(kListsDirName)
      .AppendASCII(base::StrCat({id, ".txt"}));
}

}  // namespace

AdblockService::CachedState::CachedState() = default;
AdblockService::CachedState::CachedState(CachedState&&) = default;
AdblockService::CachedState& AdblockService::CachedState::operator=(
    CachedState&&) = default;
AdblockService::CachedState::~CachedState() = default;

// static
AdblockService* AdblockService::GetInstance() {
  static base::NoDestructor<AdblockService> instance;
  return instance.get();
}

AdblockService::AdblockService() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

AdblockService::~AdblockService() = default;

void AdblockService::Start(
    const base::FilePath& data_dir,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  InitializeAdblockRust();

  data_dir_ = data_dir;
  url_loader_factory_ = std::move(url_loader_factory);
  file_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&AdblockService::LoadCachedState, data_dir_),
      base::BindOnce(&AdblockService::OnCachedStateLoaded,
                     weak_factory_.GetWeakPtr()));
}

void AdblockService::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_factory_.InvalidateWeakPtrs();
  update_timer_.Stop();
  loaders_.clear();
  url_loader_factory_.reset();
}

scoped_refptr<Engine> AdblockService::engine() const {
  base::AutoLock lock(lock_);
  return engine_;
}

// static
AdblockService::CachedState AdblockService::LoadCachedState(
    const base::FilePath& data_dir) {
  CachedState state;
  const base::FilePath engine_path = data_dir.Append(kEngineFileName);

  if (std::optional<std::vector<uint8_t>> data =
          base::ReadFileToBytes(engine_path)) {
    state.engine = Engine::FromSerialized(*data);
  }
  if (state.engine) {
    base::File::Info info;
    if (base::GetFileInfo(engine_path, &info)) {
      state.last_update = info.last_modified;
    }
    return state;
  }

  // No usable engine (first run, or adblock-rust's format changed): rebuild
  // from the cached lists, if any, and refresh them right away.
  std::vector<std::string> lists;
  for (const FilterList& list : DefaultFilterLists()) {
    std::string text;
    if (base::ReadFileToString(ListPath(data_dir, list.id), &text)) {
      lists.push_back(std::move(text));
    }
  }
  if (!lists.empty()) {
    state.engine = Engine::FromFilterLists(lists);
  }
  return state;
}

void AdblockService::OnCachedStateLoaded(CachedState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state.engine) {
    SetEngine(std::move(state.engine));
  }

  base::TimeDelta delay;
  if (state.last_update) {
    const base::TimeDelta age = base::Time::Now() - *state.last_update;
    delay = std::max(kUpdateInterval - age, kStartupUpdateDelay);
  }
  ScheduleUpdate(delay);
}

void AdblockService::ScheduleUpdate(base::TimeDelta delay) {
  update_timer_.Start(FROM_HERE, delay,
                      base::BindOnce(&AdblockService::UpdateLists,
                                     weak_factory_.GetWeakPtr()));
}

void AdblockService::UpdateLists() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!url_loader_factory_ || pending_downloads_) {
    return;
  }

  const base::span<const FilterList> lists = DefaultFilterLists();
  loaders_.clear();
  downloaded_.assign(lists.size(), std::nullopt);
  pending_downloads_ = lists.size();

  for (size_t i = 0; i < lists.size(); ++i) {
    auto request = std::make_unique<network::ResourceRequest>();
    request->url = GURL(lists[i].url);
    request->credentials_mode = network::mojom::CredentialsMode::kOmit;

    loaders_.push_back(
        network::SimpleURLLoader::Create(std::move(request), kTrafficAnnotation));
    loaders_.back()->DownloadToString(
        url_loader_factory_.get(),
        base::BindOnce(&AdblockService::OnListDownloaded,
                       weak_factory_.GetWeakPtr(), i),
        kMaxListSize);
  }
}

void AdblockService::OnListDownloaded(size_t index,
                                      std::optional<std::string> body) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (body && !body->empty()) {
    downloaded_[index] = std::move(body);
  } else {
    LOG(WARNING) << "Couldn't download filter list "
                 << DefaultFilterLists()[index].id;
  }
  if (--pending_downloads_) {
    return;
  }

  loaders_.clear();
  const bool any_downloaded =
      std::ranges::any_of(downloaded_, [](const auto& list) {
        return list.has_value();
      });
  if (!any_downloaded) {
    ScheduleUpdate(kRetryDelay);
    return;
  }

  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&AdblockService::SaveListsAndCompile, data_dir_,
                     std::exchange(downloaded_, {})),
      base::BindOnce(&AdblockService::OnEngineCompiled,
                     weak_factory_.GetWeakPtr()));
}

// static
scoped_refptr<Engine> AdblockService::SaveListsAndCompile(
    const base::FilePath& data_dir,
    std::vector<std::optional<std::string>> downloaded) {
  const base::span<const FilterList> lists = DefaultFilterLists();
  base::CreateDirectory(data_dir.Append(kListsDirName));

  std::vector<std::string> texts;
  for (size_t i = 0; i < lists.size(); ++i) {
    const base::FilePath path = ListPath(data_dir, lists[i].id);
    if (downloaded[i]) {
      base::ImportantFileWriter::WriteFileAtomically(path, *downloaded[i]);
      texts.push_back(std::move(*downloaded[i]));
    } else if (std::string cached; base::ReadFileToString(path, &cached)) {
      texts.push_back(std::move(cached));
    }
  }

  scoped_refptr<Engine> engine = Engine::FromFilterLists(texts);
  const std::vector<uint8_t> data = engine->Serialize();
  base::ImportantFileWriter::WriteFileAtomically(
      data_dir.Append(kEngineFileName),
      std::string_view(reinterpret_cast<const char*>(data.data()),
                       data.size()));
  return engine;
}

void AdblockService::OnEngineCompiled(scoped_refptr<Engine> engine) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetEngine(std::move(engine));
  ScheduleUpdate(kUpdateInterval);
}

void AdblockService::SetEngine(scoped_refptr<Engine> engine) {
  base::AutoLock lock(lock_);
  engine_ = std::move(engine);
}

}  // namespace rooster::adblock
