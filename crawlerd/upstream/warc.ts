import { randomUUID } from "node:crypto";

import type { CapturedExchange } from "./http_capture.js";

export function buildWarc(exchange: CapturedExchange): Uint8Array {
  const now = new Date().toISOString();
  const responseRecordId = `urn:uuid:${randomUUID()}`;
  const requestRecordId = `urn:uuid:${randomUUID()}`;

  const httpResponse = [
    `HTTP/1.1 ${exchange.statusCode} ${exchange.statusMessage}`,
    ...Object.entries(exchange.headers).map(([name, value]) => `${name}: ${value}`),
    "",
    "",
  ].join("\r\n");

  const requestBody = [
    `GET ${new URL(exchange.finalUrl).pathname || "/"}${new URL(exchange.finalUrl).search} HTTP/1.1`,
    `Host: ${new URL(exchange.finalUrl).host}`,
    "User-Agent: crawlerd/0.1.0",
    "",
    "",
  ].join("\r\n");

  const responseHeader = [
    "WARC/1.1",
    "WARC-Type: response",
    `WARC-Target-URI: ${exchange.finalUrl}`,
    `WARC-Date: ${now}`,
    `WARC-Record-ID: <${responseRecordId}>`,
    "Content-Type: application/http; msgtype=response",
    `Content-Length: ${Buffer.byteLength(httpResponse) + exchange.body.byteLength}`,
    "",
    httpResponse,
  ].join("\r\n");

  const requestRecord = [
    "WARC/1.1",
    "WARC-Type: request",
    `WARC-Target-URI: ${exchange.finalUrl}`,
    `WARC-Date: ${now}`,
    `WARC-Record-ID: <${requestRecordId}>`,
    `WARC-Concurrent-To: <${responseRecordId}>`,
    "Content-Type: application/http; msgtype=request",
    `Content-Length: ${Buffer.byteLength(requestBody)}`,
    "",
    requestBody,
    "\r\n",
  ].join("\r\n");

  return Buffer.concat([
    Buffer.from(responseHeader, "utf8"),
    Buffer.from(exchange.body),
    Buffer.from("\r\n\r\n", "utf8"),
    Buffer.from(requestRecord, "utf8"),
  ]);
}
