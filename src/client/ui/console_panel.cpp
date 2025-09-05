#include "console_panel.h"
#include <algorithm>
#include <cstdarg>
#include <cstring>
#include <spdlog/spdlog.h>

ConsolePanel::ConsolePanel() {
    clearLog();
    addLogInfo("Console ready - type 'help' for commands");
}

void ConsolePanel::render() {
    if (!is_visible_) return;
    
    ImGui::SetNextWindowSize(ImVec2(800, 400), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Console", &is_visible_)) {
        ImGui::End();
        return;
    }
    
    // Options menu
    if (ImGui::BeginPopup("Options")) {
        ImGui::Checkbox("Auto-scroll", &auto_scroll_);
        ImGui::Separator();
        ImGui::Text("Log Level Filters:");
        ImGui::Checkbox("Debug", &show_debug_);
        ImGui::Checkbox("Info", &show_info_);
        ImGui::Checkbox("Warning", &show_warning_);
        ImGui::Checkbox("Error", &show_error_);
        ImGui::EndPopup();
    }
    
    // Main menu bar
    if (ImGui::Button("Options"))
        ImGui::OpenPopup("Options");
    ImGui::SameLine();
    
    bool clear = ImGui::Button("Clear");
    ImGui::SameLine();
    
    bool copy = ImGui::Button("Copy");
    ImGui::SameLine();
    
    ImGui::Text("Filter:");
    ImGui::SameLine();
    
    static ImGuiTextFilter filter;
    filter.Draw("##Filter", 200);
    
    ImGui::Separator();
    
    // Reserve enough left-over height for 1 separator + 1 input text
    const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    if (ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar)) {
        if (clear)
            clearLog();
        
        if (copy)
            ImGui::LogToClipboard();
        
        // Display log items
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));
        for (const auto& item : log_items_) {
            if (!filter.PassFilter(item.text.c_str()))
                continue;
            
            // Filter by log level
            bool show_item = false;
            switch (item.level) {
                case LogLevel::Debug:
                    show_item = show_debug_;
                    break;
                case LogLevel::Info:
                    show_item = show_info_;
                    break;
                case LogLevel::Warning:
                    show_item = show_warning_;
                    break;
                case LogLevel::Error:
                case LogLevel::Critical:
                    show_item = show_error_;
                    break;
            }
            
            if (!show_item)
                continue;
            
            ImGui::PushStyleColor(ImGuiCol_Text, item.color);
            ImGui::TextUnformatted(item.text.c_str());
            ImGui::PopStyleColor();
        }
        
        if (scroll_to_bottom_ || (auto_scroll_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
            ImGui::SetScrollHereY(1.0f);
        scroll_to_bottom_ = false;
        
        ImGui::PopStyleVar();
    }
    ImGui::EndChild();
    
    ImGui::Separator();
    
    // Command line
    bool reclaim_focus = false;
    ImGuiInputTextFlags input_text_flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_EscapeClearsAll | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory;
    
    ImGui::PushItemWidth(-ImGui::GetStyle().ItemSpacing.x * 7);
    if (ImGui::InputText("##Input", input_buffer_, IM_ARRAYSIZE(input_buffer_), input_text_flags, &ConsolePanel::textEditCallbackStub, (void*)this)) {
        char* s = input_buffer_;
        
        // Trim leading/trailing spaces
        while (*s == ' ') s++;
        char* s_end = s + strlen(s) - 1;
        while (s_end > s && *s_end == ' ') s_end--;
        s_end[1] = 0;
        
        if (s[0])
            executeCommand(s);
        strcpy(input_buffer_, "");
        reclaim_focus = true;
    }
    ImGui::PopItemWidth();
    
    // Auto-focus on window apparition
    ImGui::SetItemDefaultFocus();
    if (reclaim_focus)
        ImGui::SetKeyboardFocusHere(-1);
    
    ImGui::End();
}

void ConsolePanel::addLog(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    char buffer[2048];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    buffer[sizeof(buffer) - 1] = 0;
    
    log_items_.emplace_back(buffer, color_info_, LogLevel::Info);
    va_end(args);
}

void ConsolePanel::addLogInfo(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    char buffer[2048];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    buffer[sizeof(buffer) - 1] = 0;
    
    log_items_.emplace_back(std::string("[INFO] ") + buffer, color_info_, LogLevel::Info);
    va_end(args);
}

void ConsolePanel::addLogWarning(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    char buffer[2048];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    buffer[sizeof(buffer) - 1] = 0;
    
    log_items_.emplace_back(std::string("[WARN] ") + buffer, color_warning_, LogLevel::Warning);
    va_end(args);
}

void ConsolePanel::addLogError(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    char buffer[2048];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    buffer[sizeof(buffer) - 1] = 0;
    
    log_items_.emplace_back(std::string("[ERROR] ") + buffer, color_error_, LogLevel::Error);
    va_end(args);
}

void ConsolePanel::clearLog() {
    log_items_.clear();
}

void ConsolePanel::addLogInfoThreadSafe(const std::string& message) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    log_items_.emplace_back(std::string("[INFO] ") + message, color_info_, LogLevel::Info);
}

void ConsolePanel::addLogWarningThreadSafe(const std::string& message) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    log_items_.emplace_back(std::string("[WARN] ") + message, color_warning_, LogLevel::Warning);
}

void ConsolePanel::addLogDebugThreadSafe(const std::string& message) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    log_items_.emplace_back(std::string("[DEBUG] ") + message, IM_COL32(150, 150, 150, 255), LogLevel::Debug);
}

void ConsolePanel::addLogErrorThreadSafe(const std::string& message) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    log_items_.emplace_back(std::string("[ERROR] ") + message, color_error_, LogLevel::Error);
}

void ConsolePanel::executeCommand(const char* command_line) {
    addLog("> %s", command_line);
    
    // Insert into history
    history_pos_ = -1;
    for (int i = command_history_.size() - 1; i >= 0; i--) {
        if (command_history_[i] == command_line) {
            command_history_.erase(command_history_.begin() + i);
            break;
        }
    }
    command_history_.push_back(command_line);
    
    // Process command
    std::string cmd(command_line);
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);
    
    if (cmd == "clear") {
        clearLog();
    }
    else if (cmd == "help") {
        addLogInfo("Commands: clear | history | status | version | test");
    }
    else if (cmd == "history") {
        int first = command_history_.size() - 10;
        for (int i = first > 0 ? first : 0; i < command_history_.size(); i++) {
            addLog("  %3d: %s", i, command_history_[i].c_str());
        }
    }
    else if (cmd == "status") {
        addLogInfo("Console active - %zu logs, %zu history", log_items_.size(), command_history_.size());
    }
    else if (cmd == "version") {
        addLogInfo("OcctImgui v1.0 (ImGui %s)", IMGUI_VERSION);
    }
    else if (cmd == "test") {
        addLogInfo("Test info message");
        addLogWarning("Test warning message");
        addLogError("Test error message");
    }
    else {
        addLogError("Unknown command: '%s' (try 'help')", command_line);
    }
    
    // Scroll to bottom after command execution
    scroll_to_bottom_ = true;
}

int ConsolePanel::textEditCallback(ImGuiInputTextCallbackData* data) {
    switch (data->EventFlag) {
    case ImGuiInputTextFlags_CallbackCompletion:
        {
            // Example of TEXT COMPLETION
            const char* word_end = data->Buf + data->CursorPos;
            const char* word_start = word_end;
            while (word_start > data->Buf) {
                const char c = word_start[-1];
                if (c == ' ' || c == '\t' || c == ',' || c == ';')
                    break;
                word_start--;
            }
            
            // Build a list of candidates
            std::vector<std::string> candidates;
            static const char* commands[] = { "clear", "help", "history", "status", "version", "test" };
            for (int i = 0; i < IM_ARRAYSIZE(commands); i++) {
                if (strncmp(commands[i], word_start, (int)(word_end - word_start)) == 0)
                    candidates.push_back(commands[i]);
            }
            
            if (candidates.empty()) {
                addLogError("No completion available for \"%.*s\"", (int)(word_end - word_start), word_start);
            }
            else if (candidates.size() == 1) {
                // Single match: delete the beginning of the word and replace it entirely
                data->DeleteChars((int)(word_start - data->Buf), (int)(word_end - word_start));
                data->InsertChars(data->CursorPos, candidates[0].c_str());
                data->InsertChars(data->CursorPos, " ");
            }
            else {
                // Multiple matches: complete as much as we can, list all matches
                int match_len = (int)(word_end - word_start);
                for (;;) {
                    int c = 0;
                    bool all_candidates_matches = true;
                    for (int i = 0; i < candidates.size() && all_candidates_matches; i++)
                        if (i == 0)
                            c = toupper(candidates[i][match_len]);
                        else if (c == 0 || c != toupper(candidates[i][match_len]))
                            all_candidates_matches = false;
                    if (!all_candidates_matches)
                        break;
                    match_len++;
                }
                
                if (match_len > 0) {
                    data->DeleteChars((int)(word_start - data->Buf), (int)(word_end - word_start));
                    data->InsertChars(data->CursorPos, candidates[0].c_str(), candidates[0].c_str() + match_len);
                }
                
                addLogInfo("Possible matches:");
                for (const auto& candidate : candidates)
                    addLogInfo("  %s", candidate.c_str());
            }
            
            break;
        }
    case ImGuiInputTextFlags_CallbackHistory:
        {
            const int prev_history_pos = history_pos_;
            if (data->EventKey == ImGuiKey_UpArrow) {
                if (history_pos_ == -1)
                    history_pos_ = command_history_.size() - 1;
                else if (history_pos_ > 0)
                    history_pos_--;
            }
            else if (data->EventKey == ImGuiKey_DownArrow) {
                if (history_pos_ != -1) {
                    if (++history_pos_ >= command_history_.size())
                        history_pos_ = -1;
                }
            }
            
            if (prev_history_pos != history_pos_) {
                const char* history_str = (history_pos_ >= 0) ? command_history_[history_pos_].c_str() : "";
                data->DeleteChars(0, data->BufTextLen);
                data->InsertChars(0, history_str);
            }
        }
    }
    return 0;
}

int ConsolePanel::textEditCallbackStub(ImGuiInputTextCallbackData* data) {
    ConsolePanel* console = (ConsolePanel*)data->UserData;
    return console->textEditCallback(data);
}