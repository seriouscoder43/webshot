import test from "node:test";
import assert from "node:assert/strict";

import { buildChromiumArgs, parseGeometry } from "../src/browser_sandbox.js";

test("parseGeometry accepts valid WIDTHxHEIGHT input", () => {
  assert.deepEqual(parseGeometry("1600x900"), {
    width: 1600,
    height: 900,
  });
});

test("parseGeometry rejects malformed geometry instead of falling back", () => {
  assert.throws(() => parseGeometry("bad-input"), /invalid geometry/);
  assert.throws(() => parseGeometry(""), /invalid geometry/);
  assert.throws(() => parseGeometry("0x900"), /positive integers/);
});

test("buildChromiumArgs uses the configured default geometry when none is supplied", () => {
  const args = buildChromiumArgs({
    browserBin: "chromium",
    userDataDir: "/tmp/user-data",
    proxyUpstreamSocket: "/tmp/proxy.sock",
    cdpSocket: "/tmp/cdp.sock",
    netlogPath: "/tmp/netlog.json",
  });

  assert.ok(args.includes("--window-size=1600,900"));
});
