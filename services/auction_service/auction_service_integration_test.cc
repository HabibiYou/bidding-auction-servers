//  Copyright 2022 Google LLC
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.

#include <thread>

#include "absl/strings/str_format.h"
#include "absl/synchronization/blocking_counter.h"
#include "glog/logging.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/util/message_differencer.h"
#include "gtest/gtest.h"
#include "services/auction_service/auction_adtech_code_wrapper.h"
#include "services/auction_service/auction_service.h"
#include "services/auction_service/benchmarking/score_ads_benchmarking_logger.h"
#include "services/auction_service/benchmarking/score_ads_no_op_logger.h"
#include "services/common/clients/code_dispatcher/code_dispatch_client.h"
#include "services/common/clients/config/trusted_server_config_client.h"
#include "services/common/constants/common_service_flags.h"
#include "services/common/encryption/key_fetcher_factory.h"
#include "services/common/encryption/mock_crypto_client_wrapper.h"
#include "services/common/metric/server_definition.h"
#include "services/common/test/mocks.h"
#include "services/common/test/random.h"

namespace privacy_sandbox::bidding_auction_servers {
namespace {

constexpr char kKeyId[] = "key_id";
constexpr char kSecret[] = "secret";

using ::google::protobuf::TextFormat;
using AdWithBidMetadata =
    ScoreAdsRequest::ScoreAdsRawRequest::AdWithBidMetadata;
using ::testing::AnyNumber;

constexpr absl::string_view js_code = R"JS_CODE(
    function fibonacci(num) {
      if (num <= 1) return 1;
      return fibonacci(num - 1) + fibonacci(num - 2);
    }

    function scoreAd( ad_metadata,
                      bid,
                      auction_config,
                      scoring_signals,
                      device_signals,
                      direct_from_seller_signals
    ) {
      // Do a random amount of work to generate the score:
      const score = fibonacci(Math.floor(Math.random() * 10 + 1));

      //Reshaped into AdScore
      return {
        "desirability": score,
        "allow_component_auction": false
      };
    }
  )JS_CODE";

constexpr absl::string_view js_code_with_debug_urls = R"JS_CODE(
    function fibonacci(num) {
      if (num <= 1) return 1;
      return fibonacci(num - 1) + fibonacci(num - 2);
    }

    function scoreAd( ad_metadata,
                      bid,
                      auction_config,
                      scoring_signals,
                      device_signals,
                      direct_from_seller_signals
    ) {
      // Do a random amount of work to generate the score:
      const score = fibonacci(Math.floor(Math.random() * 10 + 1));
      forDebuggingOnly.reportAdAuctionLoss("https://example-ssp.com/debugLoss");
      forDebuggingOnly.reportAdAuctionWin("https://example-ssp.com/debugWin");
      //Reshaped into AdScore
      return {
        "desirability": score,
        "allow_component_auction": false
      };
    }
  )JS_CODE";

constexpr absl::string_view js_code_throws_exception = R"JS_CODE(
    function fibonacci(num) {
      if (num <= 1) return 1;
      return fibonacci(num - 1) + fibonacci(num - 2);
    }

    function scoreAd( ad_metadata,
                      bid,
                      auction_config,
                      scoring_signals,
                      device_signals,
                      direct_from_seller_signals) {
      // Do a random amount of work to generate the score:
      const score = fibonacci(Math.floor(Math.random() * 10 + 1));
      throw new Error('Exception message');
    }
  )JS_CODE";

constexpr absl::string_view js_code_throws_exception_with_debug_urls =
    R"JS_CODE(
    function fibonacci(num) {
      if (num <= 1) return 1;
      return fibonacci(num - 1) + fibonacci(num - 2);
    }

    function scoreAd( ad_metadata,
                      bid,
                      auction_config,
                      scoring_signals,
                      device_signals,
                      direct_from_seller_signals) {
      // Do a random amount of work to generate the score:
      const score = fibonacci(Math.floor(Math.random() * 10 + 1));

      forDebuggingOnly.reportAdAuctionLoss("https://example-ssp.com/debugLoss");
      forDebuggingOnly.reportAdAuctionWin("https://example-ssp.com/debugWin");
      throw new Error('Exception message');
    }
  )JS_CODE";

void BuildScoreAdsRequest(
    ScoreAdsRequest* request,
    absl::flat_hash_map<std::string, AdWithBidMetadata>* interest_group_to_ad,
    bool enable_debug_reporting = false, int desired_ad_count = 90) {
  ScoreAdsRequest::ScoreAdsRawRequest raw_request;
  std::string trusted_scoring_signals =
      R"json({"renderUrls":{"placeholder_url":[123])json";
  for (int i = 0; i < desired_ad_count; i++) {
    auto ad = MakeARandomAdWithBidMetadata(1, 10);
    *raw_request.mutable_ad_bids()->Add() = ad;
    interest_group_to_ad->try_emplace(ad.interest_group_name(), ad);

    std::string ad_signal = absl::StrFormat(
        "\"%s\":%s", ad.render(), R"JSON(["short", "test", "signal"])JSON");
    absl::StrAppend(&trusted_scoring_signals,
                    absl::StrFormat(", %s", ad_signal));
  }
  absl::StrAppend(&trusted_scoring_signals,
                  R"json(},"adComponentRenderUrls":{}})json");
  raw_request.set_scoring_signals(trusted_scoring_signals);
  if (enable_debug_reporting) {
    raw_request.set_enable_debug_reporting(enable_debug_reporting);
  }
  *request->mutable_request_ciphertext() = raw_request.SerializeAsString();
  request->set_key_id(kKeyId);
}

void SetupMockCryptoClientWrapper(MockCryptoClientWrapper& crypto_client) {
  // Mock the HpkeDecrypt() call on the crypto_client. This is used by the
  // service to decrypt the incoming request.

  EXPECT_CALL(crypto_client, HpkeDecrypt)
      .Times(AnyNumber())
      .WillRepeatedly([](const server_common::PrivateKey& private_key,
                         const std::string& ciphertext) {
        google::cmrt::sdk::crypto_service::v1::HpkeDecryptResponse
            hpke_decrypt_response;
        hpke_decrypt_response.set_payload(ciphertext);
        hpke_decrypt_response.set_secret(kSecret);
        return hpke_decrypt_response;
      });

  // Mock the AeadEncrypt() call on the crypto_client. This is used to encrypt
  // the response coming back from the service.
  EXPECT_CALL(crypto_client, AeadEncrypt)
      .Times(AnyNumber())
      .WillRepeatedly(
          [](const std::string& plaintext_payload, const std::string& secret) {
            google::cmrt::sdk::crypto_service::v1::AeadEncryptedData data;
            data.set_ciphertext(plaintext_payload);
            google::cmrt::sdk::crypto_service::v1::AeadEncryptResponse
                aead_encrypt_response;
            *aead_encrypt_response.mutable_encrypted_data() = std::move(data);
            return aead_encrypt_response;
          });
}

class AuctionServiceIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    server_common::metric::ServerConfig config_proto;
    config_proto.set_mode(server_common::metric::ServerConfig::PROD);
    metric::AuctionContextMap(
        server_common::metric::BuildDependentConfig(config_proto));
  }
};

TEST_F(AuctionServiceIntegrationTest, ScoresAdsWithCustomScoringLogic) {
  grpc::CallbackServerContext context;
  V8Dispatcher dispatcher;
  CodeDispatchClient client(dispatcher);
  DispatchConfig config;
  ASSERT_TRUE(dispatcher.Init(config).ok());
  std::string wrapper_js_blob = GetWrappedAdtechCodeForScoring(js_code);
  ASSERT_TRUE(dispatcher.LoadSync(1, wrapper_js_blob).ok());
  auto score_ads_reactor_factory =
      [&client](const ScoreAdsRequest* request, ScoreAdsResponse* response,
                server_common::KeyFetcherManagerInterface* key_fetcher_manager,
                CryptoClientWrapperInterface* crypto_client,
                const AuctionServiceRuntimeConfig& runtime_config) {
        // You can manually flip this flag to turn benchmarking logging on or
        // off
        bool enable_benchmarking = true;
        std::unique_ptr<ScoreAdsBenchmarkingLogger> benchmarking_logger;
        if (enable_benchmarking) {
          benchmarking_logger = std::make_unique<ScoreAdsBenchmarkingLogger>(
              FormatTime(absl::Now()));
        } else {
          benchmarking_logger = std::make_unique<ScoreAdsNoOpLogger>();
        }
        std::unique_ptr<MockAsyncReporter> async_reporter =
            std::make_unique<MockAsyncReporter>(
                std::make_unique<MockHttpFetcherAsync>());
        return std::make_unique<ScoreAdsReactor>(
            client, request, response, std::move(benchmarking_logger),
            key_fetcher_manager, crypto_client, std::move(async_reporter),
            runtime_config);
      };
  auto crypto_client = std::make_unique<MockCryptoClientWrapper>();
  SetupMockCryptoClientWrapper(*crypto_client);
  TrustedServersConfigClient config_client({});
  config_client.SetFlagForTest(kTrue, ENABLE_ENCRYPTION);
  config_client.SetFlagForTest(kTrue, TEST_MODE);
  auto key_fetcher_manager = CreateKeyFetcherManager(config_client);
  AuctionServiceRuntimeConfig auction_service_runtime_config;
  auction_service_runtime_config.encryption_enabled = true;
  AuctionService service(
      std::move(score_ads_reactor_factory), std::move(key_fetcher_manager),
      std::move(crypto_client), auction_service_runtime_config);

  int requests_to_test = 10;
  for (int i = 0; i < requests_to_test; i++) {
    ScoreAdsRequest request;
    ScoreAdsResponse response;
    absl::flat_hash_map<std::string, AdWithBidMetadata> interest_group_to_ad;
    BuildScoreAdsRequest(&request, &interest_group_to_ad);
    service.ScoreAds(&context, &request, &response);

    std::this_thread::sleep_for(absl::ToChronoSeconds(absl::Seconds(2)));

    // This line may NOT break if the ad_score() object is empty.
    ScoreAdsResponse::ScoreAdsRawResponse raw_response;
    raw_response.ParseFromString(response.response_ciphertext());
    const auto& scoredAd = raw_response.ad_score();
    // If no object was returned, the following two lines SHOULD fail.
    EXPECT_GT(scoredAd.desirability(), 0);
    EXPECT_FALSE(scoredAd.interest_group_name().empty());
    // If you see an error about a hash_map, it means invalid key, hence the
    // check on interest_group name above.
    EXPECT_EQ(scoredAd.render(),
              interest_group_to_ad.at(scoredAd.interest_group_name()).render());
  }
  EXPECT_TRUE(dispatcher.Stop().ok());
}

void ScoresAdsDebugReportingTestHelper(ScoreAdsResponse* response,
                                       absl::string_view adtech_code_blob,
                                       bool enable_seller_debug_url_generation,
                                       bool enable_debug_reporting) {
  grpc::CallbackServerContext context;
  V8Dispatcher dispatcher;
  CodeDispatchClient client(dispatcher);
  DispatchConfig config;
  ASSERT_TRUE(dispatcher.Init(config).ok());
  std::string wrapper_js_blob =
      GetWrappedAdtechCodeForScoring(adtech_code_blob);
  ASSERT_TRUE(dispatcher.LoadSync(1, wrapper_js_blob).ok());

  auto score_ads_reactor_factory =
      [&client](const ScoreAdsRequest* request, ScoreAdsResponse* response,
                server_common::KeyFetcherManagerInterface* key_fetcher_manager,
                CryptoClientWrapperInterface* crypto_client,
                const AuctionServiceRuntimeConfig& runtime_config) {
        // You can manually flip this flag to turn benchmarking logging on or
        // off
        bool enable_benchmarking = true;
        std::unique_ptr<ScoreAdsBenchmarkingLogger> benchmarking_logger;
        if (enable_benchmarking) {
          benchmarking_logger = std::make_unique<ScoreAdsBenchmarkingLogger>(
              FormatTime(absl::Now()));
        } else {
          benchmarking_logger = std::make_unique<ScoreAdsNoOpLogger>();
        }
        std::unique_ptr<MockAsyncReporter> async_reporter =
            std::make_unique<MockAsyncReporter>(
                std::make_unique<MockHttpFetcherAsync>());
        return std::make_unique<ScoreAdsReactor>(
            client, request, response, std::move(benchmarking_logger),
            key_fetcher_manager, crypto_client, std::move(async_reporter),
            runtime_config);
      };

  auto crypto_client = std::make_unique<MockCryptoClientWrapper>();
  SetupMockCryptoClientWrapper(*crypto_client);
  TrustedServersConfigClient config_client({});
  config_client.SetFlagForTest(kTrue, ENABLE_ENCRYPTION);
  config_client.SetFlagForTest(kTrue, TEST_MODE);
  auto key_fetcher_manager = CreateKeyFetcherManager(config_client);
  AuctionServiceRuntimeConfig auction_service_runtime_config = {
      .encryption_enabled = true,
      .enable_seller_debug_url_generation = enable_seller_debug_url_generation,
  };
  AuctionService service(
      std::move(score_ads_reactor_factory), std::move(key_fetcher_manager),
      std::move(crypto_client), auction_service_runtime_config);

  ScoreAdsRequest request;
  absl::flat_hash_map<std::string, AdWithBidMetadata> interest_group_to_ad;
  BuildScoreAdsRequest(&request, &interest_group_to_ad, enable_debug_reporting);

  service.ScoreAds(&context, &request, response);
  std::this_thread::sleep_for(absl::ToChronoSeconds(absl::Seconds(2)));
  // This line may NOT break if the ad_score() object is empty.
  EXPECT_TRUE(dispatcher.Stop().ok());
}

TEST_F(AuctionServiceIntegrationTest, ScoresAdsReturnsDebugUrlsForWinningAd) {
  bool enable_seller_debug_url_generation = true;
  bool enable_debug_reporting = true;
  ScoreAdsResponse response;
  ScoresAdsDebugReportingTestHelper(&response, js_code_with_debug_urls,
                                    enable_seller_debug_url_generation,
                                    enable_debug_reporting);
  ScoreAdsResponse::ScoreAdsRawResponse raw_response;
  raw_response.ParseFromString(response.response_ciphertext());
  const auto& scoredAd = raw_response.ad_score();
  EXPECT_GT(scoredAd.desirability(), 0);
  EXPECT_TRUE(scoredAd.has_debug_report_urls());
  EXPECT_EQ(scoredAd.debug_report_urls().auction_debug_win_url(),
            "https://example-ssp.com/debugWin");
  EXPECT_EQ(scoredAd.debug_report_urls().auction_debug_loss_url(),
            "https://example-ssp.com/debugLoss");
  EXPECT_GT(scoredAd.ig_owner_highest_scoring_other_bids_map().size(), 0);
}

TEST_F(AuctionServiceIntegrationTest,
       ScoresAdsWillNotReturnDebugUrlsForWinningAd) {
  bool enable_seller_debug_url_generation = true;
  bool enable_debug_reporting = false;
  ScoreAdsResponse response;
  ScoresAdsDebugReportingTestHelper(&response, js_code_with_debug_urls,
                                    enable_seller_debug_url_generation,
                                    enable_debug_reporting);
  ScoreAdsResponse::ScoreAdsRawResponse raw_response;
  raw_response.ParseFromString(response.response_ciphertext());
  const auto& scoredAd = raw_response.ad_score();
  EXPECT_GT(scoredAd.desirability(), 0);
  EXPECT_FALSE(scoredAd.has_debug_report_urls());
  EXPECT_GT(scoredAd.ig_owner_highest_scoring_other_bids_map().size(), 0);
}

TEST_F(AuctionServiceIntegrationTest,
       ScoresAdsWillReturnDebugUrlsWithScriptCrash) {
  bool enable_seller_debug_url_generation = true;
  bool enable_debug_reporting = true;
  ScoreAdsResponse response;
  ScoresAdsDebugReportingTestHelper(
      &response, js_code_throws_exception_with_debug_urls,
      enable_seller_debug_url_generation, enable_debug_reporting);
  ScoreAdsResponse::ScoreAdsRawResponse raw_response;
  raw_response.ParseFromString(response.response_ciphertext());
  const auto& scoredAd = raw_response.ad_score();
  EXPECT_EQ(scoredAd.desirability(), 0);
  // if the script crashes, score is returned as 0 and hence no ad should win.
  EXPECT_FALSE(scoredAd.has_debug_report_urls());
}

TEST_F(AuctionServiceIntegrationTest,
       ScoresAdsWillNotReturnDebugUrlsWithScriptCrash) {
  bool enable_seller_debug_url_generation = true;
  bool enable_debug_reporting = true;
  ScoreAdsResponse response;
  ScoresAdsDebugReportingTestHelper(&response, js_code_throws_exception,
                                    enable_seller_debug_url_generation,
                                    enable_debug_reporting);
  ScoreAdsResponse::ScoreAdsRawResponse raw_response;
  raw_response.ParseFromString(response.response_ciphertext());
  const auto& scoredAd = raw_response.ad_score();
  EXPECT_EQ(scoredAd.desirability(), 0);
  EXPECT_FALSE(scoredAd.has_debug_report_urls());
}
}  // namespace
}  // namespace privacy_sandbox::bidding_auction_servers
