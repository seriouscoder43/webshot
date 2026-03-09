import { mkdir, lstat, unlink } from "node:fs/promises";
import { dirname, isAbsolute } from "node:path";
import { buildDefaultExecutor, buildService } from "./service.js";

const service = await buildService({
  executor: buildDefaultExecutor(),
});

const socketPath = parseSocketPath(process.argv.slice(2));

await mkdir(dirname(socketPath), { recursive: true });
await removeStaleSocket(socketPath);
await service.listen({ path: socketPath });

function parseSocketPath(argv: string[]): string {
  let socketPath: string | undefined;
  for (let i = 0; i < argv.length; i++) {
    const arg = argv[i];
    if (arg !== "--socket-path") {
      throw new Error(`unknown argument: ${arg}`);
    }

    const value = argv[++i];
    if (value === undefined || value.length === 0) {
      throw new Error("--socket-path requires a value");
    }
    if (socketPath !== undefined) {
      throw new Error("--socket-path may only be provided once");
    }
    socketPath = value;
  }

  if (socketPath === undefined) {
    throw new Error("missing required --socket-path");
  }
  if (!isAbsolute(socketPath)) {
    throw new Error(`--socket-path must be absolute: ${socketPath}`);
  }
  if (Buffer.byteLength(socketPath, "utf8") >= 108) {
    throw new Error(`--socket-path is too long for AF_UNIX: ${socketPath}`);
  }
  return socketPath;
}

async function removeStaleSocket(socketPath: string): Promise<void> {
  try {
    const existing = await lstat(socketPath);
    if (!existing.isSocket()) {
      throw new Error(`refusing to remove non-socket at ${socketPath}`);
    }
    await unlink(socketPath);
  } catch (error) {
    if (isMissingFileError(error)) {
      return;
    }
    throw error;
  }
}

function isMissingFileError(error: unknown): boolean {
  return typeof error === "object" && error !== null && "code" in error && error.code === "ENOENT";
}
