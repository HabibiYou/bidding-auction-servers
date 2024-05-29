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
"""
Creating our own browser repository because the default webtesting browser repo is outdated.
More information opn the webtesting browser repo can be found here:
https://github.com/bazelbuild/rules_webtesting/blob/d8208bddac1e44b3327430cc422f952b3244536a/web/versioned/browsers-0.3.4.bzl#L80
"""

# TODO make a good docstring
load("@io_bazel_rules_webtesting//web:web.bzl", "platform_archive")

def browser_repositories():
    org_chromium_chromedriver()
    org_chromium_chromium()

def org_chromium_chromedriver():
    platform_archive(
        name = "org_chromium_chromedriver_linux_x64",
        licenses = ["reciprocal"],  # BSD 3-clause, ICU, MPL 1.1, libpng (BSD/MIT-like), Academic Free License v. 2.0, BSD 2-clause, MIT
        sha256 = "ebbad794325e0d2a5ca4484d42ad2b552d9b4963f2dc65d59d3a3a7702809d95",
        urls = [
            "https://storage.googleapis.com/chrome-for-testing-public/125.0.6422.78/linux64/chromedriver-linux64.zip",
        ],
        named_files = {
            "CHROMEDRIVER": "chromedriver-linux64/chromedriver",
        },
    )

def org_chromium_chromium():
    platform_archive(
        name = "org_chromium_chromium_linux_x64",
        licenses = ["notice"],  # BSD 3-clause (maybe more?)
        sha256 = "ac6b39d129d80b555c153306f30cb57141a374e31a35ffdbed8c829c3de429d7",
        urls = [
            "https://storage.googleapis.com/chrome-for-testing-public/125.0.6422.78/linux64/chrome-linux64.zip",
        ],
        named_files = {
            "CHROMIUM": "chrome-linux64/chrome",
        },
    )
