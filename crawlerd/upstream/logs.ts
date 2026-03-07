import type { RunRequest } from "../src/contracts.js";
import type { CapturedExchange } from "./http_capture.js";

type SuccessLogOptions = {
  browserBin: string;
  geometry?: string;
  browserPid: number;
  reusedBrowser: boolean;
};

export function buildSuccessStdoutLog(
  run: RunRequest,
  exchange: CapturedExchange,
  options: SuccessLogOptions,
): Uint8Array {
  return Buffer.from(
    [
      "browsertrix rewrite start",
      `seed_url=${run.url}`,
      `final_url=${exchange.finalUrl}`,
      `status=${exchange.statusCode}`,
      `redirects=${exchange.redirectChain.length - 1}`,
      `browser_bin=${options.browserBin}`,
      `browser_pid=${options.browserPid}`,
      `reused_browser=${options.reusedBrowser}`,
      ...(options.geometry ? [`geometry=${options.geometry}`] : []),
      "browsertrix rewrite done",
      "",
    ].join("\n"),
    "utf8",
  );
}

export function buildFailureStderrLog(error: unknown): Uint8Array {
  const message = error instanceof Error ? error.message : String(error);
  return Buffer.from(`${message}\n`, "utf8");
}
