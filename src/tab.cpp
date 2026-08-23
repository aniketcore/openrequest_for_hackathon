#include "tab.h"
#include "sidebar.h"
#include <iostream>
#include <algorithm>
#include "httpengine.h"
#include <imgui_stdlib.h>
#include <stack>

namespace UI
{
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

    void HttpTab::Draw()
    {
        static HTTP::Engine eng;
        static HTTP::Request req;

        // Clean layout with zero vertical item spacing between panels and splitter
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

        // 1. Top Panel: Request Editor
        ImGui::BeginChild("##request_panel", ImVec2(0.0f, this->requestHeight), false, ImGuiWindowFlags_NoScrollbar);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 8.0f)); // restore standard padding inside child
        
        ImGui::Text("HTTP Request Settings");
        ImGui::Spacing();
        ImGui::InputText("#URL", &this->url, 0, nullptr, nullptr);

        const char *methods[] = {"GET", "POST", "PUT", "DELETE"};
        int current_method = static_cast<int>(this->method);
        if (ImGui::Combo("Method", &current_method, methods, IM_ARRAYSIZE(methods)))
        {
            this->method = static_cast<HTTP::Method>(current_method);
        }

        ImGui::Spacing();
        if (ImGui::Button("Send Request"))
        {
            req.url = this->url;
            req.body = "Method: ";
            req.body += methods[this->method];
            req.method = this->method;
            req.gotresponse = false;

            eng.dispatchrequest(&req);
            std::cout << "button pressed" << std::endl;
        }
        ImGui::PopStyleVar();
        ImGui::EndChild();

        // 2. Splitter Bar
        // Render a thin, subtle button line that looks like a divider
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.22f, 0.27f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.35f, 0.40f, 0.50f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.32f, 0.40f, 1.0f));
        
        ImGui::Button("##hsplitter", ImVec2(-1.0f, 4.0f));
        
        if (ImGui::IsItemHovered() || ImGui::IsItemActive())
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        }
        if (ImGui::IsItemActive())
        {
            this->requestHeight += ImGui::GetIO().MouseDelta.y;
            // Clamp height limits
            if (this->requestHeight < 80.0f) this->requestHeight = 80.0f;
            float max_height = ImGui::GetContentRegionAvail().y - 80.0f;
            if (this->requestHeight > max_height) this->requestHeight = max_height;
        }
        
        ImGui::PopStyleColor(3);

        // Spacer between splitter and bottom panel
        ImGui::Dummy(ImVec2(0.0f, 8.0f));

        // 3. Bottom Panel: Response Area
        ImGui::BeginChild("##response_panel", ImVec2(0.0f, 0.0f), false);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 8.0f));

        ImGui::Text("Response");
        ImGui::Separator();
        ImGui::Spacing();

        if (req.gotresponse)
        {
            if (req.getresponse().body.has_value())
            {
                static ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput;
                ImGui::InputTextMultiline(
                    "##source",
                    &req.getresponse().body.value(),
                    ImVec2(-FLT_MIN, -FLT_MIN),
                    flags,
                    nullptr,
                    nullptr);
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

        ImGui::PopStyleVar(); // Pop outer ItemSpacing style
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
