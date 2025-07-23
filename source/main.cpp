#include "gui.h"
#include "utils.h"
#include "engine.h"
#include "project.h"
#include "komplete.h"
#include "kompletegui.h"
#include <filesystem>
#include <gtkmm.h>
#include "simplegui.h"

using namespace std::string_literals;
using namespace std::chrono_literals;

class Application : public Gtk::Application
{
public:
    static constexpr uint32_t cMaxBlockSize = 4096;
    ~Application() noexcept override = default;
    Application(int argc, char** argv) : m_Engine(cMaxBlockSize, argc, argv, GetProjectDir(), m_MainEventLoop), Gtk::Application("nl.newhouse.jnlive")
    {
    }
    void on_activate() override {
        auto window = guiCreateApplicationWindow(m_Engine);
        add_window(*window);
        window->show();

        // Set up a timeout signal to call ProcessEvents every 1ms
        Glib::signal_timeout().connect([this]() -> bool {
            ProcessEvents();
            return true;
        }, 5);
    }

    void ProcessEvents() {
        // called every 1 ms:
        m_Engine.ProcessMessages();
        m_KompleteGuiPool.Run();
    }
    static std::string GetProjectDir()
    {
        std::string homedir = getenv("HOME");
        std::string projectdir = homedir + "/.config/jnlive-data";
        return projectdir;
    }

private:
    utils::TGtkAppEventLoop m_MainEventLoop;
    engine::Engine m_Engine;
    komplete::TGuiPool m_KompleteGuiPool { m_Engine };
};

int main(int argc, char** argv)
{
    __builtin_cpu_init();
    if(!__builtin_cpu_supports("avx2"))
    {
        throw std::runtime_error("CPU does not support AVX2");
    }

    // we must use X11, because the suil doesn't support wayland
    g_setenv("GDK_BACKEND", "x11", TRUE);

    auto app = Glib::RefPtr<Application>(new Application(argc, argv));
    return app->run(argc, argv);
}