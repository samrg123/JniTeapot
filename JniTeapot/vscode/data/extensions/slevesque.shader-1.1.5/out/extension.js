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
exports.activate = void 0;
const vscode = require("vscode");
const fs = require("fs");
const Path = require("path");
const tmp = require("tmp");
const common_1 = require("./common");
const hoverProvider_1 = require("./hlsl/hoverProvider");
const completionProvider_1 = require("./hlsl/completionProvider");
const signatureProvider_1 = require("./hlsl/signatureProvider");
const symbolProvider_1 = require("./hlsl/symbolProvider");
const definitionProvider_1 = require("./hlsl/definitionProvider");
const referenceProvider_1 = require("./hlsl/referenceProvider");
class HLSLFormatingProvider {
    provideDocumentFormattingEdits(document, options, token) {
        return __awaiter(this, void 0, void 0, function* () {
            var tmpFile = tmp.fileSync({ prefix: 'hlsl-', postfix: '.cpp' });
            fs.writeFileSync(tmpFile.name, document.getText());
            let doc = yield vscode.workspace.openTextDocument(tmpFile.name);
            return vscode.commands.executeCommand('vscode.executeFormatDocumentProvider', doc.uri, options)
                .then(r => (tmpFile.removeCallback(), r));
        });
    }
    provideDocumentRangeFormattingEdits(document, range, options, token) {
        return __awaiter(this, void 0, void 0, function* () {
            var tmpFile = tmp.fileSync({ prefix: 'hlsl-', postfix: '.cpp' });
            fs.writeFileSync(tmpFile.name, document.getText());
            let doc = yield vscode.workspace.openTextDocument(tmpFile.name);
            return vscode.commands.executeCommand('vscode.executeFormatRangeProvider', doc.uri, range, options)
                .then(r => (tmpFile.removeCallback(), r));
        });
    }
}
const documentSelector = [
    { language: 'hlsl', scheme: 'file' },
    { language: 'hlsl', scheme: 'untitled' },
];
function searchRgPath() {
    function exeName() {
        const isWin = /^win/.test(process.platform);
        return isWin ? "rg.exe" : "rg";
    }
    function exePathIsDefined(rgExePath) {
        return fs.existsSync(rgExePath) ? rgExePath : undefined;
    }
    let rgPath = "";
    rgPath = exePathIsDefined(Path.join(vscode.env.appRoot, "node_modules/vscode-ripgrep/bin/", exeName()));
    if (rgPath) {
        return rgPath;
    }
    // If vscode-ripgrep is in an .asar file, then the binary is unpacked.
    rgPath = exePathIsDefined(Path.join(vscode.env.appRoot, "node_modules.asar.unpacked/vscode-ripgrep/bin/", exeName()));
    if (rgPath) {
        return rgPath;
    }
    return rgPath;
}
function activate(context) {
    return __awaiter(this, void 0, void 0, function* () {
        console.log('vscode-shader extension started');
        const rgDiskPath = searchRgPath();
        if (!rgDiskPath) {
            console.log("vscode-shader couldn't find vscode-ripgrep binary path");
        }
        common_1.setRgPath(rgDiskPath);
        const associations = vscode.workspace.getConfiguration('files.associations');
        for (const fileType of Object.keys(associations)) {
            if (associations[fileType] === 'hlsl') {
                common_1.setHlslExtensions(fileType.substring(1));
            }
        }
        // add providers
        context.subscriptions.push(vscode.languages.registerHoverProvider(documentSelector, new hoverProvider_1.default()));
        context.subscriptions.push(vscode.languages.registerCompletionItemProvider(documentSelector, new completionProvider_1.default(), '.'));
        context.subscriptions.push(vscode.languages.registerSignatureHelpProvider(documentSelector, new signatureProvider_1.default(), '(', ','));
        context.subscriptions.push(vscode.languages.registerReferenceProvider(documentSelector, new referenceProvider_1.default()));
        let symbolProvider = new symbolProvider_1.default();
        context.subscriptions.push(vscode.languages.registerDocumentSymbolProvider(documentSelector, symbolProvider));
        context.subscriptions.push(vscode.languages.registerWorkspaceSymbolProvider(symbolProvider));
        let definitionProvider = new definitionProvider_1.default();
        context.subscriptions.push(vscode.languages.registerDefinitionProvider(documentSelector, definitionProvider));
        context.subscriptions.push(vscode.languages.registerImplementationProvider(documentSelector, definitionProvider));
        context.subscriptions.push(vscode.languages.registerTypeDefinitionProvider(documentSelector, definitionProvider));
        if (vscode.extensions.getExtension('ms-vscode.cpptools') !== undefined) {
            let formatingProvider = new HLSLFormatingProvider();
            context.subscriptions.push(vscode.languages.registerDocumentFormattingEditProvider(documentSelector, formatingProvider));
            context.subscriptions.push(vscode.languages.registerDocumentRangeFormattingEditProvider(documentSelector, formatingProvider));
        }
    });
}
exports.activate = activate;
//# sourceMappingURL=extension.js.map