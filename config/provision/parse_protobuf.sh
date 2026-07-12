#!/bin/sh
set -e

protoc --proto_path=res/protobuf/ --python_out=config/provision res/protobuf/factory_data.proto
protoc --proto_path=res/protobuf/ --python_out=config/provision res/protobuf/nfc_tag.proto
