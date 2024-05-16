#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from http.server import HTTPServer, SimpleHTTPRequestHandler
import base64
from hashlib import sha256
import json
import ssl
import sys
import urllib.request
from http import HTTPStatus
from urllib.parse import urlparse, parse_qs


class MyWebServerHandler(SimpleHTTPRequestHandler):
    kBuyer = "https://bidding-auction-server.example.com"
    kSeller = "https://bidding-auction-server.example.com"

    def is_fake_ad_service(self):
        parsed_url = urlparse(self.path)
        return parsed_url.path == "/cgi-bin/fake_ad_server.py"

    def handle_fake_server(self, b64Data):
        request = {
            "protectedAudienceCiphertext": b64Data,
            "auctionConfig": {
                "seller_signals": "{}",
                "auction_signals": "{}",
                "buyer_list": [self.kBuyer],
                "seller": self.kSeller,
                "perBuyerConfig": {self.kBuyer: {"buyerSignals": '"foo"'}},
                "codeExperimentSpec": {},
            },
            "clientType": 2,
        }

        request_str = json.dumps(request)
        response_obj = urllib.request.urlopen(
            urllib.request.Request(
                "http://localhost:51052/v1/selectAd",
                data=request_str.encode("utf-8"),
                headers={
                    "Content-Type": "application/JSON",
                    "X-BnA-Client-IP": "192.168.1.100",
                },
            )
        )

        response = response_obj.read()

        response_obj = json.loads(response)
        print(response_obj)
        ciphertext = base64.b64decode(response_obj["auctionResultCiphertext"])
        ciphertext_hash = sha256(ciphertext).digest()
        hash_b64 = base64.b64encode(ciphertext_hash).decode("utf-8")

        self.send_response(HTTPStatus.OK)
        self.send_header("Ad-Auction-Result", hash_b64)
        self.send_header("Content-type", "application/JSON")
        self.send_header("X-Allow-FLEDGE", "true")

        self.end_headers()

        self.wfile.write(response)
        return

    def send_head(self):
        if self.is_fake_ad_service():
            parsed_url = urlparse(self.path)
            query = parse_qs(parsed_url.query)
            return self.handle_fake_server(query["data"][0])
        else:
            return SimpleHTTPRequestHandler.send_head(self)


if len(sys.argv) < 3:
    print("Usage: local_server.py certfile keyfile")
    exit

certfile, keyfile = sys.argv[1], sys.argv[2]

httpd = HTTPServer(("localhost", 50071), MyWebServerHandler)
ssl_context = ssl.create_default_context(ssl.Purpose.CLIENT_AUTH)
ssl_context.load_cert_chain(certfile=certfile, keyfile=keyfile)
ssl_context.minimum_version = ssl.TLSVersion.TLSv1_3
httpd.socket = ssl_context.wrap_socket(httpd.socket, server_side=True)
httpd.serve_forever()
