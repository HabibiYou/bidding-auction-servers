#!/bin/bash
# Copyright 2024 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


echo "Handling Prerequisites..."

# Check for build mode (local or docker)
if [[ $# -eq 0 ]] ; then
    echo 'Error: Please specify build mode ("local" or "docker")'
    exit 1
fi

BUILD_MODE=$1

if [[ $BUILD_MODE == "local" ]]; then
    # Check if python3-clang is installed
    if ! dpkg -s python3-clang &> /dev/null; then
        echo "python3-clang not found. Installing..."
        sudo apt update
        sudo apt install -y python3-clang
    else
        echo "python3-clang is already installed."
    fi

    # Build buyer stack (local)
    echo "Building buyer stack (local)..."
    builders/tools/bazel-debian build //services/bidding_service:server
    builders/tools/bazel-debian build //services/buyer_frontend_service:server
    echo "Buyer stack build complete (local)."

    # Build seller stack (local)
    echo "Building Seller stack (local)..."
    builders/tools/bazel-debian build //services/auction_service:server
    builders/tools/bazel-debian build //services/seller_frontend_service:server
    echo "Seller stack build complete (local)."

elif [[ $BUILD_MODE == "docker" ]]; then

    # Run this to clear docker containers
    docker rm -v -f $(docker ps -qa)

    echo "Starting local servers..."

    gnome-terminal --title="Local server 1" -- bash -c "bash ./tools/local-servers/run_local_server.sh; exec bash"

    gnome-terminal --title="Local server 2" -- bash -c "bash ./tools/local-servers/run_local_server2.sh; exec bash"

    echo "servers are now running"

    echo "Building with Docker..."

    # Check if Docker daemon is running
    echo "Checking if docker daemon is running..."
    if ! sudo docker info &> /dev/null; then
        echo "Docker daemon is not running. Starting..."
        sudo systemctl start docker
        sleep 5
        echo "Docker daemon is now running."
    else
        echo "Docker daemon is already running."
    fi

    # Build buyer stack (Docker)
    echo "Building stack (Docker)..."
    ./production/packaging/build_and_test_all_in_docker --service-path bidding_service --service-path buyer_frontend_service --service-path seller_frontend_service --service-path auction_service --instance local --platform aws --build-flavor non_prod
    echo "Stack build complete (Docker)."

    echo "Starting buyer stack and front end"
    gnome-terminal --title="Bidding Server" -- bash -c "./tools/debug/start_bidding --rm; exec bash"
    gnome-terminal --title="Bidding Server Frontend" -- bash -c "./tools/debug/start_bfe --rm; exec bash"

    echo "Starting seller stack and front end"
    gnome-terminal --title="Auction Server" -- bash -c "./tools/debug/start_auction --rm; exec bash"
    gnome-terminal --title="Seller Server Frontend" -- bash -c "./tools/debug/start_sfe --rm; exec bash"
    echo "Seller stack started (Docker)."


    echo "starting envoy... "

    gnome-terminal --title="Envoy" -- bash -c "    docker run --rm -t --network="host" -v `pwd`/bazel-bin/api/bidding_auction_servers_descriptor_set.pb:/etc/envoy/bidding_auction_servers_descriptor_set.pb -v $(pwd)/logs:/logs -v `pwd`/tools/debug/envoy.yaml:/tmp/envoy.yaml envoyproxy/envoy:dev-3b18bc650237ce923176becc1e7ee0bd8de4b701 -c /tmp/envoy.yaml; exec bash"

    echo "started envoy"

    echo "Starting chrome to test."

    gnome-terminal --title="Chrome Test" -- bash -c "google-chrome-unstable --enable-features=\"PrivacySandboxAdsAPIsOverride,InterestGroupStorage,Fledge,BiddingAndScoringDebugReportingAPI,FencedFrames,NoncedPartitionedCookies,AllowURNsInIframes,FledgeBiddingAndAuctionServerAPI,FledgeBiddingAndAuctionServer:FledgeBiddingAndAuctionKeyURL/http%3A%2F%2F127%2E0%2E0%2E1%3A50072%2Fstatic%2Ftest_keys.json\" --disable-web-security --host-resolver-rules=\"MAP bidding-auction-server.example.com 127.0.0.1:50071\" --privacy-sandbox-enrollment-overrides=https://bidding-auction-server.example.com --ignore-certificate-errors --user-data-dir=/tmp/test_profile; exec bash"

    # To join an IG go to
    # https://bidding-auction-server.example.com/static/join.html#numGroups=1

    #Run an auction with B&A:
    # https://bidding-auction-server.example.com/static/ba.html





else
    echo "Invalid build mode. Please specify either 'local' or 'docker'"
    exit 1
fi
