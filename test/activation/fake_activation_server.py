#!/usr/bin/env python3

import argparse
import base64
import http.server
import json
import pathlib
import ssl


class ActivationHandler(http.server.BaseHTTPRequestHandler):
    server_version = "YoAuthorizeFakeActivation/1"

    def do_POST(self):
        if self.path != "/v1/activate":
            self.send_error_json(404, "not_found")
            return
        try:
            content_length = int(self.headers.get("Content-Length", "0"))
        except ValueError:
            self.send_error_json(400, "invalid_request")
            return
        if content_length <= 0 or content_length > 65536:
            self.send_error_json(413, "request_too_large")
            return
        try:
            request = json.loads(self.rfile.read(content_length))
        except (json.JSONDecodeError, UnicodeDecodeError):
            self.send_error_json(400, "invalid_json")
            return
        if (
            request.get("protocol_version") != 1
            or request.get("product_id") != self.server.product_id
            or request.get("activation_code") != self.server.activation_code
            or not isinstance(request.get("machine_id"), str)
            or not request["machine_id"]
        ):
            self.send_error_json(403, "activation_denied")
            return
        response = {
            "license": base64.b64encode(self.server.license_bytes).decode("ascii"),
            "realtime_url": "wss://localhost/v1/events",
            "access_token": "fake-short-lived-token",
            "token_expire_time": 4102444800,
        }
        self.send_json(200, response)

    def send_error_json(self, status, code):
        self.send_json(status, {"error": code})

    def send_json(self, status, body):
        encoded = json.dumps(body, separators=(",", ":")).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(encoded)))
        self.end_headers()
        self.wfile.write(encoded)

    def log_message(self, format_string, *args):
        return


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--certificate", required=True)
    parser.add_argument("--private-key", required=True)
    parser.add_argument("--license", required=True)
    parser.add_argument("--port-file", required=True)
    parser.add_argument("--product-id", required=True)
    parser.add_argument("--activation-code", required=True)
    args = parser.parse_args()

    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), ActivationHandler)
    server.product_id = args.product_id
    server.activation_code = args.activation_code
    server.license_bytes = pathlib.Path(args.license).read_bytes()
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(args.certificate, args.private_key)
    server.socket = context.wrap_socket(server.socket, server_side=True)
    pathlib.Path(args.port_file).write_text(str(server.server_port), encoding="ascii")
    server.serve_forever()


if __name__ == "__main__":
    main()
