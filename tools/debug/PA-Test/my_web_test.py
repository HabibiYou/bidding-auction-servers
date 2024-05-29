# pylint: skip-file
# Copyright 2024 Google LLC
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

import os
import unittest
from testing.web import webtest
from selenium.webdriver import ChromeOptions
from selenium.webdriver.common.desired_capabilities import DesiredCapabilities
from selenium.webdriver.common.by import By
from selenium.webdriver.support.ui import WebDriverWait
from selenium.webdriver.support import expected_conditions as EC
from selenium.common.exceptions import TimeoutException


class BrowserTest(unittest.TestCase):
    # Set up chrome with the proper flags, and start the driver.
    def setUp(self):
        chrome_options = ChromeOptions()
        chrome_options.add_argument(
            "--enable-features=PrivacySandboxAdsAPIsOverride,InterestGroupStorage,Fledge,BiddingAndScoringDebugReportingAPI,FencedFrames,NoncedPartitionedCookies,AllowURNsInIframes,FledgeBiddingAndAuctionServerAPI,FledgeBiddingAndAuctionServer:FledgeBiddingAndAuctionKeyURL/http%3A%2F%2F127%2E0%2E0%2E1%3A50072%2Fstatic%2Ftest_keys.json"
        )
        chrome_options.add_argument("--disable-web-security")
        chrome_options.add_argument("--headless=new")
        chrome_options.add_argument("--ignore-certificate-errors")
        chrome_options.add_argument("--enable-privacy-sandbox-ads-apis")
        chrome_options.add_argument(
            "--host-resolver-rules=MAP bidding-auction-server.example.com 127.0.0.1:50071"
        )
        chrome_options.add_argument(
            "--privacy-sandbox-enrollment-overrides=https://bidding-auction-server.example.com"
        )
        chrome_options.add_argument(
            "--disable-features=EnforcePrivacySandboxAttestations"
        )

        temp_profile_dir = "/tmp/test_profile"
        os.makedirs(temp_profile_dir, exist_ok=True)  # Create if doesn't exist
        chrome_options.add_argument(f"--user-data-dir={temp_profile_dir}")

        capabilities = chrome_options.to_capabilities()

        self.driver = webtest.new_webdriver_session(capabilities)

    def tearDown(self):
        try:
            self.driver.quit()
        finally:
            self.driver = None

    def test_auction_result(self):
        base_url = "https://bidding-auction-server.example.com"
        ig_count = 10  # This can be 1-100
        max_timeout = 10

        # Join the auction
        self.driver.get(f"{base_url}/static/join.html#numGroups={ig_count}")
        try:
            WebDriverWait(self.driver, max_timeout).until(
                EC.text_to_be_present_in_element(
                    (By.ID, "join-group-status"), f"Created {ig_count}, Failed 0"
                )
            )
        except TimeoutException:
            status_element = self.driver.find_element(By.ID, "join-group-status")
            print("ERROR: ", status_element.text)
            self.fail("Did not properly join the interest groups.")
        print(f"Sucessfully Joined {ig_count} interest groups")

        # Navigate to the bidding page
        self.driver.get(f"{base_url}/static/ba.html")
        print("Navigated to bidding auction page")
        try:
            WebDriverWait(self.driver, max_timeout).until(
                EC.text_to_be_present_in_element(
                    (By.ID, "result"), "Auction had a winner"
                )
            )
        except TimeoutException:
            status_element = self.driver.find_element(By.ID, "result")
            print("ERROR: ", status_element.text)
            self.fail("The auction did not have a winner")
        print("Auction did have a winner!")


if __name__ == "__main__":
    unittest.main(verbosity=15)
