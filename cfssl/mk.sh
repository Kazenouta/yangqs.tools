#!/bin/bash

set -e

cfssl genkey -loglevel 1 -initca ca.json | cfssljson -bare _ca

cfssl gencert -loglevel 1 -ca _ca.pem -ca-key _ca-key.pem server.json | cfssljson -bare _server
