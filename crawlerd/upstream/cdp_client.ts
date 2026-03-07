import { createHash, randomBytes } from "node:crypto";
import net from "node:net";

type CdpMessage = {
  id?: number;
  method?: string;
  params?: unknown;
  sessionId?: string;
  result?: unknown;
  error?: {
    message?: string;
  };
};

export type CdpEvent = {
  method: string;
  params: Record<string, unknown>;
  sessionId?: string;
};

type EventListener = (event: CdpEvent) => void;

export class CdpClient {
  readonly transport: WsTransport;
  readonly listeners = new Set<EventListener>();
  readonly pending = new Map<number, {
    resolve: (value: unknown) => void;
    reject: (error: Error) => void;
  }>();
  nextId = 1;
  closed = false;

  private constructor(transport: WsTransport) {
    this.transport = transport;
    transport.addMessageListener((text) => {
      try {
        this.handleMessage(text);
      } catch (error) {
        this.rejectPending(error instanceof Error ? error : new Error(String(error)));
      }
    });
    transport.addCloseListener(() => {
      this.closed = true;
      this.rejectPending(new Error("cdp socket closed"));
    });
  }

  static async connect(endpoint: string): Promise<CdpClient> {
    const transport = await WsTransport.connect(endpoint);
    return new CdpClient(transport);
  }

  static async connectUnix(socketPath: string, websocketPath: string): Promise<CdpClient> {
    const transport = await WsTransport.connectUnix(socketPath, websocketPath);
    return new CdpClient(transport);
  }

  async send<T>(
    method: string,
    params: Record<string, unknown> = {},
    sessionId?: string,
  ): Promise<T> {
    if (this.closed) {
      throw new Error("cdp socket is closed");
    }

    const id = this.nextId++;
    const response = new Promise<unknown>((resolve, reject) => {
      this.pending.set(id, { resolve, reject });
    });

    this.transport.sendText(JSON.stringify({
      id,
      method,
      params,
      ...(sessionId === undefined ? {} : { sessionId }),
    }));

    return response as Promise<T>;
  }

  addListener(listener: EventListener): () => void {
    this.listeners.add(listener);
    return () => {
      this.listeners.delete(listener);
    };
  }

  async close(): Promise<void> {
    if (this.closed) {
      return;
    }
    this.closed = true;
    await this.transport.close();
  }

  private handleMessage(text: string) {
    const message = JSON.parse(text) as CdpMessage;

    if (message.id !== undefined) {
      const pending = this.pending.get(message.id);
      if (pending === undefined) {
        return;
      }
      this.pending.delete(message.id);
      if (message.error !== undefined) {
        pending.reject(new Error(message.error.message ?? "cdp command failed"));
        return;
      }
      pending.resolve(message.result ?? {});
      return;
    }

    if (message.method === undefined) {
      return;
    }

    const event: CdpEvent = {
      method: message.method,
      params: asRecord(message.params),
      ...(message.sessionId === undefined ? {} : { sessionId: message.sessionId }),
    };
    for (const listener of this.listeners) {
      listener(event);
    }
  }

  private rejectPending(error: Error) {
    for (const { reject } of this.pending.values()) {
      reject(error);
    }
    this.pending.clear();
  }
}

class WsTransport {
  readonly socket: net.Socket;
  readonly messageListeners = new Set<(text: string) => void>();
  readonly closeListeners = new Set<() => void>();
  buffer = Buffer.alloc(0);
  closed = false;
  handshakeDone = false;
  fragmentedText?: Buffer;

  private constructor(socket: net.Socket) {
    this.socket = socket;
    socket.on("data", (chunk) => {
      this.handleData(chunk);
    });
    socket.once("close", () => {
      this.closed = true;
      for (const listener of this.closeListeners) {
        listener();
      }
    });
    socket.once("error", () => {
      socket.destroy();
    });
  }

  static async connect(endpoint: string): Promise<WsTransport> {
    const url = new URL(endpoint);
    if (url.protocol !== "ws:") {
      throw new Error(`unsupported websocket protocol ${url.protocol}`);
    }

    const socket = net.createConnection({
      host: url.hostname,
      port: Number.parseInt(url.port, 10),
    });
    const transport = new WsTransport(socket);

    await new Promise<void>((resolve, reject) => {
      socket.once("connect", () => resolve());
      socket.once("error", (error) => reject(error));
    });

    await transport.performHandshake(
      `${url.pathname}${url.search}` === "" ? "/" : `${url.pathname}${url.search}`,
      url.host,
    );
    return transport;
  }

  static async connectUnix(socketPath: string, websocketPath: string): Promise<WsTransport> {
    const socket = net.createConnection({
      path: socketPath,
    });
    const transport = new WsTransport(socket);

    await new Promise<void>((resolve, reject) => {
      socket.once("connect", () => resolve());
      socket.once("error", (error) => reject(error));
    });

    const path = websocketPath === "" ? "/" : websocketPath;
    await transport.performHandshake(path.startsWith("/") ? path : `/${path}`, "localhost");
    return transport;
  }

  addMessageListener(listener: (text: string) => void) {
    this.messageListeners.add(listener);
  }

  addCloseListener(listener: () => void) {
    this.closeListeners.add(listener);
  }

  sendText(text: string) {
    this.socket.write(encodeFrame(Buffer.from(text, "utf8"), 0x1));
  }

  async close(): Promise<void> {
    if (this.closed) {
      return;
    }

    this.socket.write(encodeFrame(Buffer.alloc(0), 0x8));
    await new Promise<void>((resolve) => {
      this.socket.once("close", () => resolve());
      this.socket.end();
    });
  }

  private async performHandshake(path: string, host: string) {
    const key = randomBytes(16).toString("base64");
    this.socket.write(
      [
        `GET ${path === "" ? "/" : path} HTTP/1.1`,
        `Host: ${host}`,
        "Upgrade: websocket",
        "Connection: Upgrade",
        `Sec-WebSocket-Key: ${key}`,
        "Sec-WebSocket-Version: 13",
        "",
        "",
      ].join("\r\n"),
    );

    await new Promise<void>((resolve, reject) => {
      const onHandshake = () => {
        try {
          const delimiterIndex = this.buffer.indexOf("\r\n\r\n");
          if (delimiterIndex === -1) {
            return;
          }
          const head = this.buffer.subarray(0, delimiterIndex).toString("utf8");
          const rest = this.buffer.subarray(delimiterIndex + 4);
          const lines = head.split("\r\n");
          const statusLine = lines[0] ?? "";
          if (!statusLine.startsWith("HTTP/1.1 101 ")) {
            throw new Error(`websocket handshake failed: ${lines[0] ?? "empty response"}`);
          }
          const headers = new Map(
            lines.slice(1).map((line) => {
              const colon = line.indexOf(":");
              return [line.slice(0, colon).trim().toLowerCase(), line.slice(colon + 1).trim()];
            }),
          );
          const accept = headers.get("sec-websocket-accept");
          const expected = createHash("sha1")
            .update(`${key}258EAFA5-E914-47DA-95CA-C5AB0DC85B11`)
            .digest("base64");
          if (accept !== expected) {
            throw new Error("websocket handshake accept mismatch");
          }
          this.handshakeDone = true;
          this.buffer = rest;
          this.socket.off("data", onHandshake);
          resolve();
          this.processFrames();
        } catch (error) {
          this.socket.off("data", onHandshake);
          reject(error);
        }
      };
      this.socket.on("data", onHandshake);
      onHandshake();
    });
  }

  private handleData(chunk: Buffer | string) {
    const chunkBuffer = Buffer.isBuffer(chunk) ? chunk : Buffer.from(chunk);
    this.buffer = Buffer.concat([this.buffer, chunkBuffer]);
    if (!this.handshakeDone) {
      return;
    }
    this.processFrames();
  }

  private processFrames() {
    while (true) {
      const frame = decodeFrame(this.buffer);
      if (frame === undefined) {
        return;
      }
      this.buffer = this.buffer.subarray(frame.consumedBytes);

      if (frame.opcode === 0x8) {
        this.socket.end();
        return;
      }
      if (frame.opcode === 0x9) {
        this.socket.write(encodeFrame(frame.payload, 0xA));
        continue;
      }
      if (frame.opcode === 0xA) {
        continue;
      }
      if (frame.opcode === 0x0) {
        this.fragmentedText = Buffer.concat([this.fragmentedText ?? Buffer.alloc(0), frame.payload]);
        if (frame.fin) {
          this.emitText(this.fragmentedText.toString("utf8"));
          this.fragmentedText = undefined;
        }
        continue;
      }
      if (frame.opcode === 0x1) {
        if (frame.fin) {
          this.emitText(frame.payload.toString("utf8"));
        } else {
          this.fragmentedText = frame.payload;
        }
      }
    }
  }

  private emitText(text: string) {
    for (const listener of this.messageListeners) {
      listener(text);
    }
  }
}

function encodeFrame(payload: Buffer, opcode: number): Buffer {
  const mask = randomBytes(4);
  let header: Buffer;
  if (payload.byteLength < 126) {
    header = Buffer.alloc(2);
    header[1] = 0x80 | payload.byteLength;
  } else if (payload.byteLength <= 0xffff) {
    header = Buffer.alloc(4);
    header[1] = 0x80 | 126;
    header.writeUInt16BE(payload.byteLength, 2);
  } else {
    header = Buffer.alloc(10);
    header[1] = 0x80 | 127;
    header.writeBigUInt64BE(BigInt(payload.byteLength), 2);
  }
  header[0] = 0x80 | opcode;

  const maskedPayload = Buffer.from(payload);
  for (let index = 0; index < maskedPayload.length; index++) {
    maskedPayload[index] ^= mask[index % 4]!;
  }

  return Buffer.concat([header, mask, maskedPayload]);
}

function decodeFrame(buffer: Buffer): {
  fin: boolean;
  opcode: number;
  payload: Buffer;
  consumedBytes: number;
} | undefined {
  if (buffer.length < 2) {
    return undefined;
  }

  const first = buffer[0]!;
  const second = buffer[1]!;
  const fin = (first & 0x80) !== 0;
  const opcode = first & 0x0f;
  const masked = (second & 0x80) !== 0;
  let length = second & 0x7f;
  let offset = 2;

  if (length === 126) {
    if (buffer.length < offset + 2) {
      return undefined;
    }
    length = buffer.readUInt16BE(offset);
    offset += 2;
  } else if (length === 127) {
    if (buffer.length < offset + 8) {
      return undefined;
    }
    const longLength = buffer.readBigUInt64BE(offset);
    if (longLength > BigInt(Number.MAX_SAFE_INTEGER)) {
      throw new Error("websocket frame too large");
    }
    length = Number(longLength);
    offset += 8;
  }

  let mask: Buffer | undefined;
  if (masked) {
    if (buffer.length < offset + 4) {
      return undefined;
    }
    mask = buffer.subarray(offset, offset + 4);
    offset += 4;
  }
  if (buffer.length < offset + length) {
    return undefined;
  }

  const payload = Buffer.from(buffer.subarray(offset, offset + length));
  if (mask !== undefined) {
    for (let index = 0; index < payload.length; index++) {
      payload[index] ^= mask[index % 4]!;
    }
  }

  return {
    fin,
    opcode,
    payload,
    consumedBytes: offset + length,
  };
}

function asRecord(value: unknown): Record<string, unknown> {
  if (typeof value === "object" && value !== null) {
    return value as Record<string, unknown>;
  }
  return {};
}
