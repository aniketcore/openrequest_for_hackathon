#include "tab.h"
#include "sidebar.h"
#include <iostream>
#include <algorithm>
#include <sstream>
#include <fstream>
#include "httpengine.h"
#include <imgui_stdlib.h>
#include "imgui_helpers.h"
#include "TextEditor.h"
#include <stack>

namespace UI
{
    namespace
    {
        void ConfigureTextEditor(TextEditor& editor, bool readOnly, const TextEditor::Language* language = nullptr)
        {
            editor.SetTabSize(2);
            editor.SetInsertSpacesOnTabs(true);
            editor.SetAutoIndentEnabled(true);
            editor.SetReadOnlyEnabled(readOnly);
            editor.SetShowLineNumbersEnabled(true);
            editor.SetShowWhitespacesEnabled(false);
            editor.SetShowMatchingBrackets(true);
            editor.SetCompletePairedGlyphs(true);
            editor.SetShowPanScrollIndicatorEnabled(true);
            if (language != nullptr)
            {
                editor.SetLanguage(language);
            }
        }
    }

    // Helper function to initialize global tabs vector
    std::vector<std::unique_ptr<Tab>> InitializeTabs()
    {
        std::vector<std::unique_ptr<Tab>> tabs;
        tabs.push_back(std::make_unique<HttpTab>(1, "Tab 1"));
        return tabs;
    }

    std::vector<std::unique_ptr<Tab>> g_Tabs = InitializeTabs();
    int g_ActiveTabId = 1;
    int g_NextTabId = 4;
    int g_SelectedTopTab = 0;
    
    std::vector<SavedRequest> g_Collection;

    void SaveCollectionToFile(const std::string& path) {
        std::ofstream out(path);
        if (!out) return;
        out << g_Collection.size() << "\n";
        for (const auto& req : g_Collection) {
            out << req.serialized.size() << "\n" << req.serialized;
        }
    }

    void LoadCollectionFromFile(const std::string& path) {
        std::ifstream in(path);
        if (!in) return;
        g_Collection.clear();
        std::string line;
        if (std::getline(in, line)) {
            try {
                int count = std::stoi(line);
                for (int i=0; i<count; ++i) {
                    if (std::getline(in, line)) {
                        int size = std::stoi(line);
                        std::string serialized;
                        serialized.resize(size);
                        in.read(&serialized[0], size);
                        
                        // Extract name from first line of serialized string
                        std::string name = "Unnamed";
                        size_t pos = serialized.find('\n');
                        if (pos != std::string::npos) {
                            name = serialized.substr(0, pos);
                        }
                        g_Collection.push_back({name, serialized});
                    }
                }
            } catch (...) {}
        }
    }

    std::string HttpTab::SerializeToString(const std::string& name) {
        std::string export_str = name + "\n" + 
                                 std::to_string(static_cast<int>(this->method)) + "\n" +
                                 this->url + "\n" +
                                 std::to_string(this->requestHeaders.size()) + "\n";
        for(auto& h : this->requestHeaders) {
            export_str += h.first + "\n" + h.second + "\n";
        }
        std::string body = requestEditor ? requestEditor->GetText() : "";
        export_str += std::to_string(body.length()) + "\n" + body;
        return export_str;
    }

    void HttpTab::DeserializeFromString(const std::string& serialized) {
        std::istringstream iss(serialized);
        std::string line;
        std::getline(iss, line); // ignore name
        if (std::getline(iss, line)) {
            try { this->method = static_cast<HTTP::Method>(std::stoi(line)); } catch(...) {}
        }
        if (std::getline(iss, line)) {
            this->url = line;
        }
        if (std::getline(iss, line)) {
            try {
                int num_headers = std::stoi(line);
                this->requestHeaders.clear();
                for(int i=0; i<num_headers; ++i) {
                    std::string k, v;
                    std::getline(iss, k);
                    std::getline(iss, v);
                    this->requestHeaders.push_back({k, v});
                }
            } catch(...) {}
        }
        if (std::getline(iss, line)) {
            try {
                int body_len = std::stoi(line);
                std::string body;
                if (body_len > 0) {
                    body.resize(body_len);
                    iss.read(&body[0], body_len);
                }
                if (requestEditor) requestEditor->SetText(body);
            } catch(...) {}
        }
    }

    void HttpTab::Draw()
    {
        ImGui::PushID(this->id);
        static bool responseReadOnly = true;

        if (!requestEditor)
        {
            requestEditor = std::make_unique<TextEditor>();
            ConfigureTextEditor(*requestEditor, false, TextEditor::Language::Json());
        }
        if (!responseEditor)
        {
            responseEditor = std::make_unique<TextEditor>();
            ConfigureTextEditor(*responseEditor, true, TextEditor::Language::Json());
        }

        if (!requestEditorInitialized)
        {
            requestEditor->SetText(req.body);
            requestEditorInitialized = true;
        }

        if (req.gotresponse && req.getresponse().body.has_value())
        {
            const auto &body = req.getresponse().body.value();
            if (body != lastResponseText)
            {
                responseEditor->SetText(body);
                lastResponseText = body;
            }
        }

        // Clean layout with zero vertical item spacing between panels and splitter
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

        // Clamp requestHeight to keep response panel slightly visible (min 40px)
        float max_height = ImGui::GetContentRegionAvail().y - 40.0f;
        if (this->requestHeight < 0.0f) {
            this->requestHeight = ImGui::GetContentRegionAvail().y * 0.80f;
        }
        if (this->requestHeight > max_height) this->requestHeight = max_height;
        if (this->requestHeight < 40.0f) this->requestHeight = 40.0f;

        // 1. Top Panel: Request Editor
        ImGui::BeginChild("##request_panel", ImVec2(0.0f, this->requestHeight), false, ImGuiWindowFlags_NoScrollbar);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 8.0f));

        ImGui::Text("HTTP Request Settings");
        ImGui::SameLine();
        if (ImGui::Button("Copy to Clipboard")) {
            ImGui::SetClipboardText(this->SerializeToString("Clipboard Request").c_str());
        }
        ImGui::SameLine();
        if (ImGui::Button("Paste from Clipboard")) {
            const char* clip = ImGui::GetClipboardText();
            if (clip) {
                this->DeserializeFromString(clip);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Save to Collection")) {
            std::string name = "Saved Request " + std::to_string(g_Collection.size() + 1);
            g_Collection.push_back({name, this->SerializeToString(name)});
        }
        ImGui::Spacing();
        ImGui::InputText("#URL", &this->url, 0, nullptr, nullptr);

        const char *methods[] = {"GET", "POST", "PUT", "DELETE"};
        int current_method = static_cast<int>(this->method);
        if (ImGui::Combo("Method", &current_method, methods, IM_ARRAYSIZE(methods)))
        {
            this->method = static_cast<HTTP::Method>(current_method);
        }

        ImGui::Spacing();
        ImGui::Text("Headers");
        ImGui::SameLine();
        if (ImGui::Button("+##add_header")) {
            requestHeaders.push_back({"", ""});
        }
        for (size_t i = 0; i < requestHeaders.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            ImGui::SetNextItemWidth(150.0f);
            ImGui::InputText("##key", &requestHeaders[i].first);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(250.0f);
            ImGui::InputText("##value", &requestHeaders[i].second);
            ImGui::SameLine();
            if (ImGui::Button("X")) {
                requestHeaders.erase(requestHeaders.begin() + i);
                --i;
            }
            ImGui::PopID();
        }
        ImGui::Spacing();
        ImGui::Text("Request Body");
        ImGui::Separator();

        ImFont* editorFont = UI::Vulkan::g_MonoFont ? UI::Vulkan::g_MonoFont : ImGui::GetFont();
        ImGui::PushFont(editorFont);
        requestEditor->Render("##request_body_editor", ImVec2(-1.0f, 160.0f), true);
        ImGui::PopFont();

        req.body = requestEditor->GetText();
        ImGui::Spacing();

        if (ImGui::Button("Send Request"))
        {
            req.url = this->url;
            req.method = this->method;
            req.gotresponse = false;
            req.body = requestEditor->GetText();

            req.headers.clear();
            for (const auto& h : requestHeaders) {
                if (!h.first.empty()) {
                    req.headers[h.first] = h.second;
                }
            }

            eng.dispatchrequest(&req);
            std::cout << "button pressed" << std::endl;
        }
        ImGui::PopStyleVar();
        ImGui::EndChild();

        // 2. Splitter Bar
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.22f, 0.27f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.35f, 0.40f, 0.50f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.32f, 0.40f, 1.0f));

        ImGui::Button("##hsplitter", ImVec2(-1.0f, 8.0f));

        // Removed custom cursor changes so it relies purely on the OS default cursor.
        if (ImGui::IsItemActive())
        {
            this->requestHeight += ImGui::GetIO().MouseDelta.y;
        }

        ImGui::PopStyleColor(3);

        ImGui::Dummy(ImVec2(0.0f, 8.0f));

        // 3. Bottom Panel: Response Area
        ImGui::BeginChild("##response_panel", ImVec2(0.0f, 0.0f), false);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 8.0f));

        ImGui::Text("Response");
        ImGui::Separator();
        ImGui::Spacing();

        if (req.gotresponse)
        {
            auto &resp = req.getresponse();

            ImGui::BeginGroup();
            if (ImGui::Button("Copy"))
            {
                responseEditor->Copy();
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear"))
            {
                responseEditor->ClearText();
                if (resp.body.has_value()) resp.body.reset();
                lastResponseText.clear();
            }
            ImGui::SameLine();
            ImGui::Checkbox("Read-only", &responseReadOnly);
            ImGui::EndGroup();

            ImGui::Spacing();

            if (resp.body.has_value())
            {
                responseEditor->SetReadOnlyEnabled(responseReadOnly);
                ImFont* editorFont = UI::Vulkan::g_MonoFont ? UI::Vulkan::g_MonoFont : ImGui::GetFont();
                ImGui::PushFont(editorFont);
                responseEditor->Render("##response_editor", ImGui::GetContentRegionAvail(), true);
                ImGui::PopFont();
            }
            else
            {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Response body is empty.");
            }
        }
        else
        {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No response yet. Send a request to see output.");
        }

        ImGui::PopStyleVar();
        ImGui::EndChild();

        ImGui::PopStyleVar();
        ImGui::PopID();
    }

    void WsTab::Draw()
    {
        ImGui::Text("WebSocket Connection Settings");
        ImGui::Spacing();

        ImGui::InputText("Address", &this->address);

        ImGui::Spacing();
        if (ImGui::Button("Connect"))
        {
            // Dummy action
        }
    }

    void DrawTabContent()
    {
        ImGuiViewport *viewport = ImGui::GetMainViewport();

        // Position the window directly to the right of the sidebar and let it occupy the space
        float offset = g_SidebarCollapsed ? 0.0f : g_SidebarWidth + 4.0f;
        ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x + offset, viewport->Pos.y));
        ImGui::SetNextWindowSize(ImVec2(viewport->Size.x - offset, viewport->Size.y));

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar;

        // Use clean padding
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 16.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

        if (ImGui::Begin("Tab Content Window", nullptr, flags))
        {
            if (g_SidebarCollapsed)
            {
                if (ImGui::Button(">>", ImVec2(32, 24)))
                {
                    g_SidebarCollapsed = false;
                    g_SidebarRestoreRequested = true;
                }
                ImGui::SameLine();
            }

            Tab *active_tab = nullptr;
            for (auto &tab : g_Tabs)
            {
                if (tab->id == g_ActiveTabId)
                {
                    active_tab = tab.get();
                    break;
                }
            }

            if (active_tab)
            {
                ImGui::Text("Active Tab: %s", active_tab->name.c_str());
                ImGui::Separator();
                ImGui::Spacing();

                active_tab->Draw();
            }
            else
            {
                ImGui::Text("No active tab.");
                ImGui::Text("Click the '+' button in the sidebar to create an HTTP or WebSocket tab.");
            }
        }
        ImGui::End();
        ImGui::PopStyleVar(2);
    }
} // namespace UI
