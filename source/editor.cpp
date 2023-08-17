#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <nfd.h>

#include "editor.hpp"
#include "palette.hpp"
#include "context.hpp"

#include "actions/change_color_count.hpp"
#include "actions/modify_color.hpp"
#include "actions/swap_colors.hpp"

#include "popups/combine.hpp"
#include "popups/error.hpp"
#include "popups/logger.hpp"
#include "popups/prompt.hpp"

#include <iostream>
#include <algorithm>
#include <fstream>

static void glfw_error_callback(int error, const char *description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

Editor::Editor()
{
    this->InitGLFW();
    this->InitImGui();
}

Editor::~Editor()
{
    this->ExitImGui();
    this->ExitGLFW();
}

void Editor::InitGLFW()
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        std::abort();

#if defined(__APPLE__)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);

    m_Window = glfwCreateWindow(400, 400, "Palette Editor", NULL, NULL);
    if (m_Window == NULL)
        std::abort();

    glfwSetWindowUserPointer(m_Window, this);
    glfwSetWindowSizeLimits(m_Window, 400, 400, 400, 400);

    glfwMakeContextCurrent(m_Window);
    glfwSwapInterval(1);

    int win_w, win_h, fb_w, fb_h;
    glfwGetWindowSize(m_Window, &win_w, &win_h);
    glfwGetFramebufferSize(m_Window, &fb_w, &fb_h);

    glfwSetKeyCallback(m_Window, [](GLFWwindow *window, int key, int scancode, int action, int mods) {
        Editor *editor = static_cast<Editor *>(glfwGetWindowUserPointer(window));

        if (action == GLFW_PRESS || action == GLFW_REPEAT)
        {
            editor->ProcessShortcuts(key, mods);
            glfwPollEvents();
            editor->StartFrame();
            editor->Frame();
            editor->EndFrame();
        }
    });

    glfwSetDropCallback(m_Window, [](GLFWwindow *window, int path_count, const char *paths[]) {
        Editor *editor = static_cast<Editor *>(glfwGetWindowUserPointer(window));

        if (path_count <= 0)
            return;

        std::string path = std::string(paths[0], strlen(paths[0]));
        if (path.substr(path.find_last_of(".") + 1) != "pal")
            return;
        editor->OpenPalette(path.c_str());
    });

    glfwSetWindowCloseCallback(m_Window, [](GLFWwindow *window) {
        Editor *editor = static_cast<Editor *>(glfwGetWindowUserPointer(window));
        if (Context::GetContext().isDirty)
        {
            glfwSetWindowShouldClose(window, GLFW_FALSE);
            editor->m_PopupManager.OpenPopup<Popups::Prompt>("dirty_buffer_prompt", "There are unsaved changes.\nDo you want to quit?", [window](){
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            });
        }
    });
}

void Editor::InitImGui(void)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = "palette_editor.ini";

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(m_Window, true);


#if defined(__APPLE__)
    ImGui_ImplOpenGL3_Init("#version 150");
#else
    ImGui_ImplOpenGL3_Init("#version 130");
#endif
}

void Editor::StartFrame(void)
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGuiWindowFlags windowflags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;

    ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size - ImVec2(0.0f, ImGui::GetFrameHeight()));

    ImGui::Begin("PalEditor", NULL, windowflags);
    m_PopupManager.UpdateAndDraw();
    this->MenuBar();
    ImGui::End();

    this->StatusBar();
}

void Editor::Frame(void)
{
    ImGui::Begin("PalEditor", NULL, 0);
    this->DetailsBar();
    ImGui::Spacing();
    this->PaletteEditor();
    ImGui::End();
}

void Editor::EndFrame(void)
{
    int display_w, display_h;

    ImGui::Render();
    glfwGetFramebufferSize(m_Window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(m_Window);
}

void Editor::Loop(void)
{
    while (!glfwWindowShouldClose(m_Window))
    {
        if (glfwGetWindowAttrib(m_Window, GLFW_ICONIFIED) || 
            !glfwGetWindowAttrib(m_Window, GLFW_FOCUSED) ||
            !glfwGetWindowAttrib(m_Window, GLFW_VISIBLE))
        {
            glfwWaitEvents();
        }
        else
        {
            glfwPollEvents();
            this->StartFrame();
            this->Frame();
            this->EndFrame();
        }
    }
}

void Editor::ExitImGui(void)
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void Editor::ExitGLFW(void)
{
    glfwDestroyWindow(m_Window);
    glfwTerminate();
}

#if defined(__APPLE__)
static const char *sText_FileShortcuts[] =
{
    "Cmd+O",
    "Cmd+S",
    "Cmd+Shift+S",
    "Cmd+Q"
};
#else
static const char *sText_FileShortcuts[] =
{
    "Ctrl+O",
    "Ctrl+S",
    "Ctrl+Shift+S",
    "Ctrl+Q"
};
#endif

void Editor::MenuBar(void)
{
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Open", sText_FileShortcuts[0])) 
            {
                this->PromptOpenPalette();
            }
            if (ImGui::MenuItem("Save", sText_FileShortcuts[1])) 
            {
                this->SavePalette(false);
            }
            if (ImGui::MenuItem("Save As", sText_FileShortcuts[2]))
            {
                this->SavePalette(true);
            }
            if (ImGui::MenuItem("Logger", nullptr))
            {
                m_PopupManager.OpenPopup<Popups::Logger>();
            }
            if (ImGui::MenuItem("Quit", sText_FileShortcuts[3]))
            {
                const char *s;
                if (Context::GetContext().isDirty)
                    s = "There are unsaved changes.\nDo you want to quit?";
                else
                    s = "Do you want to quit?";

                m_PopupManager.OpenPopup<Popups::Prompt>("dirty_buffer_prompt", s, [this](){
                    glfwSetWindowShouldClose(this->m_Window, GLFW_TRUE);
                });
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit"))
        {
#if defined(__APPLE__)
            if (ImGui::MenuItem("Undo", "Cmd+Z", nullptr, m_ActionRegister.CanUndo())) m_ActionRegister.Undo();
            if (ImGui::MenuItem("Redo", "Cmd+R", nullptr, m_ActionRegister.CanRedo())) m_ActionRegister.Redo();
#else
            if (ImGui::MenuItem("Undo", "Ctrl+Z", nullptr, Context::GetContext().actionRegister.CanUndo())) Context::GetContext().actionRegister.Undo();
            if (ImGui::MenuItem("Redo", "Ctrl+R", nullptr, Context::GetContext().actionRegister.CanRedo())) Context::GetContext().actionRegister.Redo();
#endif
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Others"))
        {
#if defined(__APPLE__)
            if (ImGui::MenuItem("Combine Palettes")) StartPalettesCombine();
            if (ImGui::MenuItem("Split Palette")) {}
#else
            if (ImGui::MenuItem("Combine Palettes", "Ctrl+Shift+K")) StartPalettesCombine();
            if (ImGui::MenuItem("Split Palette")) {}
#endif
            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }
}

void Editor::DetailsBar(void)
{
    int num_colors = Context::GetContext().palette.size();

    if (ImGui::InputInt("No. of Colors", &num_colors))
        num_colors = std::min(std::max(1, num_colors), 255);

    if (ImGui::IsItemDeactivatedAfterEdit() && Context::GetContext().palette.size() != num_colors)
    {
        Context::GetContext().actionRegister.RegisterAction<Actions::ChangeColorCount>(Context::GetContext().palette.size(), (size_t)num_colors);
        Context::GetContext().isDirty = true;
    }

    ImGui::TextWrapped("Path:\n%s", Context::GetContext().loadedFile.empty() ? "No file opened." : Context::GetContext().loadedFile.c_str());
}

void Editor::PaletteEditor(void)
{
    static bool hasCachedColor = false;
    static Color cachedColor;
    auto &palette = Context::GetContext().palette;

    ImGui::BeginChild("Colors", ImVec2(0.0f, 0.0f), true, ImGuiWindowFlags_AlwaysAutoResize);
    for (int i = 0; i < palette.size(); i++)
    {
        char label[10];
        snprintf(label, 10, "Color #%i", i);

        auto &color = palette[i];
        ImGui::ColorEdit3(label, (float *)&color, ImGuiColorEditFlags_NoDragDrop);

        if (ImGui::IsItemActivated())
        {
            if (!hasCachedColor)
                cachedColor = palette[i];

            hasCachedColor = true;
        }

        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            Context::GetContext().actionRegister.RegisterAction<Actions::ModifyColor>(i, cachedColor, color);
            Context::GetContext().isDirty = true;
            hasCachedColor = false;
        }

        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
        {
            ImGui::SetDragDropPayload("ColorDND", &i, sizeof(size_t));

            const auto &col = palette[i];
            const ImVec4 col_v4(col[0], col[1], col[2], 1.0f);
            ImGui::ColorButton("##preview", col_v4); ImGui::SameLine();
            ImGui::Text("Color");
            ImGui::EndDragDropSource();
        }

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ColorDND"))
            {
                IM_ASSERT(payload->DataSize == sizeof(size_t));
                int target = *(const size_t*)payload->Data;
                Context::GetContext().actionRegister.RegisterAction<Actions::SwapColors>(i, target);
                Context::GetContext().isDirty = true;
            }
            ImGui::EndDragDropTarget();
        }
    }

    ImGui::EndChild();
}

void Editor::StatusBar(void)
{
    const ImGuiViewport *viewport = ImGui::GetMainViewport();

    auto barPos = ImVec2(viewport->Pos.x, viewport->Pos.y + viewport->Size.y - ImGui::GetFrameHeight());
    auto barSize = ImVec2(viewport->Size.x, ImGui::GetFrameHeight());

    ImGui::SetNextWindowPos(barPos);
    ImGui::SetNextWindowSize(barSize);

    ImGuiWindowFlags flags = 
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_MenuBar |
        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground | 
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize;


    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    ImGui::Begin("##StatusBar", nullptr, flags);

    ImGui::SetCursorPosX(8);
    if (ImGui::BeginMenuBar())
    {
        const auto &undoStack = Context::GetContext().actionRegister.GetRedoStack(), &redoStack = Context::GetContext().actionRegister.GetUndoStack();
        ImGui::Text("Action Stack: %lu (Undo) | %lu (Redo)", undoStack.size(), redoStack.size());
        ImGui::EndMenuBar();
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

static const nfdfilteritem_t sFilterPatterns[] = { {"Palette Files", "pal"} };

void Editor::OpenPalette(const char *path)
{

    std::string new_file = std::string(path, strlen(path));
    std::ifstream palfile(new_file, std::ios::in);

    if (palfile.is_open())
    {
        try
        {
            Context::GetContext().palette.LoadFromFile(palfile);
        }
        catch (const char *c)
        {
            m_PopupManager.OpenPopup<Popups::Error>("cannot_open_palette", c);
            return;
        }
    }

    palfile.close();

    Context::GetContext().loadedFile = new_file;
    Context::GetContext().isDirty = false;
}

void Editor::PromptOpenPalette(void)
{
    if (Context::GetContext().isDirty)
    {
        m_PopupManager.OpenPopup<Popups::Prompt>(
            "dirty_buffer_prompt",
            "Opening another palette will discard unsaved changes.\nDo you want to continue?",
            [this](void)
            {
                char *path;
                nfdresult_t result = NFD_OpenDialog(&path, sFilterPatterns, 1, 0);
                if (result != NFD_OKAY)
                    return;

                this->OpenPalette(path);
                NFD_FreePath(path);
            }
        );
    }
    else
    {
        char *path;
        nfdresult_t result = NFD_OpenDialog(&path, sFilterPatterns, 1, 0);
        if (result != NFD_OKAY)
            return;
        this->OpenPalette(path);
        NFD_FreePath(path);
    }
}

void Editor::SavePalette(bool promptFilepath)
{
    if (Context::GetContext().loadedFile.empty() || promptFilepath)
    {
        char *path;
        nfdresult_t result = NFD_SaveDialog(&path, sFilterPatterns, 1, 0, 0);

        if (result == NFD_OKAY)
            Context::GetContext().loadedFile = std::string(path);
        else
            return;
    }

    std::ofstream palfile(Context::GetContext().loadedFile, std::ios::out);
    if (palfile.is_open())
    {
        try
        {
            Context::GetContext().palette.SaveToFile(palfile);
        }
        catch (const char *c)
        {
            printf("%s\n", c);
            return;
        }
    }

    Context::GetContext().isDirty = false;
    palfile.close();
}

void Editor::StartPalettesCombine(void)
{
    m_PopupManager.OpenPopup<Popups::Combine>();
}

void Editor::ProcessShortcuts(int key, int mods)
{
    if (mods & (GLFW_MOD_CONTROL | GLFW_MOD_SUPER))
    {
        switch (key)
        {
        case GLFW_KEY_O:
            this->PromptOpenPalette();
            break;
        case GLFW_KEY_S:
            if (mods & GLFW_MOD_SHIFT)
                this->SavePalette(true);
            else
                this->SavePalette(false);
            break;
        case GLFW_KEY_Z:
            Context::GetContext().actionRegister.Undo();
            break;
        case GLFW_KEY_R:
            Context::GetContext().actionRegister.Redo();
            break;
        case GLFW_KEY_K:
            if (mods & GLFW_MOD_SHIFT)
                StartPalettesCombine();
            break;
        }
    }
}
