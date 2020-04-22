/*
 * Configurator.cpp
 *
 * Copyright (C) 2019 by Universitaet Stuttgart (VIS).
 * Alle Rechte vorbehalten.
 */

/**
 * USED HOTKEYS:
 *
 * - Search module:        Ctrl + Shift + m
 * - Search parameter:     Ctrl + Shift + p
 * - Save active project:  Ctrl + Shift + s
 * - Delete graph item:    Delete
 */

#include "stdafx.h"
#include "Configurator.h"


using namespace megamol;
using namespace megamol::gui;
using namespace megamol::gui::configurator;


std::vector<std::string> megamol::gui::configurator::Configurator::dropped_files;


megamol::gui::configurator::Configurator::Configurator()
    : param_slots()
    , state_param("configurator::state", "State of the configurator.")
    , graph_manager()
    , file_utils()
    , utils()
    , init_state(0)
    , left_child_width(250.0f)
    , selected_list_module_uid(GUI_INVALID_ID)
    , add_project_graph_uid(GUI_INVALID_ID)
    , show_module_list_sidebar(false)
    , show_module_list_child(false)
    , module_list_popup_pos()
    , last_selected_callslot_uid(GUI_INVALID_ID)
    , graph_state() {

    this->state_param << new core::param::StringParam("");
    this->state_param.Parameter()->SetGUIVisible(false);
    this->state_param.Parameter()->SetGUIReadOnly(true);

    this->param_slots.clear();
    this->param_slots.push_back(&this->state_param);

    this->graph_state.hotkeys[megamol::gui::HotkeyIndex::MODULE_SEARCH] = megamol::gui::HotkeyDataType(
        core::view::KeyCode(core::view::Key::KEY_M, (core::view::Modifier::CTRL | core::view::Modifier::SHIFT)), false);
    this->graph_state.hotkeys[megamol::gui::HotkeyIndex::PARAMETER_SEARCH] = megamol::gui::HotkeyDataType(
        core::view::KeyCode(core::view::Key::KEY_P, (core::view::Modifier::CTRL | core::view::Modifier::SHIFT)), false);
    this->graph_state.hotkeys[megamol::gui::HotkeyIndex::DELETE_GRAPH_ITEM] =
        megamol::gui::HotkeyDataType(core::view::KeyCode(core::view::Key::KEY_DELETE), false);
    this->graph_state.hotkeys[megamol::gui::HotkeyIndex::SAVE_PROJECT] = megamol::gui::HotkeyDataType(
        megamol::core::view::KeyCode(core::view::Key::KEY_S, core::view::Modifier::CTRL | core::view::Modifier::SHIFT),
        false);
    this->graph_state.font_scalings = {0.85f, 0.95f, 1.0f, 1.5f, 2.5f};
    this->graph_state.child_width = 0.0f;
    this->graph_state.show_parameter_sidebar = false;
    this->graph_state.graph_selected_uid = GUI_INVALID_ID;
    this->graph_state.graph_delete = false;
}


Configurator::~Configurator() {}


bool megamol::gui::configurator::Configurator::CheckHotkeys(void) {

    if (ImGui::GetCurrentContext() == nullptr) {
        vislib::sys::Log::DefaultLog.WriteError(
            "No ImGui context available. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
        return false;
    }
    ImGuiIO& io = ImGui::GetIO();

    bool hotkey_pressed = false;
    for (auto& h : this->graph_state.hotkeys) {
        auto key = std::get<0>(h).key;
        auto mods = std::get<0>(h).mods;
        if (ImGui::IsKeyDown(static_cast<int>(key)) && (mods.test(core::view::Modifier::CTRL) == io.KeyCtrl) &&
            (mods.test(core::view::Modifier::ALT) == io.KeyAlt) &&
            (mods.test(core::view::Modifier::SHIFT) == io.KeyShift)) {
            std::get<1>(h) = true;
            hotkey_pressed = true;
        }
    }

    return hotkey_pressed;
}


bool megamol::gui::configurator::Configurator::Draw(
    WindowManager::WindowConfiguration& wc, megamol::core::CoreInstance* core_instance) {

    if (core_instance == nullptr) {
        vislib::sys::Log::DefaultLog.WriteError(
            "Pointer to Core Instance is nullptr. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
        return false;
    }
    if (ImGui::GetCurrentContext() == nullptr) {
        vislib::sys::Log::DefaultLog.WriteError(
            "No ImGui context available. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
        return false;
    }

    // Draw
    if (this->init_state < 2) {
        /// Step 1] (two frames!)

        // Show pop-up before calling UpdateAvailableModulesCallsOnce of graph.
        /// Rendering of pop-up requires two complete Draw calls!
        bool open = true;
        std::string popup_label = "Loading";
        if (this->init_state == 0) {
            ImGui::OpenPopup(popup_label.c_str());
        }
        ImGuiWindowFlags popup_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove;
        if (ImGui::BeginPopupModal(popup_label.c_str(), &open, popup_flags)) {
            ImGui::TextUnformatted("Please wait...\nLoading available modules and calls for configurator.");
            ImGui::EndPopup();
        }

        this->init_state++;

    } else if (this->init_state == 2) {
        /// Step 2] (one frame)

        // Load available modules and calls and currently loaded project from core once(!)
        this->graph_manager.UpdateModulesCallsStock(core_instance);

        // Load inital project
        /// this->graph_manager.LoadProjectCore(core_instance);
        /// or: this->add_empty_project();

        // Enable drag and drop of files for configurator (if glfw is available here)
#ifdef GUI_USE_GLFW
        auto glfw_win = ::glfwGetCurrentContext();
        ::glfwSetDropCallback(glfw_win, this->file_drop_callback);
#endif

        this->init_state++;
    } else {
        /// Step 3]
        // Render configurator gui content

        // Check for parameter changes
        if (this->state_param.IsDirty()) {
            std::string state = std::string(this->state_param.Param<core::param::StringParam>()->Value().PeekBuffer());
            this->configurator_state_from_json_string(state);
            this->state_param.ResetDirty();
        }

        // Child Windows
        this->draw_window_menu(core_instance);
        this->graph_state.child_width = 0.0f;
        if (this->show_module_list_sidebar) {
            this->utils.VerticalSplitter(
                GUIUtils::FixedSplitterSide::LEFT, this->left_child_width, this->graph_state.child_width);
            this->draw_window_module_list(this->left_child_width);
            ImGui::SameLine();
        }
        this->graph_manager.GUI_Present(this->graph_state);

        // Module Stock List in separate child window
        GraphPtrType selected_graph_ptr;
        if (this->graph_manager.GetGraph(this->graph_state.graph_selected_uid, selected_graph_ptr)) {
            ImGuiID selected_callslot_uid = selected_graph_ptr->GUI_GetSelectedCallSlot();
            bool double_click_anywhere = (ImGui::IsMouseDoubleClicked(0) && !this->show_module_list_child &&
                                          selected_graph_ptr->GUI_GetCanvasHoverd());
            bool double_click_callslot =
                (ImGui::IsMouseDoubleClicked(0) && (selected_callslot_uid != GUI_INVALID_ID) &&
                    ((!this->show_module_list_child) || (this->last_selected_callslot_uid != selected_callslot_uid)));
            if (double_click_anywhere || double_click_callslot) {
                std::get<1>(this->graph_state.hotkeys[megamol::gui::HotkeyIndex::MODULE_SEARCH]) = true;
                this->last_selected_callslot_uid = selected_callslot_uid;
            }
        }
        if (std::get<1>(this->graph_state.hotkeys[megamol::gui::HotkeyIndex::MODULE_SEARCH])) {
            this->show_module_list_child = true;
            this->module_list_popup_pos = ImGui::GetMousePos();
            ImGui::SetNextWindowPos(this->module_list_popup_pos);
        }
        if (this->show_module_list_child) {
            ImGuiStyle& style = ImGui::GetStyle();
            ImVec4 tmpcol = style.Colors[ImGuiCol_ChildBg];
            tmpcol = ImVec4(tmpcol.x * tmpcol.w, tmpcol.y * tmpcol.w, tmpcol.z * tmpcol.w, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, tmpcol);
            ImGui::SetCursorScreenPos(this->module_list_popup_pos);
            float child_width = 250.0f;
            float child_height = std::min(350.0f, (ImGui::GetContentRegionAvail().y - ImGui::GetWindowPos().y));
            auto child_flags = ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NavFlattened;
            ImGui::BeginChild("module_list_child", ImVec2(child_width, child_height), true, child_flags);
            if (ImGui::Button("Close") || ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape))) {
                this->show_module_list_child = false;
            }
            ImGui::Separator();
            this->draw_window_module_list(0.0f);
            ImGui::EndChild();
            ImGui::PopStyleColor();
        }
    }

    // Reset hotkeys
    for (auto& h : this->graph_state.hotkeys) {
        std::get<1>(h) = false;
    }

    return true;
}


void megamol::gui::configurator::Configurator::UpdateStateParameter(void) {

    // Save current state of configurator to state parameter
    nlohmann::json configurator_json;
    if (this->configurator_state_to_json(configurator_json)) {
        std::string state;
        state = configurator_json.dump(2); /// pass nothing for unformatted output or pass number of indent spaces
        this->state_param.Param<core::param::StringParam>()->SetValue(state.c_str(), false);
    }
}


void megamol::gui::configurator::Configurator::draw_window_menu(megamol::core::CoreInstance* core_instance) {

    if (core_instance == nullptr) {
        vislib::sys::Log::DefaultLog.WriteError(
            "Pointer to Core Instance is nullptr. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
        return;
    }

    bool confirmed, aborted;
    bool popup_save_project_file = false;
    bool popup_load_file = false;

    // Hotkeys
    if (std::get<1>(this->graph_state.hotkeys[megamol::gui::HotkeyIndex::SAVE_PROJECT])) {
        popup_save_project_file = true;
    }

    // Clear dropped file list, when configurator window is opened, after it was closed.
    if (ImGui::IsWindowAppearing()) {
        megamol::gui::configurator::Configurator::dropped_files.clear();
    }
    // Process dropped files ...
    if (!megamol::gui::configurator::Configurator::dropped_files.empty()) {
        // ... only if configurator is focused.
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
            for (auto& dropped_file : megamol::gui::configurator::Configurator::dropped_files) {
                this->graph_manager.LoadAddProjectFile(this->graph_state.graph_selected_uid, dropped_file);
            }
        }
        megamol::gui::configurator::Configurator::dropped_files.clear();
    }

    // Menu
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::BeginMenu("Load Project")) {
                if (ImGui::MenuItem("New", nullptr)) {
                    this->add_empty_project();
                }
                // Load project from LUA file
                if (ImGui::MenuItem("File", nullptr)) {
                    this->add_project_graph_uid = GUI_INVALID_ID;
                    popup_load_file = true;
                }
                if (ImGui::MenuItem("Running")) {
                    this->graph_manager.LoadProjectCore(core_instance);
                    // this->GetCoreInstance()->LoadProject(vislib::StringA(projectFilename.c_str()));
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Add Project")) {
                // Add project from LUA file to current project
                if (ImGui::MenuItem("File", nullptr, false, (this->graph_state.graph_selected_uid != GUI_INVALID_ID))) {
                    this->add_project_graph_uid = this->graph_state.graph_selected_uid;
                    popup_load_file = true;
                }
                if (ImGui::MenuItem(
                        "Running", nullptr, false, (this->graph_state.graph_selected_uid != GUI_INVALID_ID))) {
                    this->graph_manager.AddProjectCore(this->graph_state.graph_selected_uid, core_instance);
                    // this->GetCoreInstance()->LoadProject(vislib::StringA(projectFilename.c_str()));
                }
                ImGui::EndMenu();
            }

            // Save currently active project to LUA file
            if (ImGui::MenuItem("Save Project",
                    std::get<0>(this->graph_state.hotkeys[megamol::gui::HotkeyIndex::SAVE_PROJECT]).ToString().c_str(),
                    false, (this->graph_state.graph_selected_uid != GUI_INVALID_ID))) {
                popup_save_project_file = true;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Modules Sidebar", nullptr, this->show_module_list_sidebar)) {
                this->show_module_list_sidebar = !this->show_module_list_sidebar;
                this->show_module_list_child = false;
            }
            if (ImGui::MenuItem("Parameter Sidebar", nullptr, this->graph_state.show_parameter_sidebar)) {
                this->graph_state.show_parameter_sidebar = !this->graph_state.show_parameter_sidebar;
            }
            ImGui::EndMenu();
        }

        ImGui::SameLine();

        if (ImGui::BeginMenu("Help")) {
            const std::string docu_link =
                "https://github.com/UniStuttgart-VISUS/megamol/tree/master/plugins/gui#configurator";
            if (ImGui::Button("Readme on GitHub (Copy Link)")) {
#ifdef GUI_USE_GLFW
                auto glfw_win = ::glfwGetCurrentContext();
                ::glfwSetClipboardString(glfw_win, docu_link.c_str());
#elif _WIN32
                ImGui::SetClipboardText(docu_link.c_str());
#else // LINUX
                vislib::sys::Log::DefaultLog.WriteWarn(
                    "No clipboard use provided. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
                vislib::sys::Log::DefaultLog.WriteInfo("[Configurator] Readme Link:\n%s", docu_link.c_str());
#endif
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    // Pop-ups-----------------------------------
    bool popup_failed = false;
    std::string project_filename;
    GraphPtrType graph_ptr;
    if (this->graph_manager.GetGraph(add_project_graph_uid, graph_ptr)) {
        project_filename = graph_ptr->GetFilename();
    }
    if (this->file_utils.FileBrowserPopUp(
            FileUtils::FileBrowserFlag::LOAD, "Load Project", popup_load_file, project_filename)) {
        popup_failed = !this->graph_manager.LoadAddProjectFile(add_project_graph_uid, project_filename);
        this->add_project_graph_uid = GUI_INVALID_ID;
    }
    this->utils.MinimalPopUp("Failed to Load Project", popup_failed, "See console log output for more information.", "",
        confirmed, "Cancel", aborted);

    popup_failed = false;
    project_filename.clear();
    graph_ptr.reset();
    if (this->graph_manager.GetGraph(this->graph_state.graph_selected_uid, graph_ptr)) {
        project_filename = graph_ptr->GetFilename();
    }
    if (this->file_utils.FileBrowserPopUp(
            FileUtils::FileBrowserFlag::SAVE, "Save Project", popup_save_project_file, project_filename)) {
        popup_failed = !this->graph_manager.SaveProjectFile(this->graph_state.graph_selected_uid, project_filename);
    }
    this->utils.MinimalPopUp("Failed to Save Project", popup_failed, "See console log output for more information.", "",
        confirmed, "Cancel", aborted);
}


void megamol::gui::configurator::Configurator::draw_window_module_list(float width) {

    ImGui::BeginGroup();

    const float search_child_height = ImGui::GetFrameHeightWithSpacing() * 2.25f;
    auto child_flags =
        ImGuiWindowFlags_AlwaysUseWindowPadding | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NavFlattened;
    ImGui::BeginChild("module_search_child_window", ImVec2(width, search_child_height), false, child_flags);

    ImGui::TextUnformatted("Available Modules");
    ImGui::Separator();

    if (std::get<1>(this->graph_state.hotkeys[megamol::gui::HotkeyIndex::MODULE_SEARCH])) {
        this->utils.SetSearchFocus(true);
    }
    std::string help_text =
        "[" + std::get<0>(this->graph_state.hotkeys[megamol::gui::HotkeyIndex::MODULE_SEARCH]).ToString() +
        "] Set keyboard focus to search input field.\n"
        "Case insensitive substring search in module names.";
    this->utils.StringSearch("configurator_module_search", help_text);
    auto search_string = this->utils.GetSearchString();

    ImGui::EndChild();

    child_flags = ImGuiWindowFlags_NavFlattened;
    ImGui::BeginChild("module_list_child_window", ImVec2(width, 0.0f), true, child_flags);

    bool search_filter = true;
    bool compat_filter = true;

    std::string compat_callslot_name;
    CallSlotPtrType selected_callslot_ptr;
    GraphPtrType graph_ptr;
    if (this->graph_manager.GetGraph(this->graph_state.graph_selected_uid, graph_ptr)) {
        auto callslot_id = graph_ptr->GUI_GetSelectedCallSlot();
        if (callslot_id != GUI_INVALID_ID) {
            for (auto& module_ptr : graph_ptr->GetModules()) {
                CallSlotPtrType callslot_ptr;
                if (module_ptr->GetCallSlot(callslot_id, callslot_ptr)) {
                    selected_callslot_ptr = callslot_ptr;
                }
            }
        }
        auto interfaceslot_id = graph_ptr->GUI_GetSelectedInterfaceSlot();
        if (interfaceslot_id != GUI_INVALID_ID) {
            for (auto& group_ptr : graph_ptr->GetGroups()) {
                InterfaceSlotPtrType interfaceslot_ptr;
                if (group_ptr->GetInterfaceSlot(interfaceslot_id, interfaceslot_ptr)) {
                    CallSlotPtrType callslot_ptr;
                    if (interfaceslot_ptr->GetCompatibleCallSlot(callslot_ptr)) {
                        selected_callslot_ptr = callslot_ptr;
                    }
                }
            }
        }
    }

    ImGuiID id = 1;
    for (auto& mod : this->graph_manager.GetModulesStock()) {

        // Filter module by given search string
        search_filter = true;
        if (!search_string.empty()) {
            search_filter = this->utils.FindCaseInsensitiveSubstring(mod.class_name, search_string);
        }

        // Filter module by compatible call slots
        compat_filter = true;
        if (selected_callslot_ptr != nullptr) {
            compat_filter = false;
            for (auto& stock_callslot_map : mod.callslots) {
                for (auto& stock_callslot : stock_callslot_map.second) {
                    ImGuiID cpcidx = CallSlot::GetCompatibleCallIndex(selected_callslot_ptr, stock_callslot);
                    if (cpcidx != GUI_INVALID_ID) {
                        compat_callslot_name = stock_callslot.name;
                        compat_filter = true;
                    }
                }
            }
        }

        if (search_filter && compat_filter) {
            ImGui::PushID(id);

            std::string label = mod.class_name + " (" + mod.plugin_name + ")";
            if (mod.is_view) {
                label += " [View]";
            }
            if (ImGui::Selectable(label.c_str(), (id == this->selected_list_module_uid))) {
                this->selected_list_module_uid = id;
            }
            bool add_module = false;
            // Left mouse button double click action
            if ((ImGui::IsMouseDoubleClicked(0) && ImGui::IsItemHovered()) ||
                (ImGui::IsItemFocused() && ImGui::IsItemActivated())) {
                add_module = true;
            }
            // Context menu
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Add", "'Double-Click'")) {
                    add_module = true;
                }
                ImGui::EndPopup();
            }

            if (add_module) {
                if (graph_ptr != nullptr) {
                    ImGuiID module_uid = graph_ptr->AddModule(this->graph_manager.GetModulesStock(), mod.class_name);
                    ModulePtrType module_ptr;
                    if (graph_ptr->GetModule(module_uid, module_ptr)) {
                        // If there is a call slot selected, create call to compatible call slot of new module
                        if (compat_filter && (selected_callslot_ptr != nullptr)) {
                            // Get call slots of last added module
                            for (auto& callslot_map : module_ptr->GetCallSlots()) {
                                for (auto& callslot : callslot_map.second) {
                                    if (callslot->name == compat_callslot_name) {
                                        graph_ptr->AddCall(
                                            this->graph_manager.GetCallsStock(), selected_callslot_ptr, callslot);
                                    }
                                }
                            }
                        } else if (this->show_module_list_child) {
                            // Place new module at mouse pos if added via separate module list child window.
                            module_ptr->GUI_PlaceAtMousePosition();
                        }
                    }
                    this->show_module_list_child = false;
                } else {
                    vislib::sys::Log::DefaultLog.WriteError(
                        "No project loaded. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
                }
            }
            // Hover tool tip
            this->utils.HoverToolTip(mod.description, id, 0.5f, 5.0f);

            ImGui::PopID();
        }
        id++;
    }

    ImGui::EndChild();

    ImGui::EndGroup();
}


void megamol::gui::configurator::Configurator::add_empty_project(void) {

    ImGuiID graph_uid = this->graph_manager.AddGraph();
    if (graph_uid != GUI_INVALID_ID) {

        // Add initial GUIView and set as view instance
        GraphPtrType graph_ptr;
        if (this->graph_manager.GetGraph(graph_uid, graph_ptr)) {
            std::string guiview_class_name = "GUIView";
            ImGuiID module_uid = graph_ptr->AddModule(this->graph_manager.GetModulesStock(), guiview_class_name);
            ModulePtrType module_ptr;
            if (graph_ptr->GetModule(module_uid, module_ptr)) {
                auto graph_module = graph_ptr->GetModules().back();
                graph_module->is_view_instance = true;
            } else {
                vislib::sys::Log::DefaultLog.WriteError(
                    "Unable to add initial gui view module: '%s'. [%s, %s, line %d]\n", guiview_class_name.c_str(),
                    __FILE__, __FUNCTION__, __LINE__);
            }
        } else {
            vislib::sys::Log::DefaultLog.WriteError(
                "Unable to get last added graph. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
        }
    } else {
        vislib::sys::Log::DefaultLog.WriteError(
            "Unable to create new graph. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
    }
}


bool megamol::gui::configurator::Configurator::configurator_state_from_json_string(const std::string& in_json_string) {

    try {
        if (in_json_string.empty()) {
            return false;
        }

        bool found = false;

        nlohmann::json json;
        json = nlohmann::json::parse(in_json_string);

        if (!json.is_object()) {
            vislib::sys::Log::DefaultLog.WriteError(
                "State is no valid JSON object. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
            return false;
        }

        for (auto& header_item : json.items()) {
            if (header_item.key() == GUI_JSON_TAG_CONFIGURATOR) {
                found = true;
                auto config_state = header_item.value();

                // show_module_list_sidebar
                if (config_state.at("show_module_list_sidebar").is_boolean()) {
                    config_state.at("show_module_list_sidebar").get_to(this->show_module_list_sidebar);
                } else {
                    vislib::sys::Log::DefaultLog.WriteError(
                        "JSON state: Failed to read 'show_module_list_sidebar' as boolean. [%s, %s, line %d]\n",
                        __FILE__, __FUNCTION__, __LINE__);
                }
            } else if (header_item.key() == GUI_JSON_TAG_GRAPHS) {
                for (auto& config_item : header_item.value().items()) {
                    std::string json_graph_id = config_item.key(); /// = graph filename
                    // Load graph from file
                    this->graph_manager.LoadAddProjectFile(GUI_INVALID_ID, json_graph_id);
                    /*
                    if (graph_uid != GUI_INVALID_ID) {
                        GraphPtrType graph_ptr;
                        if (this->graph_manager.GetGraph(graph_uid, graph_ptr)) {
                            // Let graph search for his configurator state
                            graph_ptr->GUI_StateFromJsonString(in_json_string);
                        }
                    }
                    */
                }
            }
        }

        if (found) {
            vislib::sys::Log::DefaultLog.WriteInfo("[Configurator] Read configurator state from JSON string.");
        } else {
            /// vislib::sys::Log::DefaultLog.WriteWarn("Could not find configurator state in JSON. [%s, %s, line
            /// %d]\n", __FILE__, __FUNCTION__, __LINE__);
            return false;
        }

    } catch (nlohmann::json::type_error& e) {
        vislib::sys::Log::DefaultLog.WriteError(
            "JSON ERROR - %s: %s (%s:%d)", __FUNCTION__, e.what(), __FILE__, __LINE__);
        return false;
    } catch (nlohmann::json::invalid_iterator& e) {
        vislib::sys::Log::DefaultLog.WriteError(
            "JSON ERROR - %s: %s (%s:%d)", __FUNCTION__, e.what(), __FILE__, __LINE__);
        return false;
    } catch (nlohmann::json::out_of_range& e) {
        vislib::sys::Log::DefaultLog.WriteError(
            "JSON ERROR - %s: %s (%s:%d)", __FUNCTION__, e.what(), __FILE__, __LINE__);
        return false;
    } catch (nlohmann::json::other_error& e) {
        vislib::sys::Log::DefaultLog.WriteError(
            "JSON ERROR - %s: %s (%s:%d)", __FUNCTION__, e.what(), __FILE__, __LINE__);
        return false;
    } catch (...) {
        vislib::sys::Log::DefaultLog.WriteError(
            "Unknown Error - Unable to parse JSON string. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
        return false;
    }

    return true;
}


bool megamol::gui::configurator::Configurator::configurator_state_to_json(nlohmann::json& out_json) {

    try {
        out_json.clear();

        out_json[GUI_JSON_TAG_CONFIGURATOR]["show_module_list_sidebar"] = this->show_module_list_sidebar;

        for (auto& graph_ptr : this->graph_manager.GetGraphs()) {
            nlohmann::json graph_json;
            if (graph_ptr->GUI_StateToJSON(graph_json)) {
                out_json.update(graph_json);
            }
        }

        vislib::sys::Log::DefaultLog.WriteInfo("[Configurator] Wrote configurator state to JSON.");

    } catch (nlohmann::json::type_error& e) {
        vislib::sys::Log::DefaultLog.WriteError(
            "JSON ERROR - %s: %s (%s:%d)", __FUNCTION__, e.what(), __FILE__, __LINE__);
        return false;
    } catch (nlohmann::json::invalid_iterator& e) {
        vislib::sys::Log::DefaultLog.WriteError(
            "JSON ERROR - %s: %s (%s:%d)", __FUNCTION__, e.what(), __FILE__, __LINE__);
        return false;
    } catch (nlohmann::json::out_of_range& e) {
        vislib::sys::Log::DefaultLog.WriteError(
            "JSON ERROR - %s: %s (%s:%d)", __FUNCTION__, e.what(), __FILE__, __LINE__);
        return false;
    } catch (nlohmann::json::other_error& e) {
        vislib::sys::Log::DefaultLog.WriteError(
            "JSON ERROR - %s: %s (%s:%d)", __FUNCTION__, e.what(), __FILE__, __LINE__);
        return false;
    } catch (...) {
        vislib::sys::Log::DefaultLog.WriteError(
            "Unknown Error - Unable to write JSON of state. [%s, %s, line %d]\n", __FILE__, __FUNCTION__, __LINE__);
        return false;
    }

    return true;
}


#ifdef GUI_USE_GLFW
void megamol::gui::configurator::Configurator::file_drop_callback(
    ::GLFWwindow* window, int count, const char* paths[]) {

    int i;
    for (i = 0; i < count; i++) {
        megamol::gui::configurator::Configurator::dropped_files.emplace_back(std::string(paths[i]));
    }
}
#endif
