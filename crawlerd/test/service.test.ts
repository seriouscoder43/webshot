import test from "node:test";
import assert from "node:assert/strict";

import { buildService } from "../src/service.js";
import { FakeRunExecutor, type RunExecutor } from "../src/run_executor.js";

async function createService(executor: RunExecutor = new FakeRunExecutor({ delayMs: 1 })) {
  return buildService({ executor });
}
test("health endpoint reports capacity", async () => {
  const service = await createService();
  try {
    const response = await service.inject({
      method: "GET",
      url: "/healthz",
    });
    assert.equal(response.statusCode, 200);
    assert.deepEqual(response.json(), {
      ready: true,
      capacity: {
        running: 0,
      },
    });
  } finally {
    await service.close();
  }
});

test("run endpoint blocks until completion and returns tar artifacts", async () => {
  const service = await createService();
  try {
    const response = await service.inject({
      method: "POST",
      url: "/run",
      payload: createRunPayload(),
    });

    assert.equal(response.statusCode, 200);
    const contentType = getSingleHeaderValue(response.headers["content-type"]);
    assert.equal(contentType, "application/x-tar");

    const entries = parseTarArchive(response.rawPayload);

    assert.deepEqual(JSON.parse(entries["result.json"]!.toString("utf8")), {
      status: "succeeded",
      exit_code: 0,
      seed_probe: {
        status: 200,
        load_state: 2,
      },
    });
    assert.deepEqual(
      Object.keys(entries).sort(),
      ["capture.wacz", "pages.jsonl", "result.json", "stderr.log", "stdout.log"],
    );
    assert.equal(entries["capture.wacz"]!.toString("utf8"), "fake-wacz");
    assert.match(entries["pages.jsonl"]!.toString("utf8"), /"url":"https:\/\/example.com\/"/);
    assert.equal(entries["stdout.log"]!.toString("utf8"), "fake crawler stdout\n");
    assert.equal(entries["stderr.log"]!.toString("utf8"), "");
  } finally {
    await service.close();
  }
});

test("failed run returns tar result without success artifacts", async () => {
  const service = await createService(new FakeRunExecutor({ delayMs: 1, outcome: "failure" }));
  try {
    const response = await service.inject({
      method: "POST",
      url: "/run",
      payload: createRunPayload(),
    });

    assert.equal(response.statusCode, 200);
    const entries = parseTarArchive(response.rawPayload);

    assert.deepEqual(JSON.parse(entries["result.json"]!.toString("utf8")), {
      status: "failed",
      exit_code: 9,
      failure_detail: 'stdout="fake crawler stdout", stderr="fake crawler stderr"',
    });
    assert.deepEqual(
      Object.keys(entries).sort(),
      ["result.json", "stderr.log", "stdout.log"],
    );
    assert.equal(entries["stdout.log"]!.toString("utf8"), "fake crawler stdout\n");
    assert.equal(entries["stderr.log"]!.toString("utf8"), "fake crawler stderr\n");
    assert.equal(entries["capture.wacz"], undefined);
    assert.equal(entries["pages.jsonl"], undefined);
  } finally {
    await service.close();
  }
});

test("successful run without a wacz fails hard", async () => {
  const service = await createService({
    kind: "broken-success",
    async executeRun() {
      return {
        exitCode: 0,
        artifacts: {
          stdoutLog: Buffer.from("ok\n", "utf8"),
          stderrLog: new Uint8Array(),
        },
      };
    },
  });
  try {
    const response = await service.inject({
      method: "POST",
      url: "/run",
      payload: createRunPayload(),
    });

    assert.equal(response.statusCode, 500);
    assert.deepEqual(response.json(), {
      error: {
        message: "successful run must include capture.wacz",
      },
    });
  } finally {
    await service.close();
  }
});

test("run endpoint rejects a missing job timeout", async () => {
  const service = await createService();
  try {
    const response = await service.inject({
      method: "POST",
      url: "/run",
      payload: {
        url: "https://example.com/",
      },
    });

    assert.equal(response.statusCode, 400);
    assert.match(response.json().error.message, /timeout_ms/);
  } finally {
    await service.close();
  }
});

test("run endpoint rejects an invalid job timeout", async () => {
  const service = await createService();
  try {
    const response = await service.inject({
      method: "POST",
      url: "/run",
      payload: createRunPayload({
        timeout_ms: 0,
      }),
    });

    assert.equal(response.statusCode, 400);
    assert.match(response.json().error.message, /timeout_ms/);
  } finally {
    await service.close();
  }
});

function createRunPayload(overrides: Partial<{ url: string; timeout_ms: number }> = {}) {
  return {
    url: overrides.url ?? "https://example.com/",
    timeout_ms: overrides.timeout_ms ?? 120_000,
  };
}

function parseTarArchive(bytes: Buffer): Record<string, Buffer> {
  const entries: Record<string, Buffer> = {};
  let offset = 0;

  while (offset + 512 <= bytes.length) {
    const header = bytes.subarray(offset, offset + 512);
    if (header.every((value) => value === 0)) {
      break;
    }

    const path = readTarString(header, 0, 100);
    assert.notEqual(path, "");
    const size = readTarOctal(header, 124, 12);
    const typeFlag = header[156];
    assert.ok(typeFlag === 0 || typeFlag === "0".charCodeAt(0));

    const bodyStart = offset + 512;
    const bodyEnd = bodyStart + size;
    assert.ok(bodyEnd <= bytes.length);

    entries[path] = bytes.subarray(bodyStart, bodyEnd);
    offset = bodyStart + alignTarSize(size);
  }

  return entries;
}

function readTarString(header: Buffer, offset: number, length: number): string {
  const field = header.subarray(offset, offset + length);
  const zeroIndex = field.indexOf(0);
  const end = zeroIndex === -1 ? field.length : zeroIndex;
  return field.subarray(0, end).toString("utf8");
}

function readTarOctal(header: Buffer, offset: number, length: number): number {
  const text = readTarString(header, offset, length).trim();
  assert.notEqual(text, "");
  return Number.parseInt(text, 8);
}

function alignTarSize(size: number): number {
  const remainder = size % 512;
  return remainder === 0 ? size : size + (512 - remainder);
}

function getSingleHeaderValue(header: string | string[] | number | undefined): string {
  if (typeof header === "string") {
    return header;
  }
  if (Array.isArray(header)) {
    return header.join(", ");
  }
  throw new Error(`expected string header, got ${String(header)}`);
}
