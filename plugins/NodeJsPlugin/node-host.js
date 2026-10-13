const readline = require('readline');
const path = require('path');
const { spawn } = require('child_process');
const fs = require('fs');
const { StringDecoder } = require('string_decoder');

const rl = readline.createInterface({
    input: process.stdin,
    output: process.stdout,
    terminal: false
});

const plugins = {};
// CLI 模式查询正在运行的子进程：C++ 侧 queryId -> { 请求 id -> 取消函数 }。
// 同一个 queryId 可能同时匹配多个脚本、每个脚本一个子进程，收到 cancel 时要全部杀掉；
// 某个子进程结束时只能移除自己那一项，不能把别的脚本的子进程当成已取消
const activeQueries = {};

function trackQueryChild(queryKey, requestId, cancel) {
    if (queryKey === undefined) return;
    if (!activeQueries[queryKey]) activeQueries[queryKey] = {};
    activeQueries[queryKey][requestId] = cancel;
}

function untrackQueryChild(queryKey, requestId) {
    if (queryKey === undefined) return;
    const children = activeQueries[queryKey];
    if (!children) return;
    delete children[requestId];
    if (Object.keys(children).length === 0) delete activeQueries[queryKey];
}

// 子进程 stdout 的一个 data 块不一定是完整的一行，也可能把一个多字节 UTF-8 字符拆开：
// 用流式解码器拼接，只把以换行结尾的完整行交给 onLine，close 时冲掉最后一段没有换行的内容
function createLineReader(onLine) {
    const decoder = new StringDecoder('utf8');
    let pending = '';
    return {
        push(data) {
            pending += decoder.write(data);
            let index;
            while ((index = pending.indexOf('\n')) >= 0) {
                onLine(pending.slice(0, index));
                pending = pending.slice(index + 1);
            }
        },
        end() {
            pending += decoder.end();
            if (pending) onLine(pending);
            pending = '';
        }
    };
}

rl.on('line', async (line) => {
    if (!line.trim()) return;
    try {
        const request = JSON.parse(line);
        const { method, params, id } = request;

        if (method === 'cancel') {
            // 通知，没有 id：输入已变化，停止该查询的全部子进程（模块模式无法中断，C++ 侧会丢弃过期结果）
            const key = String(params && params.queryId);
            const children = activeQueries[key];
            if (children) {
                delete activeQueries[key];
                for (const cancel of Object.values(children)) {
                    try { cancel(); } catch (e) {}
                }
            }
            return;
        }

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
            const { query, pluginId, queryId } = params;
            const plugin = plugins[pluginId];
            if (!plugin) {
                sendResponse(id, { success: false, error: 'Plugin not found' });
                return;
            }

            if (plugin.isModule) {
                try {
                    if (plugin.instance && plugin.instance.query) {
                        // ctx 原样传回 API.ShowResults / RenderResponse，让异步推送带上来源查询 id
                        const results = await plugin.instance.query({ queryId }, { Search: query });
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

            const queryKey = queryId !== undefined ? String(queryId) : undefined;
            // 子进程的 error 和 close 都可能触发，一个请求只能回复一次
            let settled = false;
            let cancelled = false;
            const settle = (result) => {
                if (settled) return;
                settled = true;
                untrackQueryChild(queryKey, id);
                sendResponse(id, result);
            };

            let child;
            try {
                child = spawn('node', [plugin.entry, JSON.stringify(flowRequest)], {
                    cwd: plugin.dir,
                    env: { ...process.env, FLOW_LAUNCHER_API_VER: '1.0.0' }
                });
            } catch (e) {
                settle({ success: false, error: 'Failed to start script: ' + e.message });
                return;
            }
            trackQueryChild(queryKey, id, () => {
                cancelled = true;
                try { child.kill(); } catch (e) {}
            });

            let stdout = '';
            const stderrDecoder = new StringDecoder('utf8');
            let stderr = '';

            const handlePluginOutput = (line) => {
                if (!line.trim()) return;
                try {
                    const output = JSON.parse(line);
                    if (output.method && output.method.startsWith('Flow.Launcher.')) {
                        const method = output.method.replace('Flow.Launcher.', '');
                        if (method === 'RenderResponse') {
                            const results = output.parameters[0].result || output.parameters[0];
                            sendNotification('showResults', { pluginId, queryId, results });
                        } else if (method === 'ChangeQuery') {
                            // 带上来源查询 id，C++ 侧据此丢弃用户已经输入新内容之后才到的改写请求
                            sendNotification('changeQuery', { queryId, query: output.parameters[0], requery: output.parameters[1] });
                        }
                        return true; // Handled as notification
                    }
                    return false;
                } catch (e) {
                    return false;
                }
            };

            const stdoutReader = createLineReader((line) => {
                if (!handlePluginOutput(line)) {
                    stdout += line + '\n';
                }
            });
            child.stdout.on('data', (data) => stdoutReader.push(data));
            child.stderr.on('data', (data) => { stderr += stderrDecoder.write(data); });
            // 找不到 node、无法创建进程等异步错误没有监听器会直接终止本宿主进程
            child.on('error', (e) => {
                settle({ success: false, error: 'Failed to run script: ' + e.message });
            });
            child.on('close', (code) => {
                stdoutReader.end();
                stderr += stderrDecoder.end();
                if (cancelled) {
                    settle({ success: false, error: 'cancelled' });
                    return;
                }
                if (code !== 0 && !stdout.trim()) {
                    settle({ success: false, error: `Process exited with code ${code}. Stderr: ${stderr}` });
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
                    settle({ success: true, data: results });
                } catch (e) {
                    settle({ success: false, error: `Failed to parse output. Stdout: ${stdout}. Stderr: ${stderr}` });
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

            let settled = false;
            const settle = (result) => {
                if (settled) return;
                settled = true;
                sendResponse(id, result);
            };
            let child;
            try {
                child = spawn('node', [plugin.entry, JSON.stringify(actionData)], {
                    cwd: plugin.dir
                });
            } catch (e) {
                settle({ success: false, error: 'Failed to start script: ' + e.message });
                return;
            }
            child.on('error', (e) => {
                settle({ success: false, error: 'Failed to run script: ' + e.message });
            });
            child.on('close', () => {
                settle({ success: true });
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
        // ctx 是 query 时传入的 { queryId }；脚本传了别的东西就不带 id，C++ 侧按当前查询处理
        ShowResults: async (ctx, results) => {
            sendNotification('showResults', { pluginId, queryId: ctx && ctx.queryId, results });
        },
        // Support Flow Launcher helper libraries
        RenderResponse: async (ctx, results) => {
            sendNotification('showResults', { pluginId, queryId: ctx && ctx.queryId, results: results.result || results });
        },
        // 同样带上来源查询 id（ctx 不是 query 传入的对象时没有 id，C++ 侧照旧接受）
        ChangeQuery: async (ctx, query, requery = false) => {
            sendNotification('changeQuery', { queryId: ctx && ctx.queryId, query, requery });
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
