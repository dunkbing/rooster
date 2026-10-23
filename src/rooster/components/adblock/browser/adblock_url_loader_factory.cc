// Copyright 2026 The Rooster Authors
// You can use, redistribute, and/or modify this source code under
// the terms of the GPL-3.0 license that can be found in the LICENSE file.

#include "rooster/components/adblock/browser/adblock_url_loader_factory.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/self_deleting.h"
#include "net/base/net_errors.h"
#include "rooster/components/adblock/browser/adblock_service.h"
#include "rooster/components/adblock/browser/request_type.h"
#include "rooster/components/adblock/core/adblock_engine.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/cpp/url_loader_factory_builder.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

namespace rooster::adblock {

namespace {

using FactoryType = content::ContentBrowserClient::URLLoaderFactoryType;

bool IsSubframeNavigation(const network::ResourceRequest& request) {
  using Destination = network::mojom::RequestDestination;
  return request.destination == Destination::kIframe ||
         request.destination == Destination::kFrame ||
         request.destination == Destination::kFencedframe;
}

// The page making the request, as adblock-rust's source URL. Sandboxed
// frames have opaque origins; their precursor still says which site they
// belong to.
std::string SourceUrlFor(const url::Origin& initiator) {
  return initiator.GetTupleOrPrecursorTupleIfOpaque().GetURL().spec();
}

}  // namespace

// static
void AdblockURLLoaderFactory::MaybeProxy(
    FactoryType type,
    const url::Origin& request_initiator,
    network::URLLoaderFactoryBuilder& factory_builder) {
  switch (type) {
    case FactoryType::kNavigation:
    case FactoryType::kDocumentSubResource:
    case FactoryType::kWorkerSubResource:
    case FactoryType::kServiceWorkerSubResource:
      break;
    default:
      return;
  }
  auto [receiver, target] = factory_builder.Append();
  base::MakeSelfDeleting<AdblockURLLoaderFactory>(
      std::move(receiver), std::move(target), type, request_initiator);
}

AdblockURLLoaderFactory::AdblockURLLoaderFactory(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> target,
    FactoryType type,
    const url::Origin& request_initiator,
    base::SelfDeletingPassKey pass_key)
    : network::SelfDeletingURLLoaderFactory(std::move(receiver), pass_key),
      target_(std::move(target)),
      type_(type),
      request_initiator_(request_initiator) {
  target_.set_disconnect_handler(
      base::BindOnce(&AdblockURLLoaderFactory::OnTargetFactoryDisconnected,
                     base::Unretained(this)));
}

AdblockURLLoaderFactory::~AdblockURLLoaderFactory() = default;

void AdblockURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> loader,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  const url::Origin initiator =
      request.request_initiator.value_or(request_initiator_);
  scoped_refptr<Engine> engine = AdblockService::GetInstance()->engine();

  if (engine && ShouldCheck(request, initiator)) {
    const Decision decision =
        engine->Check(request.url.spec(), SourceUrlFor(initiator),
                      RequestTypeFor(request.destination), request.method);
    if (decision.block) {
      // `decision.redirect` (a stand-in resource from a `redirect` rule) isn't
      // served yet; cancelling is what a plain blocking rule does.
      mojo::Remote<network::mojom::URLLoaderClient>(std::move(client))
          ->OnComplete(
              network::URLLoaderCompletionStatus(net::ERR_BLOCKED_BY_CLIENT));
      return;
    }

    // Navigations must keep their URL; only subresources get rewritten.
    const GURL rewritten(decision.rewritten_url);
    if (type_ != FactoryType::kNavigation && rewritten.is_valid() &&
        rewritten.scheme() == request.url.scheme()) {
      network::ResourceRequest without_tracking_params = request;
      without_tracking_params.url = rewritten;
      target_->CreateLoaderAndStart(std::move(loader), request_id, options,
                                    without_tracking_params, std::move(client),
                                    traffic_annotation);
      return;
    }
  }

  target_->CreateLoaderAndStart(std::move(loader), request_id, options,
                                request, std::move(client), traffic_annotation);
}

bool AdblockURLLoaderFactory::ShouldCheck(
    const network::ResourceRequest& request,
    const url::Origin& initiator) const {
  if (!request.url.SchemeIsHTTPOrHTTPS()) {
    return false;
  }
  // Leave requests from extensions and browser pages alone.
  const std::string& initiator_scheme =
      initiator.GetTupleOrPrecursorTupleIfOpaque().scheme();
  if (initiator_scheme != url::kHttpScheme &&
      initiator_scheme != url::kHttpsScheme) {
    return false;
  }
  // Ad iframes are blocked; the page the user asked for never is.
  if (type_ == FactoryType::kNavigation) {
    return IsSubframeNavigation(request);
  }
  return true;
}

void AdblockURLLoaderFactory::OnTargetFactoryDisconnected() {
  DisconnectReceiversAndDestroy();
}

}  // namespace rooster::adblock
