import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { runInNewContext } from 'node:vm';

test('capture worklet turns 48 kHz microphone samples into 50 PCM16 frames', async () => {
  const source = await readFile(new URL('../public/capture-worklet.js', import.meta.url), 'utf8');
  let Worklet;
  const messages = [];
  class Base { port = { postMessage: (buffer) => messages.push(buffer) }; }
  runInNewContext(source, {
    AudioWorkletProcessor: Base, Int16Array, sampleRate: 48000,
    registerProcessor: (_, klass) => { Worklet = klass; }
  });
  const processor = new Worklet();
  const samples = new Float32Array(48000).fill(0.25);
  for (let offset = 0; offset < samples.length; offset += 128) {
    processor.process([[samples.subarray(offset, offset + 128)]]);
  }
  assert.equal(messages.length, 50);
  for (const buffer of messages) {
    const frame = new Int16Array(buffer);
    assert.equal(frame.length, 320);
    assert.equal(frame[0], 8192);
  }
});
