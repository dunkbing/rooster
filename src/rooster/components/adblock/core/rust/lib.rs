// Copyright 2026 The Rooster Authors
// You can use, redistribute, and/or modify this source code under
// the terms of the GPL-3.0 license that can be found in the LICENSE file.

//! Rooster's bridge to the `adblock` crate (adblock-rust).
//!
//! Exposes just what the browser needs: building an engine from filter list
//! text, caching it in serialized form, and deciding about network requests.
//! The engine is `Send + Sync` (the crate's `single-thread` feature is off),
//! so C++ builds it on a background thread and queries it from any thread.

use adblock::lists::ParseOptions;
use adblock::request::Request;
use adblock::url_parser::ResolvesDomain;
use adblock::FilterSet;

#[cxx::bridge(namespace = "rooster::adblock::ffi")]
mod ffi {
    /// What to do with one network request.
    struct RequestDecision {
        /// Cancel the request.
        block: bool,
        /// Body to serve instead of the resource, from a `redirect` rule (a
        /// data: URL), or empty. Only meaningful when `block` is true.
        redirect: String,
        /// The URL with tracking parameters removed by a `removeparam` rule,
        /// or empty. Only meaningful when `block` is false.
        rewritten_url: String,
    }

    extern "Rust" {
        type Engine;

        /// Compiles an engine from the text of each filter list.
        fn engine_from_filter_lists(lists: &[String]) -> Box<Engine>;

        /// Restores an engine from `Engine::serialize`. Sets `ok` to false (and
        /// returns an empty engine) if the data is corrupt or was written by a
        /// different adblock-rust format version. Not a `Result`: Chromium
        /// builds without C++ exceptions, which cxx would use for `Err`.
        fn engine_from_serialized(data: &[u8], ok: &mut bool) -> Box<Engine>;

        fn serialize(self: &Engine) -> Vec<u8>;

        /// `request_type` is one of adblock-rust's request types ("script",
        /// "image", "xmlhttprequest", ...); see `request_type_for` in C++.
        fn check_request(
            self: &Engine,
            url: &str,
            source_url: &str,
            request_type: &str,
            method: &str,
        ) -> RequestDecision;

        /// Makes adblock-rust use Chromium's public suffix list. Must run once
        /// before any engine is used; later calls do nothing.
        fn install_chromium_domain_resolver();
    }

    unsafe extern "C++" {
        include!("rooster/components/adblock/core/domain_resolver.h");

        /// Sets `start` and `end` to the byte range of `host`'s registrable
        /// domain (eTLD+1), or of the whole host if it has none.
        fn RegistrableDomainRange(host: &str, start: &mut usize, end: &mut usize);
    }
}

pub struct Engine(adblock::Engine);

const _: () = {
    const fn assert_send_sync<T: Send + Sync>() {}
    assert_send_sync::<adblock::Engine>();
};

fn engine_from_filter_lists(lists: &[String]) -> Box<Engine> {
    let mut filter_set = FilterSet::new(false);
    for list in lists {
        filter_set.add_filter_list(list.clone(), ParseOptions::default());
    }
    Box::new(Engine(adblock::Engine::new_with_filter_set(filter_set)))
}

fn engine_from_serialized(data: &[u8], ok: &mut bool) -> Box<Engine> {
    let mut engine = adblock::Engine::default();
    *ok = engine.deserialize(data).is_ok();
    if !*ok {
        engine = adblock::Engine::default();
    }
    Box::new(Engine(engine))
}

impl Engine {
    fn serialize(&self) -> Vec<u8> {
        self.0.serialize()
    }

    fn check_request(
        &self,
        url: &str,
        source_url: &str,
        request_type: &str,
        method: &str,
    ) -> ffi::RequestDecision {
        let mut decision = ffi::RequestDecision {
            block: false,
            redirect: String::new(),
            rewritten_url: String::new(),
        };
        // URLs adblock-rust can't parse (no host, odd schemes) are left alone.
        let Ok(request) = Request::new(url, source_url, request_type, method) else {
            return decision;
        };
        let result = self.0.check_network_request(&request);
        decision.block = result.should_block();
        if decision.block {
            decision.redirect = result.redirect.unwrap_or_default();
        } else {
            decision.rewritten_url = result.rewritten_url.unwrap_or_default();
        }
        decision
    }
}

struct ChromiumDomainResolver;

impl ResolvesDomain for ChromiumDomainResolver {
    fn get_host_domain(&self, host: &str) -> (usize, usize) {
        let (mut start, mut end) = (0, host.len());
        ffi::RegistrableDomainRange(host, &mut start, &mut end);
        (start, end)
    }
}

fn install_chromium_domain_resolver() {
    // An error only means a resolver is already installed.
    let _ = adblock::url_parser::set_domain_resolver(Box::new(ChromiumDomainResolver));
}
