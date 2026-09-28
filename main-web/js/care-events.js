// Local AP mirror of the relay's care-event decisions. These are observations,
// not identity recognition or proof that the child has left the room.
export class CareEventTracker {
  constructor() {
    this.noFaceSince = null; this.faceAlert = false;
    this.seat = null; this.seatCandidate = null; this.seatCandidateSince = 0;
    this.callSequence = null;
  }
  update(frame, now = Date.now()) {
    if (frame?.type !== 'telemetry') return [];
    const events = [];
    const camera = frame.connection?.camera === true;
    const person = frame.vision?.person;
    const detected = camera && person?.found === true && person.predicted !== true &&
      Number.isFinite(person.age_ms) && person.age_ms >= 0 && person.age_ms <= 1500;
    if (detected) {
      this.noFaceSince = null;
      if (this.faceAlert) { this.faceAlert = false; events.push('face_restored'); }
    } else if (camera && person?.found === false) {
      if (this.noFaceSince === null) this.noFaceSince = now;
      if (!this.faceAlert && now - this.noFaceSince >= 30_000) {
        this.faceAlert = true; events.push('face_absent');
      }
    } else if (!camera && !this.faceAlert) this.noFaceSince = null;
    const front = frame.front;
    const seatValid = frame.robot?.mode === 'IDLE' && front?.valid === true &&
      front.call_paused !== true && typeof front.seated === 'boolean';
    if (seatValid) {
      if (this.seat === null) this.seat = front.seated;
      else if (front.seated === this.seat) this.seatCandidate = null;
      else if (this.seatCandidate !== front.seated) {
        this.seatCandidate = front.seated; this.seatCandidateSince = now;
      } else if (now - this.seatCandidateSince >= 800) {
        this.seat = front.seated; this.seatCandidate = null;
        events.push(this.seat ? 'seat_returned' : 'seat_left');
      }
    } else this.seatCandidate = null;
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
