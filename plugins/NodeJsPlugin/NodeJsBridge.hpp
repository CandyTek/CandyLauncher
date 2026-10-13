#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <atomic>
#include <functional>
#include <stdexcept>
#include <unordered_map>
#include "../../util/json.hpp"
#include "../../util/StringUtil.hpp"
#include "../../util/LogUtil.hpp"

class NodeJsBridge {
private:
    HANDLE m_hProcess = nullptr;
    HANDLE m_hStdInWrite = nullptr;
    HANDLE m_hStdOutRead = nullptr;
    std::thread m_readerThread;
    std::mutex m_mutex;
    std::mutex m_writeMutex;
    std::unordered_map<int, std::promise<nlohmann::json>> m_pendingRequests;
    int m_nextRequestId = 1;
    std::atomic<bool> m_running{false};

    typedef std::function<void(const std::string& method, const nlohmann::json& params)> NotificationHandler;
    NotificationHandler m_notificationHandler;

    bool WriteLine(std::string line) {
        line += '\n';
        std::lock_guard<std::mutex> lock(m_writeMutex);
        if (!m_hStdInWrite) return false;
        DWORD bytesWritten = 0;
        return WriteFile(m_hStdInWrite, line.c_str(), (DWORD)line.size(), &bytesWritten, nullptr) &&
            bytesWritten == line.size();
    }

    // 让所有等待回复的 future 抛出异常，查询线程才不会永远卡在 get() 上
    void FailPendingRequests(const char* reason) {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& [id, promise] : m_pendingRequests) {
            try {
                promise.set_exception(std::make_exception_ptr(std::runtime_error(reason)));
            } catch (const std::future_error&) {
                // promise 已经有值，忽略
            }
        }
        m_pendingRequests.clear();
    }

    void ReaderThread() {
        char buffer[8192];
        std::string accumulated;
        DWORD bytesRead;

        while (m_running) {
            if (!ReadFile(m_hStdOutRead, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) || bytesRead == 0) {
                // node 退出或管道断开：之后的请求不会再有回复
                if (m_running) Loge(L"NodeJsBridge", L"node host exited unexpectedly");
                m_running = false;
                FailPendingRequests("node host exited");
                break;
            }
            buffer[bytesRead] = '\0';
            accumulated += buffer;

            size_t pos;
            while ((pos = accumulated.find('\n')) != std::string::npos) {
                std::string line = accumulated.substr(0, pos);
                accumulated.erase(0, pos + 1);
                
                if (line.empty()) continue;
                if (line.back() == '\r') line.pop_back();
                if (line.empty()) continue;

                if (line[0] != '{') {
                    // This is likely a log message, not a JSON response
                    Logi(L"NodeJsBridge", L"[JS] ", line);
                    continue;
                }

                try {
                    auto response = nlohmann::json::parse(line);
                    if (response.contains("id") && response["id"].is_number()) {
                        int id = response["id"];
                        std::lock_guard<std::mutex> lock(m_mutex);
                        auto it = m_pendingRequests.find(id);
                        if (it != m_pendingRequests.end()) {
                            it->second.set_value(response["result"]);
                            m_pendingRequests.erase(it);
                        }
                    } else if (response.contains("method") && response["method"].is_string()) {
                        std::string method = response["method"];
                        nlohmann::json params = response.contains("params") ? response["params"] : nlohmann::json();
                        if (m_notificationHandler) {
                            m_notificationHandler(method, params);
                        }
                    }
                } catch (const std::exception&) {
                    // If it failed to parse but started with '{', it might still be a log or malformed JSON
                    Logw(L"NodeJsBridge", L"[JS Log/Error] ", line);
                }
            }
        }
    }

public:
    NodeJsBridge() = default;
    ~NodeJsBridge() {
        Stop();
    }

    void SetNotificationHandler(NotificationHandler handler) {
        m_notificationHandler = handler;
    }

    bool Start(const std::wstring& nodeExePath, const std::wstring& scriptPath) {
        Stop();

        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = nullptr;

        // 启动的任一步失败都要关掉已创建的管道两端，并把成员置空，否则 Stop() 会再关一次过期句柄
        HANDLE hStdInRead = nullptr, hStdOutWrite = nullptr;
        const auto fail = [&] {
            if (m_hStdOutRead) CloseHandle(m_hStdOutRead);
            if (hStdOutWrite) CloseHandle(hStdOutWrite);
            if (hStdInRead) CloseHandle(hStdInRead);
            if (m_hStdInWrite) CloseHandle(m_hStdInWrite);
            m_hStdOutRead = nullptr;
            m_hStdInWrite = nullptr;
            return false;
        };
        if (!CreatePipe(&m_hStdOutRead, &hStdOutWrite, &sa, 0)) return fail();
        if (!SetHandleInformation(m_hStdOutRead, HANDLE_FLAG_INHERIT, 0)) return fail();

        if (!CreatePipe(&hStdInRead, &m_hStdInWrite, &sa, 0)) return fail();
        if (!SetHandleInformation(m_hStdInWrite, HANDLE_FLAG_INHERIT, 0)) return fail();

        STARTUPINFOW si = { sizeof(STARTUPINFOW) };
        si.cb = sizeof(STARTUPINFOW);
        si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si.hStdInput = hStdInRead;
        si.hStdOutput = hStdOutWrite;
        si.hStdError = hStdOutWrite; 
        si.wShowWindow = SW_HIDE;

        PROCESS_INFORMATION pi = { 0 };
        std::wstring commandLine = L"\"" + nodeExePath + L"\" \"" + scriptPath + L"\"";
        
        std::vector<wchar_t> cmdBuffer(commandLine.begin(), commandLine.end());
        cmdBuffer.push_back(L'\0');

        if (!CreateProcessW(nullptr, cmdBuffer.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi)) {
            return fail();
        }

        m_hProcess = pi.hProcess;
        CloseHandle(pi.hThread);
        CloseHandle(hStdInRead);
        CloseHandle(hStdOutWrite);

        m_running = true;
        m_readerThread = std::thread(&NodeJsBridge::ReaderThread, this);

        return true;
    }

    std::future<nlohmann::json> CallAsync(const std::string& method, const nlohmann::json& params) {
        int id;
        std::future<nlohmann::json> future;

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_running) {
                // 桥接已停止：直接返回一个带异常的 future，调用方不用再等
                std::promise<nlohmann::json> dead;
                dead.set_exception(std::make_exception_ptr(std::runtime_error("node host is not running")));
                return dead.get_future();
            }
            id = m_nextRequestId++;
            future = m_pendingRequests[id].get_future();
        }

        nlohmann::json request;
        request["jsonrpc"] = "2.0";
        request["id"] = id;
        request["method"] = method;
        request["params"] = params;

        if (!WriteLine(request.dump())) {
            // 写入失败不会有回复，立即让这个请求失败
            std::lock_guard<std::mutex> lock(m_mutex);
            const auto it = m_pendingRequests.find(id);
            if (it != m_pendingRequests.end()) {
                it->second.set_exception(std::make_exception_ptr(std::runtime_error("failed to write to node host")));
                m_pendingRequests.erase(it);
            }
        }
        return future;
    }

    // 不需要回复的通知（例如取消查询）
    void Notify(const std::string& method, const nlohmann::json& params) {
        nlohmann::json request;
        request["jsonrpc"] = "2.0";
        request["method"] = method;
        request["params"] = params;
        WriteLine(request.dump());
    }

    bool IsRunning() const {
        return m_running;
    }

    void Stop() {
        m_running = false;
        if (m_hProcess) {
            TerminateProcess(m_hProcess, 0);
            CloseHandle(m_hProcess);
            m_hProcess = nullptr;
        }
        {
            std::lock_guard<std::mutex> lock(m_writeMutex);
            if (m_hStdInWrite) {
                CloseHandle(m_hStdInWrite);
                m_hStdInWrite = nullptr;
            }
        }
        if (m_hStdOutRead) {
            // Close the read handle to unblock ReadFile if needed, 
            // but usually TerminateProcess or closing the other end does it.
            CloseHandle(m_hStdOutRead);
            m_hStdOutRead = nullptr;
        }
        if (m_readerThread.joinable()) {
            m_readerThread.join();
        }

        FailPendingRequests("node host stopped");
    }
};
