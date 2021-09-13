"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.setHlslExtensions = exports.getRgPath = exports.setRgPath = exports.hlslExtensions = exports.rgPath = void 0;
exports.rgPath = '';
exports.hlslExtensions = [];
function setRgPath(path) {
    exports.rgPath = path;
}
exports.setRgPath = setRgPath;
function getRgPath() {
    return exports.rgPath;
}
exports.getRgPath = getRgPath;
function setHlslExtensions(ext) {
    exports.hlslExtensions.push(ext);
}
exports.setHlslExtensions = setHlslExtensions;
//# sourceMappingURL=common.js.map