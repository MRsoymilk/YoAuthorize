#!/bin/bash
# Generate self-signed certificate and key for testing
mkdir -p certs
openssl req -x509 -newkey rsa:4096 -keyout certs/server.key -out certs/server.crt -days 365 -nodes \
    -subj "/C=CN/ST=State/L=City/O=Organization/OU=Unit/CN=localhost"
chmod 600 certs/server.key
