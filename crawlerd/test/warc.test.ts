import test from "node:test";
import assert from "node:assert/strict";

import type { CapturedExchange } from "../upstream/http_capture.js";
import { buildWarc } from "../upstream/warc.js";

test("buildWarc preserves captured subresource methods in request records", () => {
  const exchange: CapturedExchange = {
    finalUrl: "https://example.test/seed",
    statusCode: 200,
    statusMessage: "OK",
    headers: {
      "content-type": "text/html; charset=utf-8",
    },
    body: Buffer.from("<html><body>seed</body></html>", "utf8"),
    timestamp: "2026-03-10T00:00:00.000Z",
    redirectChain: ["https://example.test/seed"],
    mainDocumentRedirects: [],
    resources: [
      {
        url: "https://example.test/submit?source=page",
        method: "POST",
        statusCode: 201,
        statusMessage: "Created",
        headers: {
          "content-type": "text/plain; charset=utf-8",
        },
        body: Buffer.from("submitted", "utf8"),
        timestamp: "2026-03-10T00:00:01.000Z",
      },
    ],
    title: "Seed",
  };

  const warc = buildWarc(exchange);
  const warcText = Buffer.from(warc.bytes).toString("utf8");

  assert.equal(warc.cdxRecords.length, 2);
  assert.match(warcText, /WARC-Target-URI: https:\/\/example\.test\/submit\?source=page/);
  assert.match(warcText, /POST \/submit\?source=page HTTP\/1\.1/);
});
