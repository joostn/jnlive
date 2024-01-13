#include <iostream>
#include "utils.h"
#include "engine.h"
#include "project.h"
#include <filesystem>
#include <gtkmm.h>
#include "lilvutils.h"

namespace 
{

void DoAndShowException(std::function<void()> f)
{
    try
    {
        f();
    }
    catch(const std::exception &e)
    {
        std::string msg = e.what();
        // show in gtk messafe box:
        Gtk::MessageDialog dialog(msg, false, Gtk::MESSAGE_ERROR, Gtk::BUTTONS_OK, true);
        dialog.run();
    }
}

class PluginSelectorDialog : public Gtk::Dialog
{
public:
    PluginSelectorDialog(PluginSelectorDialog &&other) = delete;
    PluginSelectorDialog(const PluginSelectorDialog &other) = delete;
    PluginSelectorDialog()
    {
        set_title("Select Plugin");
        set_default_size(400, 300);
        add_button("Cancel", Gtk::RESPONSE_CANCEL);
        m_OkButton = add_button("OK", Gtk::RESPONSE_OK);
        m_PluginListScrolledWindow.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
        m_PluginListScrolledWindow.add(m_PluginList);
        get_vbox()->pack_start(m_PluginListScrolledWindow, Gtk::PACK_EXPAND_WIDGET);

        m_PluginList.signal_cursor_changed().connect([this](){
            Gtk::TreeModel::Path path;
            Gtk::TreeViewColumn* focus_column;
            m_PluginList.get_cursor(path, focus_column);
            std::optional<std::string> selectedpluginuri;
            if(path)
            {
                auto iter = m_ListStore->get_iter(path);
                if(iter)
                {
                    selectedpluginuri = iter->get_value(m_Columns.m_Lv2Uri);
                }
            }
            m_SelectedPluginUri = selectedpluginuri;
            enableItems();
        });
        m_PluginList.set_model(m_ListStore);
        m_PluginList.append_column("Name", m_Columns.m_Name);
        m_PluginList.append_column("Class", m_Columns.m_Class);
        Populate();
        enableItems();
        show_all();
    }
    void enableItems()
    {
        bool canok = true;
        if(!m_SelectedPluginUri)
        {
            canok = false;
        }
        m_OkButton->set_sensitive(canok);
    }
    void Populate()
    {
        const auto &pluginlist = lilvutils::World::Static().PluginList();
        for(const auto &plugin: pluginlist.Plugins())
        {
            Gtk::TreeModel::Row row = *(m_ListStore->append());
            row[m_Columns.m_Name] = plugin.Name();
            row[m_Columns.m_Lv2Uri] = plugin.Uri();
            row[m_Columns.m_Class] = pluginlist.Classes().at(plugin.ClassIndex()).Description();
        }
    }
    std::optional<std::string> SelectedPluginUri() const { return m_SelectedPluginUri; }

private:
    class Columns : public Gtk::TreeModel::ColumnRecord
    {
    public:
        Columns()
        {
            add(m_Name);
            add(m_Class);
            add(m_Lv2Uri);
        }
        Gtk::TreeModelColumn<std::string> m_Name;
        Gtk::TreeModelColumn<std::string> m_Class;
        Gtk::TreeModelColumn<std::string> m_Lv2Uri;
    };
    Gtk::ScrolledWindow m_PluginListScrolledWindow;
    std::optional<std::string> m_SelectedPluginUri;
    Columns m_Columns;
    Glib::RefPtr<Gtk::ListStore> m_ListStore {Gtk::ListStore::create(m_Columns)};
    Gtk::TreeView m_PluginList;
    Gtk::Button *m_OkButton = nullptr;
};

class PresetSelectorDialog : public Gtk::Dialog
{
public:
    PresetSelectorDialog(PresetSelectorDialog &&other) = delete;
    PresetSelectorDialog(const PresetSelectorDialog &other) = delete;
    PresetSelectorDialog(const project::Project &project) : m_Project(project)
    {
        set_title("Select Preset");
        set_default_size(400, 300);
        add_button("Cancel", Gtk::RESPONSE_CANCEL);
        m_OkButton = add_button("OK", Gtk::RESPONSE_OK);
        m_PresetListScrolledWindow.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
        m_PresetListScrolledWindow.add(m_PresetList);

// Add a text box for entering the name of the preset
        //m_PresetNameEntry.set_text("Enter preset name");
        get_vbox()->set_spacing(5);
        get_vbox()->pack_start(m_PresetListScrolledWindow, Gtk::PACK_EXPAND_WIDGET);

        get_vbox()->pack_start(m_PresetNameEntry, Gtk::PACK_SHRINK);

        //m_PresetList.set_selection_mode(Gtk::SELECTION_SINGLE);
        m_PresetList.signal_cursor_changed().connect([this](){
            Gtk::TreeModel::Path path;
            Gtk::TreeViewColumn* focus_column;
            m_PresetList.get_cursor(path, focus_column);
            const project::TQuickPreset *preset = nullptr;
            if(path)
            {
                auto iter = m_ListStore->get_iter(path);
                if(iter)
                {
                    m_SelectedPreset = iter->get_value(m_Columns.m_Number);
                    if(m_SelectedPreset && *m_SelectedPreset < m_Project.QuickPresets().size())
                    {
                        if(m_Project.QuickPresets()[*m_SelectedPreset])
                        {
                            preset = &m_Project.QuickPresets()[*m_SelectedPreset].value();
                        }
                    }

                }
            }
            if(preset)
            {
                m_PresetNameEntry.set_text(preset->Name());
            }
            else
            {
                m_PresetNameEntry.set_text("");
            }
            enableItems();
        });
        m_PresetList.set_model(m_ListStore);
        m_PresetList.append_column("Number", m_Columns.m_Number);
        m_PresetList.append_column("Name", m_Columns.m_Name);
        show_all();
        m_PresetNameEntry.signal_changed().connect([this](){
            enableItems();
        });
        Populate();
        enableItems();
    }
    void enableItems()
    {
        bool canok = true;
        if(m_PresetNameEntry.get_text().empty())
        {
            canok = false;
        }
        if(!m_SelectedPreset)
        {
            canok = false;
        }
        m_OkButton->set_sensitive(canok);
    }
    std::optional<size_t> SelectedPreset() const { return m_SelectedPreset; }
    std::string PresetName() const { return m_PresetNameEntry.get_text(); }
    void Populate()
    {
        m_ListStore->clear();
        size_t numpresets = std::max((size_t)128, m_Project.QuickPresets().size());
        for(size_t i = 0; i < numpresets; i++)
        {
            const project::TQuickPreset *preset = nullptr;
            if(i < m_Project.QuickPresets().size())
            {
                if(m_Project.QuickPresets()[i])
                {
                    preset = &m_Project.QuickPresets()[i].value();
                }
            }
            Gtk::TreeModel::Row row = *(m_ListStore->append());
            row[m_Columns.m_Number] = i;
            row[m_Columns.m_Name] = preset ? preset->Name() : "(empty)";
        }
    }
private:
    class Columns : public Gtk::TreeModel::ColumnRecord
    {
    public:
        Columns()
        {
            add(m_Number);
            add(m_Name);
        }
        Gtk::TreeModelColumn<size_t> m_Number;
        Gtk::TreeModelColumn<std::string> m_Name;
    };
    const project::Project &m_Project;
    Gtk::ScrolledWindow m_PresetListScrolledWindow;
    std::optional<size_t> m_SelectedPreset;
    Columns m_Columns;
    Glib::RefPtr<Gtk::ListStore> m_ListStore {Gtk::ListStore::create(m_Columns)};
    Gtk::TreeView m_PresetList;
    Gtk::Entry m_PresetNameEntry;
    Gtk::Button *m_OkButton = nullptr;
};



class ApplicationWindow : public Gtk::ApplicationWindow
{
public:
    enum class FocusedTab { Presets, Instruments };
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
                    DoAndShowException([this](){
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
    class PresetsPanel : public Gtk::ScrolledWindow
    {
    public:
        PresetsPanel(PresetsPanel &&other) = delete;
        PresetsPanel(const PresetsPanel &other) = delete;
        PresetsPanel(engine::Engine &engine) : m_Engine(engine)
        {
            set_policy(Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
            add(m_PresetsBox);
            OnProjectChanged();
        }
        void OnProjectChanged()
        {
            const auto &project = m_Engine.Project();
            std::optional<size_t> activePreset;
            if(project.FocusedPart() && *project.FocusedPart() < project.Parts().size())
            {
                const auto &part = project.Parts()[*project.FocusedPart()];
                activePreset = part.ActivePresetIndex();
            }

            while(m_PresetButtons.size() > project.QuickPresets().size())
            {
                m_PresetsBox.remove(*m_PresetButtons.back());
                m_PresetButtons.pop_back();
            }
            while(m_PresetButtons.size() < project.QuickPresets().size())
            {
                auto button = std::make_unique<Gtk::ToggleButton>();
                size_t presetindex = m_PresetButtons.size();
                button->signal_clicked().connect([this, presetindex](){
                    DoAndShowException([this, presetindex](){
                        auto newproject = m_Engine.Project().SwitchFocusedPartToPreset(presetindex);
                        m_Engine.SetProject(std::move(newproject));
                        OnProjectChanged();
                        m_Engine.LoadCurrentPreset();
                    });
                });
                m_PresetButtons.push_back(std::move(button));
                m_PresetsBox.add(*m_PresetButtons.back());
            }
            for(size_t i = 0; i < m_PresetButtons.size(); i++)
            {
                const auto &preset = project.QuickPresets()[i];
                std::string label = std::to_string(i) + " ";
                if(preset)
                {
                    label += preset.value().Name();
                }
                else
                {
                    label += "(empty)";
                }
                auto &button = *m_PresetButtons[i];
                button.set_label(label);
                button.set_state_flags(activePreset && *activePreset == i ? Gtk::STATE_FLAG_CHECKED : Gtk::STATE_FLAG_NORMAL, true);
            }
            show_all_children();
        }
    private:
        engine::Engine &m_Engine;
        utils::NotifySink m_OnProjectChanged {m_Engine.OnProjectChanged(), [this](){OnProjectChanged();}};
        Gtk::FlowBox m_PresetsBox;
        std::vector<std::unique_ptr<Gtk::ToggleButton>> m_PresetButtons;
    };
    class InstrumentsPanel : public Gtk::ScrolledWindow
    {
    public:
        InstrumentsPanel(InstrumentsPanel &&other) = delete;
        InstrumentsPanel(const InstrumentsPanel &other) = delete;
        InstrumentsPanel(engine::Engine &engine) : m_Engine(engine)
        {
            set_policy(Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
            add(m_InstrumentsBox);
            OnProjectChanged();
        }
        void OnProjectChanged()
        {
            const auto &project = m_Engine.Project();
            std::optional<size_t> activeInstrument;
            if(project.FocusedPart() && *project.FocusedPart() < project.Parts().size())
            {
                const auto &part = project.Parts()[*project.FocusedPart()];
                activeInstrument = part.ActiveInstrumentIndex();
            }

            while(m_InstrumentButtons.size() > project.Instruments().size())
            {
                m_InstrumentsBox.remove(*m_InstrumentButtons.back());
                m_InstrumentButtons.pop_back();
            }
            while(m_InstrumentButtons.size() < project.Instruments().size())
            {
                auto button = std::make_unique<Gtk::ToggleButton>();
                size_t instrumentindex = m_InstrumentButtons.size();
                button->signal_clicked().connect([this, instrumentindex](){
                    DoAndShowException([this, instrumentindex](){
                        const auto &project = m_Engine.Project();
                        if(project.FocusedPart() && (*project.FocusedPart() < project.Parts().size()))
                        {
                            const auto &part = project.Parts()[*project.FocusedPart()];
                            auto newproject = project.ChangePart(*project.FocusedPart(), instrumentindex, std::nullopt, part.AmplitudeFactor());
                            m_Engine.SetProject(std::move(newproject));
                        }
                        OnProjectChanged();
                    });
                });
                m_InstrumentButtons.push_back(std::move(button));
                m_InstrumentsBox.add(*m_InstrumentButtons.back());
            }
            for(size_t i = 0; i < m_InstrumentButtons.size(); i++)
            {
                const auto &instrument = project.Instruments()[i];
                auto &button = *m_InstrumentButtons[i];
                button.set_label(instrument.Name());
                button.set_state_flags(activeInstrument && *activeInstrument == i ? Gtk::STATE_FLAG_CHECKED : Gtk::STATE_FLAG_NORMAL, true);
            }
            show_all_children();
        }
    private:
        engine::Engine &m_Engine;
        utils::NotifySink m_OnProjectChanged {m_Engine.OnProjectChanged(), [this](){OnProjectChanged();}};
        Gtk::FlowBox m_InstrumentsBox;
        std::vector<std::unique_ptr<Gtk::ToggleButton>> m_InstrumentButtons;
    };
    class TopBar : public Gtk::Box
    {
    public:
        TopBar(TopBar&&) = delete;
        TopBar(ApplicationWindow &applicationWindow) : m_ApplicationWindow(applicationWindow)
        {
            set_orientation(Gtk::ORIENTATION_HORIZONTAL);
            m_ModePresetButton.signal_clicked().connect([this](){
                m_ApplicationWindow.SetFocusedTab(FocusedTab::Presets);
                Update();
            });
            m_ModeInstrumentsButton.signal_clicked().connect([this](){
                m_ApplicationWindow.SetFocusedTab(FocusedTab::Instruments);
                Update();
            });
            m_ShowGuiButton.signal_clicked().connect([this](){
                DoAndShowException([this](){
                    auto prj = m_ApplicationWindow.Engine().Project().Change(m_ApplicationWindow.Engine().Project().FocusedPart(), !m_ApplicationWindow.Engine().Project().ShowUi());
                    m_ApplicationWindow.Engine().SetProject(std::move(prj));
                });
            });
            m_MenuButton.set_popup(applicationWindow.PopupMenu());
            pack_start(m_ModePresetButton, Gtk::PACK_SHRINK);
            pack_start(m_ModeInstrumentsButton, Gtk::PACK_SHRINK);
            pack_start(m_ShowGuiButton, Gtk::PACK_SHRINK);
            pack_start(m_MenuButton, Gtk::PACK_SHRINK);
            Update();
        }
        void Update()
        {
            auto focusedTab = m_ApplicationWindow.focusedTab();
            m_ModePresetButton.set_state_flags(focusedTab == FocusedTab::Presets?  Gtk::STATE_FLAG_CHECKED : Gtk::STATE_FLAG_NORMAL, true);
            m_ModeInstrumentsButton.set_state_flags(focusedTab == FocusedTab::Instruments?  Gtk::STATE_FLAG_CHECKED : Gtk::STATE_FLAG_NORMAL, true);
            m_ShowGuiButton.set_state_flags(m_ApplicationWindow.Engine().Project().ShowUi()?  Gtk::STATE_FLAG_CHECKED : Gtk::STATE_FLAG_NORMAL, true);
        }
    private:
        Gtk::ToggleButton m_ModePresetButton {"Presets"};
        Gtk::ToggleButton m_ModeInstrumentsButton {"Instruments"};
        Gtk::ToggleButton m_ShowGuiButton {"GUI"};
        Gtk::MenuButton m_MenuButton;
        ApplicationWindow &m_ApplicationWindow;
        utils::NotifySink m_OnProjectChanged {m_ApplicationWindow.Engine().OnProjectChanged(), [this](){Update();}};
    };
    ApplicationWindow(engine::Engine &engine) : m_Engine(engine), m_PartsContainer(engine)
    {
        set_default_size(800, 600);
        add(m_Box1);
        m_Box1.pack_start(*m_TopBar, Gtk::PACK_SHRINK);
        m_Box1.pack_start(m_Box2, Gtk::PACK_EXPAND_WIDGET);
        m_Box2.pack_start(m_MainPanelStack, Gtk::PACK_EXPAND_WIDGET);
        m_Box2.pack_start(m_PartsContainer, Gtk::PACK_SHRINK);
        m_MainPanelStack.add(*m_PresetsPanel);
        m_MainPanelStack.add(*m_InstrumentsPanel);
        show_all_children();
        m_SavePresetMenuItem.signal_activate().connect([this](){
            DoAndShowException([this](){
                auto project_copy = m_Engine.Project();
                PresetSelectorDialog dialog(project_copy);
                int result = dialog.run();
                if(result == Gtk::RESPONSE_OK)
                {
                    if(dialog.SelectedPreset())
                    {
                        m_Engine.SaveCurrentPreset(*dialog.SelectedPreset(), dialog.PresetName());
                    }
                }
            });
        });
        m_SaveProjectMenuItem.signal_activate().connect([this](){
            DoAndShowException([this](){
                m_Engine.SaveProject();
            });
        });
        m_ReverbGuiMenuItem.signal_activate().connect([this](){
            DoAndShowException([this](){
                auto reverb = m_Engine.Project().Reverb().ChangeShowGui(!m_Engine.Project().Reverb().ShowGui());
                auto project = m_Engine.Project().ChangeReverb(std::move(reverb));
                m_Engine.SetProject(std::move(project));
            });
        });
        m_ReverbChoosePluginMenuItem.signal_activate().connect([this](){
            DoAndShowException([this](){
                PluginSelectorDialog dialog;
                int result = dialog.run();
                if(result == Gtk::RESPONSE_OK)
                {
                    auto reverburi = dialog.SelectedPluginUri().value();
                    m_Engine.ChangeReverbLv2Uri(std::move(reverburi));
                }
            });
        });
        m_ReverbStorePresetMenuItem.signal_activate().connect([this](){
            DoAndShowException([this](){
                m_Engine.StoreReverbPreset();
            });
        });
        m_PopupMenu.append(m_SavePresetMenuItem);
        m_PopupMenu.append(m_SaveProjectMenuItem);
        m_PopupMenu.append(m_ReverbGuiMenuItem);
        m_PopupMenu.append(m_ReverbStorePresetMenuItem);
        m_PopupMenu.append(m_ReverbChoosePluginMenuItem);
        m_PopupMenu.show_all();
        Update();
    }
    void Update()
    {
        if(m_FocusedTab == FocusedTab::Presets)
        {
            m_MainPanelStack.set_visible_child(*m_PresetsPanel);
        }
        else if(m_FocusedTab == FocusedTab::Instruments)
        {
            m_MainPanelStack.set_visible_child(*m_InstrumentsPanel);
        }
    }
    const FocusedTab& focusedTab() const { return m_FocusedTab; }
    void SetFocusedTab(const FocusedTab& focusedTab) 
    { 
        m_FocusedTab = focusedTab;
        m_TopBar->Update();
        Update();
    }
    engine::Engine& Engine() { return m_Engine; }
    Gtk::Menu& PopupMenu() { return m_PopupMenu; }

private:
    FocusedTab m_FocusedTab = FocusedTab::Presets;
    engine::Engine &m_Engine;
    utils::NotifySink m_OnProjectChanged {m_Engine.OnProjectChanged(), [this](){Update();}};
    PartsContainer m_PartsContainer;
    Gtk::Box m_Box1 {Gtk::ORIENTATION_VERTICAL};
    Gtk::Box m_Box2 {Gtk::ORIENTATION_HORIZONTAL};
    Gtk::Menu m_PopupMenu;
    std::unique_ptr<TopBar> m_TopBar {std::make_unique<TopBar>(*this)};
    std::unique_ptr<PresetsPanel> m_PresetsPanel { std::make_unique<PresetsPanel>(m_Engine) };
    std::unique_ptr<InstrumentsPanel> m_InstrumentsPanel { std::make_unique<InstrumentsPanel>(m_Engine) };
    Gtk::Stack m_MainPanelStack;
    Gtk::MenuItem m_SavePresetMenuItem {"Save Preset"};
    Gtk::MenuItem m_SaveProjectMenuItem {"Save Project"};
    Gtk::MenuItem m_ReverbGuiMenuItem {"Reverb: Gui"};
    Gtk::MenuItem m_ReverbStorePresetMenuItem {"Reverb: Store Preset"};
    Gtk::MenuItem m_ReverbChoosePluginMenuItem {"Reverb: Choose plugin"};
};

} // anonymous namespace

Gtk::ApplicationWindow* guiCreateApplicationWindow(engine::Engine &engine)
{
    return new ApplicationWindow(engine);
}

