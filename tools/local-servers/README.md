# Running a purely local setup with Chrome

This configuration runs the B&A servers locally. It doesn't use Docker so some of the steps to build
and configure B&A may be different.

## Setup:

1. Build B&A:

```bash
  /usr/bin/bazel-5.4.1 build -c opt  //... --//:instance=local --//:platform=local -k --noincompatible_use_python_toolchains --python3_path=`which python3`
```

1. Build Extra files (needed by Envoy):

```bash
/usr/bin/bazel-5.4.1 build -c opt  //production/packaging/gcp/seller_frontend_service:etc_envoy_files -k --noincompatible_use_python_toolchains --python3_path=`which python3`
```

1. Copy the generated `bazel-bin/api/bidding_auction_servers_descriptor_set.pb` to the
   `tools/local-servers/envoy_config` directory.

1. Fetch and build envoy. There are instructions
   [here](https://github.com/envoyproxy/envoy/blob/main/bazel/README.md#quick-start-bazel-build-for-developers),
   thought I have not verified them.

## Running

1. Open 8 terminal windows.

1. In the first window, run `tools/local-servers/run_local_server.sh`. This generates a self-signed
   certificant and starts the HTTPS server that Chrome will talk to. Chrome requires HTTPS for newer
   APIs, so we need to serve pages and scripts to it via HTTPS.

1. In the second window, run `tools/local-servers/run_local_server2.sh`. This starts the HTTP server
   that the B&A stack will talk to. B&A does not require HTTPS and there is not an easy way to
   inject certificates to the root store so it's easier to just use HTTP locally. _For production
   you should always use HTTPS with valid certificates._

1. In the third window, run `tools/debug/start_bidding`. This will start the bidder microservice.
   For this demo there is only one bidder, which is the same domain as the seller.

1. In the fourth window, run `tools/debug/start_bfe`. This will start the buyer front-end
   microservice.

1. In the fifth window, run `tools/debug/start_auction`. This will start the seller auction
   microservice.

1. In the sixth window, run `tools/debug/start_sfe`. This will run the seller front-end.

1. In the seventh window, run the Envoy proxy:

```bash
cd tools/local-servers/envoy-config/
~/envoy/bazel-bin/source/exe/envoy-static -c `pwd`/envoy.yaml
```

The Envoy proxy is needed to translate the JSON HTTP requests into GRPC that is used by the B&A
stack.

9. Make sure that all other Chrome windows are closed. In the final terminal window, you can run
   Chrome:

```bash
google-chrome-unstable --enable-features="PrivacySandboxAdsAPIsOverride,InterestGroupStorage,Fledge,BiddingAndScoringDebugReportingAPI,FencedFrames,NoncedPartitionedCookies,AllowURNsInIframes,FledgeBiddingAndAuctionServer:FledgeBiddingAndAuctionKeyURL/https%3A%2F%2F127%2E0%2E0%2E1%3A50071%2Fstatic%2Ftest_keys.json,FledgeBiddingAndAuctionServerAPI" --user-data-dir=/tmp/test_profile --ignore-certificate-errors --host-resolver-rules="MAP bidding-auction-server.example.com 127.0.0.1:50071"
```

This will run Chrome with a new profile directory in `/tmp/test_profile`, enabling features required
for using the Protected Audience Bidding and Auction Services APIs, _ignoring certificate errors_,
and resolving the `bidding-auction-server.example.com` domain to the local HTTPS server on
port 50071. Note that since this command line causes Chrome to ignore certificate errors, you should
not use this Chrome to browse general websites.

10. Navigate the browser to `https://bidding-auction-server.example.com/static/join.html`. This web
    page will join 10 interest groups.

11. Navigate the browser to `https://bidding-auction-server.example.com/static/ba.html`. This web
    page will run the B&A auction, outputing various pieces of information to the Chrome Dev
    Console.
