#include "gui.h"
#include "utils.h"
#include "engine.h"
#include "project.h"
#include "komplete.h"
#include "conspiracy.h"
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
        // auto mainwindow = std::make_unique<simplegui::PlainWindow>(nullptr, Gdk::Rectangle(20, 10, 200, 150), simplegui::Rgba(1, 0, 0));
        // mainwindow->AddChild<simplegui::PlainWindow>(Gdk::Rectangle(2, 2, 196, 146), simplegui::Rgba(0.2, 0.2, 0.2));
        // mainwindow->AddChild<simplegui::TextWindow>(Gdk::Rectangle(2, 2, 192, 146), "Hello, world!", simplegui::Rgba(1, 1, 1), 15);
        // m_KompleteGui.SetWindow(std::move(mainwindow));
    }
    void on_activate() override {
        auto window = guiCreateApplicationWindow(m_Engine);
        add_window(*window);
        window->show_all();

        // Set up a timeout signal to call ProcessEvents every 1ms
        Glib::signal_timeout().connect([this]() -> bool {
            ProcessEvents();
            return true;
        }, 5);
    }

    void ProcessEvents() {
        // called every 1 ms:
        m_Engine.ProcessMessages();
        m_KompleteGui1.Run();
        m_KompleteGui2.Run();
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
    conspiracy::Controller m_ConspiracyController { m_Engine };
    komplete::Gui m_KompleteGui1 {{0x17cc, 0x1620}, m_Engine}; // 61mk2
    komplete::Gui m_KompleteGui2 {{0x17cc, 0x1630}, m_Engine}; // 88mk2
};

int main(int argc, char** argv)
{
    __builtin_cpu_init();
    if(!__builtin_cpu_supports("avx2"))
    {
        throw std::runtime_error("CPU does not support AVX2");
    }

    auto app = Glib::RefPtr<Application>(new Application(argc, argv));
    return app->run(argc, argv);
}