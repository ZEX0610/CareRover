import test from 'node:test';
import assert from 'node:assert/strict';
import { once } from 'node:events';
import { execFileSync } from 'node:child_process';
import { mkdtemp, readFile, unlink, rmdir } from 'node:fs/promises';
import { existsSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { dirname, join } from 'node:path';
import { WebSocket } from 'ws';
import { makeServer } from '../server.mjs';

test('local TLS/WSS certificate validation and authenticated two-way relay', async (t) => {
  try { execFileSync('openssl', ['version'], { stdio: 'ignore' }); }
  catch { t.skip('OpenSSL not available on this machine'); return; }
  const dir = await mkdtemp(join(tmpdir(), 'carerover-audio-tls-'));
  const keyPath = join(dir, 'key.pem'), certPath = join(dir, 'cert.pem');
  let app, device, parent;
  try {
    const args = ['req', '-x509', '-newkey', 'rsa:2048', '-nodes',
      '-keyout', keyPath, '-out', certPath, '-days', '1', '-subj', '/CN=localhost',
      '-addext', 'subjectAltName=DNS:localhost,IP:127.0.0.1'];
    // Some Windows OpenSSL distributions point at a non-existent default cnf.
    if (process.platform === 'win32') {
      const exe = execFileSync('where.exe', ['openssl'], { encoding: 'utf8' }).split(/\r?\n/)[0];
      const cnf = join(dirname(dirname(exe)), 'ssl', 'openssl.cnf');
      if (existsSync(cnf)) args.push('-config', cnf);
    }
    execFileSync('openssl', args, { stdio: 'pipe' });
    const key = await readFile(keyPath), cert = await readFile(certPath);
    app = await makeServer({ deviceToken: 'device-secret-for-audio-lab-2026',
      parentToken: 'parent-secret-for-audio-lab-2026',
      tlsKey: key, tlsCert: cert, listenHost: '127.0.0.1' });
    const url = `wss://127.0.0.1:${app.address.port}/audio`;
    device = new WebSocket(url, { ca: cert,
      headers: { Authorization: 'Bearer device-secret-for-audio-lab-2026' } });
    await once(device, 'open');
    parent = new WebSocket(url, ['audio-v1', 'parent.parent-secret-for-audio-lab-2026'], { ca: cert,
      headers: { Origin: `https://127.0.0.1:${app.address.port}` } });
    await once(parent, 'open');
    const pcm = Buffer.alloc(648); pcm[0] = 67; pcm[1] = 82; pcm[2] = 1; pcm[3] = 1;
    const got = once(parent, 'message'); device.send(pcm);
    assert.deepEqual((await got)[0], pcm);
  } finally {
    device?.terminate(); parent?.terminate(); await app?.close();
    await unlink(keyPath).catch(() => {});
    await unlink(certPath).catch(() => {});
    await rmdir(dir);
  }
});
