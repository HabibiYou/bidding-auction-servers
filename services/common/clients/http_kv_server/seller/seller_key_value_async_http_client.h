//   Copyright 2022 Google LLC
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.
//

#ifndef SERVICES_COMMON_CLIENTS_SELLER_KEY_VALUE_ASYNC_HTTP_CLIENT_H_
#define SERVICES_COMMON_CLIENTS_SELLER_KEY_VALUE_ASYNC_HTTP_CLIENT_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "services/common/clients/async_client.h"
#include "services/common/clients/client_params.h"
#include "services/common/clients/http/http_fetcher_async.h"

namespace privacy_sandbox::bidding_auction_servers {

// The data used to build the Seller KV look url suffix
struct GetSellerValuesInput {
  // [SSP] List of keys to query values for, under the namespace renderUrls.
  std::vector<std::string> render_urls;

  // [SSP] List of keys to query values for, under the namespace
  // adComponentRenderUrls.
  std::vector<std::string> ad_component_render_urls;
};

// Response from Seller Key Value server as JSON string.
struct GetSellerValuesOutput {
  std::string result;
};

// The data used to build the Seller KV look url suffix
struct GetSellerValuesRawInput {
  // [SSP] List of keys to query values for, under the namespace renderUrls.
  std::vector<std::string> render_urls;

  // [SSP] List of keys to query values for, under the namespace
  // adComponentRenderUrls.
  std::vector<std::string> ad_component_render_urls;
};

// Response from Seller Key Value server as JSON string.
struct GetSellerValuesRawOutput {
  std::string result;
};

// This class fetches Key/Value pairs from a Seller Key/Value Server instance
// using a http client.
class SellerKeyValueAsyncHttpClient
    : public AsyncClient<GetSellerValuesInput, GetSellerValuesOutput,
                         GetSellerValuesRawInput, GetSellerValuesRawOutput> {
 public:
  // HttpFetcherAsync argument must outlive instance.
  // This class uses the http client to fetch KV values in real time.
  // If pre_warm is true, it will send an empty request to the
  // KV client to establish connection and cache connection data with the
  // underlying HTTP server. It's false by default.
  explicit SellerKeyValueAsyncHttpClient(
      absl::string_view kv_server_base_address,
      std::unique_ptr<HttpFetcherAsync> http_fetcher_async,
      bool pre_warm = false);

  // Executes the http request to a Key-Value Server asynchronously.
  //
  // keys: the request object to execute (the keys to query).
  // on_done: callback called when the request is finished executing.
  // timeout: a timeout value for the request.
  absl::Status Execute(
      std::unique_ptr<GetSellerValuesInput> keys,
      const RequestMetadata& metadata,
      absl::AnyInvocable<
          void(absl::StatusOr<std::unique_ptr<GetSellerValuesOutput>>) &&>
          on_done,
      absl::Duration timeout) const override;

 private:
  std::unique_ptr<HttpFetcherAsync> http_fetcher_async_;
  const std::string kv_server_base_address_;
};
}  // namespace privacy_sandbox::bidding_auction_servers

#endif  // SERVICES_COMMON_CLIENTS_SELLER_KEY_VALUE_ASYNC_HTTP_CLIENT_H_
