// The MCU publishes a latched CALL gesture state with a monotonically
// increasing sequence. Telemetry is frequent; only a new sequence is an event.
export class CallSignalTracker {
  lastSequence = null;

  update(call) {
    if (!call || !Number.isSafeInteger(call.sequence) || call.sequence < 0 ||
        typeof call.requested !== 'boolean') return null;
    if (call.sequence === this.lastSequence) return null;
    const initial = this.lastSequence === null;
    this.lastSequence = call.sequence;
    if (initial && !call.requested) return null;
    return call.requested ? 'invite' : 'hangup';
  }
}
