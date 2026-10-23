// Copyright 2026 The Rooster Authors
// You can use, redistribute, and/or modify this source code under
// the terms of the GPL-3.0 license that can be found in the LICENSE file.

#ifndef ROOSTER_COMPONENTS_ADBLOCK_BROWSER_ADBLOCK_URL_LOADER_FACTORY_H_
#define ROOSTER_COMPONENTS_ADBLOCK_BROWSER_ADBLOCK_URL_LOADER_FACTORY_H_

#include "base/types/pass_key.h"
#include "content/public/browser/content_browser_client.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/self_deleting_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/origin.h"

namespace network {
class URLLoaderFactoryBuilder;
}  // namespace network

namespace rooster::adblock {

// Sits in front of the network for a frame's or worker's requests and
// cancels the ones the ad-blocking engine matches.
class AdblockURLLoaderFactory : public network::SelfDeletingURLLoaderFactory {
 public:
  // Called from ContentBrowserClient::WillCreateURLLoaderFactory: adds the
  // blocker to `factory_builder` if factories of `type` carry web content.
  static void MaybeProxy(
      content::ContentBrowserClient::URLLoaderFactoryType type,
      const url::Origin& request_initiator,
      network::URLLoaderFactoryBuilder& factory_builder);

  AdblockURLLoaderFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> target,
      content::ContentBrowserClient::URLLoaderFactoryType type,
      const url::Origin& request_initiator,
      base::SelfDeletingPassKey pass_key);

  AdblockURLLoaderFactory(const AdblockURLLoaderFactory&) = delete;
  AdblockURLLoaderFactory& operator=(const AdblockURLLoaderFactory&) = delete;

  // network::mojom::URLLoaderFactory:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override;

 private:
  ~AdblockURLLoaderFactory() override;

  // Whether the blocker should look at `request` at all.
  bool ShouldCheck(const network::ResourceRequest& request,
                   const url::Origin& initiator) const;
  void OnTargetFactoryDisconnected();

  mojo::Remote<network::mojom::URLLoaderFactory> target_;
  const content::ContentBrowserClient::URLLoaderFactoryType type_;
  // The frame or worker the factory is for, used when a request carries no
  // initiator of its own.
  const url::Origin request_initiator_;
};

}  // namespace rooster::adblock

#endif  // ROOSTER_COMPONENTS_ADBLOCK_BROWSER_ADBLOCK_URL_LOADER_FACTORY_H_
