export type TarEntry = {
  path: string;
  body: Uint8Array;
};

const kTarBlockSize = 512;
const kTarHeaderPathLength = 100;

export function* streamTarArchive(entries: readonly TarEntry[]): Generator<Uint8Array> {
  for (const entry of entries) {
    yield buildTarHeader(entry);
    yield entry.body;

    const paddingLength = getTarPaddingLength(entry.body.byteLength);
    if (paddingLength > 0) {
      yield new Uint8Array(paddingLength);
    }
  }

  yield new Uint8Array(kTarBlockSize * 2);
}

function buildTarHeader(entry: TarEntry): Uint8Array {
  const pathBytes = Buffer.from(entry.path, "utf8");
  if (pathBytes.byteLength < 1 || pathBytes.byteLength > kTarHeaderPathLength) {
    throw new Error(`tar entry path must be 1-${kTarHeaderPathLength} bytes: ${entry.path}`);
  }
  if (entry.path.includes("\0")) {
    throw new Error(`tar entry path must not contain NUL bytes: ${entry.path}`);
  }

  const header = Buffer.alloc(kTarBlockSize, 0);
  writeStringField(header, 0, kTarHeaderPathLength, pathBytes);
  writeOctalField(header, 100, 8, 0o644);
  writeOctalField(header, 108, 8, 0);
  writeOctalField(header, 116, 8, 0);
  writeOctalField(header, 124, 12, entry.body.byteLength);
  writeOctalField(header, 136, 12, 0);
  header.fill(0x20, 148, 156);
  header[156] = "0".charCodeAt(0);
  writeStringField(header, 257, 6, Buffer.from("ustar\0", "ascii"));
  writeStringField(header, 263, 2, Buffer.from("00", "ascii"));
  writeChecksumField(header);
  return header;
}

function writeStringField(target: Uint8Array, offset: number, length: number, value: Uint8Array) {
  if (value.byteLength > length) {
    throw new Error(`tar header field is too small for ${value.byteLength} bytes`);
  }
  target.set(value, offset);
}

function writeOctalField(target: Uint8Array, offset: number, length: number, value: number) {
  if (!Number.isSafeInteger(value) || value < 0) {
    throw new Error(`tar header field must be a non-negative safe integer: ${value}`);
  }

  const text = value.toString(8).padStart(length - 1, "0");
  if (text.length !== length - 1) {
    throw new Error(`tar header field overflow for ${value}`);
  }

  writeStringField(target, offset, length, Buffer.from(`${text}\0`, "ascii"));
}

function writeChecksumField(header: Uint8Array) {
  let checksum = 0;
  for (const byte of header) {
    checksum += byte;
  }

  const text = checksum.toString(8).padStart(6, "0");
  if (text.length > 6) {
    throw new Error(`tar checksum overflow for ${checksum}`);
  }

  writeStringField(header, 148, 8, Buffer.from(`${text}\0 `, "ascii"));
}

function getTarPaddingLength(size: number): number {
  const remainder = size % kTarBlockSize;
  return remainder === 0 ? 0 : kTarBlockSize - remainder;
}
