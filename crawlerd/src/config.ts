import { fileURLToPath } from "node:url";

export const kBrowserBin = "chromium";
export const kBrowserGeometry = "1600x900";
export const kListenSocketPath = fileURLToPath(new URL("../../../.crawlerd.sock", import.meta.url));

export const kChromiumVerboseLogging = false;
export const kChromiumVmodule = "";

export const kCaptureTrace = false;
export const kJobScopedCdp = false;
export const kCaptureTargetBarrierMs = 0;

export const kPrintBrowserLogs = false;
export const kPrintBrowserNetlog = false;
