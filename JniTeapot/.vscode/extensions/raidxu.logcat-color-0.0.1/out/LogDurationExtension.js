"use strict";
const vscode = require("vscode");
class LogDurationExtension {
    constructor(calculator) {
        this._calculator = calculator;
        this._statusBarItem = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Left);
        let subscriptions = [];
        vscode.window.onDidChangeTextEditorSelection(this._onEvent, this, subscriptions);
        vscode.window.onDidChangeActiveTextEditor(this._onEvent, this, subscriptions);
        this.updateStatusBar();
        this._disposable = vscode.Disposable.from(...subscriptions);
    }
    dispose() {
        this._disposable.dispose();
        this._statusBarItem.dispose();
    }
    updateStatusBar() {
        let editor = vscode.window.activeTextEditor;
        if (!editor) {
            this._statusBarItem.hide();
            return;
        }
        let doc = editor.document;
        if (doc.languageId === "logcat") {
            let startLine = editor.selection.start.line;
            let endLine = editor.selection.end.line;
            while (true) {
                if (endLine <= startLine) {
                    this._statusBarItem.hide();
                    break;
                }
                let startMatchResult = editor.document
                    .lineAt(startLine).text
                    .match("^(([\\d-]+?)\\s+)?(([\\d:\\.]+?)\\s+)?((\\d+)(\\s+|-)(\\d+)(\/)?)?(([\\w\\.?]+?)?\\s+)?([VIEWAD])((\\s+|\/)(.+?))?:\\s+(.*)$");
                let startTimeString = null;
                if (startMatchResult) {
                    startTimeString = `${startMatchResult[2] || ""} ${startMatchResult[4]}`;
                }
                if (!startTimeString) {
                    startLine += 1;
                    continue;
                }
                let endMatchResult = editor.document
                    .lineAt(endLine).text
                    .match("^(([\\d-]+?)\\s+)?(([\\d:\\.]+?)\\s+)?((\\d+)(\\s+|-)(\\d+)(\/)?)?(([\\w\\.?]+?)?\\s+)?([VIEWAD])((\\s+|\/)(.+?))?:\\s+(.*)$");
                let endTimeString = null;
                if (endMatchResult) {
                    endTimeString = `${endMatchResult[2] || ""} ${endMatchResult[4]}`;
                }
                if (!endTimeString) {
                    endLine -= 1;
                    continue;
                }
                let durationString = this._calculator.calculateByString(startTimeString, endTimeString);
                if (durationString) {
                    this._statusBarItem.text = durationString;
                    this._statusBarItem.show();
                }
                else {
                    this._statusBarItem.hide();
                }
                break;
            }
        }
        else {
            this._statusBarItem.hide();
        }
    }
    _onEvent() {
        this.updateStatusBar();
    }
}
module.exports = LogDurationExtension;
//# sourceMappingURL=LogDurationExtension.js.map