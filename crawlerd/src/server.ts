import { unlink } from "node:fs/promises";

import { kListenSocketPath } from "./config.js";
import { buildDefaultExecutor, buildService } from "./service.js";

const service = await buildService({
  executor: buildDefaultExecutor(),
});

await unlink(kListenSocketPath).catch(() => undefined);
await service.listen({ path: kListenSocketPath });
