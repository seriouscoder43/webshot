import { runBrowsertrixCapture, type BrowsertrixCaptureOptions } from "../upstream/run_capture.js";
import { BrowserPool } from "../upstream/browser_pool.js";
import type { SeedProbe, RunRequest } from "./contracts.js";

export type RunArtifacts = {
  wacz?: Uint8Array;
  pages?: Uint8Array;
  stdoutLog: Uint8Array;
  stderrLog: Uint8Array;
};

export type RunExecutionResult = {
  exitCode: number;
  seedProbe?: SeedProbe;
  failureDetail?: string;
  artifacts: RunArtifacts;
};

export interface RunExecutor {
  readonly kind: string;
  executeRun(run: RunRequest): Promise<RunExecutionResult>;
  close?(): Promise<void>;
  isReady?(): Promise<boolean>;
}

export type FakeExecutorOptions = {
  delayMs?: number;
  outcome?: "success" | "failure";
};

export class FakeRunExecutor implements RunExecutor {
  readonly kind = "fake";
  readonly delayMs: number;
  readonly outcome: "success" | "failure";

  constructor(options: FakeExecutorOptions = {}) {
    this.delayMs = options.delayMs ?? 5;
    this.outcome = options.outcome ?? "success";
  }

  async executeRun(run: RunRequest): Promise<RunExecutionResult> {
    await sleep(this.delayMs);

    const stdoutLog = Buffer.from("fake crawler stdout\n", "utf8");
    const stderrLog = Buffer.from(
      this.outcome === "failure" ? "fake crawler stderr\n" : "",
      "utf8",
    );

    if (this.outcome === "failure") {
      return {
        exitCode: 9,
        failureDetail: 'stdout="fake crawler stdout", stderr="fake crawler stderr"',
        artifacts: {
          stdoutLog,
          stderrLog,
        },
      };
    }

    return {
      exitCode: 0,
      seedProbe: {
        status: 200,
        load_state: 2,
      },
      artifacts: {
        wacz: Buffer.from("fake-wacz", "utf8"),
        pages: Buffer.from(
          `${JSON.stringify({
            id: "fake-run",
            url: run.url,
            seed: true,
            depth: 0,
            status: 200,
            loadState: 2,
          })}\n`,
          "utf8",
        ),
        stdoutLog,
        stderrLog,
      },
    };
  }
}

export class UpstreamRunExecutor implements RunExecutor {
  readonly kind = "browsertrix-in-memory";
  readonly options: BrowsertrixCaptureOptions;
  readonly browserPool: BrowserPool;

  constructor(options: Omit<BrowsertrixCaptureOptions, "browserPool">) {
    this.browserPool = new BrowserPool({
      browserBin: options.browserBin,
      geometry: options.geometry,
      proxyServer: options.proxyServer,
    });
    this.options = {
      ...options,
      browserPool: this.browserPool,
    };
  }

  async executeRun(run: RunRequest): Promise<RunExecutionResult> {
    return runBrowsertrixCapture(run, this.options);
  }

  async close(): Promise<void> {
    await this.browserPool.close();
  }

  async isReady(): Promise<boolean> {
    return this.browserPool.isReady();
  }

  getPoolStats() {
    return this.browserPool.getStats();
  }
}

function sleep(delayMs: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, delayMs));
}
