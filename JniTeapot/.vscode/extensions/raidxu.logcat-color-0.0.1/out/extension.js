"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
const LogDurationExtension = require("./LogDurationExtension");
const LogDurationCalculator = require("./LogDurationCalculator");
// this method is called when your extension is activated
// your extension is activated the very first time the command is executed
function activate(context) {
    console.log('Congratulations, your extension "logcat-color" is now active!');
    let logDurationExtension = new LogDurationExtension(new LogDurationCalculator());
    context.subscriptions.push(logDurationExtension);
}
exports.activate = activate;
// this method is called when your extension is deactivated
function deactivate() { }
exports.deactivate = deactivate;
//# sourceMappingURL=extension.js.map