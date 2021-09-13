'use strict';
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    function adopt(value) { return value instanceof P ? value : new P(function (resolve) { resolve(value); }); }
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : adopt(result.value).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.linkToMarkdownString = exports.textToMarkedString = void 0;
const vscode_1 = require("vscode");
const html_1 = require("./html");
const hlslGlobals = require("./hlslGlobals");
const follow_redirects_1 = require("follow-redirects");
const jsdom_1 = require("jsdom");
function textToMarkedString(text) {
    return text.replace(/[\\`*_{}[\]()#+\-.!]/g, '\\$&'); // escape markdown syntax tokens: http://daringfireball.net/projects/markdown/syntax#backslash
}
exports.textToMarkedString = textToMarkedString;
function linkToMarkdownString(linkUrl) {
    if (linkUrl === undefined || linkUrl === '') {
        return;
    }
    let link = new vscode_1.MarkdownString('[HLSL documentation][1]\n\n[1]: ');
    let openDocOnSide = vscode_1.workspace.getConfiguration('hlsl').get('openDocOnSide', false);
    if (openDocOnSide) {
        link.appendText(encodeURI('command:shader.openLink?' + JSON.stringify([linkUrl, true])));
    }
    else {
        link.appendText(linkUrl);
    }
    link.isTrusted = true;
    return link;
}
exports.linkToMarkdownString = linkToMarkdownString;
class HLSLHoverProvider {
    constructor() {
        this._subscriptions = [];
        this._panel = null;
        this._subscriptions.push(vscode_1.commands.registerCommand('shader.openLink', (link, newWindow) => {
            if (!this._panel) {
                this._panel = vscode_1.window.createWebviewPanel('hlsldoc', 'HLSL Documentation', newWindow ? vscode_1.ViewColumn.Two : vscode_1.ViewColumn.Active, {
                    // Enable scripts in the webview
                    enableScripts: true
                });
                this._panel.onDidDispose(() => {
                    this._panel = null;
                });
                this._panel.webview.onDidReceiveMessage(message => {
                    switch (message.command) {
                        case 'clickLink':
                            vscode_1.commands.executeCommand('shader.openLink', message.text);
                            return;
                    }
                });
            }
            this._panel.reveal();
            // And set its HTML content
            getWebviewContent(link).then(html => this._panel.webview.html = html);
        }));
    }
    getSymbols(document) {
        return vscode_1.commands.executeCommand('vscode.executeDocumentSymbolProvider', document.uri);
    }
    dispose() {
        this._subscriptions.forEach(s => { s.dispose(); });
    }
    provideHover(document, position, token) {
        return __awaiter(this, void 0, void 0, function* () {
            let enable = vscode_1.workspace.getConfiguration('hlsl').get('suggest.basic', true);
            if (!enable) {
                return null;
            }
            let wordRange = document.getWordRangeAtPosition(position);
            if (!wordRange) {
                return null;
            }
            let name = document.getText(wordRange);
            let backchar = '';
            if (wordRange.start.character > 0) {
                let backidx = wordRange.start.translate({ characterDelta: -1 });
                backchar = backidx.character < 0 ? '' : document.getText(new vscode_1.Range(backidx, wordRange.start));
            }
            if (backchar === '#') {
                const key = name.substr(1);
                var entry = hlslGlobals.preprocessors[name.toUpperCase()];
                if (entry && entry.description) {
                    let signature = '(*preprocessor*) ';
                    signature += '**#' + name + '**';
                    let contents = [];
                    contents.push(new vscode_1.MarkdownString(signature));
                    contents.push(textToMarkedString(entry.description));
                    contents.push(linkToMarkdownString(entry.link));
                    return new vscode_1.Hover(contents, wordRange);
                }
            }
            var entry = hlslGlobals.intrinsicfunctions[name];
            if (entry && entry.description) {
                let signature = '(*function*) ';
                signature += '**' + name + '**';
                signature += '(';
                if (entry.parameters && entry.parameters.length != 0) {
                    let params = '';
                    entry.parameters.forEach(p => params += p.label + ',');
                    signature += params.slice(0, -1);
                }
                signature += ')';
                let contents = [];
                contents.push(new vscode_1.MarkdownString(signature));
                contents.push(textToMarkedString(entry.description));
                contents.push(linkToMarkdownString(entry.link));
                return new vscode_1.Hover(contents, wordRange);
            }
            entry = hlslGlobals.datatypes[name];
            if (entry && entry.description) {
                let signature = '(*datatype*) ';
                signature += '**' + name + '**';
                let contents = [];
                contents.push(new vscode_1.MarkdownString(signature));
                contents.push(textToMarkedString(entry.description));
                contents.push(linkToMarkdownString(entry.link));
                return new vscode_1.Hover(contents, wordRange);
            }
            entry = hlslGlobals.semantics[name.toUpperCase()];
            if (entry && entry.description) {
                let signature = '(*semantic*) ';
                signature += '**' + name + '**';
                let contents = [];
                contents.push(new vscode_1.MarkdownString(signature));
                contents.push(textToMarkedString(entry.description));
                contents.push(linkToMarkdownString(entry.link));
                return new vscode_1.Hover(contents, wordRange);
            }
            let key = name.replace(/\d+$/, ''); //strip tailing number
            entry = hlslGlobals.semanticsNum[key.toUpperCase()];
            if (entry && entry.description) {
                let signature = '(*semantic*) ';
                signature += '**' + name + '**';
                let contents = [];
                contents.push(new vscode_1.MarkdownString(signature));
                contents.push(textToMarkedString(entry.description));
                contents.push(linkToMarkdownString(entry.link));
                return new vscode_1.Hover(contents, wordRange);
            }
            entry = hlslGlobals.keywords[name];
            if (entry) {
                let signature = '(*keyword*) ';
                signature += '**' + name + '**';
                let contents = [];
                contents.push(new vscode_1.MarkdownString(signature));
                contents.push(textToMarkedString(entry.description));
                contents.push(linkToMarkdownString(entry.link));
                return new vscode_1.Hover(contents, wordRange);
            }
            let symbols = yield this.getSymbols(document);
            for (let s of symbols) {
                if (s.name === name) {
                    let contents = [];
                    let signature = '(*' + vscode_1.SymbolKind[s.kind].toLowerCase() + '*) ';
                    signature += s.containerName ? s.containerName + '.' : '';
                    signature += '**' + name + '**';
                    contents.push(new vscode_1.MarkdownString(signature));
                    if (s.location.uri.toString() === document.uri.toString()) {
                        //contents = [];
                        contents.push({ language: 'hlsl', value: document.getText(s.location.range) });
                    }
                    return new vscode_1.Hover(contents, wordRange);
                }
            }
        });
    }
}
exports.default = HLSLHoverProvider;
function getWebviewContent(link) {
    const uri = vscode_1.Uri.parse(link);
    return new Promise((resolve, reject) => {
        let request = follow_redirects_1.https.request({
            host: uri.authority,
            path: uri.path,
            rejectUnauthorized: vscode_1.workspace.getConfiguration().get("http.proxyStrictSSL", true)
        }, (response) => {
            if (response.statusCode == 301 || response.statusCode == 302)
                return resolve(response.headers.location);
            if (response.statusCode != 200)
                return resolve(response.statusCode.toString());
            let html = '';
            response.on('data', (data) => { html += data.toString(); });
            response.on('end', () => {
                var _a, _b;
                const dom = new jsdom_1.JSDOM(html);
                let topic = '';
                let node = dom.window.document.querySelector('.content');
                if (node) {
                    let num = node.getElementsByTagName('a').length;
                    for (let i = 0; i < num; ++i) {
                        const href = node.getElementsByTagName('a')[i].href;
                        const fulllink = new dom.window.URL(href, uri.toString()).href;
                        node.getElementsByTagName('a')[i].href = '#';
                        node.getElementsByTagName('a')[i].setAttribute('onclick', `clickLink('${fulllink}')`);
                    }
                    (_a = node.querySelector('.metadata.page-metadata')) === null || _a === void 0 ? void 0 : _a.remove();
                    (_b = node.querySelector('#center-doc-outline')) === null || _b === void 0 ? void 0 : _b.remove();
                    topic = node.outerHTML;
                }
                else {
                    let link = uri.with({ scheme: 'https' }).toString();
                    topic = `<a href="${link}">No topic found, click to follow link</a>`;
                }
                resolve(html_1.HTML_TEMPLATE.replace('{0}', topic));
            });
            response.on('error', (error) => { console.log(error); });
        });
        request.on('error', (error) => { console.log(error); });
        request.end();
    });
}
//# sourceMappingURL=hoverProvider.js.map