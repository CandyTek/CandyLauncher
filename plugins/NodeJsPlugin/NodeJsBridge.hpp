#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
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
    std::unordered_map<int, std::promise<nlohmann::json>> m_pendingRequests;
    int m_nextRequestId = 1;
    bool m_running = false;

    typedef std::function<void(const std::string& method, const nlohmann::json& params)> NotificationHandler;
    NotificationHandler m_notificationHandler;

    void ReaderThread() {
        char buffer[8192];
        std::string accumulated;
        DWORD bytesRead;

        while (m_running) {
            if (!ReadFile(m_hStdOutRead, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) || bytesRead == 0) {
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
                    ConsolePrintln(L"NodeJsBridge", L"[JS] " + utf8_to_wide(line));
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
                    ConsolePrintln(L"NodeJsBridge", L"[JS Log/Error] " + utf8_to_wide(line));
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

        HANDLE hStdInRead, hStdOutWrite;
        if (!CreatePipe(&m_hStdOutRead, &hStdOutWrite, &sa, 0)) return false;
        if (!SetHandleInformation(m_hStdOutRead, HANDLE_FLAG_INHERIT, 0)) return false;

        if (!CreatePipe(&hStdInRead, &m_hStdInWrite, &sa, 0)) return false;
        if (!SetHandleInformation(m_hStdInWrite, HANDLE_FLAG_INHERIT, 0)) return false;

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
            CloseHandle(m_hStdOutRead);
            CloseHandle(hStdOutWrite);
            CloseHandle(hStdInRead);
            CloseHandle(m_hStdInWrite);
            return false;
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
            id = m_nextRequestId++;
            future = m_pendingRequests[id].get_future();
        }

        nlohmann::json request;
        request["jsonrpc"] = "2.0";
        request["id"] = id;
        request["method"] = method;
        request["params"] = params;

        std::string line = request.dump() + "\n";
        DWORD bytesWritten;
        WriteFile(m_hStdInWrite, line.c_str(), (DWORD)line.size(), &bytesWritten, nullptr);

        return future;
    }

    void Stop() {
        m_running = false;
        if (m_hProcess) {
            TerminateProcess(m_hProcess, 0);
            CloseHandle(m_hProcess);
            m_hProcess = nullptr;
        }
        if (m_hStdInWrite) {
            CloseHandle(m_hStdInWrite);
            m_hStdInWrite = nullptr;
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

        std::lock_guard<std::mutex> lock(m_mutex);
        m_pendingRequests.clear();
    }
};
