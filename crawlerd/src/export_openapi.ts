import { writeFile } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";

import YAML from "yaml";

import {
  ErrorEnvelopeSchema,
  HealthResponseSchema,
  RunRequestSchema,
  RunResultSchema,
  TarRunResponseSchema,
} from "./contracts.js";
import { buildService } from "./service.js";
import { FakeRunExecutor } from "./run_executor.js";

const currentDir = path.dirname(fileURLToPath(import.meta.url));
const crawlerRoot = path.basename(path.resolve(currentDir, "..")) === "dist"
  ? path.resolve(currentDir, "..", "..")
  : path.resolve(currentDir, "..");
const repoRoot = path.resolve(crawlerRoot, "..");
const outputPath = path.join(repoRoot, "schema", "crawlerd.yaml");

const service = await buildService({
  executor: new FakeRunExecutor(),
});

try {
  const document = service.swagger() as OpenApiDocument;
  const schemas = normalizeSchemas([
    ErrorEnvelopeSchema,
    HealthResponseSchema,
    RunRequestSchema,
    RunResultSchema,
    TarRunResponseSchema,
  ]);
  document.components = {
    ...document.components,
    schemas,
  };
  await writeFile(outputPath, YAML.stringify(document, null, { lineWidth: 0 }), "utf8");
} finally {
  await service.close();
}

type JsonValue = null | boolean | number | string | JsonValue[] | { [key: string]: JsonValue };

type OpenApiDocument = {
  components?: {
    schemas?: Record<string, JsonValue>;
  };
};

function normalizeSchemas(rawSchemas: Array<unknown>): Record<string, JsonValue> {
  const out: Record<string, JsonValue> = {};

  for (const rawSchema of rawSchemas) {
    const schemaObject = rawSchema as { $id?: string };
    const schemaId = schemaObject.$id;
    if (schemaId === undefined || schemaId.trim() === "") {
      throw new Error("shared schema is missing $id");
    }
    out[schemaId] = normalizeJsonValue(structuredClone(rawSchema) as JsonValue);
  }

  return out;
}

function normalizeJsonValue(value: JsonValue): JsonValue {
  if (Array.isArray(value)) {
    return value.map((item) => normalizeJsonValue(item));
  }

  if (value === null || typeof value !== "object") {
    return value;
  }

  const out: { [key: string]: JsonValue } = {};
  for (const [key, item] of Object.entries(value)) {
    if (key === "$id") {
      continue;
    }

    if (key === "$ref" && typeof item === "string") {
      out[key] = toOpenApiRef(item);
      continue;
    }

    out[key] = normalizeJsonValue(item);
  }

  return out;
}

function toOpenApiRef(value: string): string {
  if (value.startsWith("#/")) {
    return value;
  }

  const normalized = value.endsWith("#") ? value.slice(0, -1) : value;
  return `#/components/schemas/${normalized}`;
}
