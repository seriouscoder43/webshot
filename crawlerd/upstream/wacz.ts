import type { RunRequest } from "../src/contracts.js";
import type { CapturedExchange } from "./http_capture.js";
import { createStoredZip } from "./zip.js";

export function buildWacz(
  run: RunRequest,
  exchange: CapturedExchange,
  pagesJsonl: Uint8Array,
  warcBytes: Uint8Array,
  stdoutLog: Uint8Array,
  stderrLog: Uint8Array,
): Uint8Array {
  const datapackage = Buffer.from(
    JSON.stringify(
      {
        profile: "data-package",
        created: new Date().toISOString(),
        wacz_version: "1.1.1",
        software: "crawlerd rewritten upstream",
        title: run.url,
        resources: [
          {
            name: "archive",
            path: "archive/data.warc",
            bytes: warcBytes.byteLength,
          },
          {
            name: "pages",
            path: "pages/pages.jsonl",
            bytes: pagesJsonl.byteLength,
          },
          {
            name: "stdout log",
            path: "logs/stdout.log",
            bytes: stdoutLog.byteLength,
          },
          {
            name: "stderr log",
            path: "logs/stderr.log",
            bytes: stderrLog.byteLength,
          },
          {
            name: "index",
            path: "indexes/index.cdxj",
            bytes: Buffer.byteLength(buildCdxj(exchange), "utf8"),
          },
        ],
      },
      null,
      2,
    ),
    "utf8",
  );

  return createStoredZip([
    { name: "datapackage.json", body: datapackage },
    { name: "archive/data.warc", body: warcBytes },
    { name: "pages/pages.jsonl", body: pagesJsonl },
    { name: "logs/stdout.log", body: stdoutLog },
    { name: "logs/stderr.log", body: stderrLog },
    {
      name: "indexes/index.cdxj",
      body: Buffer.from(buildCdxj(exchange), "utf8"),
    },
  ]);
}

function buildCdxj(exchange: CapturedExchange): string {
  const timestamp = new Date().toISOString().replace(/[-:TZ.]/g, "").slice(0, 14);
  const resources = [
    {
      url: exchange.finalUrl,
      statusCode: exchange.statusCode,
      headers: exchange.headers,
    },
    ...exchange.resources,
  ];
  const seen = new Set<string>();

  return resources
    .filter((resource) => {
      const key = `${resource.url}\n${resource.statusCode}`;
      if (seen.has(key)) {
        return false;
      }
      seen.add(key);
      return true;
    })
    .map((resource) => `${resource.url} ${timestamp} ${JSON.stringify({
      url: resource.url,
      status: `${resource.statusCode}`,
      mime: resource.headers["content-type"] ?? "application/octet-stream",
      filename: "archive/data.warc",
      length: "0",
      offset: "0",
    })}\n`)
    .join("");
}
