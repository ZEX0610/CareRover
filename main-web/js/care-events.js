// Local AP mirror of the relay's face-based seating reminders. A face box is
// not identity recognition or proof that the child has left/returned.
export class CareEventTracker {
  constructor() {
    this.noFaceSince = null; this.lastNoFaceAt = null; this.faceAlert = false;
    this.callSequence = null;
  }
  update(frame, now = Date.now()) {
    if (frame?.type !== 'telemetry') return [];
    const events = [];
    const camera = frame.connection?.camera === true;
    const person = frame.vision?.person;
    const fresh = camera && person?.predicted !== true &&
      Number.isFinite(person?.age_ms) && person.age_ms >= 0 && person.age_ms <= 1500;
    const detected = fresh && person.found === true;
    const absent = fresh && person.found === false;
    if (detected) {
      this.noFaceSince = this.lastNoFaceAt = null;
      if (this.faceAlert) { this.faceAlert = false; events.push('seat_returned'); }
    } else if (absent) {
      if (this.noFaceSince === null || (this.lastNoFaceAt !== null && now - this.lastNoFaceAt > 2500))
        this.noFaceSince = now;
      this.lastNoFaceAt = now;
      if (!this.faceAlert && now - this.noFaceSince >= 30_000) {
        this.faceAlert = true; events.push('seat_left');
      }
    } else if (!camera || (this.lastNoFaceAt !== null && now - this.lastNoFaceAt > 2500)) {
      this.noFaceSince = this.lastNoFaceAt = null;
    }
    const call = frame.call;
    if (typeof call?.requested === 'boolean' && Number.isSafeInteger(call.sequence) &&
        call.sequence >= 0 && call.sequence !== this.callSequence) {
      const initial = this.callSequence === null;
      this.callSequence = call.sequence;
      if (!initial || call.requested) events.push(call.requested ? 'call_invite' : 'call_cancelled');
    }
    return events;
  }
}
