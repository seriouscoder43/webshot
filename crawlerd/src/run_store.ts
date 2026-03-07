import type { RunRequest } from "./contracts.js";
import type { RunArtifacts, RunExecutionResult, RunExecutor } from "./run_executor.js";

export type CompletedRun = {
  result: {
    status: "succeeded" | "failed";
    exit_code: number;
    seed_probe?: RunExecutionResult["seedProbe"];
    failure_detail?: string;
  };
  artifacts: RunArtifacts;
};

export class RunStore {
  readonly executor: RunExecutor;
  running = 0;

  constructor(options: { executor: RunExecutor }) {
    this.executor = options.executor;
  }

  async executeBlockingRun(request: RunRequest): Promise<CompletedRun> {
    this.running++;

    try {
      const boundedResult = await this.executor.executeRun(request);

      return {
        result: {
          status: boundedResult.exitCode === 0 ? "succeeded" : "failed",
          exit_code: boundedResult.exitCode,
          ...(boundedResult.seedProbe !== undefined ? { seed_probe: boundedResult.seedProbe } : {}),
          ...(boundedResult.failureDetail !== undefined ? { failure_detail: boundedResult.failureDetail } : {}),
        },
        artifacts: boundedResult.artifacts,
      };
    } catch (error) {
      const failureDetail = error instanceof Error ? error.message : String(error);
      return {
        result: {
          status: "failed",
          exit_code: 1,
          failure_detail: failureDetail,
        },
        artifacts: {
          stdoutLog: new Uint8Array(),
          stderrLog: Buffer.from(`${failureDetail}\n`, "utf8"),
        },
      };
    } finally {
      this.running--;
    }
  }

  getCapacity() {
    return {
      running: this.running,
    };
  }
}
