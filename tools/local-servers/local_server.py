#!/usr/bin/env python3
# pylint: skip-file
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import datetime
import email
from http.server import HTTPServer, SimpleHTTPRequestHandler
import base64
from hashlib import sha256
import json
import os
import ssl
import sys
import urllib.request
from http import HTTPStatus
from urllib.parse import urlparse, parse_qs

# Global variables to keep track of whether the reporting beacon was seen.
SELLER_RESULT_REPORTED = False
BIDDING_RESULT_REPORTED = False


class MyWebServerHandler(SimpleHTTPRequestHandler):
    kBuyer = "https://bidding-auction-server.example.com"
    kSeller = "https://bidding-auction-server.example.com"
    kMultiSellerURL = "ba-multiseller.html"
    kSingleSellerURL = "ba.html"
    kResetURL = "reset-vars"
    kGetValuesURL = "get-vars"
    auction_config = {
            "seller_signals": "{}",
            "auction_signals": "{}",
            "buyer_list": [kBuyer],
            "seller": kSeller,
            "perBuyerConfig": {kBuyer: {"buyerSignals": '"foo"'}},
            "codeExperimentSpec": {},
        }
    # Overrides the POST method to ensure the global variables are
    # set if the report event from the ad beacon hits this local server.
    def do_POST(self): 
        global SELLER_RESULT_REPORTED, BIDDING_RESULT_REPORTED
        
        self.send_response(HTTPStatus.OK)
        self.end_headers()
        # Check if the request is for seller_result or bidding_winner
        if "/static/seller_result" in self.path:
            SELLER_RESULT_REPORTED = True
        elif "/static/bidding_winner" in self.path:
            BIDDING_RESULT_REPORTED = True
    # Overrides the GET method to ensure the 'sendReportTo' reporting is seen,
    # from the decision logic and the bidding logic.
    def do_GET(self):
        global SELLER_RESULT_REPORTED, BIDDING_RESULT_REPORTED
        if self.kResetURL in self.path:
            SELLER_RESULT_REPORTED = False
            BIDDING_RESULT_REPORTED = False
            self.send_response(HTTPStatus.OK) 
            self.end_headers()
        elif self.kGetValuesURL in self.path:
            self.send_response(HTTPStatus.OK)
            self.send_header("Seller-Result-Reported", str(SELLER_RESULT_REPORTED))
            self.send_header("Bidding-Winner-Reported", str(BIDDING_RESULT_REPORTED))
            self.send_header("Both-Ad-Beacons-Reported",str(BIDDING_RESULT_REPORTED and SELLER_RESULT_REPORTED))
            self.end_headers()
        else:  
            SimpleHTTPRequestHandler.do_GET(self)

    def isFakeAdService(self):
        parsed_url = urlparse(self.path)
        return parsed_url.path == "/cgi-bin/fake_ad_server.py"

    def handleRequest(self, b64Data, auction_config):
        request = {
            "protectedAudienceCiphertext": b64Data,
            "auctionConfig":auction_config,
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
        hash_b64 = base64.urlsafe_b64encode(ciphertext_hash).decode("utf-8")

        self.send_response(HTTPStatus.OK)
        self.send_header("Ad-Auction-Result", hash_b64)
        self.send_header("Content-type", "application/JSON")
        self.send_header("X-Allow-FLEDGE", "true")

        self.end_headers()

        self.wfile.write(response)
        return
    
    def send_head_original(self):
        """Rewritten SimpleHTTPRequestHandler's method in order to
        pass proper headers needed.
        
        Common code for GET and HEAD commands.

        This sends the response code and MIME headers.

        Return value is either a file object (which has to be copied
        to the outputfile by the caller unless the command was HEAD,
        and must be closed by the caller under all circumstances), or
        None, in which case the caller has nothing further to do.

        """
        path = self.translate_path(self.path)
        f = None
        if os.path.isdir(path):
            parts = urllib.parse.urlsplit(self.path)
            if not parts.path.endswith('/'):
                # redirect browser - doing basically what apache does
                self.send_response(HTTPStatus.MOVED_PERMANENTLY)
                new_parts = (parts[0], parts[1], parts[2] + '/',
                             parts[3], parts[4])
                new_url = urllib.parse.urlunsplit(new_parts)
                self.send_header("Location", new_url)
                self.send_header("Content-Length", "0")
                self.end_headers()
                return None
            for index in "index.html", "index.htm":
                index = os.path.join(path, index)
                if os.path.isfile(index):
                    path = index
                    break
            else:
                return self.list_directory(path)
        ctype = self.guess_type(path)
        # check for trailing "/" which should return 404. See Issue17324
        # The test for this was added in test_httpserver.py
        # However, some OS platforms accept a trailingSlash as a filename
        # See discussion on python-dev and Issue34711 regarding
        # parsing and rejection of filenames with a trailing slash
        if path.endswith("/"):
            self.send_error(HTTPStatus.NOT_FOUND, "File not found")
            return None
        try:
            f = open(path, 'rb')
        except OSError:
            self.send_error(HTTPStatus.NOT_FOUND, "File not found")
            return None

        try:
            fs = os.fstat(f.fileno())
            # Use browser cache if possible
            if ("If-Modified-Since" in self.headers
                    and "If-None-Match" not in self.headers):
                # compare If-Modified-Since and time of last file modification
                try:
                    ims = email.utils.parsedate_to_datetime(
                        self.headers["If-Modified-Since"])
                except (TypeError, IndexError, OverflowError, ValueError):
                    # ignore ill-formed values
                    pass
                else:
                    if ims.tzinfo is None:
                        # obsolete format with no timezone, cf.
                        # https://tools.ietf.org/html/rfc7231#section-7.1.1.1
                        ims = ims.replace(tzinfo=datetime.timezone.utc)
                    if ims.tzinfo is datetime.timezone.utc:
                        # compare to UTC datetime of last modification
                        last_modif = datetime.datetime.fromtimestamp(
                            fs.st_mtime, datetime.timezone.utc)
                        # remove microseconds, like in If-Modified-Since
                        last_modif = last_modif.replace(microsecond=0)

                        if last_modif <= ims:
                            self.send_response(HTTPStatus.NOT_MODIFIED)
                            self.end_headers()
                            f.close()
                            return None

            self.send_response(HTTPStatus.OK)
            self.send_header('Supports-Loading-Mode', 'fenced-frame')
            self.send_header("Content-type", ctype)
            self.send_header("Content-Length", str(fs[6]))
            self.send_header("Ad-Auction-Allowed","true")
            self.send_header("Last-Modified",
                self.date_time_string(fs.st_mtime))
            self.end_headers()
            return f
        except:
            f.close()
            raise


    def send_head(self):   
        #Make sure the auctionConfig is made accordingly for the test. 
        print("path we got: ",self.path) 
        if(self.kMultiSellerURL in self.path):
            self.auction_config["top_level_seller"] = self.kSeller
            
        elif(self.kSingleSellerURL in self.path):
            self.auction_config.pop("top_level_seller", None)
            
        if self.isFakeAdService():
            parsed_url = urlparse(self.path)
            query = parse_qs(parsed_url.query)
            return self.handleRequest(query["data"][0], self.auction_config)
        else:
            return self.send_head_original()
        

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
