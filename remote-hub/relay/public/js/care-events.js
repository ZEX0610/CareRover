// Event semantics shared by the relay and browser. A missing camera is unknown,
// never evidence that the child has left the camera's field of view.
export class CareEventTracker {
  constructor({ faceDelayMs = 30_000 } = {}) {
    this.faceDelayMs = faceDelayMs;
    this.noFaceSince = null;
    this.lastNoFaceAt = null;
    this.faceAlert = false;
    this.seat = null;
    this.callSequence = null;
    this.callRequested = false;
    this.cameraOnline = false;
  }

  update(frame, now = Date.now()) {
    const events = [];
    if (frame?.type !== 'telemetry') return events;
    this.cameraOnline = frame.connection?.camera === true;
    const person = frame.vision?.person;
    const fresh = this.cameraOnline && person?.predicted !== true &&
      Number.isFinite(person?.age_ms) && person.age_ms >= 0 && person.age_ms <= 1500;
    const detected = fresh && person.found === true;
    const absent = fresh && person.found === false;
    if (detected) {
      this.noFaceSince = this.lastNoFaceAt = null;
      this.seat = true;
      if (this.faceAlert) {
        this.faceAlert = false;
        events.push({ kind: 'seat_returned', active: false });
      }
    } else if (absent) {
      if (this.noFaceSince === null || (this.lastNoFaceAt !== null && now - this.lastNoFaceAt > 2500))
        this.noFaceSince = now;
      this.lastNoFaceAt = now;
      if (!this.faceAlert && now - this.noFaceSince >= this.faceDelayMs) {
        this.faceAlert = true;
        this.seat = false;
        events.push({ kind: 'seat_left', active: true });
      }
    } else if (!this.cameraOnline || (this.lastNoFaceAt !== null && now - this.lastNoFaceAt > 2500)) {
      this.noFaceSince = this.lastNoFaceAt = null;
    }

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
  seat_left: { title: '离座提醒', body: '已连续 30 秒未识别到人脸，请查看画面确认。', sound: 'none', display_ms: 6500 },
  seat_returned: { title: '入座提醒', body: '重新识别到人脸，请查看画面确认。', sound: 'none', display_ms: 6500 },
  call_invite: { title: '孩子请求通话', body: '请确认是否接听。', sound: 'ring' },
  call_cancelled: { title: '孩子已取消通话', body: '来电请求已结束。', sound: 'none' },
  call_connected: { title: '通话已接通', body: '双向音频连接已建立。', sound: 'none' },
  call_ended: { title: '通话已结束', body: '双向音频连接已断开。', sound: 'none' }
});
