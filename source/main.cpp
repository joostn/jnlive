#include <iostream>
#include <lilv/lilv.h>
#include <jack/jack.h>
#include <thread>

#include "utils.h"
#include "engine.h"
#include "project.h"
#include "komplete.h"
#include <filesystem>
#include <gtkmm.h>

using namespace std::string_literals;
using namespace std::chrono_literals;


class ApplicationWindow : public Gtk::ApplicationWindow
{
public:
    class PartsContainer : public Gtk::Box
    {
    public:
        class PartContainer : public Gtk::Box
        {
        public:
            PartContainer(PartContainer &&other) = delete;
            PartContainer(const PartContainer &other) = delete;
            PartContainer(engine::Engine &engine, size_t partindex) : m_PartIndex(partindex), m_Engine(engine)
            {
                set_orientation(Gtk::ORIENTATION_VERTICAL);
                pack_start(m_ActivateButton);
                m_ActivateButton.signal_clicked().connect([this](){
                    const auto &prj = m_Engine.Project();
                    if(m_PartIndex < prj.Parts().size())
                    {
                        bool isSelected = prj.FocusedPart() == m_PartIndex;
                        if(prj.FocusedPart() != m_PartIndex)
                        {
                            auto newproject = prj.Change(m_PartIndex, prj.ShowUi());
                            m_Engine.SetProject(std::move(newproject));
                        }
                        else
                        {
                            // still focused:
                            m_ActivateButton.set_state_flags(Gtk::STATE_FLAG_CHECKED);
                        }
                    }
                });
                OnProjectChanged();
            }
            void OnProjectChanged()
            {
                const auto &prj = m_Engine.Project();
                bool isSelected = prj.FocusedPart() == m_PartIndex;
                if(m_PartIndex < prj.Parts().size())
                {
                    const auto &part = prj.Parts()[m_PartIndex];
                    m_ActivateButton.set_label(part.Name());
                    m_ActivateButton.set_state_flags(isSelected ? Gtk::STATE_FLAG_CHECKED : Gtk::STATE_FLAG_NORMAL, true);
                }
            }
        private:
            Gtk::ToggleButton m_ActivateButton;
            engine::Engine &m_Engine;
            size_t m_PartIndex;
            utils::NotifySink m_OnProjectChanged {m_Engine.OnProjectChanged(), [this](){OnProjectChanged();}};
        };
        PartsContainer(engine::Engine &engine) : m_Engine(engine)
        {
            set_orientation(Gtk::ORIENTATION_VERTICAL);
            OnProjectChanged();
        }
        void OnProjectChanged()
        {
            const auto &project = m_Engine.Project();
            while(m_PartContainers.size() > project.Parts().size())
            {
                remove(*m_PartContainers.back());
                m_PartContainers.pop_back();
            }
            while(m_PartContainers.size() < project.Parts().size())
            {
                auto container = std::make_unique<PartContainer>(m_Engine, m_PartContainers.size());
                m_PartContainers.push_back(std::move(container));
                pack_start(*m_PartContainers.back());
            }
            show_all_children();
        }
    private:
        engine::Engine &m_Engine;
        utils::NotifySink m_OnProjectChanged {m_Engine.OnProjectChanged(), [this](){OnProjectChanged();}};
        std::vector<std::unique_ptr<PartContainer>> m_PartContainers;
    };
    ApplicationWindow(engine::Engine &engine) : m_Engine(engine), m_PartsContainer(engine)
    {
        set_default_size(800, 600);
        add(m_Box);

        // Left widget (Expanding)
        m_Box.pack_start(m_LeftButton, Gtk::PACK_EXPAND_WIDGET);

        m_Box.pack_start(m_PartsContainer, Gtk::PACK_SHRINK);
        show_all_children();
    }
    ~ApplicationWindow()
    {
        m_Box.remove(m_LeftButton);
        m_Box.remove(m_PartsContainer);
    }
    void OnProjectChanged()
    {        
    }

private:
    engine::Engine &m_Engine;
    utils::NotifySink m_OnProjectChanged {m_Engine.OnProjectChanged(), [this](){OnProjectChanged();}};
    PartsContainer m_PartsContainer;
    Gtk::Box m_Box {Gtk::ORIENTATION_HORIZONTAL};
    Gtk::Button m_LeftButton {"Left Button"};
};

class Application : public Gtk::Application
{
public:
    static constexpr uint32_t cMaxBlockSize = 4096;
    Application(int argc, char** argv) : m_Engine(cMaxBlockSize, argc, argv), Gtk::Application("nl.newhouse.jnlive")
    {
    }
    void on_activate() override {
        InitEngine();
        auto window = new ApplicationWindow(m_Engine);
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