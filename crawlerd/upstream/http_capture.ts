import { setTimeout as delay } from "node:timers/promises";

import { kCaptureTargetBarrierMs, kCaptureTrace, kJobScopedCdp } from "../src/config.js";
import type { BrowserInstance, BrowserPool } from "./browser_pool.js";
import type { CdpClient, CdpEvent } from "./cdp_client.js";

export type CapturedExchange = {
  finalUrl: string;
  statusCode: number;
  statusMessage: string;
  headers: Record<string, string>;
  body: Uint8Array;
  timestamp: string;
  redirectChain: string[];
  mainDocumentRedirects: CapturedMainDocumentRedirect[];
  resources: CapturedResource[];
  title?: string;
};

export type CapturedMainDocumentRedirect = {
  url: string;
  statusCode: number;
  statusMessage: string;
  headers: Record<string, string>;
  timestamp: string;
};

export type CapturedResource = {
  url: string;
  method: string;
  statusCode: number;
  statusMessage: string;
  headers: Record<string, string>;
  body: Uint8Array;
  timestamp: string;
};

export type CaptureMetadata = {
  browserPid: number;
  reusedBrowser: boolean;
};

export type BrowserCaptureResult = {
  exchange: CapturedExchange;
  metadata: CaptureMetadata;
};

type OpenCapture = {
  exchange: CapturedExchange;
  targetId: string;
  browserContextId: string;
  sessionId: string;
};

type ProxyOptions = {
  browserPool: BrowserPool;
  pageLoadTimeoutMs: number;
  postLoadDelayMs: number;
  netIdleWaitMs: number;
  pageExtraDelayMs: number;
  behaviorTimeoutMs: number;
  jobTimeoutMs: number;
  maxBodyBytes: number;
  lang: string;
  scopeType: "page" | "page-spa";
};

const kCleanupTimeoutMs = 1500;

export async function captureViaProxy(
  targetUrl: string,
  options: ProxyOptions,
): Promise<BrowserCaptureResult> {
  const trace = createCaptureTrace(targetUrl);
  const lease = await options.browserPool.acquire();
  trace("lease_acquired", {
    browserPid: lease.browser.proc.pid ?? 0,
    reusedBrowser: lease.reused,
  });
  const cdp = kJobScopedCdp ? await lease.browser.connectCdp() : lease.browser.cdp;
  const cleanupTimeoutMs = Math.max(kCleanupTimeoutMs, kCaptureTargetBarrierMs + 500);
  trace("cdp_client_ready", {
    scoped: kJobScopedCdp,
    barrierMs: kCaptureTargetBarrierMs,
    cleanupTimeoutMs,
  });
  try {
    const capture = await raceWithTimeout(
      captureWithBrowser(lease.browser, cdp, targetUrl, options, trace),
      options.jobTimeoutMs,
      `capture exceeded job timeout ${Math.floor(options.jobTimeoutMs / 1000)}s`,
    );
    trace("capture_complete");
    let cleanupError: Error | undefined;
    try {
      trace("cleanup_start", {
        targetId: capture.targetId,
        browserContextId: capture.browserContextId,
        sessionId: capture.sessionId,
      });
      cleanupError = await raceWithTimeout(
        closeCaptureTarget(
          lease.browser,
          cdp,
          capture.targetId,
          capture.sessionId,
          capture.browserContextId,
          trace,
        ),
        cleanupTimeoutMs,
        `capture cleanup exceeded ${cleanupTimeoutMs}ms`,
      );
    } catch (error) {
      cleanupError = error instanceof Error ? error : new Error(String(error));
    }

    if (cleanupError !== undefined) {
      trace("cleanup_failed", { message: cleanupError.message });
      await options.browserPool.discard(lease.browser);
    } else {
      trace("cleanup_complete");
      await options.browserPool.release(lease.browser);
    }
    return {
      exchange: capture.exchange,
      metadata: {
        browserPid: lease.browser.proc.pid ?? 0,
        reusedBrowser: lease.reused,
      },
    };
  } catch (error) {
    trace("capture_failed", {
      message: error instanceof Error ? error.message : String(error),
    });
    if (kJobScopedCdp) {
      await cdp.close().catch(() => undefined);
    }
    await options.browserPool.discard(lease.browser);
    throw error;
  }
}

async function captureWithBrowser(
  browser: BrowserInstance,
  cdp: CdpClient,
  targetUrl: string,
  options: ProxyOptions,
  trace: CaptureTrace,
): Promise<OpenCapture> {
  trace("context_create_start");
  const browserContext = await cdp.send<{ browserContextId: string }>(
    "Target.createBrowserContext",
    kJobScopedCdp ? { disposeOnDetach: true } : {},
  );
  trace("context_create_complete", { browserContextId: browserContext.browserContextId });
  trace("target_create_start");
  const target = await cdp.send<{ targetId: string }>("Target.createTarget", {
    url: "about:blank",
    browserContextId: browserContext.browserContextId,
  });
  trace("target_create_complete", { targetId: target.targetId });
  trace("target_attach_start");
  const attached = await cdp.send<{ sessionId: string }>("Target.attachToTarget", {
    targetId: target.targetId,
    flatten: true,
  });
  trace("target_attach_complete", { sessionId: attached.sessionId });

  const sessionId = attached.sessionId;
  const tracker = new PageTracker(sessionId, target.targetId, trace);
  const removeListener = cdp.addListener((event) => {
    tracker.handleEvent(event);
  });

  try {
    trace("page_enable_start");
    await cdp.send("Page.enable", {}, sessionId);
    await cdp.send("Runtime.enable", {}, sessionId);
    await cdp.send("Network.enable", {}, sessionId);
    await cdp.send("Page.setLifecycleEventsEnabled", { enabled: true }, sessionId);
    await cdp.send("Network.setCacheDisabled", { cacheDisabled: true }, sessionId);
    await cdp.send("Network.setBypassServiceWorker", { bypass: true }, sessionId);
    await cdp.send("Network.setExtraHTTPHeaders", {
      headers: {
        "Accept-Language": options.lang,
      },
    }, sessionId);
    trace("page_enable_complete");

    trace("frame_tree_start");
    const frameTree = await cdp.send<{
      frameTree: {
        frame: {
          id: string;
        };
      };
    }>("Page.getFrameTree", {}, sessionId);
    tracker.mainFrameId = frameTree.frameTree.frame.id;
    trace("frame_tree_complete", { mainFrameId: tracker.mainFrameId });

    trace("navigate_start", { url: targetUrl });
    await cdp.send("Page.navigate", { url: targetUrl }, sessionId);
    trace("navigate_complete");
    trace("wait_for_load_start", { timeoutMs: options.pageLoadTimeoutMs });
    await tracker.waitForLoad(options.pageLoadTimeoutMs);
    trace("wait_for_load_complete");

    if (options.postLoadDelayMs > 0) {
      trace("post_load_delay_start", { delayMs: options.postLoadDelayMs });
      await delay(options.postLoadDelayMs);
      trace("post_load_delay_complete");
    }
    if (options.scopeType === "page-spa" && options.behaviorTimeoutMs > 0) {
      trace("site_behavior_start", { timeoutMs: options.behaviorTimeoutMs });
      await runSiteBehavior(cdp, sessionId, options.behaviorTimeoutMs);
      trace("site_behavior_complete");
    }
    if (options.netIdleWaitMs > 0) {
      trace("wait_for_idle_start", {
        idleMs: options.netIdleWaitMs,
        timeoutMs: options.netIdleWaitMs + 1000,
      });
      await tracker.waitForIdle(options.netIdleWaitMs, options.netIdleWaitMs + 1000);
      trace("wait_for_idle_complete");
    }
    if (options.pageExtraDelayMs > 0) {
      trace("page_extra_delay_start", { delayMs: options.pageExtraDelayMs });
      await delay(options.pageExtraDelayMs);
      trace("page_extra_delay_complete");
    }

    trace("wait_for_main_document_start", { timeoutMs: options.pageLoadTimeoutMs });
    await tracker.waitForMainDocument(options.pageLoadTimeoutMs);
    trace("wait_for_main_document_complete");
    trace("read_dom_state_start");
    const domState = await readDomState(cdp, sessionId);
    trace("read_dom_state_complete", { finalUrl: domState.finalUrl });
    trace("read_body_start");
    const retainedBodyBudget = {
      maxBytes: options.maxBodyBytes,
      retainedBytes: 0,
    };
    const body = await tracker.readBody(
      cdp,
      sessionId,
      retainedBodyBudget,
      Buffer.from(domState.html, "utf8"),
    );
    trace("read_body_complete", { bodyBytes: body.byteLength });
    trace("read_resources_start");
    const resources = await tracker.readResources(cdp, sessionId, retainedBodyBudget);
    trace("read_resources_complete", { count: resources.length });

    return {
      exchange: {
        finalUrl: domState.finalUrl,
        statusCode: tracker.mainResponse?.statusCode ?? 0,
        statusMessage: tracker.mainResponse?.statusMessage ?? "",
        headers: tracker.mainResponse?.headers ?? {},
        body,
        timestamp: tracker.mainResponse?.timestamp ?? new Date().toISOString(),
        redirectChain: tracker.redirectChain.length > 0 ? tracker.redirectChain : [targetUrl],
        mainDocumentRedirects: tracker.mainDocumentRedirects,
        resources,
        ...(domState.title === undefined ? {} : { title: domState.title }),
      },
      targetId: target.targetId,
      browserContextId: browserContext.browserContextId,
      sessionId,
    };
  } catch (error) {
    trace("capture_with_browser_failed", {
      message: error instanceof Error ? error.message : String(error),
    });
    await closeCaptureTarget(
      browser,
      cdp,
      target.targetId,
      sessionId,
      browserContext.browserContextId,
      trace,
    ).catch(() => undefined);
    throw error;
  } finally {
    removeListener();
  }
}

type CaptureTrace = (
  event: string,
  details?: Record<string, string | number | boolean>,
) => void;

function createCaptureTrace(targetUrl: string): CaptureTrace {
  if (!kCaptureTrace) {
    return () => undefined;
  }

  const startedAt = Date.now();
  return (event, details = {}) => {
    const payload = {
      trace: "capture",
      targetUrl,
      elapsedMs: Date.now() - startedAt,
      event,
      ...details,
    };
    process.stderr.write(`${JSON.stringify(payload)}\n`);
  };
}

class PageTracker {
  readonly sessionId: string;
  readonly targetId: string;
  readonly inflight = new Set<string>();
  readonly resourcesByRequestId = new Map<string, CapturedResourceState>();
  readonly redirectedResources: CapturedResource[] = [];
  readonly redirectChain: string[] = [];
  readonly mainDocumentRedirects: CapturedMainDocumentRedirect[] = [];
  readonly loadWaiters: Array<{ resolve: () => void; reject: (error: Error) => void }> = [];
  readonly mainDocumentWaiters: Array<{ resolve: () => void; reject: (error: Error) => void }> = [];
  readonly trace: CaptureTrace;
  mainFrameId?: string;
  mainRequestId?: string;
  mainResponse?: {
    statusCode: number;
    statusMessage: string;
    headers: Record<string, string>;
    timestamp: string;
  };
  mainRequestLoaded = false;
  mainRequestFailure?: string;
  loaded = false;
  lastNetworkAt = Date.now();

  constructor(sessionId: string, targetId: string, trace: CaptureTrace) {
    this.sessionId = sessionId;
    this.targetId = targetId;
    this.trace = trace;
  }

  handleEvent(event: CdpEvent) {
    if (event.method === "Target.targetCrashed") {
      this.handleTargetCrashed(event.params);
      return;
    }

    if (event.sessionId !== this.sessionId) {
      return;
    }

    switch (event.method) {
      case "Page.loadEventFired":
        this.loaded = true;
        this.resolveWaiters(this.loadWaiters);
        return;
      case "Network.requestWillBeSent":
        this.handleRequestWillBeSent(event.params);
        return;
      case "Network.responseReceived":
        this.handleResponseReceived(event.params);
        return;
      case "Network.loadingFinished":
        this.handleLoadingFinished(event.params);
        return;
      case "Network.loadingFailed":
        this.handleLoadingFailed(event.params);
        return;
      default:
        return;
    }
  }

  async waitForLoad(timeoutMs: number): Promise<void> {
    if (this.loaded) {
      return;
    }
    await waitForCondition(
      () => this.loaded,
      timeoutMs,
      "timed out waiting for page load",
      this.loadWaiters,
    );
  }

  async waitForMainDocument(timeoutMs: number): Promise<void> {
    if (this.mainRequestFailure !== undefined) {
      throw new Error(this.mainRequestFailure);
    }
    if (this.mainResponse !== undefined && this.mainRequestLoaded) {
      return;
    }
    await waitForCondition(
      () => this.mainResponse !== undefined && this.mainRequestLoaded,
      timeoutMs,
      "timed out waiting for main document response",
      this.mainDocumentWaiters,
    );
    if (this.mainRequestFailure !== undefined) {
      throw new Error(this.mainRequestFailure);
    }
  }

  async waitForIdle(idleMs: number, timeoutMs: number): Promise<void> {
    const deadline = Date.now() + timeoutMs;
    while (true) {
      if (this.inflight.size === 0 && Date.now() - this.lastNetworkAt >= idleMs) {
        return;
      }
      if (Date.now() >= deadline) {
        throw new Error(`timed out waiting for network idle ${idleMs}ms`);
      }
      await delay(50);
    }
  }

  async readBody(
    cdp: CdpClient,
    sessionId: string,
    retainedBodyBudget: RetainedBodyBudget,
    fallbackBody: Buffer,
  ): Promise<Uint8Array> {
    if (this.mainRequestId === undefined) {
      return retainBody(fallbackBody, retainedBodyBudget);
    }

    try {
      const body = await cdp.send<{ body: string; base64Encoded: boolean }>(
        "Network.getResponseBody",
        { requestId: this.mainRequestId },
        sessionId,
      );
      const bytes = body.base64Encoded
        ? Buffer.from(body.body, "base64")
        : Buffer.from(body.body, "utf8");
      return retainBody(bytes, retainedBodyBudget);
    } catch {
      return retainBody(fallbackBody, retainedBodyBudget);
    }
  }

  async readResources(
    cdp: CdpClient,
    sessionId: string,
    retainedBodyBudget: RetainedBodyBudget,
  ): Promise<CapturedResource[]> {
    const resources = [...this.redirectedResources];

    for (const [requestId, resource] of this.resourcesByRequestId.entries()) {
      if (!resource.loaded ||
        resource.statusCode === undefined ||
        resource.statusMessage === undefined ||
        resource.headers === undefined ||
        resource.timestamp === undefined) {
        continue;
      }

      const finalizedResource = {
        url: resource.url,
        method: resource.method,
        statusCode: resource.statusCode,
        statusMessage: resource.statusMessage,
        headers: resource.headers,
        timestamp: resource.timestamp,
      };
      const body = await this.readResourceBody(
        cdp,
        sessionId,
        requestId,
        finalizedResource,
        retainedBodyBudget,
      );
      if (body === undefined) {
        continue;
      }

      resources.push({
        ...finalizedResource,
        body,
      });
    }

    resources.sort((left, right) => left.timestamp.localeCompare(right.timestamp));
    return resources;
  }

  private handleRequestWillBeSent(params: Record<string, unknown>) {
    const requestId = stringValue(params.requestId);
    const request = recordValue(params.request);
    const url = stringValue(request.url);
    const method = stringValue(request.method) ?? "GET";
    if (requestId === undefined || url === undefined || url.startsWith("data:")) {
      return;
    }

    this.inflight.add(requestId);
    this.lastNetworkAt = Date.now();

    const frameId = stringValue(params.frameId);
    const type = stringValue(params.type);
    if (frameId === this.mainFrameId && type === "Document") {
      if (this.mainRequestId === undefined) {
        this.mainRequestId = requestId;
        this.trace("main_request_started", { requestId, url });
      }
      if (requestId !== this.mainRequestId) {
        return;
      }
      this.recordMainDocumentRedirect(recordValue(params.redirectResponse));
      if (this.redirectChain[this.redirectChain.length - 1] !== url) {
        this.redirectChain.push(url);
      }
      return;
    }

    this.recordResourceRedirect(requestId, recordValue(params.redirectResponse));
    this.resourcesByRequestId.set(requestId, {
      url,
      method,
      loaded: false,
    });
  }

  private handleResponseReceived(params: Record<string, unknown>) {
    const requestId = stringValue(params.requestId);
    if (requestId === undefined) {
      return;
    }

    const response = recordValue(params.response);
    const headers = normalizeHeaders(recordValue(response.headers));
    const resource = this.resourcesByRequestId.get(requestId);
    if (resource !== undefined) {
      resource.url = stringValue(response.url) ?? resource.url;
      resource.statusCode = numberValue(response.status) ?? 0;
      resource.statusMessage = stringValue(response.statusText) ?? "";
      resource.headers = headers;
      resource.timestamp = new Date().toISOString();
    }

    if (requestId !== this.mainRequestId) {
      return;
    }
    this.mainResponse = {
      statusCode: numberValue(response.status) ?? 0,
      statusMessage: stringValue(response.statusText) ?? "",
      headers,
      timestamp: new Date().toISOString(),
    };
    this.trace("main_response_received", {
      requestId,
      statusCode: this.mainResponse.statusCode,
      url: stringValue(response.url) ?? "",
    });
  }

  private handleLoadingFinished(params: Record<string, unknown>) {
    const requestId = stringValue(params.requestId);
    if (requestId !== undefined) {
      this.inflight.delete(requestId);
      this.lastNetworkAt = Date.now();
      const resource = this.resourcesByRequestId.get(requestId);
      if (resource !== undefined) {
        resource.loaded = true;
      }
    }
    if (requestId !== this.mainRequestId) {
      return;
    }
    this.mainRequestLoaded = true;
    this.trace("main_request_loaded", { requestId: requestId ?? "" });
    this.resolveWaiters(this.mainDocumentWaiters);
  }

  private handleLoadingFailed(params: Record<string, unknown>) {
    const requestId = stringValue(params.requestId);
    if (requestId !== undefined) {
      this.inflight.delete(requestId);
      this.lastNetworkAt = Date.now();
      const resource = this.resourcesByRequestId.get(requestId);
      if (resource !== undefined &&
        resource.statusCode !== undefined &&
        !responseCanHaveBody(resource.method, resource.statusCode)) {
        resource.loaded = true;
      } else {
        this.resourcesByRequestId.delete(requestId);
      }
    }
    if (requestId !== this.mainRequestId) {
      return;
    }
    this.mainRequestFailure = stringValue(params.errorText) ?? "main document request failed";
    this.trace("main_request_failed", {
      requestId: requestId ?? "",
      errorText: this.mainRequestFailure,
    });
    this.rejectWaiters(this.mainDocumentWaiters, new Error(this.mainRequestFailure));
  }

  private handleTargetCrashed(params: Record<string, unknown>) {
    if (stringValue(params.targetId) !== this.targetId) {
      return;
    }

    this.mainRequestFailure = "page target crashed";
    this.trace("target_crashed", { targetId: this.targetId });
    const error = new Error(this.mainRequestFailure);
    this.rejectWaiters(this.loadWaiters, error);
    this.rejectWaiters(this.mainDocumentWaiters, error);
  }

  private resolveWaiters(waiters: Array<{ resolve: () => void; reject: (error: Error) => void }>) {
    for (const waiter of waiters.splice(0)) {
      waiter.resolve();
    }
  }

  private rejectWaiters(
    waiters: Array<{ resolve: () => void; reject: (error: Error) => void }>,
    error: Error,
  ) {
    for (const waiter of waiters.splice(0)) {
      waiter.reject(error);
    }
  }

  private recordMainDocumentRedirect(redirectResponse: Record<string, unknown>) {
    if (Object.keys(redirectResponse).length === 0) {
      return;
    }

    const url = stringValue(redirectResponse.url);
    const statusCode = numberValue(redirectResponse.status);
    if (url === undefined || statusCode === undefined) {
      return;
    }

    const redirect: CapturedMainDocumentRedirect = {
      url,
      statusCode,
      statusMessage: stringValue(redirectResponse.statusText) ?? "",
      headers: normalizeHeaders(recordValue(redirectResponse.headers)),
      timestamp: new Date().toISOString(),
    };

    const previous = this.mainDocumentRedirects[this.mainDocumentRedirects.length - 1];
    if (previous?.url === redirect.url && previous.statusCode === redirect.statusCode) {
      return;
    }
    this.mainDocumentRedirects.push(redirect);
  }

  private recordResourceRedirect(requestId: string, redirectResponse: Record<string, unknown>) {
    if (Object.keys(redirectResponse).length === 0) {
      return;
    }

    const resource = this.resourcesByRequestId.get(requestId);
    if (resource === undefined) {
      return;
    }

    const url = stringValue(redirectResponse.url) ?? resource.url;
    const statusCode = numberValue(redirectResponse.status);
    if (statusCode === undefined) {
      return;
    }

    this.redirectedResources.push({
      url,
      method: resource.method,
      statusCode,
      statusMessage: stringValue(redirectResponse.statusText) ?? "",
      headers: normalizeHeaders(recordValue(redirectResponse.headers)),
      body: new Uint8Array(),
      timestamp: new Date().toISOString(),
    });
  }

  private async readResourceBody(
    cdp: CdpClient,
    sessionId: string,
    requestId: string,
    resource: {
      url: string;
      method: string;
      statusCode: number;
      statusMessage: string;
      headers: Record<string, string>;
      timestamp: string;
    },
    retainedBodyBudget: RetainedBodyBudget,
  ): Promise<Uint8Array | undefined> {
    if (!responseCanHaveBody(resource.method, resource.statusCode)) {
      return new Uint8Array();
    }

    let body: { body: string; base64Encoded: boolean };
    try {
      body = await cdp.send<{ body: string; base64Encoded: boolean }>(
        "Network.getResponseBody",
        { requestId },
        sessionId,
      );
    } catch (error) {
      this.trace("resource_body_unavailable", {
        requestId,
        url: resource.url,
        statusCode: resource.statusCode,
        message: error instanceof Error ? error.message : String(error),
      });
      return undefined;
    }

    const bytes = body.base64Encoded
      ? Buffer.from(body.body, "base64")
      : Buffer.from(body.body, "utf8");
    return retainBody(bytes, retainedBodyBudget);
  }
}

type RetainedBodyBudget = {
  maxBytes: number;
  retainedBytes: number;
};

type CapturedResourceState = {
  url: string;
  method: string;
  statusCode?: number;
  statusMessage?: string;
  headers?: Record<string, string>;
  timestamp?: string;
  loaded: boolean;
};

async function readDomState(
  cdp: CdpClient,
  sessionId: string,
): Promise<{ finalUrl: string; title?: string; html: string }> {
  const runtime = await cdp.send<{
    result: {
      value?: {
        finalUrl?: string;
        title?: string;
        html?: string;
      };
    };
  }>("Runtime.evaluate", {
    expression: `(() => ({
      finalUrl: location.href,
      title: document.title || undefined,
      html: document.documentElement ? document.documentElement.outerHTML : ""
    }))()`,
    returnByValue: true,
    awaitPromise: false,
  }, sessionId);

  const value = runtime.result.value ?? {};
  return {
    finalUrl: value.finalUrl ?? "about:blank",
    title: value.title,
    html: value.html ?? "",
  };
}

async function runSiteBehavior(
  cdp: CdpClient,
  sessionId: string,
  behaviorTimeoutMs: number,
) {
  await cdp.send("Runtime.evaluate", {
    expression: `(() => new Promise((resolve) => {
      const startedAt = Date.now();
      const stepDelayMs = 100;
      const maxSteps = Math.max(1, Math.floor(${behaviorTimeoutMs} / stepDelayMs));
      let steps = 0;

      const tick = () => {
        const root = document.scrollingElement || document.documentElement || document.body;
        if (!root) {
          resolve(true);
          return;
        }
        const previous = root.scrollTop;
        root.scrollBy(0, Math.max(window.innerHeight, 600));
        steps++;
        const exhausted = Date.now() - startedAt >= ${behaviorTimeoutMs} || steps >= maxSteps;
        const stuck = root.scrollTop === previous;
        if (stuck || exhausted) {
          root.scrollTo(0, 0);
          resolve(true);
          return;
        }
        setTimeout(tick, stepDelayMs);
      };

      tick();
    }))()`,
    awaitPromise: true,
    returnByValue: true,
  }, sessionId);
}

async function closeCaptureTarget(
  browser: BrowserInstance,
  cdp: CdpClient,
  targetId: string,
  sessionId: string,
  browserContextId: string,
  trace: CaptureTrace,
): Promise<Error | undefined> {
  try {
    if (kJobScopedCdp) {
      trace("scoped_cdp_close_start", {
        targetId,
        browserContextId,
      });
      await cdp.close();
      trace("scoped_cdp_close_complete");
    } else {
      await cdp.send("Target.detachFromTarget", { sessionId });
      await cdp.send("Target.disposeBrowserContext", { browserContextId });
    }
    if (kCaptureTargetBarrierMs > 0) {
      trace("target_barrier_start", {
        targetId,
        browserContextId,
        timeoutMs: kCaptureTargetBarrierMs,
      });
      await waitForTargetBarrier(browser, targetId, browserContextId, kCaptureTargetBarrierMs);
      trace("target_barrier_complete");
    }
    return undefined;
  } catch (error) {
    return new Error(
      `failed to clean up capture target: ${error instanceof Error ? error.message : String(error)}`,
    );
  }
}

async function waitForTargetBarrier(
  browser: BrowserInstance,
  targetId: string,
  browserContextId: string,
  timeoutMs: number,
): Promise<void> {
  const deadline = Date.now() + timeoutMs;
  while (true) {
    const contexts = await browser.cdp.send<{ browserContextIds: string[] }>(
      "Target.getBrowserContexts",
    );
    const targets = await browser.cdp.send<{
      targetInfos: Array<{
        targetId: string;
        browserContextId?: string;
      }>;
    }>("Target.getTargets");
    const contextPresent = contexts.browserContextIds.includes(browserContextId);
    const targetPresent = targets.targetInfos.some((targetInfo) =>
      targetInfo.targetId === targetId || targetInfo.browserContextId === browserContextId
    );
    if (!contextPresent && !targetPresent) {
      return;
    }
    if (Date.now() >= deadline) {
      throw new Error(`capture target barrier exceeded ${timeoutMs}ms`);
    }
    await delay(50);
  }
}

function normalizeHeaders(headers: Record<string, unknown>): Record<string, string> {
  return Object.fromEntries(
    Object.entries(headers)
      .filter(([, value]) => value !== undefined)
      .map(([name, value]) => [name.toLowerCase(), String(value)]),
  );
}

function stringValue(value: unknown): string | undefined {
  return typeof value === "string" ? value : undefined;
}

function numberValue(value: unknown): number | undefined {
  return typeof value === "number" ? value : undefined;
}

function recordValue(value: unknown): Record<string, unknown> {
  if (typeof value === "object" && value !== null) {
    return value as Record<string, unknown>;
  }
  return {};
}

function responseCanHaveBody(method: string, statusCode: number): boolean {
  if (method.toUpperCase() === "HEAD") {
    return false;
  }
  return (statusCode < 100 || statusCode >= 200) &&
    statusCode !== 204 &&
    statusCode !== 304;
}

function retainBody(body: Buffer, retainedBodyBudget: RetainedBodyBudget): Uint8Array {
  const nextRetainedBytes = retainedBodyBudget.retainedBytes + body.byteLength;
  if (nextRetainedBytes > retainedBodyBudget.maxBytes) {
    throw new Error(
      `retained body bytes ${nextRetainedBytes} exceeded size limit ${retainedBodyBudget.maxBytes}`,
    );
  }
  retainedBodyBudget.retainedBytes = nextRetainedBytes;
  return body;
}

async function waitForCondition(
  predicate: () => boolean,
  timeoutMs: number,
  timeoutMessage: string,
  waiters: Array<{ resolve: () => void; reject: (error: Error) => void }>,
): Promise<void> {
  if (predicate()) {
    return;
  }
  await new Promise<void>((resolve, reject) => {
    const timer = setTimeout(() => {
      cleanup();
      reject(new Error(timeoutMessage));
    }, timeoutMs);
    const cleanup = () => {
      clearTimeout(timer);
      const index = waiters.indexOf(waiter);
      if (index !== -1) {
        waiters.splice(index, 1);
      }
    };
    const waiter = {
      resolve: () => {
        cleanup();
        resolve();
      },
      reject: (error: Error) => {
        cleanup();
        reject(error);
      },
    };
    waiters.push(waiter);
  });
}

async function raceWithTimeout<T>(
  promise: Promise<T>,
  timeoutMs: number,
  message: string,
): Promise<T> {
  return new Promise<T>((resolve, reject) => {
    const timer = setTimeout(() => {
      reject(new Error(message));
    }, timeoutMs);
    promise.then(
      (value) => {
        clearTimeout(timer);
        resolve(value);
      },
      (error) => {
        clearTimeout(timer);
        reject(error);
      },
    );
  });
}
