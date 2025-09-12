import * as path from 'path';
import * as cp from 'child_process';
import * as vscode from 'vscode';

import {
  LanguageClient,
  LanguageClientOptions,
  ServerOptions,
  TransportKind
} from 'vscode-languageclient/node';

let client: LanguageClient;

export function activate(context: vscode.ExtensionContext) {
  const outputChannel = vscode.window.createOutputChannel('Organ 16 Language Server');

  const serverScript = context.asAbsolutePath(
    path.join('server', 'lsp-server.py')
  );

  const serverOptions: ServerOptions = {
    command: 'python',
    args: [serverScript],
    transport: TransportKind.stdio
  };

  const clientOptions: LanguageClientOptions = {
    documentSelector: [{ scheme: 'file', language: 'organ' }],
    outputChannel,
  };

  client = new LanguageClient(
    'Organ 16 Language',
    'Organ 16 Language Server',
    serverOptions,
    clientOptions
  );

  client.start();

  const command = 'organ-16-asm-support.compile';

  const commandHandler = () => {
    // Pick the .l file using VS Code’s file picker
    vscode.window.showOpenDialog({
      canSelectMany: false,
      filters: { 'Linker Files': ['l'] },
      openLabel: 'Select linker file'
    }).then(fileUri => {
      if (!fileUri || fileUri.length === 0) {
        vscode.window.showWarningMessage('No file selected.');
        return;
      }

      const linkerFilePath = fileUri[0].fsPath;

      // Build Python command
      const pythonScriptPath = path.join(context.extensionPath, 'compiler', 'compiler.py');
      const command = `python "${pythonScriptPath}" "${linkerFilePath}"`;

      // Execute
      cp.exec(command, (err, stdout, stderr) => {
        if (err) {
          vscode.window.showErrorMessage(`Python Error: ${err.message}`);
          return;
        }

        if (stderr) {
          vscode.window.showErrorMessage(`Python Error: ${stderr}`);
        }

        if(stdout) {
          vscode.window.showInformationMessage(`Python Info/Warning: ${stdout}`);
        }

        if(!err && !stderr){
          vscode.window.showInformationMessage('✔ Compilation done!');
        }
      });
    });
  };

  context.subscriptions.push(vscode.commands.registerCommand(command, commandHandler));
}

export function deactivate(): Thenable<void> | undefined {
  return client ? client.stop() : undefined;
}