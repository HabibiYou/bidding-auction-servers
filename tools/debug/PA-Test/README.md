# Protected Audience API Integration Test

## Overview

This is an integration test designed to verify the correct functionality of the Protected Audience
(PA) API in conjunction with the Bidding and Auction (B&A) servers.

The test simulates the behavior of a web browser interacting with the B&A system. It does this by:

1. Launching Chrome with specific command-line arguments to enable experimental features (such as
   Fledge) and to configure network settings for local testing.
2. Joining a predetermined number of interest groups via the B&A server's endpoints.
3. Triggering an auction using the B&A server's auction server enviroment.
4. Verifying that the auction process successfully identifies a winning bid.

## How to Run the Test

**Prerequisites:**

-   **Docker:** Make sure
    [Docker](https://g3doc.corp.google.com/cloud/containers/g3doc/glinux-docker/install.md?cl=head)
    is installed and running on your system.
-   **Envoy:** Make sure [Envoy](https://g3doc.corp.google.com/company/teams/envoy/index.md?cl=head)
    is installed and running on your system.

**Steps:**

1. **Start B&A Server (Docker):**

    - Navigate to the root directory of your project.
    - Execute the `build_and_test` bash script using:

        ```bash
        bash build_and_test
        ```

        This script builds and runs the necessary Docker containers for the B&A server and its
        dependencies.

2. **Run the Bazel Test:**

    - From the root directory of this `PA-Test` directory, run the following command:

        ```bash
        bazel test //tools/debug/PA-Test:my_web_test --noincompatible_use_python_toolchains --python3_path=`which python3`
        ```

        - `--noincompatible_use_python_toolchains`: This flag helps ensure compatibility with your
          Python 3 setup.
        - `--python3_path=`which
          python3``: This specifies the location of your Python 3 interpreter if it's not in the default `/opt/bin/python3`
          location.

**Important Note:**

Currently, there's a known issue where the Docker build process can interfere with Bazel's build
output (`bazel-bin`). Due to this, it's recommended to run the Docker build and the Bazel test in
separate repositories to avoid conflicts.

## Troubleshooting

**Common Issues and Solutions:**

most can be resolved by running `sudo rm -rf bazel-bin/`

**Error:** Could not find SellerFrontEnd in the proto descriptor

**Solution:** `sudo rm -rf bazel-bin/`

**Error:** Could not find BuyerFrontEnd in the proto descriptor

**Solution:** `sudo rm -rf bazel-bin/`

**Error:** Unable to determine mount point for /src/workspace... /server: Is a directory

**Solution:** `sudo rm -rf bazel-bin/`

**Error:** The container X is already in use by container...

**Solution:** `docker rm -v -f $(docker ps -qa)`
