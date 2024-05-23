#!/usr/bin/env python3
# pylint: skip-file
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
#
import cgi
import json
import sys

form = cgi.FieldStorage()

print("Content-type: application/JSON")
print("X-Allow-FLEDGE: true")
print()  # blank line. End of headers

response = {}
if "keys" in form:
    keys = {}
    for key in form.getvalue("keys").split(","):
        keys[key] = {"blah": {}}
    response["keys"] = keys

if "interestGroupNames" in form:
    igs = {}
    for ig in form.getvalue("interestGroupNames").split(","):
        igs[ig] = {"priorityVector": {}}
    response["perInterestGroupData"] = igs

if "renderUrls" in form:
    renderURLs = {}
    for renderURL in form.getvalue("renderUrls").split(","):
        renderURLs[renderURL] = True
    response["renderUrls"] = renderURLs

if "adComponentRenderUrls" in form:
    components = {}
    for com in form.getvalue("adComponentRenderUrls").split(","):
        components[com] = True
    response["adComponentRenderUrls"] = components

print(json.dumps(response), file=sys.stderr)
print(json.dumps(response))
