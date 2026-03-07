import assert from "node:assert/strict";
import { setTimeout as delay } from "node:timers/promises";

import {
  kBrowserBin,
  kCaptureTargetBarrierMs,
  kCaptureTrace,
  kChromiumVerboseLogging,
  kChromiumVmodule,
  kJobScopedCdp,
  kPrintBrowserLogs,
  kPrintBrowserNetlog,
} from "../src/config.js";
import { runBrowsertrixCapture } from "../upstream/run_capture.js";
import { BrowserInstance, type BrowserPool } from "../upstream/browser_pool.js";

type Options = {
  urls: string[];
  jobTimeoutMs: number;
  squidProxyServer: string;
  geometry: string;
  delayMs: number;
};

type RunSummary = {
  index: number;
  url: string;
  elapsedMs: number;
  exitCode: number;
  failureDetail?: string;
  browserPid?: number;
  reusedBrowser?: boolean;
  browserStdoutTail?: string;
  browserStderrTail?: string;
  browserNetlogTail?: string;
};

const kDefaultUrl = "https://test-target/with-subresource";
const kDefaultTestsuiteSequence = [
  "https://test-target/",
  "https://test-target/with-subresource",
  "https://test-target/with-https-asset-subresource",
  "http://test-target/https-first-http-fails",
  "http://test-target/http-fallback-success",
];

async function main() {
  const options = parseArgs(process.argv.slice(2));
  const browserBin = kBrowserBin;
  assert.ok(browserBin);

  console.log(JSON.stringify({
    mode: "sticky-browser-repro",
    urls: options.urls,
    jobTimeoutMs: options.jobTimeoutMs,
    squidProxyServer: options.squidProxyServer,
    geometry: options.geometry,
    delayMs: options.delayMs,
    captureTrace: kCaptureTrace,
    jobScopedCdp: kJobScopedCdp,
    captureTargetBarrierMs: String(kCaptureTargetBarrierMs),
    chromiumLogging: kChromiumVerboseLogging,
    chromiumVmodule: kChromiumVmodule,
    printBrowserLogs: kPrintBrowserLogs,
    printBrowserNetlog: kPrintBrowserNetlog,
  }));

  const browserPool = new StickyBrowserPool({
    browserBin,
    geometry: options.geometry,
    squidProxyServer: options.squidProxyServer,
  });

  const summaries: RunSummary[] = [];
  try {
    for (const [index, url] of options.urls.entries()) {
      const startedAt = Date.now();
      const result = await runBrowsertrixCapture(
        { url, jobTimeoutMs: options.jobTimeoutMs },
        {
          browserBin,
          geometry: options.geometry,
          squidProxyServer: options.squidProxyServer,
          browserPool: browserPool as unknown as BrowserPool,
        },
      );
      const stdout = Buffer.from(result.artifacts.stdoutLog).toString("utf8");
      const leaseMetadata = browserPool.getLastLeaseMetadata();
      const browserLogs = browserPool.drainBrowserLogs();
      const browserNetlog = await browserPool.readBrowserNetlog();
      const summary: RunSummary = {
        index,
        url,
        elapsedMs: Date.now() - startedAt,
        exitCode: result.exitCode,
        ...(result.failureDetail === undefined ? {} : { failureDetail: result.failureDetail }),
        ...leaseMetadata,
        ...parseStdoutMetadata(stdout),
        ...(kPrintBrowserLogs
          ? {
              ...(browserLogs.stdout === "" ? {} : { browserStdoutTail: tailText(browserLogs.stdout) }),
              ...(browserLogs.stderr === "" ? {} : { browserStderrTail: tailText(browserLogs.stderr) }),
            }
          : {}),
        ...(kPrintBrowserNetlog && browserNetlog !== ""
          ? { browserNetlogTail: tailText(browserNetlog) }
          : {}),
      };
      summaries.push(summary);
      console.log(JSON.stringify(summary));

      if (options.delayMs > 0 && index + 1 < options.urls.length) {
        await delay(options.delayMs);
      }
    }

  } finally {
    await browserPool.close();
  }

  console.log(JSON.stringify({
    mode: "sticky-browser-repro-summary",
    launchedBrowsers: browserPool.launched,
    timeoutRuns: summaries.filter((summary) =>
      summary.failureDetail?.includes("capture exceeded job timeout"),
    ).length,
    reusedTimeoutRuns: summaries.filter((summary) =>
      summary.reusedBrowser === true &&
      summary.failureDetail?.includes("capture exceeded job timeout"),
    ).length,
    ...(kPrintBrowserNetlog && browserPool.finalNetlog !== ""
      ? { finalBrowserNetlogTail: tailText(browserPool.finalNetlog) }
      : {}),
  }));
}

function parseArgs(argv: string[]): Options {
  const urls: string[] = [];
  let jobTimeoutMs = 15_000;
  let squidProxyServer = "http://127.0.0.1:3128";
  let geometry = "1600x900";
  let delayMs = 0;

  for (let i = 0; i < argv.length; i++) {
    const arg = argv[i]!;
    switch (arg) {
      case "--url":
        urls.push(requireValue(argv, ++i, "--url"));
        break;
      case "--count": {
        const count = parsePositiveInt(requireValue(argv, ++i, "--count"), "--count");
        while (urls.length < count) {
          urls.push(kDefaultUrl);
        }
        break;
      }
      case "--timeout-ms":
        jobTimeoutMs = parsePositiveInt(requireValue(argv, ++i, "--timeout-ms"), "--timeout-ms");
        break;
      case "--proxy":
        squidProxyServer = requireValue(argv, ++i, "--proxy");
        break;
      case "--geometry":
        geometry = requireValue(argv, ++i, "--geometry");
        break;
      case "--delay-ms":
        delayMs = parseNonNegativeInt(requireValue(argv, ++i, "--delay-ms"), "--delay-ms");
        break;
      case "--sequence":
        if (requireValue(argv, ++i, "--sequence") !== "testsuite") {
          throw new Error("only --sequence testsuite is supported");
        }
        urls.push(...kDefaultTestsuiteSequence);
        break;
      case "--help":
        printUsage();
        process.exit(0);
      default:
        throw new Error(`unknown argument ${arg}`);
    }
  }

  if (urls.length === 0) {
    urls.push(kDefaultUrl, kDefaultUrl, kDefaultUrl);
  }

  return {
    urls,
    jobTimeoutMs,
    squidProxyServer,
    geometry,
    delayMs,
  };
}

function printUsage() {
  console.log([
    "Usage: npm run repro:sticky-browser -- [options]",
    "",
    "Options:",
    "  --url <url>           Append one URL to the sequential run list",
    "  --count <n>           Repeat the default URL until the list has n entries",
    "  --sequence testsuite  Use the URL sequence that exposed the testsuite regression",
    "  --timeout-ms <n>      Per-run job timeout in milliseconds (default: 15000)",
    "  --proxy <url>         Squid proxy URL (default: http://127.0.0.1:3128)",
    "  --geometry <wxh>      Browser geometry (default: 1600x900)",
    "  --delay-ms <n>        Delay between runs in milliseconds",
  ].join("\n"));
}

function requireValue(argv: string[], index: number, option: string): string {
  const value = argv[index];
  if (value === undefined) {
    throw new Error(`missing value for ${option}`);
  }
  return value;
}

function parsePositiveInt(raw: string, option: string): number {
  const value = Number.parseInt(raw, 10);
  if (!Number.isInteger(value) || value < 1) {
    throw new Error(`${option} must be a positive integer`);
  }
  return value;
}

function parseNonNegativeInt(raw: string, option: string): number {
  const value = Number.parseInt(raw, 10);
  if (!Number.isInteger(value) || value < 0) {
    throw new Error(`${option} must be a non-negative integer`);
  }
  return value;
}

function parseStdoutMetadata(stdout: string): {
  browserPid?: number;
  reusedBrowser?: boolean;
} {
  const browserPidMatch = stdout.match(/browser_pid=(\d+)/);
  const reusedBrowserMatch = stdout.match(/reused_browser=(true|false)/);
  return {
    ...(browserPidMatch === null ? {} : { browserPid: Number.parseInt(browserPidMatch[1]!, 10) }),
    ...(reusedBrowserMatch === null ? {} : { reusedBrowser: reusedBrowserMatch[1] === "true" }),
  };
}

class StickyBrowserPool {
  readonly browserBin: string;
  readonly geometry: string;
  readonly squidProxyServer: string;
  browser?: BrowserInstance;
  launched = 0;
  handoutCount = 0;
  finalNetlog = "";
  lastLeaseMetadata: {
    browserPid?: number;
    reusedBrowser?: boolean;
  } = {};

  constructor(options: {
    browserBin: string;
    geometry: string;
    squidProxyServer: string;
  }) {
    this.browserBin = options.browserBin;
    this.geometry = options.geometry;
    this.squidProxyServer = options.squidProxyServer;
  }

  async acquire() {
    if (this.browser === undefined || !await this.browser.isHealthy()) {
      if (this.browser !== undefined) {
        await this.browser.close().catch(() => undefined);
      }
      this.browser = await BrowserInstance.launch({
        browserBin: this.browserBin,
        geometry: this.geometry,
        squidProxyServer: this.squidProxyServer,
      });
      this.launched++;
      this.handoutCount = 0;
    }

    this.browser.drainLogs();
    const reused = this.handoutCount > 0;
    this.handoutCount++;
    this.lastLeaseMetadata = {
      browserPid: this.browser.proc.pid,
      reusedBrowser: reused,
    };
    return {
      browser: this.browser,
      reused,
    };
  }

  async release(browser: BrowserInstance) {
    if (!await browser.isHealthy()) {
      await this.discard(browser);
    }
  }

  async discard(browser: BrowserInstance) {
    if (this.browser === browser) {
      this.browser = undefined;
    }
    await browser.close().catch(() => undefined);
  }

  getLastLeaseMetadata() {
    return this.lastLeaseMetadata;
  }

  drainBrowserLogs() {
    if (this.browser === undefined) {
      return { stdout: "", stderr: "" };
    }
    return this.browser.drainLogs();
  }

  async readBrowserNetlog() {
    if (this.browser === undefined) {
      return "";
    }
    return await this.browser.readNetlog();
  }

  async close() {
    if (this.browser !== undefined) {
      await this.browser.close().catch(() => undefined);
      this.finalNetlog = this.browser.finalNetlog;
      this.browser = undefined;
    }
  }
}

function tailText(value: string): string {
  const trimmed = value.trim();
  if (trimmed === "") {
    return "";
  }
  const lines = trimmed.split("\n");
  const tail = lines.slice(-20).join("\n");
  return tail.length <= 2000 ? tail : tail.slice(tail.length - 2000);
}

await main();
