#include "gui.h"
#include "utils.h"
#include "engine.h"
#include "project.h"
#include "komplete.h"
#include <filesystem>
#include <gtkmm.h>

using namespace std::string_literals;
using namespace std::chrono_literals;

class Application : public Gtk::Application
{
public:
    static constexpr uint32_t cMaxBlockSize = 4096;
    Application(int argc, char** argv) : m_Engine(cMaxBlockSize, argc, argv), Gtk::Application("nl.newhouse.jnlive")
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
        }, 1);
    }

    void ProcessEvents() {
        // called every 1 ms:
        m_Engine.ProcessMessages();
    }
    void InitEngine()
    {
        std::string homedir = getenv("HOME");
        std::string projectdir = homedir + "/.config/jn-live";
        if(!std::filesystem::exists(projectdir))
        {
            std::filesystem::create_directory(projectdir);
        }
        {
            std::string projectfile = projectdir + "/project.json";
            if(!std::filesystem::exists(projectfile))
            {
                auto prj = project::TestProject();
                ProjectToFile(prj, projectfile);
            }
            {
                auto prj = project::ProjectFromFile(projectfile);
                m_Engine.SetProject(std::move(prj));
            }
        }
        {
            std::string jackconnectionfile = projectdir + "/jackconnection.json";
            if(!std::filesystem::exists(jackconnectionfile))
            {
                std::array<std::string, 2> m_AudioOutputs {
                    "Family 17h/19h HD Audio Controller Analog Stereo:playback_FL",
                    "Family 17h/19h HD Audio Controller Analog Stereo:playback_FR",
                };
                std::vector<std::string> midiInputs {};
                project::TJackConnections jackconn(std::move(m_AudioOutputs), std::move(midiInputs));
                JackConnectionsToFile(jackconn, jackconnectionfile);
            }
            {
                auto jackconn = project::JackConnectionsFromFile(jackconnectionfile);
                m_Engine.SetJackConnections(std::move(jackconn));
            }
        }

        {
            auto prj = m_Engine.Project();
            prj = prj.ChangePart(0, 3, std::nullopt, 1.0f);
            prj = prj.ChangePart(1, 2, std::nullopt, 1.0f);
            if(!prj.Parts().empty())
            {
                if(!prj.FocusedPart())
                {
                    prj = prj.Change(0,prj.ShowUi());

                }
            }
            m_Engine.SetProject(std::move(prj));
        }

        
    }
private:
    engine::Engine m_Engine;
};

int main(int argc, char** argv)
{
    auto app = Glib::RefPtr<Application>(new Application(argc, argv));
    return app->run(argc, argv);
}