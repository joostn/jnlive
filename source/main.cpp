#include "gui.h"
#include "utils.h"
#include "engine.h"
#include "project.h"
#include "komplete.h"
#include "conspiracy.h"
#include <filesystem>
#include <gtkmm.h>

using namespace std::string_literals;
using namespace std::chrono_literals;

class Application : public Gtk::Application
{
public:
    static constexpr uint32_t cMaxBlockSize = 4096;
    Application(int argc, char** argv) : m_Engine(cMaxBlockSize, argc, argv, GetProjectDir()), Gtk::Application("nl.newhouse.jnlive")
    {
    }
    void on_activate() override {
        InitEngine();
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
        m_KompleteHid.Run();
        m_KompleteDisplay.Run();
        m_KompleteDisplay.Test();
    }
    static std::string GetProjectDir()
    {
        std::string homedir = getenv("HOME");
        std::string projectdir = homedir + "/.config/jnlive-data";
        return projectdir;
    }
    void InitEngine()
    {
        {
            std::string jackconnectionfile = m_Engine.ProjectDir() + "/jackconnection.json";
            if(!std::filesystem::exists(jackconnectionfile))
            {
                std::array<std::vector<std::string>, 2> m_AudioOutputs {
                    std::vector<std::string>{"Family 17h/19h HD Audio Controller Analog Stereo:playback_FL"s},
                    std::vector<std::string>{"Family 17h/19h HD Audio Controller Analog Stereo:playback_FR"s},
                };
                std::vector<std::vector<std::string>> midiInputs {};
                std::vector<std::pair<std::string, std::vector<std::string>>> controllermidiports;
                project::TJackConnections jackconn(std::move(m_AudioOutputs), std::move(midiInputs), std::move(controllermidiports));
                JackConnectionsToFile(jackconn, jackconnectionfile);
            }
            {
                auto jackconn = project::JackConnectionsFromFile(jackconnectionfile);
                m_Engine.SetJackConnections(std::move(jackconn));
            }
        }        
    }
private:
    engine::Engine m_Engine;
    conspiracy::Controller m_ConspiracyController { m_Engine };
    komplete::Hid m_KompleteHid;
    komplete::Display m_KompleteDisplay;
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