const readline = require('readline');
const path = require('path');
const { spawn } = require('child_process');
const fs = require('fs');

const rl = readline.createInterface({
    input: process.stdin,
    output: process.stdout,
    terminal: false
});

const plugins = {};

rl.on('line', async (line) => {
    if (!line.trim()) return;
    try {
        const request = JSON.parse(line);
        const { method, params, id } = request;

        if (method === 'init') {
            const { pluginDir, pluginId } = params;
            const pluginJson = JSON.parse(fs.readFileSync(path.join(pluginDir, 'plugin.json'), 'utf8'));
            const pluginPath = path.join(pluginDir, pluginJson.Entry || pluginJson.ExecuteFileName);
            
            const isModule = !!pluginJson.Runtime || !pluginJson.ActionKeyword;

            plugins[pluginId] = {
                dir: pluginDir,
                entry: pluginPath,
                json: pluginJson,
                isModule: isModule
            };

            if (isModule) {
                try {
                    delete require.cache[require.resolve(pluginPath)];
                    const pluginModule = require(pluginPath);
                    plugins[pluginId].instance = pluginModule.plugin || pluginModule;
                    if (plugins[pluginId].instance.init) {
                        await plugins[pluginId].instance.init({}, { API: createMockAPI(pluginId, pluginDir) });
                    }
                } catch (e) {
                    sendResponse(id, { success: false, error: 'Failed to load module: ' + e.message });
                    return;
                }
            }

            sendResponse(id, { success: true });
        } else if (method === 'query') {
            const { query, pluginId } = params;
            const plugin = plugins[pluginId];
            if (!plugin) {
                sendResponse(id, { success: false, error: 'Plugin not found' });
                return;
            }

            if (plugin.isModule) {
                try {
                    if (plugin.instance && plugin.instance.query) {
                        const results = await plugin.instance.query({}, { Search: query });
                        sendResponse(id, { success: true, data: results });
                    } else {
                        sendResponse(id, { success: true, data: [] });
                    }
                } catch (e) {
                    sendResponse(id, { success: false, error: 'Query failed: ' + e.message });
                }
                return;
            }

            let search = query;
            const kw = plugin.json.ActionKeyword;
            if (kw && search.startsWith(kw)) {
                search = search.substring(kw.length).trim();
            }

            const flowRequest = {
                method: "query",
                parameters: [search],
                settings: {
                    enableNotification: true
                }
            };

            const child = spawn('node', [plugin.entry, JSON.stringify(flowRequest)], {
                cwd: plugin.dir,
                env: { ...process.env, FLOW_LAUNCHER_API_VER: '1.0.0' }
            });

            let stdout = '';
            let stderr = '';
            
            const handlePluginOutput = (line) => {
                if (!line.trim()) return;
                try {
                    const output = JSON.parse(line);
                    if (output.method && output.method.startsWith('Flow.Launcher.')) {
                        const method = output.method.replace('Flow.Launcher.', '');
                        if (method === 'RenderResponse') {
                            const results = output.parameters[0].result || output.parameters[0];
                            sendNotification('showResults', { pluginId, results });
                        } else if (method === 'ChangeQuery') {
                            sendNotification('changeQuery', { query: output.parameters[0], requery: output.parameters[1] });
                        }
                        return true; // Handled as notification
                    }
                    return false;
                } catch (e) {
                    return false;
                }
            };

            child.stdout.on('data', (data) => { 
                const chunk = data.toString();
                const lines = chunk.split('\n');
                lines.forEach(line => {
                    if (!handlePluginOutput(line)) {
                        stdout += line + '\n';
                    }
                });
            });
            child.stderr.on('data', (data) => { stderr += data; });
            child.on('close', (code) => {
                if (code !== 0 && !stdout.trim()) {
                    sendResponse(id, { success: false, error: `Process exited with code ${code}. Stderr: ${stderr}` });
                    return;
                }
                try {
                    // Find the last valid JSON in stdout if there were multiple lines
                    const lines = stdout.trim().split('\n');
                    let lastValidOutput = null;
                    for (let i = lines.length - 1; i >= 0; i--) {
                        try {
                            const output = JSON.parse(lines[i]);
                            if (output.result || output.data) {
                                lastValidOutput = output;
                                break;
                            }
                        } catch(e) {}
                    }

                    const results = lastValidOutput ? (lastValidOutput.result || lastValidOutput.data) : [];
                    sendResponse(id, { success: true, data: results });
                } catch (e) {
                    sendResponse(id, { success: false, error: `Failed to parse output. Stdout: ${stdout}. Stderr: ${stderr}` });
                }
            });

        } else if (method === 'execute') {
            const { pluginId, actionData } = params;
            const plugin = plugins[pluginId];
            if (!plugin) {
                sendResponse(id, { success: false, error: 'Plugin not found' });
                return;
            }

            if (plugin.isModule) {
                try {
                    if (plugin.instance && plugin.instance.execute) {
                        await plugin.instance.execute(actionData);
                    }
                    sendResponse(id, { success: true });
                } catch (e) {
                    sendResponse(id, { success: false, error: 'Execute failed: ' + e.message });
                }
                return;
            }

            const child = spawn('node', [plugin.entry, JSON.stringify(actionData)], {
                cwd: plugin.dir
            });
            child.on('close', () => {
                sendResponse(id, { success: true });
            });
        }
    } catch (error) {
        try {
            const request = JSON.parse(line);
            sendResponse(request.id, { success: false, error: error.message });
        } catch (e) {
            process.stderr.write('Error handling line: ' + error.message + '\n');
        }
    }
});

function createMockAPI(pluginId, pluginDir) {
    return {
        GetSetting: async (ctx, key) => {
            // Basic settings mock
            if (key === "shortcuts") return "[]";
            return "true";
        },
        Log: async (ctx, type, msg) => process.stderr.write(`[JS LOG][${pluginId}] ${type}: ${msg}\n`),
        OnSettingChanged: async () => {},
        ShowResults: async (ctx, results) => {
            sendNotification('showResults', { pluginId, results });
        },
        // Support Flow Launcher helper libraries
        RenderResponse: async (ctx, results) => {
            sendNotification('showResults', { pluginId, results: results.result || results });
        },
        ChangeQuery: async (ctx, query, requery = false) => {
            sendNotification('changeQuery', { query, requery });
        }
    };
}

function sendResponse(id, result) {
    process.stdout.write(JSON.stringify({
        jsonrpc: "2.0",
        id: id,
        result: result
    }) + "\n");
}

function sendNotification(method, params) {
    process.stdout.write(JSON.stringify({
        jsonrpc: "2.0",
        method: method,
        params: params
    }) + "\n");
}
