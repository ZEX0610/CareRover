// Event semantics shared by the relay and browser. A missing camera is unknown,
// never evidence that the child has left the camera's field of view.
export class CareEventTracker {
  constructor({ faceDelayMs = 30_000, seatDelayMs = 800 } = {}) {
    this.faceDelayMs = faceDelayMs;
    this.seatDelayMs = seatDelayMs;
    this.noFaceSince = null;
    this.faceAlert = false;
    this.seat = null;
    this.seatCandidate = null;
    this.seatCandidateSince = 0;
    this.callSequence = null;
    this.callRequested = false;
    this.cameraOnline = false;
  }

  update(frame, now = Date.now()) {
    const events = [];
    if (frame?.type !== 'telemetry') return events;
    this.cameraOnline = frame.connection?.camera === true;
    const person = frame.vision?.person;
    const detected = this.cameraOnline && person?.found === true &&
      person.predicted !== true && Number.isFinite(person.age_ms) &&
      person.age_ms >= 0 && person.age_ms <= 1500;
    const absent = this.cameraOnline && person?.found === false;
    if (detected) {
      this.noFaceSince = null;
      if (this.faceAlert) {
        this.faceAlert = false;
        events.push({ kind: 'face_restored', active: false });
      }
    } else if (absent) {
      if (this.noFaceSince === null) this.noFaceSince = now;
      if (!this.faceAlert && now - this.noFaceSince >= this.faceDelayMs) {
        this.faceAlert = true;
        events.push({ kind: 'face_absent', active: true });
      }
    } else if (!this.cameraOnline && !this.faceAlert) {
      this.noFaceSince = null;
    }

    const front = frame.front;
    const seatValid = frame.robot?.mode === 'IDLE' && front?.valid === true &&
      front.call_paused !== true && typeof front.seated === 'boolean';
    if (seatValid) {
      if (this.seat === null) this.seat = front.seated; // Baseline is not an arrival.
      else if (front.seated === this.seat) this.seatCandidate = null;
      else {
        if (this.seatCandidate !== front.seated) {
          this.seatCandidate = front.seated;
          this.seatCandidateSince = now;
        } else if (now - this.seatCandidateSince >= this.seatDelayMs) {
          this.seat = front.seated;
          this.seatCandidate = null;
          events.push({ kind: this.seat ? 'seat_returned' : 'seat_left', active: !this.seat });
        }
      }
    } else this.seatCandidate = null;

    const call = frame.call;
    if (typeof call?.requested === 'boolean' && Number.isSafeInteger(call.sequence) && call.sequence >= 0 &&
        call.sequence !== this.callSequence) {
      const initial = this.callSequence === null;
      this.callSequence = call.sequence;
      this.callRequested = call.requested;
      if (!initial || call.requested)
        events.push({ kind: call.requested ? 'call_invite' : 'call_cancelled', active: call.requested,
          sequence: call.sequence });
    }
    return events;
  }

  snapshot() {
    return { type: 'care_state', camera_online: this.cameraOnline,
      face_alert_active: this.faceAlert, seat: this.seat === null ? 'unknown' : this.seat ? 'seated' : 'vacant',
      call_requested: this.callRequested, call_sequence: this.callSequence };
  }
}

export const CARE_EVENT_TEXT = Object.freeze({
  face_absent: { title: '30 秒未识别到人脸', body: '请查看实时画面确认孩子情况。', sound: 'none' },
  face_restored: { title: '重新识别到人脸', body: '无人脸提醒已解除。', sound: 'none' },
  seat_left: { title: '离座提醒', body: '前方距离变化提示可能离座，请查看画面确认。', sound: 'none' },
  seat_returned: { title: '入座提醒', body: '前方距离恢复，提示可能已经入座。', sound: 'none' },
  call_invite: { title: '孩子请求通话', body: '请确认是否接听。', sound: 'ring' },
  call_cancelled: { title: '孩子已取消通话', body: '来电请求已结束。', sound: 'none' },
  call_connected: { title: '通话已接通', body: '双向音频连接已建立。', sound: 'none' },
  call_ended: { title: '通话已结束', body: '双向音频连接已断开。', sound: 'none' }
});
