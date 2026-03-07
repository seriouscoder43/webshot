import Type from "typebox";

export const ErrorEnvelopeSchema = Type.Object(
  {
    error: Type.Object(
      {
        message: Type.String(),
      },
      {
        additionalProperties: false,
      },
    ),
  },
  {
    $id: "CrawlerErrorEnvelope",
    additionalProperties: false,
  },
);

export const SeedProbeSchema = Type.Object(
  {
    status: Type.Optional(Type.Integer()),
    load_state: Type.Optional(Type.Integer()),
  },
  {
    $id: "CrawlerSeedProbe",
    additionalProperties: false,
  },
);

export const TerminalRunStatusSchema = Type.String({
  $id: "CrawlerTerminalRunStatus",
  enum: ["succeeded", "failed"],
  description: "Terminal crawler daemon execution state for a run.",
});

export const RunResultSchema = Type.Object(
  {
    status: TerminalRunStatusSchema,
    exit_code: Type.Integer(),
    seed_probe: Type.Optional(SeedProbeSchema),
    failure_detail: Type.Optional(Type.String()),
  },
  {
    $id: "CrawlerRunResult",
    additionalProperties: false,
  },
);

export const TarRunResponseSchema = Type.String({
  $id: "CrawlerTarRunResponse",
  format: "binary",
  description:
    "Tar archive with fixed entry names result.json, stdout.log, stderr.log, optional capture.wacz, and optional pages.jsonl.",
});

export const RunRequestSchema = Type.Object(
  {
    url: Type.String({ minLength: 1 }),
    timeout_ms: Type.Integer({ minimum: 1 }),
  },
  {
    $id: "CrawlerRunRequest",
    additionalProperties: false,
  },
);

export type RunRequestBody = {
  url: string;
  timeout_ms: number;
};

export const HealthResponseSchema = Type.Object(
  {
    ready: Type.Boolean(),
    capacity: Type.Object(
      {
        running: Type.Integer({ minimum: 0 }),
      },
      { additionalProperties: false },
    ),
  },
  {
    $id: "CrawlerHealthResponse",
    additionalProperties: false,
  },
);

export type RunRequest = {
  url: string;
  jobTimeoutMs: number;
};

export type SeedProbe = {
  status?: number;
  load_state?: number;
};
