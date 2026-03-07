import { Readable } from "node:stream";

import fastify, { type FastifyInstance, type FastifyReply } from "fastify";
import swagger from "@fastify/swagger";

import {
  ErrorEnvelopeSchema,
  HealthResponseSchema,
  RunRequestSchema,
  TarRunResponseSchema,
  type RunRequestBody,
  type RunRequest,
} from "./contracts.js";
import {
  type CompletedRun,
  RunStore,
} from "./run_store.js";
import { UpstreamRunExecutor, type RunExecutor } from "./run_executor.js";
import { streamTarArchive, type TarEntry } from "./tar_stream.js";
import { kBrowserBin, kBrowserGeometry } from "./config.js";

export type ServiceOptions = {
  executor: RunExecutor;
};

export async function buildService(options: ServiceOptions): Promise<FastifyInstance> {
  const service = fastify({
    logger: true,
    disableRequestLogging: (request) => request.url === "/healthz",
  });
  const store = new RunStore({
    executor: options.executor,
  });

  await service.register(swagger, {
    openapi: {
      openapi: "3.0.3",
      info: {
        title: "crawlerd internal API",
        version: "0.1.0",
      },
      servers: [{ url: "/" }],
    },
  });

  service.setErrorHandler((error, _request, reply) => {
    service.log.error(error);
    const statusCode = getHttpStatusCode(error);
    return reply.code(statusCode).send({
      error: {
        message: error instanceof Error ? error.message : "internal server error",
      },
    });
  });

  service.get("/healthz", {
    schema: {
      operationId: "getCrawlerHealth",
      response: {
        200: HealthResponseSchema,
      },
    },
  }, async () => ({
    ready: await getExecutorReadiness(options.executor),
    capacity: store.getCapacity(),
  }));

  service.post<{ Body: RunRequestBody }>("/run", {
    schema: {
      operationId: "runCapture",
      body: RunRequestSchema,
      response: {
        200: {
          description: "Blocking terminal crawler result as a tar archive with fixed entry names.",
          content: {
            "application/x-tar": {
              schema: TarRunResponseSchema,
            },
          },
        },
        400: {
          description: "Request validation error.",
          content: {
            "application/json": {
              schema: ErrorEnvelopeSchema,
            },
          },
        },
      },
    },
  }, async (request, reply) => {
    const run = buildRunRequest(request.body);
    const completed = await store.executeBlockingRun(run);
    return sendTarRunResponse(reply, completed);
  });

  service.addHook("onClose", async () => {
    await options.executor.close?.();
  });
  await service.ready();
  return service;
}

function getHttpStatusCode(error: unknown): number {
  if (
    typeof error === "object" &&
    error !== null &&
    "statusCode" in error &&
    typeof error.statusCode === "number" &&
    error.statusCode >= 400
  ) {
    return error.statusCode;
  }

  return 500;
}

function sendTarRunResponse(reply: FastifyReply, completed: CompletedRun) {
  reply.code(200);
  reply.header("content-type", "application/x-tar");
  return reply.send(Readable.from(streamTarArchive(buildTarEntries(completed))));
}

function buildTarEntries(completed: CompletedRun): TarEntry[] {
  if (completed.result.status === "succeeded" && completed.artifacts.wacz === undefined) {
    throw new Error("successful run must include capture.wacz");
  }

  const entries: TarEntry[] = [
    {
      path: "result.json",
      body: Buffer.from(JSON.stringify(completed.result), "utf8"),
    },
    {
      path: "stdout.log",
      body: completed.artifacts.stdoutLog,
    },
    {
      path: "stderr.log",
      body: completed.artifacts.stderrLog,
    },
  ];

  if (completed.artifacts.wacz !== undefined) {
    entries.push({
      path: "capture.wacz",
      body: completed.artifacts.wacz,
    });
  }

  if (completed.artifacts.pages !== undefined) {
    entries.push({
      path: "pages.jsonl",
      body: completed.artifacts.pages,
    });
  }

  return entries;
}

export function buildDefaultExecutor(): RunExecutor {
  return new UpstreamRunExecutor({
    browserBin: kBrowserBin,
    geometry: kBrowserGeometry,
  });
}

async function getExecutorReadiness(executor: RunExecutor): Promise<boolean> {
  return await executor.isReady?.() ?? true;
}

function buildRunRequest(body: RunRequestBody): RunRequest {
  return {
    url: body.url,
    jobTimeoutMs: body.timeout_ms,
  };
}

class RequestValidationError extends Error {
  readonly statusCode = 400;
}
