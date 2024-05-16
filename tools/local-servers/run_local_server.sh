#!/usr/bin/env bash
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

#
# A simple server for local testing. It serves bidding scripts out of the
# static directory. Dispatches KV requests to cgi-bin/kv.py

SCRIPT_DIR="$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")"
cd "$SCRIPT_DIR"

openssl req -x509 -newkey rsa:4096 -keyout key.pem -out cert.pem \
            -sha256 -days 3650 -nodes \
            -subj "/C=XX/ST=StateName/L=CityName/O=CompanyName/OU=CompanySectionName/CN=CommonNameOrHostname"
python3 ./local_server.py cert.pem key.pem
