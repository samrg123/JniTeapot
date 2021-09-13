"use strict";
const moment = require("moment");
class LogDurationCalculator {
    calculateByString(start, end) {
        let startMoment = moment(start);
        let endMoment = moment(end);
        let duration = null;
        if (startMoment.isValid() && endMoment.isValid()) {
            duration = this._calculateByMoment(startMoment, endMoment);
        }
        else {
            let startDuration = moment.duration((+start) * 1000);
            let endDuration = moment.duration((+end) * 1000);
            if (startDuration.isValid() && endDuration.isValid()) {
                duration = this._calculateByDuration(startDuration, endDuration);
            }
        }
        if (duration) {
            return this._durationToString(duration);
        }
        else {
            return null;
        }
    }
    _durationToString(duration) {
        let text = '';
        let keys = ["y", "M", "d", "h", "m", "s"];
        for (let key of keys) {
            if (duration.as(key) > 1) {
                text += duration.get(key) + key;
            }
        }
        text += duration.get("ms") + "ms";
        return text;
    }
    _calculateByMoment(start, end) {
        return moment.duration(end.diff(start));
    }
    _calculateByDuration(start, end) {
        return moment.duration(end.asMilliseconds() - start.asMilliseconds());
    }
}
module.exports = LogDurationCalculator;
//# sourceMappingURL=LogDurationCalculator.js.map